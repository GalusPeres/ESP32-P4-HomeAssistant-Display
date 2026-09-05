#include "src/web/server/web_admin.h"
#include "src/web/server/web_admin_utils.h"
#include "src/network/network_manager.h"
#include "src/core/firmware/firmware_metadata.h"
#include "src/core/firmware/firmware_version.h"
#include "src/devices/guition_esp32_4848s040/s3_diagnostics.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <algorithm>
#include <string.h>
#include <lwip/sockets.h>
#include "src/web/server/handlers/web_admin_handler_utils.h"

namespace {

constexpr size_t kOtaStagingBlockBytes = 256 * 1024;
constexpr size_t kOtaMaxStagingBlocks = 64;
static_assert(kOtaStagingBlockBytes >=
              firmware_meta::kDeviceDescriptorImageBytes);
struct OtaUploadState {
  bool upload_started = false;
  bool upload_success = false;
  bool upload_prepared = false;
  bool image_validated = false;
  bool install_started = false;
  bool install_success = false;
  bool restart_pending = false;
  uint32_t restart_at_ms = 0;
  uint32_t prepared_at_ms = 0;
  size_t buffered_len = 0;
  size_t upload_total_bytes = 0;
  size_t upload_received_bytes = 0;
  size_t install_total_bytes = 0;
  size_t install_written_bytes = 0;
  size_t next_progress_log = 0;
  uint8_t* staging_blocks[kOtaMaxStagingBlocks] = {};
  size_t staging_block_count = 0;
  size_t staging_capacity = 0;
  size_t staged_bytes = 0;
  uint8_t buffered_bytes[firmware_meta::kDeviceDescriptorImageBytes] = {0};
  String prepared_filename;
  String upload_filename;
  String error;
};

OtaUploadState g_ota_upload_state;
bool g_ota_display_reduced = false;
bool g_ota_display_restore_pending = false;
uint32_t g_ota_display_restore_retry_at = 0;
uint16_t g_ota_display_restore_attempts = 0;
bool g_ota_storage_guard_active = false;

constexpr uint32_t kOtaDisplayRestoreRetryMs = 750;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
// Supported P4 profiles are sized for full-image PSRAM staging, and allocation
// must succeed completely before flash writes can start. When WiFi is active,
// ESP-Hosted uses SDIO; starting Update while the browser is still sending
// consumes about 75 KB of internal/DMA RAM. Receive the complete image first
// so SDIO RX and flash writes never compete for that memory.
constexpr bool kStageWebOtaInPsram = true;
#else
constexpr bool kStageWebOtaInPsram = false;
#endif
#if defined(DEVICE_ESP32_S3_RGB_480)
constexpr bool kRequireWebOtaSize = true;
#else
constexpr bool kRequireWebOtaSize = kStageWebOtaInPsram;
#endif
constexpr size_t kOtaFlashWriteChunk = 16 * 1024;
constexpr const char* kOtaRawContentType = "application/octet-stream";
constexpr const char* kOtaRawFilenameHeader = "X-HomeTiles-OTA-Filename";

void logOtaMemory(const char* tag) {
  Serial.printf("[OTA/Mem] %s | Int free=%u KB | DMA free=%u KB | DMA largest=%u KB | PSRAM free=%u KB | MQTT buf=%u B\n",
                tag ? tag : "?",
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) / 1024),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                static_cast<unsigned>(networkManager.getMqttBufferSize()));
  Serial.flush();
}

void beginOtaStorageGuard() {
#if defined(DEVICE_ESP32_S3_RGB_480)
  if (!g_ota_storage_guard_active) {
    Device::storageWriteBegin();
    g_ota_storage_guard_active = true;
  }
#endif
}

void endOtaStorageGuard() {
#if defined(DEVICE_ESP32_S3_RGB_480)
  if (g_ota_storage_guard_active) {
    g_ota_storage_guard_active = false;
    Device::storageWriteEnd();
  }
#endif
}

void prepareDisplayForOtaInstall() {
  logOtaMemory("before-ota-prep");
  displayManager.setInputEnabled(false);
  BoardHAL::displayPowerSaveOn();

#if defined(CONFIG_IDF_TARGET_ESP32P4)
  // The ESP32-P4 build backports ESP-Hosted's PSRAM-first transport allocator.
  // Keep the fast UI band alive across OTA: freeing and reallocating its large
  // internal DMA block fragmented the heap and could permanently leave the UI
  // on the slow PSRAM fallback after an otherwise failed upload.
  g_ota_display_reduced = false;
#else
  // On targets without the P4 ESP-Hosted patch, temporarily reduce the draw
  // buffer while the OTA connection is active.
  if (!g_ota_display_reduced) {
    if (displayManager.setBufferLines(8)) {
      g_ota_display_reduced = true;
    } else {
      Serial.println(
          "[OTA] WARNING: Display buffer could not be reduced for OTA");
    }
  }
#endif
  g_ota_display_restore_pending = false;
  g_ota_display_restore_retry_at = 0;
  g_ota_display_restore_attempts = 0;

  networkManager.prepareMqttForOta();
  logOtaMemory("after-ota-prep");
}

void invalidateDisplayAfterOtaRestore() {
  lv_display_t* disp = displayManager.getDisplay();
  if (!disp) return;
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(disp);
}

bool tryRestoreFastDisplayAfterOta() {
  if (!g_ota_display_reduced) {
    g_ota_display_restore_pending = false;
    return true;
  }

  ++g_ota_display_restore_attempts;
  const bool restored = displayManager.restoreBufferLinesAfterOta(
      SCREEN_HEIGHT / Device::kDisplayFlushBands);
  if (restored && displayManager.isUsingFastInternalBuffer()) {
    g_ota_display_reduced = false;
    g_ota_display_restore_pending = false;
    g_ota_display_restore_retry_at = 0;
    Serial.printf(
        "[OTA] Fast display buffer restored after %u attempt(s)\n",
        static_cast<unsigned>(g_ota_display_restore_attempts));
    return true;
  }

  // The HTTP upload callback still owns active TCP/RX buffers. Keep the
  // small PSRAM buffer and retry after the connection releases its buffers;
  // never report a PSRAM fallback as successful restoration.
  g_ota_display_restore_pending = true;
  g_ota_display_restore_retry_at = millis() + kOtaDisplayRestoreRetryMs;
  if (g_ota_display_restore_attempts == 1 ||
      (g_ota_display_restore_attempts % 8) == 0) {
    Serial.printf(
        "[OTA] Fast display restore deferred (attempt %u)\n",
        static_cast<unsigned>(g_ota_display_restore_attempts));
  }
  return false;
}

void restoreDisplayAfterOtaFailure() {
  // Complete the Guition S3's hidden RGB/DMA resynchronisation before the
  // panel is exposed again after a failed or aborted flash write.
  endOtaStorageGuard();
  BoardHAL::displayPowerSaveOff();
  const bool fast_display_restored = tryRestoreFastDisplayAfterOta();
  if (fast_display_restored) {
    // Grow and reconnect MQTT only after the fast display buffer has secured
    // its contiguous DMA block.
    networkManager.restoreMqttBufferNormal();
  }
  displayManager.setInputEnabled(true);
  invalidateDisplayAfterOtaRestore();
  logOtaMemory(
      fast_display_restored ? "after-ota-restore" : "ota-restore-pending");
}

void releaseOtaStagingBuffer() {
  for (size_t i = 0; i < g_ota_upload_state.staging_block_count; ++i) {
    if (g_ota_upload_state.staging_blocks[i]) {
      heap_caps_free(g_ota_upload_state.staging_blocks[i]);
      g_ota_upload_state.staging_blocks[i] = nullptr;
    }
  }
  g_ota_upload_state.staging_block_count = 0;
  g_ota_upload_state.staging_capacity = 0;
  g_ota_upload_state.staged_bytes = 0;
}

bool hasOtaStagingBuffer() {
  return g_ota_upload_state.staging_block_count > 0 &&
         g_ota_upload_state.staging_blocks[0] != nullptr;
}

bool copyToOtaStagingBuffer(size_t offset, const uint8_t* data, size_t len) {
  if (!data || len == 0) return true;
  if (!hasOtaStagingBuffer() ||
      offset > g_ota_upload_state.staging_capacity ||
      len > g_ota_upload_state.staging_capacity - offset) {
    return false;
  }

  size_t copied = 0;
  while (copied < len) {
    const size_t absolute_offset = offset + copied;
    const size_t block_index = absolute_offset / kOtaStagingBlockBytes;
    const size_t offset_in_block = absolute_offset % kOtaStagingBlockBytes;
    if (block_index >= g_ota_upload_state.staging_block_count ||
        !g_ota_upload_state.staging_blocks[block_index]) {
      return false;
    }
    const size_t chunk =
        std::min(len - copied, kOtaStagingBlockBytes - offset_in_block);
    memcpy(g_ota_upload_state.staging_blocks[block_index] + offset_in_block,
           data + copied, chunk);
    copied += chunk;
  }
  return true;
}

bool allocateOtaStagingBuffer(size_t size) {
  if (!kStageWebOtaInPsram) return true;
  if (size == 0) {
    g_ota_upload_state.error = "Firmware size is missing";
    return false;
  }
  if (hasOtaStagingBuffer() &&
      g_ota_upload_state.staging_capacity == size) {
    return true;
  }

  releaseOtaStagingBuffer();
  const size_t required_blocks =
      size / kOtaStagingBlockBytes +
      ((size % kOtaStagingBlockBytes) != 0 ? 1 : 0);
  if (required_blocks == 0 || required_blocks > kOtaMaxStagingBlocks) {
    g_ota_upload_state.error = "Firmware is too large for safe upload";
    return false;
  }

  for (size_t i = 0; i < required_blocks; ++i) {
    const size_t allocated_before = i * kOtaStagingBlockBytes;
    const size_t block_size =
        std::min(kOtaStagingBlockBytes, size - allocated_before);
    uint8_t* block = static_cast<uint8_t*>(
        heap_caps_malloc(block_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!block) {
      g_ota_upload_state.error =
          "Not enough PSRAM for safe firmware upload";
      Serial.printf(
          "[OTA] PSRAM staging block allocation failed: block=%u/%u "
          "size=%u KB free=%u KB largest=%u KB\n",
          static_cast<unsigned>(i + 1),
          static_cast<unsigned>(required_blocks),
          static_cast<unsigned>(block_size / 1024),
          static_cast<unsigned>(
              heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
          static_cast<unsigned>(
              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
      releaseOtaStagingBuffer();
      return false;
    }
    g_ota_upload_state.staging_blocks[i] = block;
    g_ota_upload_state.staging_block_count = i + 1;
  }

  g_ota_upload_state.staging_capacity = size;
  g_ota_upload_state.staged_bytes = 0;
  Serial.printf(
      "[OTA] Safe Web-Upload staging ready: %u KB in %u PSRAM blocks "
      "(max %u KB/block, flash starts after complete RX)\n",
      static_cast<unsigned>(size / 1024),
      static_cast<unsigned>(required_blocks),
      static_cast<unsigned>(kOtaStagingBlockBytes / 1024));
  return true;
}

void resetOtaUploadState() {
  if (Update.isRunning()) {
    Update.abort();
  }
  endOtaStorageGuard();
  releaseOtaStagingBuffer();
  g_ota_upload_state.upload_started = false;
  g_ota_upload_state.upload_success = false;
  g_ota_upload_state.upload_prepared = false;
  g_ota_upload_state.image_validated = false;
  g_ota_upload_state.install_started = false;
  g_ota_upload_state.install_success = false;
  g_ota_upload_state.restart_pending = false;
  g_ota_upload_state.restart_at_ms = 0;
  g_ota_upload_state.prepared_at_ms = 0;
  g_ota_upload_state.buffered_len = 0;
  g_ota_upload_state.upload_total_bytes = 0;
  g_ota_upload_state.upload_received_bytes = 0;
  g_ota_upload_state.install_total_bytes = 0;
  g_ota_upload_state.install_written_bytes = 0;
  g_ota_upload_state.next_progress_log = 0;
  g_ota_upload_state.prepared_filename = "";
  g_ota_upload_state.upload_filename = "";
  g_ota_upload_state.error = "";
}

bool otaFilenameLooksLikeFactory(const String& filename) {
  String lowered = filename;
  lowered.toLowerCase();
  return lowered.indexOf("factory") >= 0;
}

bool parseOtaSizeValue(const String& raw, size_t& parsed) {
  parsed = 0;
  if (!raw.length()) return false;

  size_t value = 0;
  for (size_t i = 0; i < raw.length(); ++i) {
    const char ch = raw.charAt(i);
    if (ch < '0' || ch > '9') return false;
    const size_t digit = static_cast<size_t>(ch - '0');
    if (value > (SIZE_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  if (value == 0) return false;
  parsed = value;
  return true;
}

size_t parseOtaExpectedSize(WebServer& server) {
  if (!server.hasArg("size")) return 0;
  size_t parsed = 0;
  return parseOtaSizeValue(server.arg("size"), parsed) ? parsed : 0;
}

bool beginDirectOtaInstall() {
  if (g_ota_upload_state.install_started) {
    return true;
  }

  g_ota_upload_state.install_started = true;
  g_ota_upload_state.install_success = false;
  g_ota_upload_state.restart_pending = false;
  g_ota_upload_state.restart_at_ms = 0;
  g_ota_upload_state.install_written_bytes = 0;
  g_ota_upload_state.install_total_bytes = g_ota_upload_state.upload_total_bytes;
  g_ota_upload_state.next_progress_log = 512 * 1024;

  if (g_ota_upload_state.install_total_bytes > 0) {
    Serial.printf("[OTA] Install started: %u bytes\n", static_cast<unsigned>(g_ota_upload_state.install_total_bytes));
  } else {
    Serial.println("[OTA] Install started: size unknown");
  }

  prepareDisplayForOtaInstall();
  delay(20);

  const size_t ota_size = g_ota_upload_state.install_total_bytes ? g_ota_upload_state.install_total_bytes : UPDATE_SIZE_UNKNOWN;
#if defined(DEVICE_ESP32_S3_RGB_480)
  GuitionS3Diagnostics::logOtaPartitions("web-begin");
  const esp_partition_t* next_partition =
      esp_ota_get_next_update_partition(nullptr);
  if (!next_partition) {
    g_ota_upload_state.error = "OTA begin failed: no next OTA partition";
    Serial.println(
        "[OTA] Install failed: no eligible next OTA partition at runtime");
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return false;
  }
  if (ota_size != UPDATE_SIZE_UNKNOWN && ota_size > next_partition->size) {
    g_ota_upload_state.error = "OTA begin failed: image exceeds next OTA slot";
    Serial.printf(
        "[OTA] Install failed: image=%u exceeds partition=%s size=%u\n",
        static_cast<unsigned>(ota_size), next_partition->label,
        static_cast<unsigned>(next_partition->size));
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return false;
  }
#if HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE
  Serial.printf(
      "[S3Diag/WebOTA] phase=Update.begin target=%s addr=0x%08lX "
      "slot_size=%lu expected=%u\n",
      next_partition->label,
      static_cast<unsigned long>(next_partition->address),
      static_cast<unsigned long>(next_partition->size),
      static_cast<unsigned>(ota_size));
#endif
#endif
  beginOtaStorageGuard();
  if (!Update.begin(ota_size, U_FLASH)) {
    const uint8_t update_error = Update.getError();
    const String update_error_text = Update.errorString();
    Serial.printf(
        "[OTA] Install failed: Update.begin() code=%u -> %s\n",
        static_cast<unsigned>(update_error), update_error_text.c_str());
    g_ota_upload_state.error =
        String("OTA begin failed [") +
        String(static_cast<unsigned>(update_error)) + "]: " +
        update_error_text;
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return false;
  }

  if (ota_size == UPDATE_SIZE_UNKNOWN) {
    Serial.println("[OTA] Update.begin OK, target size: unknown");
  } else {
    Serial.printf("[OTA] Update.begin OK, target size: %u\n", static_cast<unsigned>(ota_size));
  }
  return true;
}

bool writeDirectOtaChunk(const uint8_t* data, size_t len) {
  if (!data || len == 0) {
    return true;
  }

  const size_t bytes_written = Update.write(const_cast<uint8_t*>(data), len);
  if (bytes_written != len) {
    const uint8_t update_error = Update.getError();
    const String update_error_text = Update.errorString();
    Serial.printf(
        "[OTA] Install failed: Update.write() requested=%u wrote=%u "
        "total=%u/%u code=%u -> %s\n",
        static_cast<unsigned>(len), static_cast<unsigned>(bytes_written),
        static_cast<unsigned>(g_ota_upload_state.install_written_bytes +
                              bytes_written),
        static_cast<unsigned>(g_ota_upload_state.install_total_bytes),
        static_cast<unsigned>(update_error), update_error_text.c_str());
    Update.abort();
    g_ota_upload_state.error =
        String("OTA write failed [") +
        String(static_cast<unsigned>(update_error)) + "]: " +
        update_error_text;
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return false;
  }

  g_ota_upload_state.install_written_bytes += bytes_written;
  if (g_ota_upload_state.install_written_bytes >= g_ota_upload_state.next_progress_log ||
      g_ota_upload_state.install_written_bytes == g_ota_upload_state.install_total_bytes) {
    if (g_ota_upload_state.install_total_bytes > 0) {
      Serial.printf("[OTA] Install progress: %u / %u bytes\n",
                    static_cast<unsigned>(g_ota_upload_state.install_written_bytes),
                    static_cast<unsigned>(g_ota_upload_state.install_total_bytes));
    } else {
      Serial.printf("[OTA] Install progress: %u bytes written\n",
                    static_cast<unsigned>(g_ota_upload_state.install_written_bytes));
    }
    g_ota_upload_state.next_progress_log += 512 * 1024;
  }
  return true;
}

bool validateRawOtaImageMetadata(const uint8_t* image, size_t image_len) {
  firmware_meta::DeviceDescriptor incoming_desc{};
  if (!firmware_meta::parseDeviceDescriptorFromImage(
          image, image_len, incoming_desc)) {
    g_ota_upload_state.error = "Firmware metadata missing or invalid";
    return false;
  }
  if (!firmware_meta::matchesCurrentDeviceKey(incoming_desc.device_key)) {
    g_ota_upload_state.error =
        String("Firmware device mismatch: got ") + incoming_desc.display_name +
        ", expected " + firmware_meta::expectedDeviceDisplayName();
    return false;
  }
  if (strcmp(incoming_desc.project_key,
             firmware_meta::currentProjectKey()) != 0) {
    g_ota_upload_state.error =
        String("Firmware project mismatch: got ") + incoming_desc.project_key +
        ", expected " + firmware_meta::currentProjectKey();
    return false;
  }
  const auto& current_silicon =
      firmware_meta::currentSiliconRevisionDescriptor();
  const uint16_t chip_revision = ESP.getChipRevision();
  if (chip_revision < current_silicon.minimum_revision ||
      chip_revision > current_silicon.maximum_revision) {
    g_ota_upload_state.error =
        String("Current firmware silicon range ") +
        current_silicon.minimum_revision + "-" +
        current_silicon.maximum_revision +
        " does not support chip revision " + chip_revision;
    return false;
  }
  bool accepted_legacy_silicon = false;
  if (!firmware_meta::imageMatchesCurrentSiliconVariant(
          image, image_len, &accepted_legacy_silicon)) {
    firmware_meta::SiliconRevisionDescriptor incoming_silicon{};
    if (firmware_meta::parseSiliconRevisionDescriptorFromImage(
            image, image_len, incoming_silicon)) {
      if (firmware_meta::matchesCurrentSiliconVariant(
              incoming_silicon.variant)) {
        if (!firmware_meta::matchesCurrentSiliconRevisionRange(
                incoming_silicon.minimum_revision,
                incoming_silicon.maximum_revision)) {
          g_ota_upload_state.error =
              String("Firmware silicon range ") +
              incoming_silicon.minimum_revision + "-" +
              incoming_silicon.maximum_revision +
              " exceeds supported range " +
              current_silicon.minimum_revision + "-" +
              current_silicon.maximum_revision;
        } else {
          g_ota_upload_state.error =
              String("Firmware silicon range ") +
              incoming_silicon.minimum_revision + "-" +
              incoming_silicon.maximum_revision +
              " does not support chip revision " + ESP.getChipRevision();
        }
      } else {
        g_ota_upload_state.error =
            String("Firmware silicon mismatch: got ") +
            incoming_silicon.variant + ", expected " +
            firmware_meta::currentSiliconVariant();
      }
    } else {
      g_ota_upload_state.error =
          String("Legacy firmware is not safe for silicon variant ") +
          firmware_meta::currentSiliconVariant();
    }
    return false;
  }
  if (accepted_legacy_silicon) {
    Serial.printf(
        "[OTA] Accepted legacy firmware metadata for silicon variant %s\n",
        firmware_meta::currentSiliconVariant());
  }
  g_ota_upload_state.image_validated = true;
  return true;
}

void beginRawOtaUpload(WebServer& server) {
  int receive_buffer_bytes = 8 * 1024;
  server.client().setSocketOption(SOL_SOCKET, SO_RCVBUF,
                                  &receive_buffer_bytes,
                                  sizeof(receive_buffer_bytes));

  // Arduino-ESP32 3.3.7 does not parse query arguments in its raw-body branch.
  // Capture the prepared contract before changing state, and use collected
  // request headers for values that must belong to this raw request.
  const bool was_prepared = g_ota_upload_state.upload_prepared;
  const size_t prepared_size = g_ota_upload_state.upload_total_bytes;
  const String prepared_filename = g_ota_upload_state.prepared_filename;
  const String encoded_filename = server.header(kOtaRawFilenameHeader);
  const String request_filename =
      encoded_filename.length() ? WebServer::urlDecode(encoded_filename) : "";
  size_t content_length = 0;
  const bool has_valid_content_length =
      parseOtaSizeValue(server.header("Content-Length"), content_length);

  if (!was_prepared) {
    resetOtaUploadState();
  }
  g_ota_upload_state.upload_started = true;
  g_ota_upload_state.upload_prepared = false;
  g_ota_upload_state.upload_received_bytes = 0;
  g_ota_upload_state.upload_filename =
      request_filename.length() ? request_filename : prepared_filename;
  g_ota_upload_state.upload_total_bytes =
      prepared_size ? prepared_size : content_length;
  g_ota_upload_state.install_total_bytes =
      g_ota_upload_state.upload_total_bytes;

  if (Update.isRunning()) Update.abort();
  endOtaStorageGuard();
  Update.clearError();

  String content_type = server.header("Content-Type");
  const int parameters_at = content_type.indexOf(';');
  if (parameters_at >= 0) content_type.remove(parameters_at);
  content_type.trim();
  if (!content_type.equalsIgnoreCase(kOtaRawContentType)) {
    g_ota_upload_state.error =
        "Raw OTA upload requires application/octet-stream";
    return;
  }
  if (!has_valid_content_length) {
    g_ota_upload_state.error =
        "Raw OTA Content-Length is missing or invalid";
    return;
  }
  if (prepared_size > 0 && content_length != prepared_size) {
    g_ota_upload_state.error =
        "Firmware size does not match the prepared upload";
    Serial.printf(
        "[OTA] Raw upload size mismatch: prepared=%u content_length=%u\n",
        static_cast<unsigned>(prepared_size),
        static_cast<unsigned>(content_length));
    return;
  }
  if (prepared_filename.length() > 0 && request_filename.length() > 0 &&
      prepared_filename != request_filename) {
    g_ota_upload_state.error =
        "Firmware filename does not match the prepared upload";
    return;
  }
  if (!g_ota_upload_state.upload_filename.length()) {
    g_ota_upload_state.error = "No firmware file received";
    return;
  }
  if (!endsWithIgnoreCase(g_ota_upload_state.upload_filename, ".bin")) {
    g_ota_upload_state.error = "Please upload a .bin firmware file";
    return;
  }
  if (otaFilenameLooksLikeFactory(g_ota_upload_state.upload_filename)) {
    g_ota_upload_state.error =
        "Please upload the update.bin, not the factory.bin";
    return;
  }
  if (g_ota_upload_state.upload_total_bytes == 0) {
    g_ota_upload_state.error = "Firmware size is missing";
    return;
  }

  if (!was_prepared) prepareDisplayForOtaInstall();
  if (!allocateOtaStagingBuffer(g_ota_upload_state.upload_total_bytes)) {
    return;
  }
  if (hasOtaStagingBuffer()) {
    g_ota_upload_state.next_progress_log = 512 * 1024;
  }
  Serial.printf("[OTA] Raw upload started: %s (%u bytes)\n",
                g_ota_upload_state.upload_filename.c_str(),
                static_cast<unsigned>(
                    g_ota_upload_state.upload_total_bytes));
#if HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE
  Serial.printf(
      "[S3Diag/WebOTA] phase=raw-upload-start file=%s announced=%u "
      "staged=%u\n",
      g_ota_upload_state.upload_filename.c_str(),
      static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
      hasOtaStagingBuffer() ? 1U : 0U);
#endif
  Serial.flush();
}

void writeRawOtaChunk(const uint8_t* data, size_t len) {
  if (g_ota_upload_state.error.length() > 0 ||
      !g_ota_upload_state.upload_started || !data || len == 0) {
    return;
  }
  if (g_ota_upload_state.upload_received_bytes >
          g_ota_upload_state.upload_total_bytes ||
      len > g_ota_upload_state.upload_total_bytes -
                g_ota_upload_state.upload_received_bytes) {
    g_ota_upload_state.error =
        "Firmware upload exceeds announced size";
    Serial.printf(
        "[OTA] Raw upload overflow: received=%u chunk=%u expected=%u\n",
        static_cast<unsigned>(g_ota_upload_state.upload_received_bytes),
        static_cast<unsigned>(len),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes));
    return;
  }
  g_ota_upload_state.upload_received_bytes += len;

  if (hasOtaStagingBuffer()) {
    if (!copyToOtaStagingBuffer(g_ota_upload_state.staged_bytes, data, len)) {
      g_ota_upload_state.error = "Firmware staging write failed";
      return;
    }
    g_ota_upload_state.staged_bytes += len;

    if (!g_ota_upload_state.image_validated &&
        g_ota_upload_state.staged_bytes >=
            firmware_meta::kDeviceDescriptorImageBytes &&
        !validateRawOtaImageMetadata(
            g_ota_upload_state.staging_blocks[0],
            std::min(g_ota_upload_state.staged_bytes,
                     kOtaStagingBlockBytes))) {
      return;
    }
    if (g_ota_upload_state.staged_bytes >=
        g_ota_upload_state.next_progress_log) {
      Serial.printf("[OTA] Raw upload RX progress: %u / %u bytes\n",
                    static_cast<unsigned>(
                        g_ota_upload_state.staged_bytes),
                    static_cast<unsigned>(
                        g_ota_upload_state.staging_capacity));
      g_ota_upload_state.next_progress_log += 512 * 1024;
    }
    return;
  }

  size_t buffered_copy_len = 0;
  if (g_ota_upload_state.buffered_len <
      sizeof(g_ota_upload_state.buffered_bytes)) {
    const size_t remaining =
        sizeof(g_ota_upload_state.buffered_bytes) -
        g_ota_upload_state.buffered_len;
    buffered_copy_len = std::min(remaining, len);
    memcpy(g_ota_upload_state.buffered_bytes +
               g_ota_upload_state.buffered_len,
           data, buffered_copy_len);
    g_ota_upload_state.buffered_len += buffered_copy_len;
  }

  if (!g_ota_upload_state.image_validated) {
    if (g_ota_upload_state.buffered_len <
        firmware_meta::kDeviceDescriptorImageBytes) {
      return;
    }
    if (!validateRawOtaImageMetadata(g_ota_upload_state.buffered_bytes,
                                    g_ota_upload_state.buffered_len)) {
      return;
    }
    if (!beginDirectOtaInstall()) return;
    if (!writeDirectOtaChunk(g_ota_upload_state.buffered_bytes,
                             g_ota_upload_state.buffered_len)) {
      return;
    }
    g_ota_upload_state.buffered_len = 0;
    if (len > buffered_copy_len) {
      writeDirectOtaChunk(data + buffered_copy_len,
                          len - buffered_copy_len);
    }
    return;
  }

  writeDirectOtaChunk(data, len);
}

void abortRawOtaUpload() {
  g_ota_upload_state.error = "OTA upload aborted";
  Serial.printf(
      "[OTA] Raw upload aborted: received=%u / %u bytes, "
      "flash_written=%u\n",
      static_cast<unsigned>(g_ota_upload_state.upload_received_bytes),
      static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
      static_cast<unsigned>(g_ota_upload_state.install_written_bytes));
  if (Update.isRunning()) Update.abort();
  releaseOtaStagingBuffer();
  if (g_ota_upload_state.upload_started ||
      g_ota_upload_state.upload_prepared ||
      g_ota_upload_state.install_started || g_ota_display_reduced) {
    restoreDisplayAfterOtaFailure();
    g_ota_upload_state.install_started = false;
  }
}

void finishRawOtaUpload(size_t parser_total_bytes) {
  if (g_ota_upload_state.error.length() > 0 ||
      !g_ota_upload_state.upload_started) {
    return;
  }
  if (parser_total_bytes != g_ota_upload_state.upload_received_bytes ||
      parser_total_bytes != g_ota_upload_state.upload_total_bytes) {
    g_ota_upload_state.error =
        "Firmware upload ended before all bytes arrived";
    Serial.printf(
        "[OTA] Incomplete raw upload: parser=%u received=%u expected=%u\n",
        static_cast<unsigned>(parser_total_bytes),
        static_cast<unsigned>(g_ota_upload_state.upload_received_bytes),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes));
    return;
  }

  if (hasOtaStagingBuffer()) {
    if (g_ota_upload_state.staged_bytes != parser_total_bytes ||
        g_ota_upload_state.staged_bytes !=
            g_ota_upload_state.staging_capacity) {
      g_ota_upload_state.error =
          "Firmware upload ended before all bytes arrived";
      return;
    }
    if (!g_ota_upload_state.image_validated) {
      g_ota_upload_state.error =
          "Firmware metadata missing or incomplete";
      return;
    }

    g_ota_upload_state.upload_success = true;
    Serial.printf(
        "[OTA] Raw upload RX finished: %s (%u bytes in PSRAM)\n",
        g_ota_upload_state.upload_filename.c_str(),
        static_cast<unsigned>(parser_total_bytes));
    Serial.println(
        "[OTA] Network receive complete; starting flash install");
    if (!beginDirectOtaInstall()) {
      releaseOtaStagingBuffer();
      return;
    }

    size_t offset = 0;
    while (offset < g_ota_upload_state.staged_bytes) {
      const size_t block_index = offset / kOtaStagingBlockBytes;
      const size_t offset_in_block = offset % kOtaStagingBlockBytes;
      if (block_index >= g_ota_upload_state.staging_block_count ||
          !g_ota_upload_state.staging_blocks[block_index]) {
        g_ota_upload_state.error = "Firmware staging read failed";
        releaseOtaStagingBuffer();
        return;
      }
      const size_t remaining = g_ota_upload_state.staged_bytes - offset;
      const size_t chunk =
          std::min(std::min(remaining, kOtaFlashWriteChunk),
                   kOtaStagingBlockBytes - offset_in_block);
      if (!writeDirectOtaChunk(
              g_ota_upload_state.staging_blocks[block_index] +
                  offset_in_block,
              chunk)) {
        releaseOtaStagingBuffer();
        return;
      }
      offset += chunk;
      delay(0);
    }
    releaseOtaStagingBuffer();

    // Preserve the established P4 staging finalization semantics.
    if (!Update.end(true)) {
      const uint8_t update_error = Update.getError();
      const String update_error_text = Update.errorString();
      Serial.printf(
          "[OTA] Install failed: Update.end(true) code=%u -> %s\n",
          static_cast<unsigned>(update_error),
          update_error_text.c_str());
      Update.abort();
      g_ota_upload_state.error =
          String("OTA finalize failed [") +
          String(static_cast<unsigned>(update_error)) + "]: " +
          update_error_text;
      g_ota_upload_state.install_started = false;
      restoreDisplayAfterOtaFailure();
      return;
    }

    g_ota_upload_state.install_written_bytes =
        g_ota_upload_state.install_total_bytes;
    g_ota_upload_state.install_success = true;
    g_ota_upload_state.restart_pending = true;
    g_ota_upload_state.restart_at_ms = millis() + 1200;
    Serial.println(
        "[OTA] Install finished successfully, restarting device...");
    return;
  }

  if (!g_ota_upload_state.image_validated) {
    g_ota_upload_state.error =
        "Firmware metadata missing or incomplete";
    return;
  }
  if (!g_ota_upload_state.install_started) {
    g_ota_upload_state.error = "OTA install did not start";
    return;
  }
  if (g_ota_upload_state.install_written_bytes !=
      g_ota_upload_state.install_total_bytes) {
    g_ota_upload_state.error =
        "OTA finalize refused: received/written byte count mismatch";
    Serial.printf(
        "[OTA] Raw upload byte mismatch: received=%u announced=%u "
        "written=%u expected=%u\n",
        static_cast<unsigned>(parser_total_bytes),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
        static_cast<unsigned>(g_ota_upload_state.install_written_bytes),
        static_cast<unsigned>(g_ota_upload_state.install_total_bytes));
    if (Update.isRunning()) Update.abort();
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return;
  }
  g_ota_upload_state.upload_success = true;
  Serial.printf("[OTA] Raw upload finished: %s (%u bytes)\n",
                g_ota_upload_state.upload_filename.c_str(),
                static_cast<unsigned>(parser_total_bytes));

#if defined(DEVICE_ESP32_S3_RGB_480)
  if (!Update.end(false)) {
    const uint8_t update_error = Update.getError();
    const String update_error_text = Update.errorString();
    Serial.printf(
        "[OTA] Install failed: Update.end(false) code=%u -> %s\n",
        static_cast<unsigned>(update_error), update_error_text.c_str());
    Update.abort();
    g_ota_upload_state.error =
        String("OTA finalize failed [") +
        String(static_cast<unsigned>(update_error)) + "]: " +
        update_error_text;
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return;
  }
#else
  if (!Update.end(true)) {
    const uint8_t update_error = Update.getError();
    const String update_error_text = Update.errorString();
    Update.abort();
    Serial.printf(
        "[OTA] Install failed: Update.end(true) code=%u -> %s\n",
        static_cast<unsigned>(update_error), update_error_text.c_str());
    g_ota_upload_state.error =
        String("OTA finalize failed [") +
        String(static_cast<unsigned>(update_error)) + "]: " +
        update_error_text;
    g_ota_upload_state.install_started = false;
    restoreDisplayAfterOtaFailure();
    return;
  }
#endif

  endOtaStorageGuard();
  g_ota_upload_state.install_success = true;
  g_ota_upload_state.restart_pending = true;
  g_ota_upload_state.restart_at_ms = millis() + 1200;
  GuitionS3Diagnostics::logOtaPartitions("web-raw-end");
  Serial.println(
      "[OTA] Install finished successfully, restarting device...");
}

}  // namespace

void WebAdminServer::handlePrepareOtaUpload() {
  resetOtaUploadState();
  g_ota_upload_state.upload_prepared = true;
  g_ota_upload_state.prepared_at_ms = millis();
  g_ota_upload_state.upload_total_bytes = parseOtaExpectedSize(server);
  g_ota_upload_state.install_total_bytes = g_ota_upload_state.upload_total_bytes;
  if (server.hasArg("filename")) {
    g_ota_upload_state.prepared_filename = server.arg("filename");
  }

  if (g_ota_upload_state.prepared_filename.length() > 0 &&
      !endsWithIgnoreCase(g_ota_upload_state.prepared_filename, ".bin")) {
    resetOtaUploadState();
    sendJsonError(server, 400, "Please upload a .bin firmware file");
    return;
  }
  if (otaFilenameLooksLikeFactory(g_ota_upload_state.prepared_filename)) {
    resetOtaUploadState();
    sendJsonError(server, 400,
                  "Please upload the update.bin, not the factory.bin");
    return;
  }

  if (kRequireWebOtaSize &&
      g_ota_upload_state.upload_total_bytes == 0) {
    resetOtaUploadState();
    sendJsonError(server, 400, "Firmware size is missing");
    return;
  }

  Serial.println("[OTA] Preparing receiver before upload");
  prepareDisplayForOtaInstall();
  if (!allocateOtaStagingBuffer(g_ota_upload_state.upload_total_bytes)) {
    const String error = g_ota_upload_state.error;
    restoreDisplayAfterOtaFailure();
    resetOtaUploadState();
    sendJsonError(server, 507, error);
    return;
  }

  String json = "{\"success\":true";
  json += ",\"size\":";
  json += String(g_ota_upload_state.upload_total_bytes);
  json += "}";
  server.send(200, "application/json", json);
}

void WebAdminServer::handleOtaUpdate() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    // Limit the receive window: file uploads have the same crash cause
    // (internal DMA heap exhaustion with the default 64 KB TCP window).
    int rcvbuf = 8 * 1024;
    server.client().setSocketOption(SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    const bool was_prepared = g_ota_upload_state.upload_prepared;
#if defined(DEVICE_ESP32_S3_RGB_480)
    const size_t prepared_size = g_ota_upload_state.upload_total_bytes;
#endif
    if (!was_prepared) {
      resetOtaUploadState();
    }
    g_ota_upload_state.upload_started = true;
    g_ota_upload_state.upload_prepared = false;
    const size_t request_size = parseOtaExpectedSize(server);
#if defined(DEVICE_ESP32_S3_RGB_480)
    g_ota_upload_state.upload_total_bytes =
        request_size ? request_size : (was_prepared ? prepared_size : 0);
#else
    g_ota_upload_state.upload_total_bytes = request_size;
#endif
    g_ota_upload_state.install_total_bytes = g_ota_upload_state.upload_total_bytes;

    if (Update.isRunning()) {
      Update.abort();
    }
    endOtaStorageGuard();
    Update.clearError();

    if (!upload.filename.length()) {
      g_ota_upload_state.error = "No firmware file received";
      return;
    }
    if (!endsWithIgnoreCase(upload.filename, ".bin")) {
      g_ota_upload_state.error = "Please upload a .bin firmware file";
      return;
    }
    if (otaFilenameLooksLikeFactory(upload.filename)) {
      g_ota_upload_state.error = "Please upload the update.bin, not the factory.bin";
      return;
    }
#if defined(DEVICE_ESP32_S3_RGB_480)
    if (g_ota_upload_state.upload_total_bytes == 0) {
      g_ota_upload_state.error = "Firmware size is missing";
      return;
    }
#endif
    if (!was_prepared) {
      prepareDisplayForOtaInstall();
    }
    if (!allocateOtaStagingBuffer(g_ota_upload_state.upload_total_bytes)) {
      return;
    }
    if (hasOtaStagingBuffer()) {
      g_ota_upload_state.next_progress_log = 512 * 1024;
    }
    Serial.printf("[OTA] Upload started: %s\n", upload.filename.c_str());
#if HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE
    Serial.printf(
        "[S3Diag/WebOTA] phase=upload-start file=%s announced=%u "
        "staged=%u\n",
        upload.filename.c_str(),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
        hasOtaStagingBuffer() ? 1U : 0U);
#endif
    Serial.flush();
    return;
  }

  if (g_ota_upload_state.error.length() > 0 || !g_ota_upload_state.upload_started) {
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (hasOtaStagingBuffer()) {
      const size_t chunk_len = static_cast<size_t>(upload.currentSize);
      if (g_ota_upload_state.staged_bytes >
              g_ota_upload_state.staging_capacity ||
          chunk_len >
          g_ota_upload_state.staging_capacity -
              g_ota_upload_state.staged_bytes) {
        g_ota_upload_state.error =
            "Firmware upload exceeds announced size";
        Serial.printf(
            "[OTA] Staging overflow: received=%u chunk=%u capacity=%u\n",
            static_cast<unsigned>(g_ota_upload_state.staged_bytes),
            static_cast<unsigned>(chunk_len),
            static_cast<unsigned>(g_ota_upload_state.staging_capacity));
        return;
      }

      if (!copyToOtaStagingBuffer(g_ota_upload_state.staged_bytes,
                                  upload.buf, chunk_len)) {
        g_ota_upload_state.error = "Firmware staging write failed";
        return;
      }
      g_ota_upload_state.staged_bytes += chunk_len;

      if (!g_ota_upload_state.image_validated &&
          g_ota_upload_state.staged_bytes >=
              firmware_meta::kDeviceDescriptorImageBytes) {
        if (!validateRawOtaImageMetadata(
                g_ota_upload_state.staging_blocks[0],
                std::min(g_ota_upload_state.staged_bytes,
                         kOtaStagingBlockBytes))) {
          return;
        }
      }

      if (g_ota_upload_state.staged_bytes >=
          g_ota_upload_state.next_progress_log) {
        Serial.printf("[OTA] Upload RX progress: %u / %u bytes\n",
                      static_cast<unsigned>(
                          g_ota_upload_state.staged_bytes),
                      static_cast<unsigned>(
                          g_ota_upload_state.staging_capacity));
        g_ota_upload_state.next_progress_log += 512 * 1024;
      }
      return;
    }

    size_t buffered_copy_len = 0;

    if (g_ota_upload_state.buffered_len < sizeof(g_ota_upload_state.buffered_bytes)) {
      const size_t remaining = sizeof(g_ota_upload_state.buffered_bytes) - g_ota_upload_state.buffered_len;
      const size_t copy_len = std::min(remaining, static_cast<size_t>(upload.currentSize));
      memcpy(g_ota_upload_state.buffered_bytes + g_ota_upload_state.buffered_len,
             upload.buf,
             copy_len);
      g_ota_upload_state.buffered_len += copy_len;
      buffered_copy_len = copy_len;
    }

    if (!g_ota_upload_state.image_validated &&
        g_ota_upload_state.buffered_len <
            firmware_meta::kDeviceDescriptorImageBytes) {
      return;
    }

    if (!g_ota_upload_state.image_validated) {
      if (!validateRawOtaImageMetadata(
              g_ota_upload_state.buffered_bytes,
              g_ota_upload_state.buffered_len)) {
        return;
      }
      if (!beginDirectOtaInstall()) {
        return;
      }
      if (!writeDirectOtaChunk(g_ota_upload_state.buffered_bytes,
                               g_ota_upload_state.buffered_len)) {
        return;
      }
      g_ota_upload_state.buffered_len = 0;
      if (static_cast<size_t>(upload.currentSize) > buffered_copy_len) {
        const size_t remaining_in_chunk =
            static_cast<size_t>(upload.currentSize) - buffered_copy_len;
        if (!writeDirectOtaChunk(upload.buf + buffered_copy_len,
                                 remaining_in_chunk)) {
          return;
        }
      }
      return;
    }

    if (g_ota_upload_state.image_validated) {
      if (!writeDirectOtaChunk(upload.buf, upload.currentSize)) {
        return;
      }
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    g_ota_upload_state.error = "OTA upload aborted";
    Serial.printf(
        "[OTA] Upload aborted: received=%u / %u bytes, flash_written=%u\n",
        static_cast<unsigned>(
            hasOtaStagingBuffer()
                ? g_ota_upload_state.staged_bytes
                : upload.totalSize),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
        static_cast<unsigned>(
            g_ota_upload_state.install_written_bytes));
    if (Update.isRunning()) {
      Update.abort();
    }
    releaseOtaStagingBuffer();
    if (g_ota_upload_state.upload_started ||
        g_ota_upload_state.upload_prepared ||
        g_ota_upload_state.install_started ||
        g_ota_display_reduced) {
      restoreDisplayAfterOtaFailure();
      g_ota_upload_state.install_started = false;
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
#if defined(DEVICE_ESP32_S3_RGB_480)
    if (g_ota_upload_state.upload_total_bytes == 0 && upload.totalSize > 0) {
#else
    if (upload.totalSize > 0) {
#endif
      g_ota_upload_state.upload_total_bytes = upload.totalSize;
      g_ota_upload_state.install_total_bytes = upload.totalSize;
    }

    if (hasOtaStagingBuffer()) {
      if (g_ota_upload_state.staged_bytes != upload.totalSize ||
          g_ota_upload_state.staged_bytes !=
              g_ota_upload_state.staging_capacity) {
        g_ota_upload_state.error =
            "Firmware upload ended before all bytes arrived";
        Serial.printf(
            "[OTA] Incomplete staged upload: received=%u parser=%u "
            "expected=%u\n",
            static_cast<unsigned>(g_ota_upload_state.staged_bytes),
            static_cast<unsigned>(upload.totalSize),
            static_cast<unsigned>(
                g_ota_upload_state.staging_capacity));
        return;
      }
      if (!g_ota_upload_state.image_validated) {
        g_ota_upload_state.error =
            "Firmware metadata missing or incomplete";
        return;
      }

      g_ota_upload_state.upload_success = true;
      Serial.printf("[OTA] Upload RX finished: %s (%u bytes in PSRAM)\n",
                    upload.filename.c_str(),
                    static_cast<unsigned>(upload.totalSize));
      Serial.println(
          "[OTA] Network receive complete; starting flash install");

      if (!beginDirectOtaInstall()) {
        releaseOtaStagingBuffer();
        return;
      }

      size_t offset = 0;
      while (offset < g_ota_upload_state.staged_bytes) {
        const size_t block_index = offset / kOtaStagingBlockBytes;
        const size_t offset_in_block = offset % kOtaStagingBlockBytes;
        if (block_index >= g_ota_upload_state.staging_block_count ||
            !g_ota_upload_state.staging_blocks[block_index]) {
          g_ota_upload_state.error = "Firmware staging read failed";
          releaseOtaStagingBuffer();
          return;
        }
        const size_t remaining =
            g_ota_upload_state.staged_bytes - offset;
        const size_t chunk =
            std::min(
                std::min(remaining, kOtaFlashWriteChunk),
                kOtaStagingBlockBytes - offset_in_block);
        if (!writeDirectOtaChunk(
                g_ota_upload_state.staging_blocks[block_index] +
                    offset_in_block,
                chunk)) {
          releaseOtaStagingBuffer();
          return;
        }
        offset += chunk;
        delay(0);
      }
      releaseOtaStagingBuffer();

      // This staging branch is the established P4 flow. Keep its previous
      // finalisation semantics; the strict exact-size path below is S3-only.
      if (!Update.end(true)) {
        const uint8_t update_error = Update.getError();
        const String update_error_text = Update.errorString();
        Serial.printf(
            "[OTA] Install failed: Update.end(true) code=%u -> %s\n",
            static_cast<unsigned>(update_error), update_error_text.c_str());
        Update.abort();
        g_ota_upload_state.error =
            String("OTA finalize failed [") +
            String(static_cast<unsigned>(update_error)) + "]: " +
            update_error_text;
        g_ota_upload_state.install_started = false;
        restoreDisplayAfterOtaFailure();
        return;
      }

      g_ota_upload_state.install_written_bytes =
          g_ota_upload_state.install_total_bytes;
      g_ota_upload_state.install_success = true;
      g_ota_upload_state.restart_pending = true;
      g_ota_upload_state.restart_at_ms = millis() + 1200;
      Serial.println(
          "[OTA] Install finished successfully, restarting device...");
      return;
    }

    if (!g_ota_upload_state.image_validated) {
      g_ota_upload_state.error = "Firmware metadata missing or incomplete";
      return;
    }
    if (!g_ota_upload_state.install_started) {
      g_ota_upload_state.error = "OTA install did not start";
      return;
    }

    g_ota_upload_state.upload_success = true;
    Serial.printf("[OTA] Upload finished: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);

#if defined(DEVICE_ESP32_S3_RGB_480)
    if (upload.totalSize != g_ota_upload_state.upload_total_bytes ||
        g_ota_upload_state.install_written_bytes !=
            g_ota_upload_state.install_total_bytes) {
      g_ota_upload_state.error =
          "OTA finalize refused: received/written byte count mismatch";
      Serial.printf(
          "[OTA] Upload byte mismatch: parser=%u announced=%u "
          "written=%u expected=%u\n",
          static_cast<unsigned>(upload.totalSize),
          static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
          static_cast<unsigned>(g_ota_upload_state.install_written_bytes),
          static_cast<unsigned>(g_ota_upload_state.install_total_bytes));
      if (Update.isRunning()) Update.abort();
      g_ota_upload_state.install_started = false;
      restoreDisplayAfterOtaFailure();
      return;
    }

    if (!Update.end(false)) {
      const uint8_t update_error = Update.getError();
      const String update_error_text = Update.errorString();
      Serial.printf(
          "[OTA] Install failed: Update.end(false) code=%u -> %s\n",
          static_cast<unsigned>(update_error), update_error_text.c_str());
      Update.abort();
      g_ota_upload_state.error =
          String("OTA finalize failed [") +
          String(static_cast<unsigned>(update_error)) + "]: " +
          update_error_text;
      g_ota_upload_state.install_started = false;
      restoreDisplayAfterOtaFailure();
      return;
    }
#else
    if (!Update.end(true)) {
      const uint8_t update_error = Update.getError();
      const String update_error_text = Update.errorString();
      Update.abort();
      Serial.printf(
          "[OTA] Install failed: Update.end(true) code=%u -> %s\n",
          static_cast<unsigned>(update_error), update_error_text.c_str());
      g_ota_upload_state.error =
          String("OTA finalize failed [") +
          String(static_cast<unsigned>(update_error)) + "]: " +
          update_error_text;
      g_ota_upload_state.install_started = false;
      restoreDisplayAfterOtaFailure();
      return;
    }
#endif

    endOtaStorageGuard();
    g_ota_upload_state.install_written_bytes = g_ota_upload_state.install_total_bytes;
    g_ota_upload_state.install_success = true;
    g_ota_upload_state.restart_pending = true;
    g_ota_upload_state.restart_at_ms = millis() + 1200;
    GuitionS3Diagnostics::logOtaPartitions("web-end");
    Serial.println("[OTA] Install finished successfully, restarting device...");
  }
}

void WebAdminServer::handleOtaRawUpdate() {
  HTTPRaw& raw = server.raw();
  if (raw.status == RAW_START) {
    beginRawOtaUpload(server);
    return;
  }
  if (raw.status == RAW_WRITE) {
    writeRawOtaChunk(raw.buf, raw.currentSize);
    return;
  }
  if (raw.status == RAW_ABORTED) {
    abortRawOtaUpload();
    return;
  }
  if (raw.status == RAW_END) {
    finishRawOtaUpload(raw.totalSize);
  }
}

void WebAdminServer::handleOtaUploadDone() {
  if (!g_ota_upload_state.upload_started) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"No OTA upload started\"}");
    return;
  }

  if (g_ota_upload_state.error.length() > 0) {
    String json = "{\"success\":false,\"error\":\"";
    appendJsonEscaped(json, g_ota_upload_state.error);
    json += "\"}";
    Serial.printf(
        "[OTA] Request failed: %s (rx=%u/%u, flash=%u)\n",
        g_ota_upload_state.error.c_str(),
        static_cast<unsigned>(g_ota_upload_state.staged_bytes),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
        static_cast<unsigned>(
            g_ota_upload_state.install_written_bytes));
    restoreDisplayAfterOtaFailure();
    resetOtaUploadState();
    server.send(500, "application/json", json);
    return;
  }

  if (!g_ota_upload_state.upload_success || !g_ota_upload_state.install_success) {
    Serial.printf(
        "[OTA] Request incomplete without updater error "
        "(upload_success=%u install_success=%u rx=%u/%u flash=%u)\n",
        g_ota_upload_state.upload_success ? 1U : 0U,
        g_ota_upload_state.install_success ? 1U : 0U,
        static_cast<unsigned>(g_ota_upload_state.staged_bytes),
        static_cast<unsigned>(g_ota_upload_state.upload_total_bytes),
        static_cast<unsigned>(
            g_ota_upload_state.install_written_bytes));
    restoreDisplayAfterOtaFailure();
    resetOtaUploadState();
    server.send(500, "application/json", "{\"success\":false,\"error\":\"OTA update failed\"}");
    return;
  }

  String json = "{\"success\":true}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleStartOtaInstall() {
  server.send(410, "application/json", "{\"success\":false,\"error\":\"OTA install starts automatically during upload\"}");
}

void WebAdminServer::handleGetOtaStatus() {
  const size_t total = g_ota_upload_state.install_total_bytes;
  const size_t written = g_ota_upload_state.install_written_bytes;
  uint8_t percent = 0;
  if (g_ota_upload_state.install_success) {
    percent = 100;
  } else if (total > 0) {
    percent = static_cast<uint8_t>(std::min<size_t>(100, (written * 100) / total));
  }

  String json = "{\"success\":true,\"upload_ready\":";
  json += g_ota_upload_state.upload_success ? "true" : "false";
  json += ",\"installing\":";
  json += (g_ota_upload_state.install_started && !g_ota_upload_state.install_success && g_ota_upload_state.error.length() == 0) ? "true" : "false";
  json += ",\"install_started\":";
  json += g_ota_upload_state.install_started ? "true" : "false";
  json += ",\"install_success\":";
  json += g_ota_upload_state.install_success ? "true" : "false";
  json += ",\"percent\":";
  json += String(percent);
  json += ",\"written\":";
  json += String(written);
  json += ",\"total\":";
  json += String(total);
  json += ",\"error\":\"";
  appendJsonEscaped(json, g_ota_upload_state.error);
  json += "\"}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleGithubUpdateCheck() {
  if (!github_check_callback) {
    sendJsonError(server, 503, "GitHub update check is unavailable");
    return;
  }

  // Allow resetting a failed GitHub OTA even if the original browser tab
  // no longer polls its error status.
  if (github_install_requested && github_install_error.length() > 0) {
    github_install_requested = false;
    github_install_error = "";
    github_check_valid = false;
  }
  if (webAdminOtaInProgress() || github_install_requested) {
    sendJsonError(server, 409, "A firmware update is already in progress");
    return;
  }

  webAdminMarkActivity();
  last_github_check = github_check_callback();
  github_check_valid = true;

  String json = "{\"success\":";
  json += last_github_check.ok ? "true" : "false";
  json += ",\"update_available\":";
  json += last_github_check.update_available ? "true" : "false";
  json += ",\"latest_tag\":\"";
  appendJsonEscaped(json, last_github_check.latest_tag);
  json += "\",\"current_version\":\"";
  appendJsonEscaped(json, FW_VERSION);
  json += "\"}";
  sendChunkedResponse(server, last_github_check.ok ? 200 : 502, "application/json", json);
}

void WebAdminServer::handleGithubUpdateInstall() {
  if (!github_install_callback) {
    sendJsonError(server, 503, "GitHub update install is unavailable");
    return;
  }
  if (webAdminOtaInProgress() || github_install_requested) {
    sendJsonError(server, 409, "A firmware update is already in progress");
    return;
  }

  const String requested_tag = server.arg("tag");
  if (!github_check_valid || !last_github_check.ok || !last_github_check.update_available ||
      requested_tag != last_github_check.latest_tag) {
    sendJsonError(server, 412, "Check for updates before installing");
    return;
  }

  github_install_requested = true;
  github_install_error = "";
  github_install_callback(last_github_check.latest_tag);

  String json = "{\"success\":true,\"tag\":\"";
  appendJsonEscaped(json, last_github_check.latest_tag);
  json += "\"}";
  sendChunkedResponse(server, 202, "application/json", json);
}

void WebAdminServer::handleGetGithubUpdateStatus() {
  const bool clear_failed_install =
      github_install_requested && github_install_error.length() > 0;

  String json = "{\"success\":true,\"current_version\":\"";
  appendJsonEscaped(json, FW_VERSION);
  json += "\",\"install_requested\":";
  json += github_install_requested ? "true" : "false";
  json += ",\"install_error\":\"";
  appendJsonEscaped(json, github_install_error);
  json += "\"}";
  sendChunkedResponse(server, 200, "application/json", json);

  if (clear_failed_install) {
    github_install_requested = false;
    github_install_error = "";
    github_check_valid = false;
  }
}

bool webAdminOtaInProgress() {
  return g_ota_display_restore_pending ||
         (g_ota_upload_state.error.length() == 0 &&
          ((g_ota_upload_state.upload_prepared && !g_ota_upload_state.upload_started) ||
           (g_ota_upload_state.upload_started &&
            (!g_ota_upload_state.install_success || g_ota_upload_state.restart_pending))));
}

void webAdminServiceOta() {
  if (g_ota_display_restore_pending &&
      !Update.isRunning() &&
      (int32_t)(millis() - g_ota_display_restore_retry_at) >= 0) {
    if (tryRestoreFastDisplayAfterOta()) {
      networkManager.restoreMqttBufferNormal();
      invalidateDisplayAfterOtaRestore();
      logOtaMemory("after-deferred-ota-restore");
    }
  }

  if (g_ota_upload_state.upload_prepared && !g_ota_upload_state.upload_started) {
    const uint32_t prepared_at = g_ota_upload_state.prepared_at_ms;
    if (prepared_at != 0 && static_cast<uint32_t>(millis() - prepared_at) > 120000UL) {
      Serial.println("[OTA] Prepare timed out, restoring display");
      restoreDisplayAfterOtaFailure();
      resetOtaUploadState();
    }
  }

  if (!g_ota_upload_state.restart_pending || g_ota_upload_state.restart_at_ms == 0) {
    return;
  }
  if ((int32_t)(millis() - g_ota_upload_state.restart_at_ms) < 0) {
    return;
  }
  prepareDisplayForRestart();
  delay(50);
  BoardHAL::restart();
}

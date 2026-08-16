#include "src/web/web_admin.h"
#include "src/web/web_admin_html.h"
#include "src/core/i18n.h"
#include "src/core/pin_access.h"
#include "src/io/hardware_io.h"
#include "src/web/web_admin_utils.h"
#include "src/core/device_entities.h"
#include "src/network/ha_bridge_config.h"
#include "src/network/network_manager.h"
#include "src/network/network_transport.h"
#include "src/network/mqtt_handlers.h"
#include "src/ui/tab_settings.h"
#include "src/tiles/tile_config.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/ui_manager.h"
#include "src/ui/image_screensaver.h"
#include "src/ui/screensaver_config.h"
#include "src/ui/ui_surface_style.h"
#include "src/web/web_admin_tile_helpers.h"
#include "src/types/types_registry.h"
#include "src/types/clock/clock_format.h"
#include "src/types/energy/energy_data.h"
#include "src/devices/device.h"
#include "src/core/board_hal.h"
#include "src/core/crash_log.h"
#include "src/core/display_manager.h"
#include "src/core/firmware_metadata.h"
#include "src/core/firmware_version.h"
#include "src/devices/guition_esp32_4848s040/s3_diagnostics.h"
#if defined(DEVICE_GUITION_JC1060P470C)
#include "src/devices/guition_jc1060p470c/vendor/guition_sdmmc.h"
#endif
#include <LittleFS.h>
#include <Update.h>
#include <esp_core_dump.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_vfs_fat.h>
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <img_converters.h>
#else
#include <driver/jpeg_encode.h>
#endif
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <lvgl.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <libs/tjpgd/tjpgd.h>
#include <cerrno>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>

namespace {

void appendJsonEscaped(String& out, const String& value);

bool endsWithIgnoreCase(const String& value, const char* suffix) {
  if (!suffix) return false;
  String v = value;
  v.toLowerCase();
  String s = suffix;
  s.toLowerCase();
  return v.endsWith(s);
}

String joinPath(const String& dir, const String& name) {
  if (name.startsWith("/")) return name;
  if (dir == "/") return String("/") + name;
  return dir + "/" + name;
}

bool storageReady() {
  return Device::storageReady();
}

fs::FS& storageFS() {
  return Device::storageFS();
}

bool sdReady() {
  return Device::sdReady();
}

fs::FS& sdFS() {
  return Device::sdFS();
}

static bool tileTypeHasDynamicMqttRoute(TileType type) {
  return type == TILE_SENSOR ||
         type == TILE_SWITCH ||
         type == TILE_MEDIA ||
         type == TILE_WEATHER ||
         type == TILE_CLIMATE ||
         type == TILE_COVER;
}

static String dynamicMqttEntityForTile(const Tile& tile) {
  if (!tileTypeHasDynamicMqttRoute(tile.type)) return "";
  String entity = tile.sensor_entity;
  entity.trim();
  return entity;
}

static bool tileChangeAffectsDynamicMqttRoutes(const Tile& before, const Tile& after) {
  const String before_entity = dynamicMqttEntityForTile(before);
  const String after_entity = dynamicMqttEntityForTile(after);
  const bool before_has_route = before_entity.length() > 0;
  const bool after_has_route = after_entity.length() > 0;
  if (before_has_route != after_has_route) return true;
  if (!before_has_route && !after_has_route) return false;
  if (before.type != after.type) return true;
  return !before_entity.equalsIgnoreCase(after_entity);
}

String normalizeFileManagerPath(const String& raw) {
  String path = raw;
  path.trim();
  path.replace("\\", "/");
  const int query = path.indexOf('?');
  if (query >= 0) path = path.substring(0, query);
  const int hash = path.indexOf('#');
  if (hash >= 0) path = path.substring(0, hash);
  if (!path.length()) path = "/";
  if (!path.startsWith("/")) path = "/" + path;
  while (path.indexOf("//") >= 0) {
    path.replace("//", "/");
  }
  while (path.length() > 1 && path.endsWith("/")) {
    path.remove(path.length() - 1);
  }
  return path;
}

bool isSafeFileManagerName(const String& name) {
  if (!name.length() || name.length() > 96) return false;
  if (name == "." || name == "..") return false;
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name.charAt(i);
    if (c == '/' || c == '\\' || static_cast<uint8_t>(c) < 32) return false;
  }
  return true;
}

bool isSafeFileManagerPath(const String& path) {
  if (!path.length() || path.charAt(0) != '/' || path.length() > 192) return false;
  if (path == "/") return true;

  int start = 1;
  while (start < static_cast<int>(path.length())) {
    int slash = path.indexOf('/', start);
    if (slash < 0) slash = path.length();
    const String part = path.substring(start, slash);
    if (!isSafeFileManagerName(part)) return false;
    start = slash + 1;
  }
  return true;
}

String fileManagerBaseName(const String& path) {
  String normalized = normalizeFileManagerPath(path);
  if (normalized == "/") return "";
  const int slash = normalized.lastIndexOf('/');
  return slash >= 0 ? normalized.substring(slash + 1) : normalized;
}

String fileManagerParentPath(const String& path) {
  String normalized = normalizeFileManagerPath(path);
  if (normalized == "/") return "/";
  const int slash = normalized.lastIndexOf('/');
  if (slash <= 0) return "/";
  return normalized.substring(0, slash);
}

String fileManagerEntryName(const char* raw_name) {
  String name = raw_name ? String(raw_name) : String();
  name.replace("\\", "/");
  while (name.endsWith("/") && name.length() > 1) {
    name.remove(name.length() - 1);
  }
  const int slash = name.lastIndexOf('/');
  if (slash >= 0) {
    name = name.substring(slash + 1);
  }
  name.trim();
  return name;
}

String sanitizeFileManagerUploadName(const String& raw_name) {
  String name = fileManagerEntryName(raw_name.c_str());
  if (!name.length() || name == "." || name == "..") {
    return "";
  }
  for (size_t i = 0; i < name.length(); ++i) {
    const char c = name.charAt(i);
    if (c == '/' || c == '\\' || static_cast<uint8_t>(c) < 32) {
      name.setCharAt(i, '_');
    }
  }
  if (name.length() > 96) {
    name = name.substring(0, 96);
  }
  return isSafeFileManagerName(name) ? name : "";
}

bool resolveFileManagerFsByKey(const String& raw_key, fs::FS*& out_fs, String& out_key, String& error) {
  String key = raw_key;
  key.trim();
  key.toLowerCase();
  if (!key.length()) key = "sd";

  if (key == "sd" || key == "sdcard") {
    if (!sdReady()) {
      error = "microSD card is not available";
      return false;
    }
    out_fs = &sdFS();
    out_key = "sd";
    return true;
  }
  error = "Only microSD is available in the file manager";
  return false;
}

bool resolveFileManagerFsFromRequest(WebServer& server, fs::FS*& out_fs, String& out_key, String& error) {
  return resolveFileManagerFsByKey(server.hasArg("fs") ? server.arg("fs") : String("sd"),
                                   out_fs,
                                   out_key,
                                   error);
}

void sendJsonError(WebServer& server, int code, const String& error) {
  String json = "{\"success\":false,\"error\":\"";
  appendJsonEscaped(json, error);
  json += "\"}";
  server.send(code, "application/json", json);
}

void sendJsonOk(WebServer& server) {
  server.send(200, "application/json", "{\"success\":true}");
}

bool requireFileManagerWriteAccess(WebServer& server, const String& fs_key) {
  if (fs_key != "sd" || Device::sdWritable()) {
    return true;
  }
  Serial.println("[FileManager/SD] write rejected: mounted card did not pass the write test");
  sendJsonError(server,
                423,
                "microSD is readable but not writable; check the serial SD write-test error");
  return false;
}

void logFileManagerWriteFailure(const char* operation,
                                const String& fs_key,
                                const String& path,
                                int error_number) {
  Serial.printf("[FileManager/SD] operation=%s fs=%s path=%s errno=%d (%s)\n",
                operation ? operation : "unknown",
                fs_key.c_str(),
                path.c_str(),
                error_number,
                error_number ? strerror(error_number) : "none");
}

String fileManagerWriteError(const char* message, int error_number) {
  String result = message ? String(message) : String("microSD write failed");
  if (error_number) {
    result += " (errno ";
    result += String(error_number);
    result += ": ";
    result += strerror(error_number);
    result += ")";
  }
  return result;
}

String fileManagerContentType(const String& path) {
  String lowered = path;
  lowered.toLowerCase();
  if (lowered.endsWith(".gif")) return "image/gif";
  if (lowered.endsWith(".png")) return "image/png";
  if (lowered.endsWith(".jpg") || lowered.endsWith(".jpeg")) return "image/jpeg";
  if (lowered.endsWith(".bmp")) return "image/bmp";
  if (lowered.endsWith(".json")) return "application/json";
  if (lowered.endsWith(".txt") || lowered.endsWith(".log") || lowered.endsWith(".url")) return "text/plain";
  if (lowered.endsWith(".html") || lowered.endsWith(".htm")) return "text/html";
  if (lowered.endsWith(".css")) return "text/css";
  if (lowered.endsWith(".js")) return "application/javascript";
  return "application/octet-stream";
}

bool removeFileManagerPathRecursive(fs::FS& fs, const String& path, String& error) {
  if (path == "/") {
    error = "Root folder cannot be deleted";
    return false;
  }

  File entry = fs.open(path, FILE_READ);
  if (!entry) {
    error = "Path not found";
    return false;
  }

  if (!entry.isDirectory()) {
    entry.close();
    if (fs.remove(path)) {
      return true;
    }
    error = "Could not delete file";
    return false;
  }

  File child = entry.openNextFile();
  while (child) {
    const String child_name = fileManagerEntryName(child.name());
    child.close();
    if (child_name.length()) {
      const String child_path = joinPath(path, child_name);
      if (!removeFileManagerPathRecursive(fs, child_path, error)) {
        entry.close();
        return false;
      }
    }
    child = entry.openNextFile();
  }
  entry.close();

  if (fs.rmdir(path)) {
    return true;
  }
  error = "Could not delete folder";
  return false;
}

struct FileManagerEntry {
  String name;
  String path;
  bool directory = false;
  size_t size = 0;
  uint32_t modified = 0;
};

File g_file_manager_upload_file;
String g_file_manager_upload_fs_key;
String g_file_manager_upload_path;
String g_file_manager_upload_error;
bool g_file_manager_upload_started = false;
bool g_file_manager_upload_finished = false;
bool g_file_manager_upload_is_append = false;
size_t g_file_manager_upload_bytes = 0;
size_t g_file_manager_upload_next_heap_log = 0;

// Der WLAN-SDIO-Treiber crasht per assert (pkt_rxbuff), wenn ihm der interne
// DMA-faehige Heap ausgeht -- ein Upload ist das Szenario mit dem hoechsten
// Empfangsdruck. Diese Logs liefern beim naechsten Fehler/Crash die Zahlen
// dazu (interner Free-Heap + groesster zusammenhaengender DMA-Block).
void logFileManagerUploadHeap(const char* phase) {
  Serial.printf("[FileManager] Upload heap (%s, %u KB geschrieben): int frei=%u KB, DMA largest=%u KB\n",
                phase,
                static_cast<unsigned>(g_file_manager_upload_bytes / 1024),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA) / 1024));
}

void resetFileManagerUploadState() {
  if (g_file_manager_upload_file) {
    g_file_manager_upload_file.close();
  }
  g_file_manager_upload_fs_key = "";
  g_file_manager_upload_path = "";
  g_file_manager_upload_error = "";
  g_file_manager_upload_started = false;
  g_file_manager_upload_finished = false;
  g_file_manager_upload_is_append = false;
  g_file_manager_upload_bytes = 0;
  g_file_manager_upload_next_heap_log = 0;
}

constexpr const char* kScreenshotPath = "/ui_screenshot.jpg";
constexpr const char* kLegacyScreenshotPath = "/ui_screenshot.bmp";
constexpr uint32_t kScreenshotJpegQuality = 92;
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
#if defined(DEVICE_GUITION_ESP32_4848S040)
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

void prepareDisplayForRestart() {
  displayManager.setInputEnabled(false);
  BoardHAL::prepareForRestart();
}

void beginOtaStorageGuard() {
#if defined(DEVICE_GUITION_ESP32_4848S040)
  if (!g_ota_storage_guard_active) {
    Device::storageWriteBegin();
    g_ota_storage_guard_active = true;
  }
#endif
}

void endOtaStorageGuard() {
#if defined(DEVICE_GUITION_ESP32_4848S040)
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
          "[OTA] WARNUNG: Display-Puffer konnte nicht fuer OTA verkleinert werden");
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
        "[OTA] Schneller Display-Puffer nach %u Versuch(en) wiederhergestellt\n",
        static_cast<unsigned>(g_ota_display_restore_attempts));
    return true;
  }

  // Der HTTP-Upload-Callback laeuft noch mit aktiven TCP-/RX-Puffern. Den
  // kleinen PSRAM-Puffer weiterverwenden und erst nach Freigabe der Verbindung
  // erneut versuchen; niemals einen PSRAM-Fallback als Erfolg markieren.
  g_ota_display_restore_pending = true;
  g_ota_display_restore_retry_at = millis() + kOtaDisplayRestoreRetryMs;
  if (g_ota_display_restore_attempts == 1 ||
      (g_ota_display_restore_attempts % 8) == 0) {
    Serial.printf(
        "[OTA] Schneller Display-Restore vertagt (Versuch %u)\n",
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
    // MQTT erst wieder vergroessern/reconnecten, nachdem das schnelle
    // Display-Band seinen zusammenhaengenden DMA-Block sicher besitzt.
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
#if defined(DEVICE_GUITION_ESP32_4848S040)
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

#if defined(DEVICE_GUITION_ESP32_4848S040)
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

bool saveDrawBufferAsJpeg(const lv_draw_buf_t* draw_buf, const String& path, String& error) {
  if (!draw_buf || !draw_buf->data) {
    error = "Screenshot buffer missing";
    return false;
  }
  if (draw_buf->header.cf != LV_COLOR_FORMAT_RGB565) {
    error = "Unsupported screenshot color format";
    return false;
  }

  const int32_t width = draw_buf->header.w;
  const int32_t height = draw_buf->header.h;
  const uint32_t src_stride = draw_buf->header.stride;
  if (width <= 0 || height <= 0) {
    error = "Invalid screenshot size";
    return false;
  }
  const size_t row_bytes = static_cast<size_t>(width) * sizeof(uint16_t);
  const size_t raw_size = row_bytes * static_cast<size_t>(height);
  if (src_stride < row_bytes || raw_size > UINT32_MAX) {
    error = "Invalid screenshot size";
    return false;
  }

  size_t input_capacity = raw_size;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  uint8_t* input = static_cast<uint8_t*>(
      heap_caps_malloc(raw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  jpeg_encode_memory_alloc_cfg_t input_mem_cfg = {};
  input_mem_cfg.buffer_direction = JPEG_ENC_ALLOC_INPUT_BUFFER;
  input_capacity = 0;
  uint8_t* input = static_cast<uint8_t*>(
      jpeg_alloc_encoder_mem(raw_size, &input_mem_cfg, &input_capacity));
#endif
  if (!input || input_capacity < raw_size) {
    if (input) free(input);
    error = "JPEG input buffer allocation failed";
    return false;
  }
  for (int32_t y = 0; y < height; ++y) {
    memcpy(input + static_cast<size_t>(y) * row_bytes,
           draw_buf->data + static_cast<size_t>(y) * src_stride,
           row_bytes);
  }

  const uint32_t started_ms = millis();
  uint8_t* output = nullptr;
  size_t jpeg_size = 0;
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3 has no hardware JPEG encoder. The Apache-2.0 esp32-camera
  // converter included with the Arduino core provides the software fallback.
  // fmt2jpg interprets RGB565 as big-endian by default, while LVGL's native
  // RGB565 snapshot buffer on this little-endian target stores the low byte
  // first. Tell the encoder the actual layout before converting.
  jpgSetRgb565BE(false);
#if HOMETILES_GUITION_S3_DIAGNOSTICS_ACTIVE
  Serial.printf(
      "[S3Diag/Screenshot] input=%ldx%ld cf=RGB565 stride=%lu "
      "rgb565_be=0 bytes=%u\n",
      static_cast<long>(width), static_cast<long>(height),
      static_cast<unsigned long>(src_stride),
      static_cast<unsigned>(raw_size));
#endif
  // Its quality scale is 0..63 (lower is better); 7 is close to the P4's 92%.
  if (!fmt2jpg(input, raw_size, static_cast<uint16_t>(width),
               static_cast<uint16_t>(height), PIXFORMAT_RGB565, 7, &output,
               &jpeg_size)) {
    free(input);
    error = "JPEG software encoding failed";
    return false;
  }
#else
  jpeg_encode_memory_alloc_cfg_t output_mem_cfg = {};
  output_mem_cfg.buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER;
  size_t output_capacity = 0;
  output = static_cast<uint8_t*>(
      jpeg_alloc_encoder_mem(raw_size, &output_mem_cfg, &output_capacity));
  if (!output || output_capacity == 0) {
    free(input);
    if (output) free(output);
    error = "JPEG output buffer allocation failed";
    return false;
  }

  jpeg_encode_engine_cfg_t engine_cfg = {};
  engine_cfg.timeout_ms = 1000;
  jpeg_encoder_handle_t encoder = nullptr;
  esp_err_t result = jpeg_new_encoder_engine(&engine_cfg, &encoder);
  if (result != ESP_OK) {
    free(output);
    free(input);
    error = String("Could not start JPEG encoder: ") + esp_err_to_name(result);
    return false;
  }

  jpeg_encode_cfg_t encode_cfg = {};
  encode_cfg.height = static_cast<uint32_t>(height);
  encode_cfg.width = static_cast<uint32_t>(width);
  encode_cfg.src_type = JPEG_ENCODE_IN_FORMAT_RGB565;
  encode_cfg.sub_sample = JPEG_DOWN_SAMPLING_YUV444;
  encode_cfg.image_quality = kScreenshotJpegQuality;

  uint32_t hardware_jpeg_size = 0;
  result = jpeg_encoder_process(
      encoder, &encode_cfg, input, static_cast<uint32_t>(raw_size), output,
      static_cast<uint32_t>(output_capacity), &hardware_jpeg_size);
  jpeg_size = hardware_jpeg_size;
  jpeg_del_encoder_engine(encoder);

  if (result != ESP_OK) {
    free(input);
    free(output);
    error = String("JPEG encoding failed: ") + esp_err_to_name(result);
    return false;
  }
  if (jpeg_size == 0 || jpeg_size > output_capacity) {
    free(input);
    free(output);
    error = "JPEG encoder returned an invalid output size";
    return false;
  }
#endif
  free(input);

  if (sdFS().exists(path)) sdFS().remove(path);
  File file = sdFS().open(path, FILE_WRITE);
  if (!file) {
    free(output);
    error = "Could not open screenshot file";
    return false;
  }

  constexpr size_t kWriteChunkBytes = 32 * 1024;
  size_t written = 0;
  while (written < jpeg_size) {
    const size_t chunk = std::min(kWriteChunkBytes, static_cast<size_t>(jpeg_size) - written);
    if (file.write(output + written, chunk) != chunk) {
      file.close();
      sdFS().remove(path);
      free(output);
      error = "Could not write JPEG screenshot";
      return false;
    }
    written += chunk;
    yield();
  }
  file.close();
  free(output);
  if (sdFS().exists(kLegacyScreenshotPath)) sdFS().remove(kLegacyScreenshotPath);
  Serial.printf("[Screenshot] JPEG %ldx%ld quality=%u: %u KB in %u ms\n",
                static_cast<long>(width),
                static_cast<long>(height),
                static_cast<unsigned>(kScreenshotJpegQuality),
                static_cast<unsigned>((jpeg_size + 1023u) / 1024u),
                static_cast<unsigned>(millis() - started_ms));
  return true;
}

void getSnapshotAreaForObject(lv_obj_t* obj, lv_area_t& area) {
  lv_obj_update_layout(obj);
  lv_obj_get_coords(obj, &area);
}

bool hasVisibleDirectChildren(const lv_obj_t* parent) {
  if (!parent) return false;
  const uint32_t child_count = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < child_count; ++i) {
    const lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (child && !lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
      return true;
    }
  }
  return false;
}

uint16_t packRgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((static_cast<uint16_t>(r) & 0xF8u) << 8) |
                               ((static_cast<uint16_t>(g) & 0xFCu) << 3) |
                               (static_cast<uint16_t>(b) >> 3));
}

bool blendArgb8888OverRgb565(lv_draw_buf_t* base,
                             const lv_area_t& base_area,
                             const lv_draw_buf_t* overlay,
                             const lv_area_t& overlay_area,
                             String& error) {
  if (!base || !base->data || !overlay || !overlay->data) {
    error = "Screenshot buffers missing";
    return false;
  }
  if (base->header.cf != LV_COLOR_FORMAT_RGB565) {
    error = "Unsupported base screenshot format";
    return false;
  }
  if (overlay->header.cf != LV_COLOR_FORMAT_ARGB8888) {
    error = "Unsupported overlay screenshot format";
    return false;
  }

  const int32_t x1 = std::max(base_area.x1, overlay_area.x1);
  const int32_t y1 = std::max(base_area.y1, overlay_area.y1);
  const int32_t x2 = std::min(base_area.x2, overlay_area.x2);
  const int32_t y2 = std::min(base_area.y2, overlay_area.y2);
  if (x1 > x2 || y1 > y2) {
    return true;
  }

  const uint32_t base_stride = base->header.stride;
  const uint32_t overlay_stride = overlay->header.stride;

  for (int32_t y = y1; y <= y2; ++y) {
    uint16_t* dst = reinterpret_cast<uint16_t*>(
        base->data + static_cast<uint32_t>(y - base_area.y1) * base_stride) + (x1 - base_area.x1);
    const lv_color32_t* src = reinterpret_cast<const lv_color32_t*>(
        overlay->data + static_cast<uint32_t>(y - overlay_area.y1) * overlay_stride) + (x1 - overlay_area.x1);

    for (int32_t x = x1; x <= x2; ++x, ++dst, ++src) {
      const uint8_t alpha = src->alpha;
      if (alpha == 0) continue;

      const uint8_t src_r = src->red;
      const uint8_t src_g = src->green;
      const uint8_t src_b = src->blue;

      if (alpha >= 255) {
        *dst = packRgb565(src_r, src_g, src_b);
        continue;
      }

      const uint16_t dst565 = *dst;
      const uint8_t dst_r = static_cast<uint8_t>((((dst565 >> 11) & 0x1Fu) * 255u) / 31u);
      const uint8_t dst_g = static_cast<uint8_t>((((dst565 >> 5) & 0x3Fu) * 255u) / 63u);
      const uint8_t dst_b = static_cast<uint8_t>(((dst565 & 0x1Fu) * 255u) / 31u);
      const uint16_t inv_alpha = static_cast<uint16_t>(255u - alpha);

      const uint8_t out_r = static_cast<uint8_t>((static_cast<uint16_t>(src_r) * alpha +
                                                  static_cast<uint16_t>(dst_r) * inv_alpha + 127u) / 255u);
      const uint8_t out_g = static_cast<uint8_t>((static_cast<uint16_t>(src_g) * alpha +
                                                  static_cast<uint16_t>(dst_g) * inv_alpha + 127u) / 255u);
      const uint8_t out_b = static_cast<uint8_t>((static_cast<uint16_t>(src_b) * alpha +
                                                  static_cast<uint16_t>(dst_b) * inv_alpha + 127u) / 255u);
      *dst = packRgb565(out_r, out_g, out_b);
    }
  }

  return true;
}

bool createUiScreenshot(String& error) {
  if (!sdReady()) {
    error = "microSD card not available for screenshots";
    return false;
  }

  lv_display_t* disp = displayManager.getDisplay();
  lv_obj_t* screen = lv_screen_active();
  if (!disp || !screen) {
    error = "Display not ready";
    return false;
  }

  lv_refr_now(disp);
  Device::displayWaitDisplay();

  lv_area_t screen_area;
  getSnapshotAreaForObject(screen, screen_area);
  lv_draw_buf_t* draw_buf = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB565);
  if (!draw_buf) {
    error = "LVGL snapshot failed";
    return false;
  }

  lv_obj_t* top_layer = lv_display_get_layer_top(disp);
  if (top_layer && hasVisibleDirectChildren(top_layer)) {
    lv_area_t top_layer_area;
    getSnapshotAreaForObject(top_layer, top_layer_area);
    lv_draw_buf_t* overlay_buf = lv_snapshot_take(top_layer, LV_COLOR_FORMAT_ARGB8888);
    if (!overlay_buf) {
      lv_draw_buf_destroy(draw_buf);
      error = "Popup overlay snapshot failed";
      return false;
    }

    const bool blended = blendArgb8888OverRgb565(draw_buf, screen_area, overlay_buf, top_layer_area, error);
    lv_draw_buf_destroy(overlay_buf);
    if (!blended) {
      lv_draw_buf_destroy(draw_buf);
      return false;
    }
  }

  const bool ok = saveDrawBufferAsJpeg(draw_buf, String(kScreenshotPath), error);
  lv_draw_buf_destroy(draw_buf);
  return ok;
}

void collectImageFiles(const String& dir, std::vector<String>& out, size_t max_entries, uint8_t depth, bool allow_bin, bool allow_jpeg, bool allow_png) {
  if (out.size() >= max_entries) return;
  if (!storageReady()) return;
  File root = storageFS().open(dir);
  if (!root) return;

  File file = root.openNextFile();
  while (file) {
    if (out.size() >= max_entries) break;
    const char* name_c = file.name();
    String name = name_c ? String(name_c) : String();
    if (file.isDirectory()) {
      if (depth > 0 && name.length()) {
        collectImageFiles(joinPath(dir, name), out, max_entries, depth - 1, allow_bin, allow_jpeg, allow_png);
      }
    } else if (name.length()) {
      const bool is_bin = endsWithIgnoreCase(name, ".bin");
      const bool is_jpeg = endsWithIgnoreCase(name, ".jpg") || endsWithIgnoreCase(name, ".jpeg");
      const bool is_png = endsWithIgnoreCase(name, ".png");
      if ((allow_bin && is_bin) || (allow_jpeg && is_jpeg) || (allow_png && is_png)) {
        out.push_back(joinPath(dir, name));
      }
    }
    file = root.openNextFile();
  }
}

void appendJsonEscaped(String& out, const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (c == '\"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
}

void appendKeyValueMapJson(String& out, const String& map) {
  out += "{";
  bool first = true;
  int start = 0;

  while (start < map.length()) {
    int eqPos = map.indexOf('=', start);
    if (eqPos < 0) break;

    int endPos = map.indexOf('\n', eqPos);
    if (endPos < 0) endPos = map.length();

    String key = map.substring(start, eqPos);
    String value = map.substring(eqPos + 1, endPos);

    key.trim();
    value.trim();

    if (key.length() > 0 && value.length() > 0) {
      if (!first) out += ",";
      out += "\"";
      appendJsonEscaped(out, key);
      out += "\":\"";
      appendJsonEscaped(out, value);
      out += "\"";
      first = false;
    }

    start = endPos + 1;
  }

  out += "}";
}

struct IconFileInfo {
  String path;
  uint32_t size = 0;
  uint16_t width = 0;
  uint16_t height = 0;
};

struct JpegInfoCtx {
  File* file = nullptr;
};

static size_t jpeg_info_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  JpegInfoCtx* ctx = static_cast<JpegInfoCtx*>(jd->device);
  if (!ctx || !ctx->file) return 0;
  if (buff) return ctx->file->read(buff, ndata);
  ctx->file->seek(ctx->file->position() + ndata);
  return ndata;
}

static bool read_jpeg_dimensions(const String& path, uint16_t& w, uint16_t& h) {
  w = 0;
  h = 0;
  if (!storageReady()) return false;
  File f = storageFS().open(path, FILE_READ);
  if (!f) return false;
  uint8_t* work = static_cast<uint8_t*>(malloc(4096));
  if (!work) {
    f.close();
    return false;
  }
  JDEC jd;
  JpegInfoCtx ctx{};
  ctx.file = &f;
  JRESULT rc = jd_prepare(&jd, jpeg_info_input, work, 4096, &ctx);
  if (rc == JDR_OK) {
    w = jd.width;
    h = jd.height;
  }
  free(work);
  f.close();
  return rc == JDR_OK;
}

static bool read_png_dimensions(const String& path, uint16_t& w, uint16_t& h) {
  w = 0;
  h = 0;
  if (!storageReady()) return false;
  File f = storageFS().open(path, FILE_READ);
  if (!f) return false;
  uint8_t buf[24] = {0};
  if (f.read(buf, sizeof(buf)) != sizeof(buf)) {
    f.close();
    return false;
  }
  f.close();
  const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (memcmp(buf, sig, sizeof(sig)) != 0) return false;
  if (memcmp(buf + 12, "IHDR", 4) != 0) return false;
  w = static_cast<uint16_t>((buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19]);
  h = static_cast<uint16_t>((buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23]);
  return (w > 0 && h > 0);
}

struct TileRect {
  uint8_t col;
  uint8_t row;
  uint8_t span_w;
  uint8_t span_h;
};

static bool buildTileRect(uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h, TileRect& out) {
  if (col >= GRID_COLS || row >= GRID_ROWS) return false;
  if (span_w < 1 || span_h < 1) return false;
  if (span_w > GRID_COLS - col) return false;
  if (span_h > GRID_ROWS - row) return false;
  out = TileRect{col, row, span_w, span_h};
  return true;
}

static bool getTileRect(const Tile& tile, TileRect& out) {
  uint8_t col = tile.col;
  uint8_t row = tile.row;
  uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;
  clamp_media_tile_layout(tile.type, col, row, span_w, span_h);
  return buildTileRect(col, row, span_w, span_h, out);
}

static bool rectsOverlap(const TileRect& a, const TileRect& b) {
  return !(a.col + a.span_w <= b.col ||
           b.col + b.span_w <= a.col ||
           a.row + a.span_h <= b.row ||
           b.row + b.span_h <= a.row);
}

static bool placementOverlaps(const TileGridConfig& grid, size_t self_index, const TileRect& rect, size_t ignore_index = static_cast<size_t>(-1)) {
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (i == self_index || i == ignore_index) continue;
    const Tile& other = grid.tiles[i];
    if (other.type == TILE_EMPTY) continue;
    TileRect other_rect{};
    if (!getTileRect(other, other_rect)) continue;
    if (rectsOverlap(rect, other_rect)) return true;
  }
  return false;
}

static bool indexInList(size_t value, const std::vector<size_t>& values) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

static bool placementOverlapsAny(
    const TileGridConfig& grid,
    size_t self_index,
    const TileRect& rect,
    const std::vector<size_t>& ignore_indices) {
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (i == self_index || indexInList(i, ignore_indices)) continue;
    const Tile& other = grid.tiles[i];
    if (other.type == TILE_EMPTY) continue;
    TileRect other_rect{};
    if (!getTileRect(other, other_rect)) continue;
    if (rectsOverlap(rect, other_rect)) return true;
  }
  return false;
}

struct TilePosSnapshot {
  size_t index;
  uint8_t col;
  uint8_t row;
};

struct PlacementCandidate {
  uint8_t col;
  uint8_t row;
  uint16_t distance;
};

static uint16_t manhattanDistance(uint8_t col_a, uint8_t row_a, uint8_t col_b, uint8_t row_b) {
  const int dx = static_cast<int>(col_a) - static_cast<int>(col_b);
  const int dy = static_cast<int>(row_a) - static_cast<int>(row_b);
  return static_cast<uint16_t>(abs(dx) + abs(dy));
}

static std::vector<PlacementCandidate> buildPlacementCandidates(
    uint8_t span_w,
    uint8_t span_h,
    int preferred_col,
    int preferred_row,
    uint8_t first_row = 0) {
  std::vector<PlacementCandidate> out;
  for (uint8_t row = first_row; row < GRID_ROWS; ++row) {
    for (uint8_t col = 0; col < GRID_COLS; ++col) {
      TileRect rect{};
      if (!buildTileRect(col, row, span_w, span_h, rect)) continue;
      uint16_t distance = static_cast<uint16_t>(row * GRID_COLS + col);
      if (preferred_col >= 0 && preferred_row >= 0) {
        distance = manhattanDistance(col, row,
                                     static_cast<uint8_t>(preferred_col),
                                     static_cast<uint8_t>(preferred_row));
      }
      out.push_back(PlacementCandidate{col, row, distance});
    }
  }

  std::sort(out.begin(), out.end(), [](const PlacementCandidate& a, const PlacementCandidate& b) {
    if (a.distance != b.distance) return a.distance < b.distance;
    if (a.row != b.row) return a.row < b.row;
    return a.col < b.col;
  });
  return out;
}

static bool findPlacementForTile(
    TileGridConfig& grid,
    size_t tile_index,
    int preferred_col,
    int preferred_row,
    const std::vector<size_t>& floating_indices,
    uint8_t first_row = 0) {
  if (tile_index >= TILES_PER_GRID) return false;
  Tile& tile = grid.tiles[tile_index];
  const uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  const uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;

  auto can_place = [&](uint8_t col, uint8_t row) -> bool {
    TileRect rect{};
    if (!buildTileRect(col, row, span_w, span_h, rect)) return false;
    return !placementOverlapsAny(grid, tile_index, rect, floating_indices);
  };

  const std::vector<PlacementCandidate> candidates =
      buildPlacementCandidates(span_w, span_h, preferred_col, preferred_row,
                               first_row);
  for (const PlacementCandidate& candidate : candidates) {
    if (!can_place(candidate.col, candidate.row)) continue;
    tile.col = candidate.col;
    tile.row = candidate.row;
    return true;
  }

  return false;
}

static bool applySmartReorder(
    TileGridConfig& grid,
    size_t from_index,
    uint8_t target_col,
    uint8_t target_row,
    uint8_t first_row = 0) {
  if (from_index >= TILES_PER_GRID) return false;
  if (target_row < first_row) return false;
  Tile& moving_tile = grid.tiles[from_index];
  if (moving_tile.type == TILE_EMPTY) return false;

  const uint8_t from_col = moving_tile.col;
  const uint8_t from_row = moving_tile.row;
  const uint8_t span_w = moving_tile.span_w < 1 ? 1 : moving_tile.span_w;
  const uint8_t span_h = moving_tile.span_h < 1 ? 1 : moving_tile.span_h;

  TileRect target_rect{};
  if (!buildTileRect(target_col, target_row, span_w, span_h, target_rect)) return false;

  std::vector<size_t> displaced_indices;
  std::vector<TilePosSnapshot> snapshots;
  snapshots.push_back(TilePosSnapshot{from_index, from_col, from_row});

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (i == from_index) continue;
    const Tile& other = grid.tiles[i];
    if (other.type == TILE_EMPTY) continue;
    TileRect other_rect{};
    if (!getTileRect(other, other_rect)) continue;
    if (!rectsOverlap(target_rect, other_rect)) continue;
    displaced_indices.push_back(i);
    snapshots.push_back(TilePosSnapshot{i, other.col, other.row});
  }

  moving_tile.col = target_col;
  moving_tile.row = target_row;

  std::sort(displaced_indices.begin(), displaced_indices.end(), [&](size_t a, size_t b) {
    if (grid.tiles[a].row != grid.tiles[b].row) return grid.tiles[a].row < grid.tiles[b].row;
    if (grid.tiles[a].col != grid.tiles[b].col) return grid.tiles[a].col < grid.tiles[b].col;
    return a < b;
  });

  std::vector<size_t> floating_indices = displaced_indices;
  for (size_t displaced_index : displaced_indices) {
    auto it = std::find(floating_indices.begin(), floating_indices.end(), displaced_index);
    if (it != floating_indices.end()) floating_indices.erase(it);

    const int preferred_col = (displaced_index == displaced_indices.front()) ? from_col : grid.tiles[displaced_index].col;
    const int preferred_row = (displaced_index == displaced_indices.front()) ? from_row : grid.tiles[displaced_index].row;
    if (findPlacementForTile(grid, displaced_index, preferred_col, preferred_row,
                             floating_indices, first_row)) {
      continue;
    }

    for (const TilePosSnapshot& snapshot : snapshots) {
      if (snapshot.index >= TILES_PER_GRID) continue;
      grid.tiles[snapshot.index].col = snapshot.col;
      grid.tiles[snapshot.index].row = snapshot.row;
    }
    return false;
  }

  return true;
}


static bool parseFolderIdArg(WebServer& server, uint16_t& out) {
  String raw;
  if (server.hasArg("folder")) raw = server.arg("folder");
  else if (server.hasArg("folder_id")) raw = server.arg("folder_id");
  else if (server.hasArg("tab")) {
    String tab = server.arg("tab");
    tab.toLowerCase();
    if (tab == "home" || tab == "tab0") raw = "0";
  }
  raw.trim();
  if (!raw.length()) return false;
  int v = raw.toInt();
  if (v < 0 || v > 0xFFFF) return false;
  out = static_cast<uint16_t>(v);
  return true;
}

}  // namespace

void WebAdminServer::handleSaveMQTT() {
  const bool ajax_save =
      server.hasArg("_ajax") && server.arg("_ajax") == "1";
  const bool access_only =
      server.hasArg("_access_only") && server.arg("_access_only") == "1";
  DeviceConfig cfg{};
  if (configManager.isConfigured()) {
    cfg = configManager.getConfig();
  } else {
    cfg.mqtt_port = 1883;
    strncpy(cfg.mqtt_base_topic, "hometiles", CONFIG_MQTT_BASE_MAX - 1);
    strncpy(cfg.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);
    cfg.tile_borders = true;
  }
  const DeviceConfig previous_cfg = cfg;

  auto sendSaveError = [this, ajax_save](int status, const char* message) {
    if (ajax_save) {
      String json = "{\"ok\":false,\"error\":\"";
      appendJsonEscaped(json, String(message ? message : ""));
      json += "\"}";
      server.send(status, "application/json", json);
    } else {
      server.send(status, "text/html",
                  String("<h1>") + (message ? message : "") + "</h1>");
    }
  };

  auto copyIfNonEmpty = [this](char* dest, size_t max_len, const char* field) {
    if (!server.hasArg(field)) return;
    String value = server.arg(field);
    value.trim();
    if (!value.length()) return;
    copyToBuffer(dest, max_len, value);
  };

  auto copySharedIpField =
      [this](char* dest, size_t max_len, const char* shared_field,
             const char* wifi_legacy_field, const char* ethernet_legacy_field) {
        const char* selected = nullptr;
        if (server.hasArg(shared_field)) selected = shared_field;
        else if (server.hasArg(wifi_legacy_field)) selected = wifi_legacy_field;
        else if (server.hasArg(ethernet_legacy_field)) selected = ethernet_legacy_field;
        if (!selected) return;
        String value = server.arg(selected);
        value.trim();
        copyToBuffer(dest, max_len, value);
      };

  const bool use_static =
      access_only
          ? cfg.wifi_static_enabled
          : server.hasArg("network_use_static") ||
                server.hasArg("wifi_use_static") ||
                server.hasArg("ethernet_use_static") ||
                (server.hasArg("network_ip_mode") &&
                 server.arg("network_ip_mode").equalsIgnoreCase("static"));

  if (server.hasArg("mqtt_host")) {
    copyToBuffer(cfg.mqtt_host, sizeof(cfg.mqtt_host), server.arg("mqtt_host"));
  }
  copyIfNonEmpty(cfg.wifi_ssid, sizeof(cfg.wifi_ssid), "wifi_ssid");
  copyIfNonEmpty(cfg.wifi_pass, sizeof(cfg.wifi_pass), "wifi_pass");
  cfg.wifi_static_enabled = use_static;
  if (use_static) {
    copySharedIpField(cfg.wifi_static_ip, sizeof(cfg.wifi_static_ip),
                      "network_static_ip", "wifi_static_ip",
                      "ethernet_static_ip");
    copySharedIpField(cfg.wifi_gateway, sizeof(cfg.wifi_gateway),
                      "network_gateway", "wifi_gateway",
                      "ethernet_gateway");
    copySharedIpField(cfg.wifi_subnet, sizeof(cfg.wifi_subnet),
                      "network_subnet", "wifi_subnet",
                      "ethernet_subnet");
    copySharedIpField(cfg.wifi_dns, sizeof(cfg.wifi_dns),
                      "network_dns", "wifi_dns", "ethernet_dns");
  }
  if (NetworkTransportManager::deviceSupportsEthernet()) {
    if (server.hasArg("network_mode")) {
      cfg.ethernet_enabled =
          server.arg("network_mode").equalsIgnoreCase("ethernet");
    }
  }
  if (server.hasArg("mqtt_port")) {
    cfg.mqtt_port = server.arg("mqtt_port").toInt();
  }
  if (server.hasArg("mqtt_user")) {
    copyToBuffer(cfg.mqtt_user, sizeof(cfg.mqtt_user), server.arg("mqtt_user"));
  }
  copyIfNonEmpty(cfg.mqtt_pass, sizeof(cfg.mqtt_pass), "mqtt_pass");
  if (server.hasArg("mqtt_client_id")) {
    String client_id = server.arg("mqtt_client_id");
    client_id.trim();
    copyToBuffer(cfg.mqtt_client_id, sizeof(cfg.mqtt_client_id), client_id);
  }
  if (server.hasArg("mqtt_base")) {
    String base = server.arg("mqtt_base");
    base.trim();
    while (base.endsWith("/")) base.remove(base.length() - 1);
    if (base.isEmpty()) base = "hometiles";
    copyToBuffer(cfg.mqtt_base_topic, sizeof(cfg.mqtt_base_topic), base);
  }
  if (server.hasArg("ha_prefix")) {
    String prefix = server.arg("ha_prefix");
    prefix.trim();
    while (prefix.endsWith("/")) prefix.remove(prefix.length() - 1);
    if (prefix.isEmpty()) prefix = "ha/statestream";
    copyToBuffer(cfg.ha_prefix, sizeof(cfg.ha_prefix), prefix);
  }
  if (server.hasArg("language")) {
    String language = server.arg("language");
    language.trim();
    strncpy(cfg.language, i18n::normalize_language_code(language.c_str()), sizeof(cfg.language) - 1);
    cfg.language[sizeof(cfg.language) - 1] = '\0';
  }
  if (server.hasArg("timezone")) {
    String timezone = server.arg("timezone");
    timezone.trim();
    copyToBuffer(cfg.timezone, sizeof(cfg.timezone), timezone);
  }
  if (server.hasArg("locale_time_format")) {
    cfg.global_time_format =
        clock_tile::normalize_time_format(server.arg("locale_time_format").toInt());
  }
  if (server.hasArg("locale_date_format")) {
    cfg.global_date_format =
        clock_tile::normalize_date_format(server.arg("locale_date_format").toInt());
  }
  bool access_changed = false;
  bool settings_visibility_changed = false;
  bool settings_visibility_commit_needed = false;
  bool settings_gesture_changed = false;
  int settings_tile_target_col = -1;
  int settings_tile_target_row = -1;
  if (server.hasArg("settings_access_present")) {
    const auto& tr = i18n::strings(cfg.language);
    const bool enable_pin = server.hasArg("settings_pin_enabled");
    const bool hide_tile = server.hasArg("settings_tile_hidden");
    const bool enable_swipe = server.hasArg("settings_swipe_enabled");
    String new_pin = server.hasArg("settings_pin")
                         ? server.arg("settings_pin")
                         : String();
    new_pin.trim();

    const bool has_target_col = server.hasArg("settings_tile_target_col");
    const bool has_target_row = server.hasArg("settings_tile_target_row");
    if (has_target_col != has_target_row) {
      sendSaveError(400, tr.save_failed);
      return;
    }
    if (has_target_col) {
      settings_tile_target_col =
          server.arg("settings_tile_target_col").toInt();
      settings_tile_target_row =
          server.arg("settings_tile_target_row").toInt();
      if (settings_tile_target_col < 0 ||
          settings_tile_target_col >= GRID_COLS ||
          settings_tile_target_row < 0 ||
          settings_tile_target_row >= GRID_ROWS) {
        sendSaveError(400, tr.save_failed);
        return;
      }
    }

    if (!enable_pin) {
      cfg.settings_pin_enabled = false;
      pin_access::clearCredential(cfg.settings_pin_salt,
                                  cfg.settings_pin_hash);
      pin_access::secureClear(cfg.settings_pin_value,
                              sizeof(cfg.settings_pin_value));
    } else {
      if (new_pin.length()) {
        if (!pin_access::isValidUserPin(new_pin) ||
            !pin_access::makeCredential(new_pin, cfg.settings_pin_salt,
                                        cfg.settings_pin_hash)) {
          new_pin = "";
          sendSaveError(400, tr.pin_invalid);
          return;
        }
        new_pin.toCharArray(cfg.settings_pin_value,
                            sizeof(cfg.settings_pin_value));
      } else if (!pin_access::credentialIsSet(cfg.settings_pin_salt,
                                               cfg.settings_pin_hash)) {
        sendSaveError(400, tr.pin_invalid);
        return;
      }
      cfg.settings_pin_enabled = true;
    }
    Tile settings_tile;
    const bool settings_tile_present = tileConfig.getSettingsTile(settings_tile);
    if (hide_tile && settings_tile_present) {
      SettingsTileSnapshot& snapshot = cfg.settings_tile_snapshot;
      memset(&snapshot, 0, sizeof(snapshot));
      snapshot.valid = true;
      strncpy(snapshot.title, settings_tile.title.c_str(),
              sizeof(snapshot.title) - 1);
      strncpy(snapshot.icon_name, settings_tile.icon_name.c_str(),
              sizeof(snapshot.icon_name) - 1);
      snapshot.bg_color = settings_tile.bg_color;
      snapshot.col = settings_tile.col;
      snapshot.row = settings_tile.row;
      snapshot.span_w = settings_tile.span_w < 1 ? 1 : settings_tile.span_w;
      snapshot.span_h = settings_tile.span_h < 1 ? 1 : settings_tile.span_h;
    }
    if (!hide_tile && previous_cfg.settings_tile_hidden &&
        settings_tile_target_col >= 0 && cfg.settings_tile_snapshot.valid) {
      cfg.settings_tile_snapshot.col =
          static_cast<uint8_t>(settings_tile_target_col);
      cfg.settings_tile_snapshot.row =
          static_cast<uint8_t>(settings_tile_target_row);
    }
    if (server.hasArg("settings_tile_snapshot_present")) {
      if (!(hide_tile || previous_cfg.settings_tile_hidden) ||
          !cfg.settings_tile_snapshot.valid) {
        sendSaveError(409, tr.save_failed);
        return;
      }
      SettingsTileSnapshot& snapshot = cfg.settings_tile_snapshot;
      if (server.hasArg("settings_tile_title")) {
        server.arg("settings_tile_title")
            .toCharArray(snapshot.title, sizeof(snapshot.title));
      }
      if (server.hasArg("settings_tile_icon")) {
        server.arg("settings_tile_icon")
            .toCharArray(snapshot.icon_name, sizeof(snapshot.icon_name));
      }
      if (server.hasArg("settings_tile_bg_color_default") &&
          server.arg("settings_tile_bg_color_default").toInt() != 0) {
        snapshot.bg_color = 0;
      } else if (server.hasArg("settings_tile_bg_color")) {
        snapshot.bg_color = makeTileBgColor(static_cast<uint32_t>(
            server.arg("settings_tile_bg_color").toInt()));
      }
      const bool has_snapshot_col = server.hasArg("settings_tile_col");
      const bool has_snapshot_row = server.hasArg("settings_tile_row");
      const bool has_snapshot_span_w = server.hasArg("settings_tile_span_w");
      const bool has_snapshot_span_h = server.hasArg("settings_tile_span_h");
      const bool has_any_snapshot_layout =
          has_snapshot_col || has_snapshot_row || has_snapshot_span_w ||
          has_snapshot_span_h;
      const bool has_complete_snapshot_layout =
          has_snapshot_col && has_snapshot_row && has_snapshot_span_w &&
          has_snapshot_span_h;
      if (has_any_snapshot_layout && !has_complete_snapshot_layout) {
        sendSaveError(400, tr.save_failed);
        return;
      }
      if (has_complete_snapshot_layout) {
        auto parse_bounded_arg = [&](const char* name, int minimum,
                                     int maximum, int& out) {
          String value = server.arg(name);
          value.trim();
          if (!value.length()) return false;
          char* end = nullptr;
          const long parsed = strtol(value.c_str(), &end, 10);
          if (end == value.c_str() || *end != '\0' || parsed < minimum ||
              parsed > maximum) {
            return false;
          }
          out = static_cast<int>(parsed);
          return true;
        };
        int snapshot_col = 0;
        int snapshot_row = 0;
        int snapshot_span_w = 1;
        int snapshot_span_h = 1;
        if (!parse_bounded_arg("settings_tile_col", 0, GRID_COLS - 1,
                               snapshot_col) ||
            !parse_bounded_arg("settings_tile_row", 0, GRID_ROWS - 1,
                               snapshot_row) ||
            !parse_bounded_arg("settings_tile_span_w", 1, GRID_COLS,
                               snapshot_span_w) ||
            !parse_bounded_arg("settings_tile_span_h", 1, GRID_ROWS,
                               snapshot_span_h) ||
            snapshot_col + snapshot_span_w > GRID_COLS ||
            snapshot_row + snapshot_span_h > GRID_ROWS) {
          sendSaveError(400, tr.save_failed);
          return;
        }
        snapshot.col = static_cast<uint8_t>(snapshot_col);
        snapshot.row = static_cast<uint8_t>(snapshot_row);
        snapshot.span_w = static_cast<uint8_t>(snapshot_span_w);
        snapshot.span_h = static_cast<uint8_t>(snapshot_span_h);
      }
    }
    cfg.settings_tile_hidden = hide_tile;
    // Hidden Settings must always retain an on-device recovery path.
    cfg.settings_swipe_enabled = hide_tile || enable_swipe;
    new_pin = "";

    if (server.hasArg("settings_reveal_edge")) {
      const int edge = server.arg("settings_reveal_edge").toInt();
      cfg.settings_reveal_edge =
          edge >= static_cast<int>(SettingsRevealEdge::Left) &&
                  edge <= static_cast<int>(SettingsRevealEdge::Bottom)
              ? static_cast<uint8_t>(edge)
              : static_cast<uint8_t>(SettingsRevealEdge::Left);
    }

    access_changed =
        cfg.settings_pin_enabled != previous_cfg.settings_pin_enabled ||
        cfg.settings_tile_hidden != previous_cfg.settings_tile_hidden ||
        cfg.settings_swipe_enabled != previous_cfg.settings_swipe_enabled ||
        cfg.settings_reveal_edge != previous_cfg.settings_reveal_edge ||
        memcmp(cfg.settings_pin_salt, previous_cfg.settings_pin_salt,
               sizeof(cfg.settings_pin_salt)) != 0 ||
        memcmp(cfg.settings_pin_hash, previous_cfg.settings_pin_hash,
               sizeof(cfg.settings_pin_hash)) != 0 ||
        strcmp(cfg.settings_pin_value,
               previous_cfg.settings_pin_value) != 0 ||
        server.hasArg("settings_tile_snapshot_present");

    settings_gesture_changed =
        cfg.settings_swipe_enabled != previous_cfg.settings_swipe_enabled ||
        cfg.settings_reveal_edge != previous_cfg.settings_reveal_edge;

    settings_visibility_changed =
        cfg.settings_tile_hidden != previous_cfg.settings_tile_hidden;
    const bool requested_visible = !cfg.settings_tile_hidden;
    settings_visibility_commit_needed =
        settings_visibility_changed ||
        settings_tile_present != requested_visible;
    if (settings_visibility_commit_needed) {
      const SettingsTileVisibilityResult visibility =
          tileConfig.validateSettingsTileVisible(
              !cfg.settings_tile_hidden, settings_tile_target_col,
              settings_tile_target_row);
      if (visibility != SettingsTileVisibilityResult::Success) {
        sendSaveError(
            visibility == SettingsTileVisibilityResult::NoFreeCell ? 409 : 500,
            visibility == SettingsTileVisibilityResult::NoFreeCell
                ? tr.settings_home_full
                : tr.save_failed);
        return;
      }
    }
  }
  bool settings_config_saved = false;
  bool settings_visibility_commit_failed = false;
  bool settings_config_rolled_back = false;
  SettingsTileVisibilityResult settings_visibility_reconcile =
      SettingsTileVisibilityResult::Success;
  auto commit_settings = [&]() {
    settings_config_saved = configManager.save(cfg);
    if (!settings_config_saved || !settings_visibility_commit_needed) return;

    const SettingsTileVisibilityResult visibility =
        tileConfig.setSettingsTileVisible(
            !cfg.settings_tile_hidden, settings_tile_target_col,
            settings_tile_target_row);
    if (visibility == SettingsTileVisibilityResult::Success) return;

    settings_visibility_commit_failed = true;
    settings_config_rolled_back = configManager.save(previous_cfg);
    const bool reconcile_visible =
        settings_config_rolled_back ? !previous_cfg.settings_tile_hidden
                                    : !cfg.settings_tile_hidden;
    settings_visibility_reconcile =
        tileConfig.setSettingsTileVisible(reconcile_visible);
  };

#if defined(DEVICE_GUITION_ESP32_4848S040)
  // Hiding or restoring Settings updates NVS and LittleFS. Keep both writes in
  // one nested display guard so the RGB scanout is restarted only once.
  {
    Device::ScopedStorageWrite settings_visibility_storage_write(
        settings_visibility_commit_needed);
    commit_settings();
  }
#else
  commit_settings();
#endif

  if (settings_config_saved) {
    if (settings_visibility_commit_failed) {
      Serial.printf(
          "[Access] Settings visibility commit failed: rollback=%s "
          "reconcile=%u\n",
          settings_config_rolled_back ? "ok" : "failed",
          static_cast<unsigned>(settings_visibility_reconcile));
      tiles_request_reload_all();
      const auto& tr = i18n::strings(configManager.getConfig().language);
      sendSaveError(500, tr.save_failed);
      return;
    }
    if (!access_only) {
      settings_refresh_language();
      uiManager.scheduleNtpSync(0);
      // Reload grids in the loop, never inside the WebServer callback.
      tiles_request_reload_all();
    } else if (settings_visibility_commit_needed) {
      tiles_request_reload_all();
    }
    if (settings_gesture_changed) {
      // Web Admin access-only saves run in the main loop. Apply the new edge
      // and enabled state immediately without rebuilding the page or device UI.
      uiManager.refreshSettingsGestureZone();
    }
    if (ajax_save) {
      String response = "{\"ok\":true,\"reload\":";
      response += access_changed && !access_only ? "true" : "false";
      response += ",\"settings_pin\":\"";
      String stored_pin;
      if (configManager.getSettingsPin(stored_pin)) {
        appendJsonEscaped(response, stored_pin);
      }
      stored_pin = "";
      response += "\"}";
      server.send(200, "application/json", response);
    } else {
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "");
    }
    // Erst antworten (wie handleRestart()), dann die bis zu 500ms blockierende
    // Anfrage an den MQTT-Worker -- damit haengt der Browser beim Speichern
    // nicht. Verbindet MQTT live mit den neuen Einstellungen, kein
    // Geraete-Neustart mehr noetig.
    if (!access_only) networkManager.requestMqttReconfigure();
  } else {
    const auto& tr = i18n::strings(cfg.language);
    if (ajax_save) {
      sendSaveError(500, tr.save_failed);
    } else {
      server.send(500, "text/html", String("<h1>") + tr.save_failed + "</h1>");
    }
  }
}

void WebAdminServer::handleSaveBridge() {
  HaBridgeConfigData updated = haBridgeConfig.get();
  const auto sensors = parseSensorList(updated.sensors_text);
  const auto scenes = parseSceneList(updated.scene_alias_text);
  bool changed = false;

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    String field = "sensor_slot";
    field += static_cast<int>(i);
    String value = server.hasArg(field) ? server.arg(field) : "";
    value = normalizeSensorSelection(value, sensors);
    if (updated.sensor_slots[i] != value) {
      updated.sensor_slots[i] = value;
      changed = true;
    }
    String label_field = "sensor_label";
    label_field += static_cast<int>(i);
    String title = server.hasArg(label_field) ? server.arg(label_field) : "";
    title.trim();
    if (updated.sensor_titles[i] != title) {
      updated.sensor_titles[i] = title;
      changed = true;
    }
    String unit_field = "sensor_unit";
    unit_field += static_cast<int>(i);
    String unit = server.hasArg(unit_field) ? server.arg(unit_field) : "";
    unit.trim();
    if (value.isEmpty()) {
      unit = "";
    }
    if (updated.sensor_custom_units[i] != unit) {
      updated.sensor_custom_units[i] = unit;
      changed = true;
    }

    // Farbe parsen (z.B. "#2A2A2A" → 0x2A2A2A)
    String color_field = "sensor_color";
    color_field += static_cast<int>(i);
    String colorStr = server.hasArg(color_field) ? server.arg(color_field) : "";
    colorStr.trim();

    uint32_t color = 0;
    if (colorStr.length() > 0 && colorStr[0] == '#') {
      colorStr = colorStr.substring(1); // "#" entfernen
      color = strtoul(colorStr.c_str(), nullptr, 16);
    }

    if (updated.sensor_colors[i] != color) {
      updated.sensor_colors[i] = color;
      changed = true;
    }
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    String field = "scene_slot";
    field += static_cast<int>(i);
    String value = server.hasArg(field) ? server.arg(field) : "";
    value = normalizeSceneSelection(value, scenes);
    if (updated.scene_slots[i] != value) {
      updated.scene_slots[i] = value;
      changed = true;
    }
    String label_field = "scene_label";
    label_field += static_cast<int>(i);
    String title = server.hasArg(label_field) ? server.arg(label_field) : "";
    title.trim();
    if (updated.scene_titles[i] != title) {
      updated.scene_titles[i] = title;
      changed = true;
    }

    // Farbe parsen (z.B. "#353535" → 0x353535)
    String color_field = "scene_color";
    color_field += static_cast<int>(i);
    String colorStr = server.hasArg(color_field) ? server.arg(color_field) : "";
    colorStr.trim();

    uint32_t color = 0;
    if (colorStr.length() > 0 && colorStr[0] == '#') {
      colorStr = colorStr.substring(1); // "#" entfernen
      color = strtoul(colorStr.c_str(), nullptr, 16);
    }

    if (updated.scene_colors[i] != color) {
      updated.scene_colors[i] = color;
      changed = true;
    }
  }

  if (!changed) {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
    return;
  }

  if (haBridgeConfig.save(updated)) {
    // Reload grids im Loop (nicht im Web-Handler)
    tiles_request_reload_all();
    mqttReloadDynamicSlots();
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  } else {
    const auto& tr = i18n::strings(configManager.getConfig().language);
    server.send(500, "text/html", String("<h1>") + tr.save_failed + "</h1>");
  }
}

void WebAdminServer::handleBridgeRefresh() {
  if (!networkManager.isMqttConnected()) {
    server.send(503, "text/html",
                "<h1>MQTT ist nicht verbunden - bitte später erneut versuchen.</h1>");
    return;
  }
  networkManager.publishBridgeRequest(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

void WebAdminServer::handleStatus() {
  webAdminMarkActivity();
  sendChunkedResponse(server, 200, "application/json", getStatusJSON());
}

void WebAdminServer::handleRestart() {
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
  Serial.println("[WebAdmin] Restart requested");
  prepareDisplayForRestart();
  delay(200);
  BoardHAL::restart();
}

// Enthaelt der Ordner noch echte Kacheln (Back-Tile zaehlt nicht)? Bei
// Lesefehlern konservativ "ja" -- dann bleibt der Typwechsel gesperrt, statt
// versehentlich einen vollen Ordner zu verwaisen.
static bool folderHasContent(uint16_t folder_id) {
  if (folder_id == 0) return true;  // Root ist nie ueber ein Ordner-Tile erreichbar
  std::unique_ptr<TileGridConfig> grid(new (std::nothrow) TileGridConfig{});
  if (!grid) return true;
  if (!tileConfig.loadFolderGrid(folder_id, *grid)) return true;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const TileType t = grid->tiles[i].type;
    if (t != TILE_EMPTY && t != TILE_BACK) return true;
  }
  return false;
}

void WebAdminServer::handleGetTiles() {
  // GET /api/tiles?folder=<id>[&index=0-23]
  webAdminMarkActivity();
  if (!server.hasArg("tab") && !server.hasArg("folder") && !server.hasArg("folder_id")) {
    server.send(400, "application/json", "{\"error\":\"Missing folder parameter\"}");
    return;
  }

  uint16_t folder_id = 0;
  if (!parseFolderIdArg(server, folder_id)) {
    server.send(404, "application/json", "{\"error\":\"Folder not found\"}");
    return;
  }
  const bool screensaver_grid =
      folder_id == TileConfig::kScreensaverGridStorageId;
  if (!screensaver_grid && !tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"error\":\"Folder not found\"}");
    return;
  }

  TileGridConfig grid{};
  bool loaded = true;
  if (screensaver_grid) {
    grid = screensaverConfig.tileGrid();
  } else {
    loaded = tileConfig.loadFolderGrid(folder_id, grid);
  }
  if (!loaded) {
    server.send(500, "application/json", "{\"error\":\"Grid load failed\"}");
    return;
  }

  auto appendTileJson = [&](String& out, const Tile& tile) {
    out += "{\"type\":";
    out += String(static_cast<int>(tile.type));
    out += ",\"title\":\"";
    appendJsonEscaped(out, tile.title);
    out += "\",\"icon_name\":\"";
    appendJsonEscaped(out, tile.icon_name);
    out += "\",\"bg_color\":";
    out += String(tile.bg_color);
    out += ",\"background_opacity\":";
    out += String(tile.background_opacity);
    out += ",\"col\":";
    out += String(tile.col);
    out += ",\"row\":";
    out += String(tile.row);
    out += ",\"span_w\":";
    out += String(tile.span_w);
    out += ",\"span_h\":";
    out += String(tile.span_h);
    out += ",\"sensor_entity\":\"";
    appendJsonEscaped(out, tile.sensor_entity);
    out += "\",\"sensor_unit\":\"";
    appendJsonEscaped(out, tile.sensor_unit);
    out += "\",\"sensor_decimals\":";
    out += String(tile.sensor_decimals == 0xFF ? -1 : static_cast<int>(tile.sensor_decimals));
    out += ",\"sensor_value_font\":";
    out += String(tile.sensor_value_font);
    out += ",\"sensor_display_mode\":";
    out += String(tile.sensor_display_mode);
    out += ",\"sensor_gauge_min\":";
    out += String(tile.sensor_gauge_min);
    out += ",\"sensor_gauge_max\":";
    out += String(tile.sensor_gauge_max);
    out += ",\"sensor_gauge_arc\":";
    out += String(tile.sensor_gauge_arc);
    out += ",\"sensor_gauge_size\":";
    out += String(tile.sensor_gauge_size);
    out += ",\"sensor_gauge_y_offset\":";
    out += String(tile.sensor_gauge_y_offset);
    out += ",\"sensor_value_y_offset\":";
    out += String(tile.sensor_value_y_offset);
    out += ",\"sensor_graph_height\":";
    out += String(tile.sensor_graph_height);
    out += ",\"image_slideshow_sec\":";
    out += String(tile.image_slideshow_sec);
    out += ",\"scene_alias\":\"";
    appendJsonEscaped(out, tile.scene_alias);
    out += "\",\"key_macro\":\"";
    appendJsonEscaped(out, tile.key_macro);
    out += "\",\"key_code\":";
    out += String(tile.key_code);
    out += ",\"key_modifier\":";
    out += String(tile.key_modifier);
    out += ",\"popup_open_mode\":";
    out += String(getTilePopupOpenMode(tile));
    out += ",\"switch_style\":";
    out += String((tile.type == TILE_SWITCH && tile.sensor_decimals == 1) ? 1 : 0);
    out += ",\"navigate_target\":";
    out += String((tile.type == TILE_FOLDER) ? getNavigateTargetId(tile) : 0);
    // Der Editor sperrt den Typwechsel fuer nicht-leere Ordner (Inhalt wuerde
    // sonst verwaisen); Loeschen bleibt erlaubt.
    out += ",\"folder_empty\":";
    out += (tile.type == TILE_FOLDER && !folderHasContent(getNavigateTargetId(tile)))
               ? "true" : "false";
    out += ",\"folder_pin_enabled\":";
    out += (tile.type == TILE_FOLDER &&
            tileConfig.isFolderPinEnabled(getNavigateTargetId(tile)))
               ? "true"
               : "false";
    out += ",\"folder_pin\":\"";
    if (tile.type == TILE_FOLDER) {
      String folder_pin;
      if (tileConfig.getFolderPin(getNavigateTargetId(tile), folder_pin)) {
        appendJsonEscaped(out, folder_pin);
      }
      folder_pin = "";
    }
    out += "\"";
    out += "}";
  };

  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    if (index < 0 || index >= TILES_PER_GRID) {
      server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
      return;
    }

    String json;
    appendTileJson(json, grid.tiles[index]);
    sendChunkedResponse(server, 200, "application/json", json);
    return;
  }

  String json = "[";
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    if (i > 0) json += ",";
    appendTileJson(json, grid.tiles[i]);
  }
  json += "]";

  sendChunkedResponse(server, 200, "application/json", json);
}


void WebAdminServer::handleSaveTiles() {
  // POST /api/tiles
  webAdminMarkActivity();
  if (!server.hasArg("index") || !server.hasArg("type")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    return;
  }

  uint16_t folder_id = 0;
  if (!parseFolderIdArg(server, folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }
  const bool screensaver_grid =
      folder_id == TileConfig::kScreensaverGridStorageId;
  if (!screensaver_grid && !tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }

  int index = server.arg("index").toInt();
  int type = server.arg("type").toInt();

  if (!get_tile_type_descriptor(static_cast<TileType>(type))) {
    server.send(
        400, "application/json",
        "{\"success\":false,\"error\":\"Tile type not supported\"}");
    return;
  }

  if (screensaver_grid && type != TILE_EMPTY && type != TILE_SENSOR &&
      type != TILE_ENERGY && type != TILE_SCENE && type != TILE_SWITCH &&
      type != TILE_MEDIA && type != TILE_COVER) {
    server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Tile type not supported in screensaver\"}");
    return;
  }

  if (index < 0 || index >= TILES_PER_GRID) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid parameters\"}");
    return;
  }

  std::unique_ptr<TileGridConfig> grid(new (std::nothrow) TileGridConfig{});
  if (!grid) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Out of memory\"}");
    return;
  }
  // Never overwrite a folder from a failed load: a single tile edit rewrites the
  // whole grid, so if the existing grid can't be read we must abort instead of
  // persisting an (empty) grid over every tile in the folder.
  bool grid_loaded = true;
  if (screensaver_grid) {
    *grid = screensaverConfig.tileGrid();
  } else {
    grid_loaded = tileConfig.loadFolderGrid(folder_id, *grid);
  }
  if (!grid_loaded) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Folder load failed\"}");
    return;
  }

  Tile& tile = grid->tiles[index];
  Tile previous_tile = tile;
  const bool is_root = (!screensaver_grid && folder_id == 0);
  const bool was_settings_tile = is_root && previous_tile.type == TILE_SETTINGS;
  const bool was_back_tile =
      (!screensaver_grid && !is_root) && previous_tile.type == TILE_BACK;
  const bool force_settings_tile = was_settings_tile;
  const bool force_back_tile = was_back_tile;

  if (force_settings_tile) type = TILE_SETTINGS;
  if (force_back_tile) type = TILE_BACK;

  if (type == TILE_SETTINGS && !is_root) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Settings tile only allowed in Home\"}");
    return;
  }
  if (type == TILE_BACK && is_root) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Back tile only allowed in folders\"}");
    return;
  }
  if (type == TILE_SETTINGS && !force_settings_tile) {
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      if (i == static_cast<size_t>(index)) continue;
        if (grid->tiles[i].type == TILE_SETTINGS) {
          server.send(409, "application/json", "{\"success\":false,\"error\":\"Settings tile already exists\"}");
          return;
        }
    }
  }
  if (type == TILE_BACK && !force_back_tile) {
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      if (i == static_cast<size_t>(index)) continue;
        if (grid->tiles[i].type == TILE_BACK) {
          server.send(409, "application/json", "{\"success\":false,\"error\":\"Back tile already exists\"}");
          return;
        }
    }
  }

  // Ordner-Tile: Typwechsel nur, wenn der Ordner leer ist -- sonst wuerden
  // seine Kacheln verwaisen (Grid bleibt gespeichert, ist aber unerreichbar).
  // Loeschen (type -> EMPTY) bleibt immer erlaubt und raeumt den Ordner mit ab.
  if (previous_tile.type == TILE_FOLDER && type != TILE_FOLDER && type != TILE_EMPTY) {
    const uint16_t target_id = getNavigateTargetId(previous_tile);
    if (target_id != 0 && folderHasContent(target_id)) {
      server.send(409, "application/json",
                  "{\"success\":false,\"error\":\"Folder not empty\"}");
      return;
    }
  }

  // Jeder Wechsel weg vom Ordner-Typ entfernt auch den Ordner selbst: beim
  // Loeschen mitsamt Inhalt, bei der (nur fuer leere Ordner erlaubten)
  // Typumwandlung den leeren Grid -- sonst bliebe ein verwaister Ordner-Tab
  // im Web-Admin zurueck.
  const bool deleting_folder =
      !screensaver_grid && previous_tile.type == TILE_FOLDER && type != TILE_FOLDER;

  // Update tile data
  tile.type = static_cast<TileType>(type);
  tile.title = server.hasArg("title") ? server.arg("title") : "";
  tile.icon_name = server.hasArg("icon_name") ? server.arg("icon_name") : "";
  // Parse color. bg_color_default keeps legacy/default tiles as true defaults;
  // bg_color=0 is reserved for an explicitly selected black background.
  if (server.hasArg("bg_color_default") && server.arg("bg_color_default").toInt() != 0) {
    tile.bg_color = 0;
  } else if (server.hasArg("bg_color")) {
    tile.bg_color = makeTileBgColor(static_cast<uint32_t>(server.arg("bg_color").toInt()));
  }
  if (server.hasArg("background_opacity")) {
    tile.background_opacity = static_cast<uint8_t>(constrain(
        server.arg("background_opacity").toInt(), 0, 255));
  } else if (screensaver_grid && previous_tile.type == TILE_EMPTY &&
             type != TILE_EMPTY) {
    tile.background_opacity = kScreensaverDefaultTileOpacity;
  }

  // Parse layout (0-based col/row, span >= 1)
  uint8_t col = tile.col;
  uint8_t row = tile.row;
  uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;

  if (server.hasArg("col")) {
    int raw = server.arg("col").toInt();
    if (raw < 0) raw = 0;
    if (raw >= GRID_COLS) raw = GRID_COLS - 1;
    col = static_cast<uint8_t>(raw);
  }
  if (server.hasArg("row")) {
    int raw = server.arg("row").toInt();
    const int first_row = screensaver_grid && GRID_ROWS > 1 ? GRID_ROWS - 2 : 0;
    if (raw < first_row) raw = first_row;
    if (raw >= GRID_ROWS) raw = GRID_ROWS - 1;
    row = static_cast<uint8_t>(raw);
  }
  if (server.hasArg("span_w")) {
    int raw = server.arg("span_w").toInt();
    if (raw < 1) raw = 1;
    if (raw > GRID_COLS) raw = GRID_COLS;
    span_w = static_cast<uint8_t>(raw);
  }
  if (server.hasArg("span_h")) {
    int raw = server.arg("span_h").toInt();
    if (raw < 1) raw = 1;
    if (raw > GRID_ROWS) raw = GRID_ROWS;
    span_h = static_cast<uint8_t>(raw);
  }

  if (screensaver_grid && GRID_ROWS > 1 && row < GRID_ROWS - 2) {
    row = GRID_ROWS - 2;
  }

  clamp_media_tile_layout(static_cast<TileType>(type), col, row,
                          span_w, span_h);
  if (screensaver_grid && GRID_ROWS > 1 && row < GRID_ROWS - 2) {
    row = GRID_ROWS - 2;
  }
  if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
  if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;

  tile.col = col;
  tile.row = row;
  tile.span_w = span_w;
  tile.span_h = span_h;

  // Type-specific fields
  String error_message;
  TileTypeApplyContext apply_ctx;
  apply_ctx.folder_id = folder_id;
  apply_ctx.tile_config = &tileConfig;
  apply_ctx.error_message = &error_message;
  // Preserve the folder a folder tile already points to so a rename reuses it
  // instead of spawning a duplicate. Only trust the stored id when the tile was
  // actually a folder before (otherwise key_code/key_modifier hold key data).
  if (previous_tile.type == TILE_FOLDER) {
    apply_ctx.previous_navigate_target = getNavigateTargetId(previous_tile);
  }
  const TileTypeDescriptor* desc = get_tile_type_descriptor(tile.type);
  if (desc && desc->apply) {
    if (!desc->apply(server, tile, apply_ctx)) {
      tile = previous_tile;
      String err = error_message;
      if (!err.length()) {
        err = (type == TILE_FOLDER) ? "Folder create failed" : "Tile apply failed";
      }
      server.send(500, "application/json", String("{\"success\":false,\"error\":\"") + err + "\"}");
      return;
    }
  }
  if (screensaver_grid && tile.type == TILE_SENSOR) {
    // Sensor history is not routed into the screensaver widget context.
    // Enforce the plain value mode for old imports and direct API callers too.
    tile.sensor_display_mode = 0;
  }

  if (deleting_folder) {
    const uint16_t target_id = getNavigateTargetId(previous_tile);
    if (target_id != 0) {
      if (!tileConfig.deleteFolder(target_id)) {
        tile = previous_tile;
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Folder delete failed\"}");
        return;
      }
      tiles_invalidate_folder(target_id);
    }
  }

  if (tile.type != TILE_EMPTY) {
    TileRect rect{};
    if (!buildTileRect(col, row, span_w, span_h, rect)) {
      tile = previous_tile;
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid layout\"}");
      return;
    }
    if (placementOverlaps(*grid, index, rect)) {
      tile = previous_tile;
      server.send(409, "application/json", "{\"success\":false,\"error\":\"Tile overlaps\"}");
      return;
    }
  }

  bool success = screensaver_grid
                     ? screensaverConfig.replaceTileGrid(*grid)
                     : tileConfig.saveFolderGrid(folder_id, *grid);
  if (success) {
    Serial.printf("[WebAdmin] Tile folder %u[%d] gespeichert - Type: %d\n", static_cast<unsigned>(folder_id), index, type);

    const bool routes_changed =
        deleting_folder || tileChangeAffectsDynamicMqttRoutes(previous_tile, tile);
    if (routes_changed) {
      // Mehrere schnelle Tile-Edits duerfen nur einen teuren Route-Rebuild
      // ausloesen. Das gilt auch fuer das getrennte Screensaver-Grid, dessen
      // Media-Entity sonst weder state noch state_fast abonnieren wuerde.
      mqttRequestDynamicSlotsReload(5000);
      Serial.println("[WebAdmin] MQTT Routes fuer spaeteren Rebuild markiert");
    } else if (!screensaver_grid) {
      Serial.println("[WebAdmin] MQTT Routes unveraendert (kein Rebuild fuer Style/Layout)");
    }

    if (!screensaver_grid) {
      tiles_invalidate_folder(folder_id);
      if (tileConfig.getActiveFolderId() == folder_id) {
        tiles_request_reload_if_loaded(GridType::TAB0);
      }
    } else {
      image_screensaver_tiles_changed();
    }

    String response = "{\"success\":true";
    if (tile.type == TILE_FOLDER) {
      response += ",\"navigate_target\":";
      response += String(getNavigateTargetId(tile));
      response += ",\"folder_pin_enabled\":";
      response += tileConfig.isFolderPinEnabled(getNavigateTargetId(tile))
                      ? "true"
                      : "false";
      response += ",\"folder_pin\":\"";
      String folder_pin;
      if (tileConfig.getFolderPin(getNavigateTargetId(tile), folder_pin)) {
        appendJsonEscaped(response, folder_pin);
      }
      folder_pin = "";
      response += "\"";
    }
    response += "}";
    sendChunkedResponse(server, 200, "application/json", response);
  } else {
    Serial.printf("[WebAdmin] Fehler beim Speichern von Tile folder %u[%d]\n", static_cast<unsigned>(folder_id), index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Save failed\"}");
  }
}


void WebAdminServer::handleReorderTiles() {
  webAdminMarkActivity();
  if (!server.hasArg("from") || !server.hasArg("to")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    return;
  }

  uint16_t folder_id = 0;
  if (!parseFolderIdArg(server, folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }
  const bool screensaver_grid =
      folder_id == TileConfig::kScreensaverGridStorageId;
  if (!screensaver_grid && !tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }

  int from = server.arg("from").toInt();
  int to = server.arg("to").toInt();

  if (from < 0 || from >= TILES_PER_GRID || to < 0 || to >= TILES_PER_GRID) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid parameters\"}");
    return;
  }

  TileGridConfig grid{};
  // Abort rather than overwrite the whole folder if the current grid can't be loaded.
  bool grid_loaded = true;
  if (screensaver_grid) {
    grid = screensaverConfig.tileGrid();
  } else {
    grid_loaded = tileConfig.loadFolderGrid(folder_id, grid);
  }
  if (!grid_loaded) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Folder load failed\"}");
    return;
  }

  Tile& tile_to = grid.tiles[to];

  int target_col_raw = server.hasArg("target_col") ? server.arg("target_col").toInt() : -1;
  int target_row_raw = server.hasArg("target_row") ? server.arg("target_row").toInt() : -1;
  uint8_t target_col = (target_col_raw >= 0 && target_col_raw < GRID_COLS) ? static_cast<uint8_t>(target_col_raw) : tile_to.col;
  uint8_t target_row = (target_row_raw >= 0 && target_row_raw < GRID_ROWS) ? static_cast<uint8_t>(target_row_raw) : tile_to.row;

  if (target_col >= GRID_COLS || target_row >= GRID_ROWS) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid target\"}");
    return;
  }

  const uint8_t first_row = screensaver_grid && GRID_ROWS > 1
                                ? GRID_ROWS - 2
                                : 0;
  if (!applySmartReorder(grid, static_cast<size_t>(from), target_col,
                         target_row, first_row)) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"Tile overlaps\"}");
    return;
  }

  bool success = screensaver_grid
                     ? screensaverConfig.replaceTileGrid(grid)
                     : tileConfig.saveFolderGrid(folder_id, grid);
  if (success) {
    if (!screensaver_grid) {
      tiles_invalidate_folder(folder_id);
      if (tileConfig.getActiveFolderId() == folder_id) {
        tiles_request_reload_if_loaded(GridType::TAB0);
      }
    } else {
      image_screensaver_tiles_changed();
    }
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Save failed\"}");
  }
}

void WebAdminServer::handleGetSensorValues() {
  webAdminMarkActivity();
  const HaBridgeConfigData& ha = haBridgeConfig.get();


  // Build JSON response with values + meta
  String json = "{";
  json += "\"values\":";
  appendKeyValueMapJson(json, ha.sensor_values_map);
  json += ",\"units\":";
  appendKeyValueMapJson(json, ha.sensor_units_map);
  json += ",\"icons\":";
  appendKeyValueMapJson(json, ha.entity_icons_map);
  json += ",\"names\":";
  appendKeyValueMapJson(json, ha.sensor_names_map);

  // Climate-Zustaende enthalten neben der Ist-Temperatur auch HVAC-Modus,
  // Aktion und Einheit. Sie liegen im zentralen Entity-Cache als komplettes
  // JSON-Payload und duerfen nicht auf den einfachen Sensorwert reduziert
  // werden, weil der Web-Editor daraus auch das dynamische Icon ableitet.
  json += ",\"climate_values\":{";
  bool first_climate_value = true;
  const auto climate_ids = parseSensorList(ha.climates_text);
  for (const auto& id : climate_ids) {
    String payload;
    if (!tiles_get_cached_entity_payload(id.c_str(), payload)) {
      payload = haBridgeConfig.findSensorInitialValue(id);
    }
    payload.trim();
    if (!payload.length()) continue;
    if (!first_climate_value) json += ',';
    first_climate_value = false;
    json += '"';
    appendJsonEscaped(json, id);
    json += "\":\"";
    appendJsonEscaped(json, payload);
    json += '"';
  }
  json += "}";

  // Aggregierte Energy-Quellen (z. B. solar_total) sind keine normalen
  // Home-Assistant-Entities und fehlen daher im allgemeinen Sensor-Cache.
  // Der Web-Editor bekommt ihre aktuellen Tages-Summen separat; der Browser
  // fuehrt beide Maps zusammen, sodass normale Sensoren unveraendert bleiben.
  auto energy_ids = parseSensorList(ha.energy_text);
  energy_append_cached_entity_ids(energy_ids);
  json += ",\"energy_values\":{";
  bool first_energy_value = true;
  for (const auto& id : energy_ids) {
    EnergyEntryData entry;
    if (!energy_find_entry(id, "day", entry)) continue;
    if (!first_energy_value) json += ',';
    first_energy_value = false;
    json += '"';
    appendJsonEscaped(json, id);
    json += "\":\"";
    appendJsonEscaped(json, String(entry.total, entry.is_cost ? 2 : 3));
    json += '"';
  }
  json += "},\"energy_units\":{";
  bool first_energy_unit = true;
  for (const auto& id : energy_ids) {
    String unit = energy_find_cached_unit(id);
    if (!unit.length()) continue;
    if (!first_energy_unit) json += ',';
    first_energy_unit = false;
    json += '"';
    appendJsonEscaped(json, id);
    json += "\":\"";
    appendJsonEscaped(json, unit);
    json += '"';
  }
  json += "}";
  json += "}";

  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleGetSdImages() {
  std::vector<String> files;
  collectImageFiles("/", files, 200, 3, true, true, false);

  String json = "[";
  for (size_t i = 0; i < files.size(); ++i) {
    if (i > 0) json += ",";
    json += "\"";
    appendJsonEscaped(json, files[i]);
    json += "\"";
  }
  json += "]";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleGetSdIcons() {
  if (!storageReady()) {
    server.send(200, "application/json", "[]");
    return;
  }
  if (!storageFS().exists("/icons")) {
#if defined(DEVICE_GUITION_ESP32_4848S040)
    Device::ScopedStorageWrite storage_write;
#endif
    storageFS().mkdir("/icons");
  }
  std::vector<IconFileInfo> files;
  std::vector<String> paths;
  collectImageFiles("/icons", paths, 100, 1, false, true, true);
  for (const auto& path : paths) {
    IconFileInfo info;
    info.path = path;
    File f = storageFS().open(path, FILE_READ);
    if (f) {
      info.size = static_cast<uint32_t>(f.size());
      f.close();
    }
    if (endsWithIgnoreCase(path, ".png")) {
      read_png_dimensions(path, info.width, info.height);
    } else {
      read_jpeg_dimensions(path, info.width, info.height);
    }
    files.push_back(info);
  }

  String json = "[";
  for (size_t i = 0; i < files.size(); ++i) {
    if (i > 0) json += ",";
    json += "{\"path\":\"";
    appendJsonEscaped(json, files[i].path);
    json += "\",\"size\":";
    json += String(files[i].size);
    json += ",\"w\":";
    json += String(files[i].width);
    json += ",\"h\":";
    json += String(files[i].height);
    json += "}";
  }
  json += "]";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleUploadIcon() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;
  static bool storage_guard_active = false;

  auto end_storage_guard = [&]() {
    if (!storage_guard_active) return;
    storage_guard_active = false;
#if defined(DEVICE_GUITION_ESP32_4848S040)
    Device::storageWriteEnd();
#endif
  };

  if (upload.status == UPLOAD_FILE_START) {
    if (uploadFile) uploadFile.close();
    end_storage_guard();
    // Empfangsfenster begrenzen -- gleiche Absturzursache wie beim
    // Filemanager-Upload (interner DMA-Heap vs. 64KB TCP-Fenster).
    int rcvbuf = 8 * 1024;
    server.client().setSocketOption(SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    if (!storageReady()) {
      Serial.println("[Icons] Upload failed: storage unavailable");
      return;
    }
#if defined(DEVICE_GUITION_ESP32_4848S040)
    Device::storageWriteBegin();
    storage_guard_active = true;
#endif
    if (!storageFS().exists("/icons")) storageFS().mkdir("/icons");
    String filename = upload.filename;
    if (filename.indexOf('/') < 0) filename = "/icons/" + filename;
    if (storageFS().exists(filename)) storageFS().remove(filename);
    uploadFile = storageFS().open(filename, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("[Icons] Upload open failed");
      end_storage_guard();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      const size_t written = uploadFile.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        Serial.printf("[Icons] Upload write failed: requested=%u wrote=%u\n",
                      static_cast<unsigned>(upload.currentSize),
                      static_cast<unsigned>(written));
        uploadFile.close();
        end_storage_guard();
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[Icons] Uploaded %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
    }
    end_storage_guard();
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    Serial.println("[Icons] Upload aborted");
    end_storage_guard();
  }
}

void WebAdminServer::handleUploadIconDone() {
  HTTPUpload& upload = server.upload();
  String path = "/icons/" + upload.filename;
  String json = "{\"ok\":true,\"path\":\"";
  appendJsonEscaped(json, path);
  json += "\"}";
  sendChunkedResponse(server, 200, "application/json", json);
}

// Liefert die aktuellen Entity-Listen der HA-Bridge als JSON ({v: id, t:
// Anzeigetext} pro Eintrag). Die Dropdowns im Tile-Editor befuellen sich
// damit beim Oeffnen einer Kachel frisch, statt auf das einmal gerenderte
// Seiten-HTML angewiesen zu sein — neue Entitaeten (und Wallpapers, siehe
// /api/files/list) waren sonst erst nach einem Seiten-Reload waehlbar.
// Die Label-Formate spiegeln die append_*_fields_html der Typ-Module.
void WebAdminServer::handleGetEntityOptions() {
  webAdminMarkActivity();
  const HaBridgeConfigData& ha = haBridgeConfig.get();

  auto appendPair = [](String& json, const String& value, const String& label, bool& first) {
    if (!first) json += ",";
    first = false;
    json += "{\"v\":\"";
    appendJsonEscaped(json, value);
    json += "\",\"t\":\"";
    appendJsonEscaped(json, label);
    json += "\"}";
  };

  auto appendHumanizedList = [&](String& json, const char* key, const std::vector<String>& ids) {
    json += "\"";
    json += key;
    json += "\":[";
    bool first = true;
    for (const auto& id : ids) {
      String label;
      if (hardwareIo.isLocalEntityId(id.c_str())) {
        label = haBridgeConfig.findSensorName(id);
      }
      if (!label.length()) label = humanizeIdentifier(id, true);
      appendPair(json, id, label + " - " + id, first);
    }
    json += "]";
  };

  auto appendLocalIds = [&](std::vector<String>& ids, HardwareIoType wanted) {
    for (uint8_t i = 0; i < hardwareIo.channelCount(); ++i) {
      String entity_id;
      String name;
      HardwareIoType type = HardwareIoType::Relay;
      if (!hardwareIo.localEntityInfo(i, entity_id, name, type) ||
          type != wanted) {
        continue;
      }
      bool duplicate = false;
      for (const auto& existing : ids) {
        if (existing.equalsIgnoreCase(entity_id)) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) ids.push_back(entity_id);
    }
  };

  // Wie label_already_has_unit_suffix im Energy-Formular.
  auto hasUnitSuffix = [](const String& name, const String& unit) {
    String n = name;
    n.trim();
    String u = unit;
    u.trim();
    if (!n.length() || !u.length()) return false;
    String suffix = "(" + u + ")";
    n.toLowerCase();
    suffix.toLowerCase();
    return n.endsWith(suffix);
  };

  String json = "{\"success\":true,";
  auto sensor_ids = parseSensorList(ha.sensors_text);
  appendLocalIds(sensor_ids, HardwareIoType::Temperature);
  appendHumanizedList(json, "sensors", sensor_ids);
  json += ",";
  appendHumanizedList(json, "weathers", parseSensorList(ha.weathers_text));
  json += ",";
  appendHumanizedList(json, "climates", parseSensorList(ha.climates_text));
  json += ",";
  appendHumanizedList(json, "covers", parseSensorList(ha.covers_text));
  json += ",\"cameras\":[";
  {
    bool first = true;
    for (const auto& id : parseSensorList(ha.cameras_text)) {
      String name = haBridgeConfig.findSensorName(id);
      if (!name.length()) name = humanizeIdentifier(id, true);
      appendPair(json, id, name + " - " + id, first);
    }
  }
  json += "],";

  json += "\"energy\":[";
  {
    auto energy_ids = parseSensorList(ha.energy_text);
    energy_append_cached_entity_ids(energy_ids);
    bool first = true;
    for (const auto& id : energy_ids) {
      String name = haBridgeConfig.findSensorName(id);
      if (!name.length()) name = humanizeIdentifier(id, true);
      String unit = haBridgeConfig.findSensorUnit(id);
      if (!unit.length()) unit = energy_find_cached_unit(id);
      String label = name;
      if (unit.length() && !hasUnitSuffix(label, unit)) {
        label += " (";
        label += unit;
        label += ")";
      }
      label += " - ";
      label += id;
      appendPair(json, id, label, first);
    }
  }
  json += "],\"media\":[";
  {
    bool first = true;
    for (const auto& id : parseSensorList(ha.media_players_text)) {
      String name = haBridgeConfig.findSensorName(id);
      if (!name.length()) name = humanizeIdentifier(id, true);
      appendPair(json, id, name + " - " + id, first);
    }
  }
  json += "],";

  // Lichter + Schalter + Geraete-Entities, dedupliziert — gleiche Reihenfolge
  // wie buildAdminFolderTabFragments.
  {
    std::vector<String> switch_options;
    auto addSwitchOption = [&](const String& entry) {
      if (!entry.length()) return;
      for (const auto& existing : switch_options) {
        if (existing.equalsIgnoreCase(entry)) return;
      }
      switch_options.push_back(entry);
    };
    for (const auto& opt : parseSensorList(ha.lights_text)) addSwitchOption(opt);
    for (const auto& opt : parseSensorList(ha.switches_text)) addSwitchOption(opt);
    addSwitchOption(kEntityDisplayBrightness);
    addSwitchOption(kEntityScreensaverBrightness);
    addSwitchOption(kEntityDisplayRotate);
    addSwitchOption(kEntityDisplaySleep);
    std::vector<String> local_relays;
    appendLocalIds(local_relays, HardwareIoType::Relay);
    for (const auto& opt : local_relays) addSwitchOption(opt);
    appendHumanizedList(json, "switches", switch_options);
  }

  json += ",\"scenes\":[";
  {
    bool first = true;
    for (const auto& scene : parseSceneList(ha.scene_alias_text)) {
      appendPair(json, scene.alias,
                 humanizeIdentifier(scene.alias, false) + " - " + scene.entity,
                 first);
    }
  }
  json += "]}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleGetScreensaver() {
  webAdminMarkActivity();
  String json = screensaverConfig.toJson(true);
  if (!json.endsWith("}")) {
    server.send(500, "application/json", "{\"success\":false}");
    return;
  }
  json.remove(json.length() - 1);
  json += ",\"success\":true,\"available_wallpapers\":[";
  bool first = true;
  if (Device::sdReady()) {
    std::vector<String> names;
    const char* directories[] = {"/images", "/wallpapers"};
    for (const char* directory : directories) {
      fs::File root = Device::sdFS().open(directory, FILE_READ);
      if (!root) continue;
      for (fs::File entry = root.openNextFile(); entry;
           entry = root.openNextFile()) {
        if (!entry.isDirectory()) {
          String name = entry.name();
          const int slash = max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
          if (slash >= 0) name = name.substring(slash + 1);
          if (endsWithIgnoreCase(name, ".jpg") ||
              endsWithIgnoreCase(name, ".jpeg")) {
            bool duplicate = false;
            for (const auto& existing : names) {
              if (existing.equalsIgnoreCase(name)) {
                duplicate = true;
                break;
              }
            }
            if (!duplicate) names.push_back(name);
          }
        }
        entry.close();
      }
      root.close();
    }
    for (const auto& name : names) {
      if (!first) json += ',';
      first = false;
      json += '"';
      appendJsonEscaped(json, name);
      json += '"';
    }
  }
  json += "]}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleSaveScreensaver() {
  webAdminMarkActivity();
  const String payload = server.arg("plain");
  String preview_wallpaper;
  String error;
  if (!screensaverConfig.replaceFromJson(payload, error, &preview_wallpaper)) {
    String json = "{\"success\":false,\"error\":\"";
    appendJsonEscaped(json, error);
    json += "\"}";
    server.send(400, "application/json", json);
    return;
  }
  image_screensaver_config_changed(preview_wallpaper);
  server.send(200, "application/json", "{\"success\":true}");
}

void WebAdminServer::handleSaveTileBorders() {
  webAdminMarkActivity();
  if (!server.hasArg("enabled")) {
    sendJsonError(server, 400, "Missing enabled value");
    return;
  }

  String value = server.arg("enabled");
  value.trim();
  value.toLowerCase();
  const bool enabled = value == "1" || value == "true" || value == "on";
  if (!configManager.saveTileBorders(enabled)) {
    sendJsonError(server, 500, "Could not save tile borders");
    return;
  }

  // Alle registrierten normalen Oberflaechen (Tiles, Folder, Back, Settings
  // und Popups) uebernehmen die Option im naechsten sicheren UI-Durchlauf.
  // Der Web-Handler selbst fasst LVGL nicht an; der Screensaver bleibt separat.
  ui_surface_style::request_global_tile_border_refresh();
  server.send(200, "application/json",
              enabled ? "{\"success\":true,\"enabled\":true}"
                      : "{\"success\":true,\"enabled\":false}");
}

void WebAdminServer::handleGetScreensaverWallpaper() {
  webAdminMarkActivity();
  if (!Device::sdReady() || !server.hasArg("name")) {
    server.send(404, "text/plain", "Wallpaper unavailable");
    return;
  }
  String name = server.arg("name");
  name.trim();
  if (!name.length() || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 ||
      name.indexOf("..") >= 0 ||
      (!endsWithIgnoreCase(name, ".jpg") &&
       !endsWithIgnoreCase(name, ".jpeg"))) {
    server.send(400, "text/plain", "Invalid wallpaper");
    return;
  }
  String path = String("/images/") + name;
  if (!Device::sdFS().exists(path)) path = String("/wallpapers/") + name;
  fs::File file = Device::sdFS().open(path, FILE_READ);
  if (!file || file.isDirectory()) {
    if (file) file.close();
    server.send(404, "text/plain", "Wallpaper not found");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, "image/jpeg");
  file.close();
}

void WebAdminServer::handleFileManagerList() {
  webAdminMarkActivity();
  fs::FS* fs = nullptr;
  String fs_key;
  String error;
  if (!resolveFileManagerFsFromRequest(server, fs, fs_key, error)) {
    sendJsonError(server, 503, error);
    return;
  }

  const String path = normalizeFileManagerPath(server.hasArg("path") ? server.arg("path") : String("/"));
  if (!isSafeFileManagerPath(path)) {
    sendJsonError(server, 400, "Invalid path");
    return;
  }

  File dir = fs->open(path, FILE_READ);
  if (!dir) {
    sendJsonError(server, 404, "Folder not found");
    return;
  }
  if (!dir.isDirectory()) {
    dir.close();
    sendJsonError(server, 400, "Path is not a folder");
    return;
  }

  std::vector<FileManagerEntry> entries;
  entries.reserve(32);
  File child = dir.openNextFile();
  while (child && entries.size() < 250) {
    FileManagerEntry info;
    info.name = fileManagerEntryName(child.name());
    info.directory = child.isDirectory();
    info.size = info.directory ? 0 : static_cast<size_t>(child.size());
    info.modified = static_cast<uint32_t>(child.getLastWrite());
    child.close();
    if (isSafeFileManagerName(info.name)) {
      info.path = joinPath(path, info.name);
      entries.push_back(info);
    }
    child = dir.openNextFile();
  }
  dir.close();

  std::sort(entries.begin(), entries.end(), [](const FileManagerEntry& a, const FileManagerEntry& b) {
    if (a.directory != b.directory) return a.directory && !b.directory;
    String an = a.name;
    String bn = b.name;
    an.toLowerCase();
    bn.toLowerCase();
    return an.compareTo(bn) < 0;
  });

  String json = "{\"success\":true,\"fs\":\"";
  appendJsonEscaped(json, fs_key);
  json += "\",\"path\":\"";
  appendJsonEscaped(json, path);
  json += "\",\"parent\":\"";
  appendJsonEscaped(json, fileManagerParentPath(path));
  json += "\",\"entries\":[";
  for (size_t i = 0; i < entries.size(); ++i) {
    if (i > 0) json += ",";
    json += "{\"name\":\"";
    appendJsonEscaped(json, entries[i].name);
    json += "\",\"path\":\"";
    appendJsonEscaped(json, entries[i].path);
    json += "\",\"dir\":";
    json += entries[i].directory ? "true" : "false";
    json += ",\"size\":";
    json += String(static_cast<unsigned long>(entries[i].size));
    json += ",\"modified\":";
    json += String(static_cast<unsigned long>(entries[i].modified));
    json += "}";
  }
  json += "]}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleFileManagerDownload() {
  webAdminMarkActivity();
  fs::FS* fs = nullptr;
  String fs_key;
  String error;
  if (!resolveFileManagerFsFromRequest(server, fs, fs_key, error)) {
    server.send(503, "text/plain", error);
    return;
  }

  const String path = normalizeFileManagerPath(server.hasArg("path") ? server.arg("path") : String());
  if (!isSafeFileManagerPath(path) || path == "/") {
    server.send(400, "text/plain", "Invalid path");
    return;
  }
  if (!fs->exists(path)) {
    server.send(404, "text/plain", "File not found");
    return;
  }

  File file = fs->open(path, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Could not open file");
    return;
  }
  if (file.isDirectory()) {
    file.close();
    server.send(400, "text/plain", "Folders cannot be downloaded");
    return;
  }

  String filename = fileManagerBaseName(path);
  filename.replace("\"", "_");
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, fileManagerContentType(path));
  file.close();
}

void WebAdminServer::handleFileManagerDelete() {
  webAdminMarkActivity();
  fs::FS* fs = nullptr;
  String fs_key;
  String error;
  if (!resolveFileManagerFsFromRequest(server, fs, fs_key, error)) {
    sendJsonError(server, 503, error);
    return;
  }
  if (!requireFileManagerWriteAccess(server, fs_key)) return;

  const String path = normalizeFileManagerPath(server.hasArg("path") ? server.arg("path") : String());
  if (!isSafeFileManagerPath(path) || path == "/") {
    sendJsonError(server, 400, "Invalid path");
    return;
  }

  if (!fs->exists(path)) {
    sendJsonError(server, 404, "Path not found");
    return;
  }

  if (!removeFileManagerPathRecursive(*fs, path, error)) {
    sendJsonError(server, 500, error.length() ? error : String("Delete failed"));
    return;
  }
  sendJsonOk(server);
}

void WebAdminServer::handleFileManagerRename() {
  webAdminMarkActivity();
  fs::FS* fs = nullptr;
  String fs_key;
  String error;
  if (!resolveFileManagerFsFromRequest(server, fs, fs_key, error)) {
    sendJsonError(server, 503, error);
    return;
  }
  if (!requireFileManagerWriteAccess(server, fs_key)) return;

  const String path = normalizeFileManagerPath(server.hasArg("path") ? server.arg("path") : String());
  String new_name = server.hasArg("name") ? server.arg("name") : String();
  new_name.trim();
  if (!isSafeFileManagerPath(path) || path == "/" || !isSafeFileManagerName(new_name)) {
    sendJsonError(server, 400, "Invalid name or path");
    return;
  }
  if (!fs->exists(path)) {
    sendJsonError(server, 404, "Path not found");
    return;
  }

  const String target = joinPath(fileManagerParentPath(path), new_name);
  if (!isSafeFileManagerPath(target)) {
    sendJsonError(server, 400, "Invalid target path");
    return;
  }
  if (fs->exists(target)) {
    sendJsonError(server, 409, "Target already exists");
    return;
  }
  errno = 0;
  if (!fs->rename(path, target)) {
    const int saved_errno = errno;
    logFileManagerWriteFailure("rename", fs_key, target, saved_errno);
    sendJsonError(server, 500, fileManagerWriteError("Rename failed", saved_errno));
    return;
  }

  String json = "{\"success\":true,\"path\":\"";
  appendJsonEscaped(json, target);
  json += "\"}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleFileManagerMkdir() {
  webAdminMarkActivity();
  fs::FS* fs = nullptr;
  String fs_key;
  String error;
  if (!resolveFileManagerFsFromRequest(server, fs, fs_key, error)) {
    sendJsonError(server, 503, error);
    return;
  }
  if (!requireFileManagerWriteAccess(server, fs_key)) return;

  const String path = normalizeFileManagerPath(server.hasArg("path") ? server.arg("path") : String("/"));
  String name = server.hasArg("name") ? server.arg("name") : String();
  name.trim();
  if (!isSafeFileManagerPath(path) || !isSafeFileManagerName(name)) {
    sendJsonError(server, 400, "Invalid name or path");
    return;
  }

  File dir = fs->open(path, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    sendJsonError(server, 404, "Folder not found");
    return;
  }
  dir.close();

  const String target = joinPath(path, name);
  if (!isSafeFileManagerPath(target)) {
    sendJsonError(server, 400, "Invalid target path");
    return;
  }
  if (fs->exists(target)) {
    sendJsonError(server, 409, "Folder already exists");
    return;
  }
  errno = 0;
  if (!fs->mkdir(target)) {
    const int saved_errno = errno;
    logFileManagerWriteFailure("mkdir", fs_key, target, saved_errno);
    sendJsonError(server,
                  500,
                  fileManagerWriteError("Could not create folder", saved_errno));
    return;
  }
  sendJsonOk(server);
}

void WebAdminServer::handleFileManagerUpload() {
  HTTPUpload& upload = server.upload();

  // Haelt den Media-Cover-Worker waehrend des gesamten Uploads angehalten
  // (der prueft webAdminRecentlyActive): ein parallel startender
  // Cover-Download wuerde den ohnehin maximal belasteten internen
  // DMA-Puffer-Pool des WLAN-Treibers zusaetzlich unter Druck setzen.
  webAdminMarkActivity();

  if (upload.status == UPLOAD_FILE_START) {
    resetFileManagerUploadState();
    g_file_manager_upload_started = true;

    // Empfangsfenster dieser Verbindung klein halten: lwIP erlaubt dem
    // Browser sonst 64KB unbestaetigte Daten (CONFIG_LWIP_TCP_WND_DEFAULT),
    // die alle in internen DMA-Puffern landen muessen -- gemessene
    // Absturzursache (assert in sdio_rx_get_buffer bei 1MB-Upload).
    int rcvbuf = 8 * 1024;
    server.client().setSocketOption(SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    fs::FS* fs = nullptr;
    String fs_key;
    String error;
    if (!resolveFileManagerFsFromRequest(server, fs, fs_key, error)) {
      g_file_manager_upload_error = error;
      return;
    }
    if (!Device::sdWritable()) {
      g_file_manager_upload_error =
          "microSD is readable but not writable; check the serial SD write-test error";
      Serial.println("[FileManager/SD] upload rejected: mounted card did not pass the write test");
      return;
    }

    // append=1: Fortsetzungs-Teil eines in kleine Requests zerlegten Uploads
    // (siehe uploadFileManagerFile im Admin-JS) -- Datei anfuegen statt neu.
    g_file_manager_upload_is_append = server.hasArg("append") && server.arg("append") == "1";

    const String dir_path = normalizeFileManagerPath(server.hasArg("path") ? server.arg("path") : String("/"));
    const String filename = sanitizeFileManagerUploadName(upload.filename);
    if (!isSafeFileManagerPath(dir_path) || !filename.length()) {
      g_file_manager_upload_error = "Invalid upload path or filename";
      return;
    }

    File dir = fs->open(dir_path, FILE_READ);
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      g_file_manager_upload_error = "Upload folder not found";
      return;
    }
    dir.close();

    const String target = joinPath(dir_path, filename);
    if (!isSafeFileManagerPath(target)) {
      g_file_manager_upload_error = "Invalid target path";
      return;
    }

    if (g_file_manager_upload_is_append) {
      if (!fs->exists(target)) {
        g_file_manager_upload_error = "Append target missing";
        return;
      }
      errno = 0;
      g_file_manager_upload_file = fs->open(target, FILE_APPEND);
    } else {
      if (fs->exists(target)) {
        File existing = fs->open(target, FILE_READ);
        const bool existing_is_dir = existing && existing.isDirectory();
        if (existing) existing.close();
        if (existing_is_dir || !fs->remove(target)) {
          g_file_manager_upload_error = "Could not replace existing file";
          return;
        }
      }
      errno = 0;
      g_file_manager_upload_file = fs->open(target, FILE_WRITE);
    }
    if (!g_file_manager_upload_file) {
      const int saved_errno = errno;
      logFileManagerWriteFailure("open-upload", fs_key, target, saved_errno);
      g_file_manager_upload_error =
          fileManagerWriteError("Could not open target file", saved_errno);
      return;
    }

    g_file_manager_upload_fs_key = fs_key;
    g_file_manager_upload_path = target;
    g_file_manager_upload_bytes = 0;
    g_file_manager_upload_next_heap_log = 512u * 1024u;
    if (!g_file_manager_upload_is_append) {
      Serial.printf("[FileManager] Upload started: %s:%s\n", fs_key.c_str(), target.c_str());
      logFileManagerUploadHeap("start");
    }
    return;
  }

  if (g_file_manager_upload_error.length() > 0 || !g_file_manager_upload_started) {
    return;
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (!g_file_manager_upload_file) {
      g_file_manager_upload_error = "Upload file is not open";
      return;
    }
    errno = 0;
    const size_t written = g_file_manager_upload_file.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      const int saved_errno = errno;
      logFileManagerWriteFailure("write-upload",
                                 g_file_manager_upload_fs_key,
                                 g_file_manager_upload_path,
                                 saved_errno);
      g_file_manager_upload_error =
          fileManagerWriteError("Could not write upload chunk", saved_errno);
      logFileManagerUploadHeap("write-fehler");
      g_file_manager_upload_file.close();
      fs::FS* fs = nullptr;
      String fs_key;
      String error;
      if (resolveFileManagerFsByKey(g_file_manager_upload_fs_key, fs, fs_key, error)) {
        fs->remove(g_file_manager_upload_path);
      }
      return;
    }
    g_file_manager_upload_bytes += written;
    if (g_file_manager_upload_bytes >= g_file_manager_upload_next_heap_log) {
      g_file_manager_upload_next_heap_log += 512u * 1024u;
      logFileManagerUploadHeap("write");
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    g_file_manager_upload_error = "Upload aborted";
    logFileManagerUploadHeap("abbruch");
    if (g_file_manager_upload_file) {
      g_file_manager_upload_file.close();
    }
    fs::FS* fs = nullptr;
    String fs_key;
    String error;
    if (resolveFileManagerFsByKey(g_file_manager_upload_fs_key, fs, fs_key, error)) {
      fs->remove(g_file_manager_upload_path);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (g_file_manager_upload_file) {
      g_file_manager_upload_file.close();
    }
    g_file_manager_upload_finished = true;
    // Append-Teile eines zerlegten Uploads nicht einzeln loggen (bis zu
    // 64 Requests pro Datei) -- Fehler/Abbruch loggen weiterhin immer.
    if (!g_file_manager_upload_is_append) {
      Serial.printf("[FileManager] Uploaded %s (%u bytes)\n",
                    g_file_manager_upload_path.c_str(),
                    static_cast<unsigned>(g_file_manager_upload_bytes));
      logFileManagerUploadHeap("end");
    }
  }
}

void WebAdminServer::handleFileManagerUploadDone() {
  webAdminMarkActivity();
  if (!g_file_manager_upload_started) {
    sendJsonError(server, 400, "No upload started");
    return;
  }

  if (g_file_manager_upload_error.length() > 0) {
    String error = g_file_manager_upload_error;
    resetFileManagerUploadState();
    sendJsonError(server, 500, error);
    return;
  }

  if (!g_file_manager_upload_finished) {
    resetFileManagerUploadState();
    sendJsonError(server, 500, "Upload did not finish");
    return;
  }

  String json = "{\"success\":true,\"path\":\"";
  appendJsonEscaped(json, g_file_manager_upload_path);
  json += "\",\"size\":";
  json += String(static_cast<unsigned long>(g_file_manager_upload_bytes));
  json += "}";
  resetFileManagerUploadState();
  sendChunkedResponse(server, 200, "application/json", json);
}

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
    // Empfangsfenster begrenzen -- gleiche Absturzursache wie beim
    // Filemanager-Upload (interner DMA-Heap vs. 64KB TCP-Fenster).
    int rcvbuf = 8 * 1024;
    server.client().setSocketOption(SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    const bool was_prepared = g_ota_upload_state.upload_prepared;
#if defined(DEVICE_GUITION_ESP32_4848S040)
    const size_t prepared_size = g_ota_upload_state.upload_total_bytes;
#endif
    if (!was_prepared) {
      resetOtaUploadState();
    }
    g_ota_upload_state.upload_started = true;
    g_ota_upload_state.upload_prepared = false;
    const size_t request_size = parseOtaExpectedSize(server);
#if defined(DEVICE_GUITION_ESP32_4848S040)
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
#if defined(DEVICE_GUITION_ESP32_4848S040)
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
        firmware_meta::DeviceDescriptor incoming_desc{};
        if (!firmware_meta::parseDeviceDescriptorFromImage(
                g_ota_upload_state.staging_blocks[0],
                std::min(g_ota_upload_state.staged_bytes,
                         kOtaStagingBlockBytes),
                incoming_desc)) {
          g_ota_upload_state.error =
              "Firmware metadata missing or invalid";
          return;
        }
        if (!firmware_meta::matchesCurrentDeviceKey(
                incoming_desc.device_key)) {
          g_ota_upload_state.error =
              String("Firmware device mismatch: got ") +
              incoming_desc.display_name + ", expected " +
              firmware_meta::expectedDeviceDisplayName();
          return;
        }
        if (strcmp(incoming_desc.project_key,
                   firmware_meta::currentProjectKey()) != 0) {
          g_ota_upload_state.error =
              String("Firmware project mismatch: got ") +
              incoming_desc.project_key + ", expected " +
              firmware_meta::currentProjectKey();
          return;
        }
        g_ota_upload_state.image_validated = true;
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

    if (!g_ota_upload_state.image_validated) {
      firmware_meta::DeviceDescriptor incoming_desc{};
      if (firmware_meta::parseDeviceDescriptorFromImage(
              g_ota_upload_state.buffered_bytes,
              g_ota_upload_state.buffered_len,
              incoming_desc)) {
        if (!firmware_meta::matchesCurrentDeviceKey(incoming_desc.device_key)) {
          g_ota_upload_state.error =
              String("Firmware device mismatch: got ") + incoming_desc.display_name +
              ", expected " + firmware_meta::expectedDeviceDisplayName();
          return;
        }
        if (strcmp(incoming_desc.project_key, firmware_meta::currentProjectKey()) != 0) {
          g_ota_upload_state.error =
              String("Firmware project mismatch: got ") + incoming_desc.project_key +
              ", expected " + firmware_meta::currentProjectKey();
          return;
        }
        g_ota_upload_state.image_validated = true;
        if (!beginDirectOtaInstall()) {
          return;
        }
        if (!writeDirectOtaChunk(g_ota_upload_state.buffered_bytes, g_ota_upload_state.buffered_len)) {
          return;
        }
        g_ota_upload_state.buffered_len = 0;
        if (static_cast<size_t>(upload.currentSize) > buffered_copy_len) {
          const size_t remaining_in_chunk = static_cast<size_t>(upload.currentSize) - buffered_copy_len;
          if (!writeDirectOtaChunk(upload.buf + buffered_copy_len, remaining_in_chunk)) {
            return;
          }
        }
        return;
      } else if (g_ota_upload_state.buffered_len >= firmware_meta::kDeviceDescriptorImageBytes) {
        g_ota_upload_state.error = "Firmware metadata missing or invalid";
        return;
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
#if defined(DEVICE_GUITION_ESP32_4848S040)
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

#if defined(DEVICE_GUITION_ESP32_4848S040)
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

  // Ein fehlgeschlagenes GitHub-OTA kann auch dann zurueckgesetzt werden,
  // wenn der urspruengliche Browser-Tab den Fehlerstatus nicht mehr abholt.
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

void WebAdminServer::handleCreateScreenshot() {
  String error;
  if (!createUiScreenshot(error)) {
    String json = "{\"success\":false,\"error\":\"";
    appendJsonEscaped(json, error);
    json += "\"}";
    server.send(500, "application/json", json);
    return;
  }

  String json = "{\"success\":true,\"path\":\"";
  appendJsonEscaped(json, kScreenshotPath);
  json += "\"}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleDownloadScreenshot() {
  if (!sdReady()) {
    server.send(503, "text/plain", "microSD card not available for screenshots");
    return;
  }
  if (!sdFS().exists(kScreenshotPath)) {
    server.send(404, "text/plain", "Screenshot not found");
    return;
  }

  File file = sdFS().open(kScreenshotPath, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "Could not open screenshot");
    return;
  }

  String filename = Device::profile().key;
  filename += "-ui-screenshot.jpg";
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, "image/jpeg");
  file.close();
}

// ========== Crash-Diagnose ==========
// Der IDF-Panic-Handler legt bei jeder Panic einen Core-Dump in der
// coredump-Partition ab (siehe src/core/crash_log.h). Diese Endpunkte machen
// ihn ohne USB-Kabel zugaenglich; crashlog.txt liegt im LittleFS und waere im
// File Manager (nur microSD) sonst unsichtbar.

void WebAdminServer::handleCoreDumpDownload() {
  webAdminMarkActivity();
  size_t dump_addr = 0;
  size_t dump_size = 0;
  if (esp_core_dump_image_get(&dump_addr, &dump_size) != ESP_OK ||
      dump_size == 0) {
    server.send(404, "text/plain", "No core dump stored");
    return;
  }
  const esp_partition_t* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
  if (!part || dump_addr < part->address ||
      dump_addr - part->address + dump_size > part->size) {
    server.send(500, "text/plain", "Core dump partition mismatch");
    return;
  }

  String filename = Device::profile().key;
  filename += "-coredump.bin";
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"" + filename + "\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(dump_size);
  server.send(200, "application/octet-stream", "");

  uint8_t buf[1024];
  size_t offset = dump_addr - part->address;
  size_t remaining = dump_size;
  while (remaining > 0) {
    const size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
    if (esp_partition_read(part, offset, buf, chunk) != ESP_OK) break;
    server.sendContent(reinterpret_cast<const char*>(buf), chunk);
    offset += chunk;
    remaining -= chunk;
    yield();
  }
}

void WebAdminServer::handleCoreDumpErase() {
  webAdminMarkActivity();
#if defined(DEVICE_GUITION_ESP32_4848S040)
  Device::ScopedStorageWrite storage_write;
#endif
  const esp_err_t err = esp_core_dump_image_erase();
  if (err != ESP_OK) {
    sendJsonError(server, 500, String("Erase failed: ") + esp_err_to_name(err));
    return;
  }
  server.send(200, "application/json", "{\"success\":true}");
}

void WebAdminServer::handleCrashLogDownload() {
  webAdminMarkActivity();
  File file = LittleFS.open(CrashLog::kLogPath, FILE_READ);
  if (!file) {
    server.send(404, "text/plain", "No crash log stored");
    return;
  }
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"crashlog.txt\"");
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, "text/plain");
  file.close();
}

void WebAdminServer::handleSdDiagnosticsDownload() {
  webAdminMarkActivity();

  String report;
  report.reserve(1400);
  report += "HomeTiles SD diagnostic report\n";
  report += "Firmware: hometiles-";
  report += FW_VERSION;
  report += '\n';
  report += "Device profile: ";
  report += Device::profile().key;
  report += '\n';
  report += "Generated at uptime: ";
  report += String(millis());
  report += " ms\n";
  report += "Mode: live on-demand check; no diagnostic history is kept in RAM or flash\n";
  report += "Test: create directory, write/read 16 bytes, remove file and directory\n\n";

  auto send_report = [&]() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain; charset=utf-8", report);
  };
  auto append_errno = [&](const char* phase, int error_number) {
    report += "Result: FAIL\nPhase: ";
    report += phase ? phase : "unknown";
    report += "\nerrno: ";
    report += String(error_number);
    report += " (";
    report += error_number ? strerror(error_number) : "not provided by filesystem";
    report += ")\n";
  };

  const bool ready = Device::sdReady();
#if defined(DEVICE_GUITION_JC1060P470C)
  report += "SD bus: SDMMC slot 0, 4-bit, CLK=43 CMD=44 D0-D3=39-42\n";
  report += "SD power: LDO VO4, GPIO45 active-low, 200 ms reset before each mount attempt\n";
  report += "Last driver phase: ";
  report += JC1060SDMMC.lastErrorPhase();
  report += "\nLast driver result: ";
  report += esp_err_to_name(JC1060SDMMC.lastError());
  report += " (0x";
  report += String(static_cast<unsigned>(JC1060SDMMC.lastError()), HEX);
  report += ")\n";
#endif
  report += "Mounted: ";
  report += ready ? "yes\n" : "no\n";
  if (!ready) {
    report += "Result: FAIL\nPhase: mount/card availability\n";
    send_report();
    return;
  }

  fs::FS& fs = Device::sdFS();
  uint64_t capacity = 0;
  uint64_t free_bytes = 0;
  const char* mountpoint = fs.mountpoint();
  const esp_err_t info_result = mountpoint
                                    ? esp_vfs_fat_info(mountpoint, &capacity,
                                                       &free_bytes)
                                    : ESP_ERR_INVALID_STATE;
  if (info_result == ESP_OK) {
    report += "Capacity: ";
    report += String(static_cast<unsigned long long>(capacity));
    report += " bytes\nUsed: ";
    report += String(static_cast<unsigned long long>(capacity - free_bytes));
    report += " bytes\n";
  } else {
    report += "Capacity: unavailable (";
    report += esp_err_to_name(info_result);
    report += ")\n";
  }

  char directory[56] = {};
  char file_path[80] = {};
  const unsigned long nonce = static_cast<unsigned long>(esp_random());
  snprintf(directory, sizeof(directory), "/.hometiles-sd-check-%08lx", nonce);
  snprintf(file_path, sizeof(file_path), "%s/probe.bin", directory);
  static constexpr uint8_t kProbeData[] = {
      0x48, 0x6f, 0x6d, 0x65, 0x54, 0x69, 0x6c, 0x65,
      0x73, 0x2d, 0x53, 0x44, 0x2d, 0x52, 0x57, 0x0a,
  };

  errno = 0;
  if (!fs.mkdir(directory)) {
    append_errno("mkdir", errno);
    send_report();
    return;
  }

  errno = 0;
  File output = fs.open(file_path, FILE_WRITE);
  if (!output) {
    const int saved_errno = errno;
    fs.rmdir(directory);
    append_errno("open for write", saved_errno);
    send_report();
    return;
  }

  errno = 0;
  const size_t written = output.write(kProbeData, sizeof(kProbeData));
  output.flush();
  const int write_error = output.getWriteError();
  output.close();
  if (written != sizeof(kProbeData) || write_error != 0) {
    const int saved_errno = errno;
    fs.remove(file_path);
    fs.rmdir(directory);
    append_errno("write/flush", saved_errno);
    report += "Written: ";
    report += String(written);
    report += "/";
    report += String(sizeof(kProbeData));
    report += " bytes\nPrint error: ";
    report += String(write_error);
    report += '\n';
    send_report();
    return;
  }

  errno = 0;
  File input = fs.open(file_path, FILE_READ);
  if (!input) {
    const int saved_errno = errno;
    fs.remove(file_path);
    fs.rmdir(directory);
    append_errno("open for readback", saved_errno);
    send_report();
    return;
  }

  uint8_t readback[sizeof(kProbeData)] = {};
  const size_t read_count = input.read(readback, sizeof(readback));
  input.close();
  if (read_count != sizeof(readback) ||
      memcmp(readback, kProbeData, sizeof(readback)) != 0) {
    const int saved_errno = errno;
    fs.remove(file_path);
    fs.rmdir(directory);
    append_errno("readback verification", saved_errno);
    report += "Read: ";
    report += String(read_count);
    report += "/";
    report += String(sizeof(readback));
    report += " bytes\n";
    send_report();
    return;
  }

  errno = 0;
  const bool file_removed = fs.remove(file_path);
  const int remove_errno = errno;
  errno = 0;
  const bool directory_removed = fs.rmdir(directory);
  const int rmdir_errno = errno;
  if (!file_removed || !directory_removed) {
    append_errno(!file_removed ? "remove probe file" : "remove probe directory",
                 !file_removed ? remove_errno : rmdir_errno);
    send_report();
    return;
  }

  report += "Result: PASS\n";
  report += "The card completed mkdir/write/read/remove successfully.\n";

  send_report();
}

// ========== Folder API ==========

void WebAdminServer::handleGetFolders() {
  const auto& folders = tileConfig.getFolders();
  String json = "[";
  for (size_t i = 0; i < folders.size(); ++i) {
    const auto& entry = folders[i];
    if (i > 0) json += ",";
    json += "{\"id\":";
    json += String(entry.id);
    json += ",\"parent_id\":";
    json += String(entry.parent_id);
    json += ",\"name\":\"";
    appendJsonEscaped(json, entry.name);
    json += "\",\"icon_name\":\"";
    appendJsonEscaped(json, entry.icon_name);
    json += "\",\"pin_enabled\":";
    json += tileConfig.isFolderPinEnabled(entry.id) ? "true" : "false";
    json += "}";
  }
  json += "]";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleGetFolderTab() {
  webAdminMarkActivity();
  if (!server.hasArg("folder_id")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing folder_id\"}");
    return;
  }

  const uint16_t folder_id = static_cast<uint16_t>(server.arg("folder_id").toInt());
  if (!tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }

  String button_html;
  String tab_html;
  String tab_id;
  if (!buildAdminFolderTabFragments(folder_id, button_html, tab_html, tab_id)) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Folder tab build failed\"}");
    return;
  }

  String json = "{\"success\":true,\"folder_id\":";
  json += String(folder_id);
  json += ",\"tab_id\":\"";
  appendJsonEscaped(json, tab_id);
  json += "\",\"button_html\":\"";
  appendJsonEscaped(json, button_html);
  json += "\",\"tab_html\":\"";
  appendJsonEscaped(json, tab_html);
  json += "\"}";
  sendChunkedResponse(server, 200, "application/json", json);
  webAdminMarkActivity();
}

void WebAdminServer::handleSaveFolderAccess() {
  webAdminMarkActivity();
  const auto& tr = i18n::strings(configManager.getConfig().language);
  auto sendError = [this](int status, const char* message) {
    String json = "{\"success\":false,\"error\":\"";
    appendJsonEscaped(json, String(message ? message : ""));
    json += "\"}";
    server.send(status, "application/json", json);
  };

  if (!server.hasArg("folder_id")) {
    sendError(400, tr.folder_pin_create_first);
    return;
  }
  const int requested_id = server.arg("folder_id").toInt();
  if (requested_id <= 0 || requested_id > 0xFFFF ||
      !tileConfig.folderExists(static_cast<uint16_t>(requested_id))) {
    sendError(404, tr.folder_pin_create_first);
    return;
  }

  const uint16_t folder_id = static_cast<uint16_t>(requested_id);
  const bool enable = server.hasArg("enabled") &&
                      server.arg("enabled") != "0";
  bool success = false;
  if (!enable) {
    success = tileConfig.clearFolderPin(folder_id);
  } else {
    String pin = server.hasArg("pin") ? server.arg("pin") : String();
    pin.trim();
    if (!pin.length() && tileConfig.isFolderPinEnabled(folder_id)) {
      success = true;
    } else if (!pin_access::isValidUserPin(pin)) {
      pin = "";
      sendError(400, tr.pin_invalid);
      return;
    } else {
      success = tileConfig.setFolderPin(folder_id, pin);
    }
    pin = "";
  }

  if (!success) {
    sendError(500, tr.folder_pin_save_failed);
    return;
  }
  String json = "{\"success\":true,\"pin_enabled\":";
  json += tileConfig.isFolderPinEnabled(folder_id) ? "true" : "false";
  json += ",\"folder_pin\":\"";
  String stored_pin;
  if (tileConfig.getFolderPin(folder_id, stored_pin)) {
    appendJsonEscaped(json, stored_pin);
  }
  stored_pin = "";
  json += "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void WebAdminServer::handleDeleteFolder() {
  webAdminMarkActivity();
  if (!server.hasArg("folder_id")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing folder_id\"}");
    return;
  }

  uint16_t folder_id = static_cast<uint16_t>(server.arg("folder_id").toInt());
  if (folder_id == 0) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Root folder cannot be deleted\"}");
    return;
  }
  if (!tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }

  // Find parent folder and clear the tile that references this folder
  uint16_t parent_id = tileConfig.getFolderParent(folder_id);
  TileGridConfig parent_grid{};
  if (tileConfig.loadFolderGrid(parent_id, parent_grid)) {
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      Tile& t = parent_grid.tiles[i];
      if (t.type == TILE_FOLDER) {
        uint16_t target = getNavigateTargetId(t);
        if (target == folder_id) {
          t = Tile{};
          break;
        }
      }
    }
    tileConfig.saveFolderGrid(parent_id, parent_grid);
    tiles_invalidate_folder(parent_id);
  }

  if (!tileConfig.deleteFolder(folder_id)) {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Delete failed\"}");
    return;
  }

  mqttRequestDynamicSlotsReload(5000);
  Serial.printf("[WebAdmin] Folder %u deleted\n", static_cast<unsigned>(folder_id));
  server.send(200, "application/json", "{\"success\":true}");
}

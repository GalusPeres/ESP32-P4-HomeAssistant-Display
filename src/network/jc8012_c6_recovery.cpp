#include "src/network/jc8012_c6_recovery.h"
#include "src/devices/device.h"

#if defined(DEVICE_GUITION_JC8012P4A1) && \
    defined(HOMETILES_JC8012_C6_RECOVERY)

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp32-hal-hosted.h>
#include <esp_app_format.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

extern "C" {
#include <esp_hosted_ota.h>
}

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>

namespace Jc8012C6Recovery {
namespace {

constexpr char kPayloadMagic[8] = {'H', 'T', 'C', '6', 'R', '0', '0', '1'};
constexpr uint32_t kPayloadHeaderSize = 64;
constexpr uint32_t kPayloadOffset = 4096;
constexpr uint32_t kPayloadSize = 1191424;
constexpr uint8_t kTargetMajor = 2;
constexpr uint8_t kTargetMinor = 11;
constexpr uint8_t kTargetPatch = 6;
constexpr uint32_t kWriteChunkSize = 1400;
constexpr uint32_t kWriteYieldMs = 10;
constexpr uint32_t kProgressInterval = 128 * 1024;
constexpr uint32_t kFlashSectorSize = 4096;
constexpr char kPreferencesNamespace[] = "htc6r2116";
constexpr char kStateKey[] = "state";
constexpr uint32_t kApp0Address = 0x10000;
constexpr uint32_t kApp1Address = 0x690000;
constexpr uint32_t kAppPartitionSize = 0x680000;

constexpr uint8_t kExpectedSha256[32] = {
    0xE3, 0x2F, 0xBA, 0x38, 0x64, 0xAB, 0x4D, 0xB8,
    0x2C, 0x28, 0x7A, 0x92, 0x2D, 0xB8, 0x3B, 0x70,
    0x93, 0xD7, 0xD8, 0x59, 0x27, 0x30, 0xD7, 0xA6,
    0x20, 0x88, 0x7B, 0x7C, 0xFD, 0xF4, 0x01, 0xE0,
};

struct RecoveryPayloadHeader {
  char magic[8];
  uint32_t header_size;
  uint32_t payload_offset;
  uint32_t payload_size;
  uint8_t sha256[32];
  uint8_t target_major;
  uint8_t target_minor;
  uint8_t target_patch;
  uint8_t flags;
  uint32_t header_crc32;
  uint8_t reserved[4];
} __attribute__((packed));

static_assert(sizeof(RecoveryPayloadHeader) == kPayloadHeaderSize,
              "The JC8012 C6 recovery header must remain 64 bytes");

bool g_blocked = false;

enum class RecoveryState : uint8_t {
  None = 0,
  InProgress = 1,
  AwaitVerify = 2,
  FailedUncertain = 3,
};

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return ~crc;
}

bool versionEquals(uint32_t major, uint32_t minor, uint32_t patch) {
  return major == kTargetMajor && minor == kTargetMinor &&
         patch == kTargetPatch;
}

bool versionIsNewer(uint32_t major, uint32_t minor, uint32_t patch) {
  if (major != kTargetMajor) return major > kTargetMajor;
  if (minor != kTargetMinor) return minor > kTargetMinor;
  return patch > kTargetPatch;
}

bool versionSupportsActivate(uint32_t major, uint32_t minor,
                             uint32_t patch) {
  (void)patch;
  return major > 2 || (major == 2 && minor >= 6);
}

RecoveryState readState() {
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, true)) {
    return RecoveryState::None;
  }
  const RecoveryState state = static_cast<RecoveryState>(
      preferences.getUChar(kStateKey,
                           static_cast<uint8_t>(RecoveryState::None)));
  preferences.end();
  return state;
}

bool writeState(RecoveryState state) {
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) return false;
  const bool stored =
      preferences.putUChar(kStateKey, static_cast<uint8_t>(state)) ==
      sizeof(uint8_t);
  preferences.end();
  return stored;
}

void clearState() {
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) return;
  preferences.remove(kStateKey);
  preferences.end();
}

const esp_partition_t* recoveryPartition() {
  const esp_partition_t* partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, "app1");
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!partition || !running || running->address == partition->address ||
      running->subtype != ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
      running->address != kApp0Address ||
      running->size != kAppPartitionSize ||
      partition->address != kApp1Address ||
      partition->size != kAppPartitionSize) {
    Serial.println(
        "[JC8012/C6] Recovery payload requires the beta to run from app0");
    return nullptr;
  }
  return partition;
}

bool readHeader(const esp_partition_t* partition,
                RecoveryPayloadHeader& header) {
  if (!partition || partition->size < kPayloadOffset + kPayloadSize) {
    return false;
  }
  if (esp_partition_read(partition, 0, &header, sizeof(header)) != ESP_OK) {
    Serial.println("[JC8012/C6] Could not read the recovery payload header");
    return false;
  }
  return true;
}

bool headerIsPresent(const RecoveryPayloadHeader& header) {
  return std::memcmp(header.magic, kPayloadMagic, sizeof(kPayloadMagic)) == 0;
}

bool headerIsValid(const RecoveryPayloadHeader& header) {
  return headerIsPresent(header) &&
         header.header_size == kPayloadHeaderSize &&
         header.payload_offset == kPayloadOffset &&
         header.payload_size == kPayloadSize &&
         header.target_major == kTargetMajor &&
         header.target_minor == kTargetMinor &&
         header.target_patch == kTargetPatch &&
         header.flags == 1 &&
         header.header_crc32 ==
             crc32(reinterpret_cast<const uint8_t*>(&header),
                   offsetof(RecoveryPayloadHeader, header_crc32)) &&
         std::memcmp(header.sha256, kExpectedSha256,
                     sizeof(kExpectedSha256)) == 0 &&
         std::all_of(std::begin(header.reserved), std::end(header.reserved),
                     [](uint8_t value) { return value == 0; });
}

bool invalidatePayload(const esp_partition_t* partition) {
  const esp_err_t err =
      esp_partition_erase_range(partition, 0, kFlashSectorSize);
  if (err != ESP_OK) {
    Serial.printf("[JC8012/C6] Could not clear recovery header: %s (%d)\n",
                  esp_err_to_name(err), static_cast<int>(err));
    return false;
  }
  clearState();
  return true;
}

bool validateImageHeader(const esp_partition_t* partition,
                         const RecoveryPayloadHeader& header) {
  esp_image_header_t image_header{};
  const esp_err_t err = esp_partition_read(
      partition, header.payload_offset, &image_header, sizeof(image_header));
  const bool valid =
      err == ESP_OK && image_header.magic == ESP_IMAGE_HEADER_MAGIC &&
      image_header.chip_id == ESP_CHIP_ID_ESP32C6 &&
      image_header.spi_mode == ESP_IMAGE_SPI_MODE_DIO &&
      image_header.spi_size == ESP_IMAGE_FLASH_SIZE_4MB;
  if (!valid) {
    Serial.println("[JC8012/C6] Recovery payload is not the expected C6 image");
  }
  return valid;
}

bool validatePayloadHash(const esp_partition_t* partition,
                         const RecoveryPayloadHeader& header) {
  constexpr size_t kHashBufferSize = 4096;
  uint8_t* buffer = static_cast<uint8_t*>(heap_caps_malloc(
      kHashBufferSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!buffer) {
    Serial.println("[JC8012/C6] Could not allocate the hash buffer");
    return false;
  }

  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  bool valid = mbedtls_sha256_starts(&context, 0) == 0;
  uint32_t offset = 0;
  while (valid && offset < header.payload_size) {
    const size_t length =
        std::min<size_t>(kHashBufferSize, header.payload_size - offset);
    valid = esp_partition_read(partition, header.payload_offset + offset,
                               buffer, length) == ESP_OK &&
            mbedtls_sha256_update(&context, buffer, length) == 0;
    offset += length;
  }

  uint8_t digest[32] = {};
  valid = valid && mbedtls_sha256_finish(&context, digest) == 0 &&
          std::memcmp(digest, kExpectedSha256, sizeof(digest)) == 0;
  mbedtls_sha256_free(&context);
  std::memset(buffer, 0, kHashBufferSize);
  heap_caps_free(buffer);

  if (!valid) {
    Serial.println("[JC8012/C6] Recovery payload SHA256 validation failed");
  }
  return valid;
}

bool sendPayload(const esp_partition_t* partition,
                 const RecoveryPayloadHeader& header,
                 bool activate_after_end) {
  uint8_t* buffer = static_cast<uint8_t*>(heap_caps_malloc(
      kWriteChunkSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!buffer) {
    Serial.println("[JC8012/C6] Could not allocate the OTA transfer buffer");
    return false;
  }

  bool success = hostedBeginUpdate();
  if (!success) {
    Serial.println("[JC8012/C6] Coprocessor OTA begin failed; not retrying");
  }

  uint32_t offset = 0;
  uint32_t next_progress = kProgressInterval;
  while (success && offset < header.payload_size) {
    const uint32_t length =
        std::min<uint32_t>(kWriteChunkSize, header.payload_size - offset);
    const esp_err_t read_err = esp_partition_read(
        partition, header.payload_offset + offset, buffer, length);
    if (read_err != ESP_OK) {
      Serial.printf("[JC8012/C6] Payload read failed at %lu: %s (%d)\n",
                    static_cast<unsigned long>(offset),
                    esp_err_to_name(read_err), static_cast<int>(read_err));
      success = false;
      break;
    }
    if (!hostedWriteUpdate(buffer, length)) {
      Serial.printf(
          "[JC8012/C6] Coprocessor OTA write failed at %lu; not retrying\n",
          static_cast<unsigned long>(offset));
      success = false;
      break;
    }
    offset += length;
    if (offset >= next_progress || offset == header.payload_size) {
      Serial.printf("[JC8012/C6] Coprocessor OTA progress: %lu/%lu bytes\n",
                    static_cast<unsigned long>(offset),
                    static_cast<unsigned long>(header.payload_size));
      next_progress += kProgressInterval;
    }
    delay(kWriteYieldMs);
  }

  std::memset(buffer, 0, kWriteChunkSize);
  heap_caps_free(buffer);
  if (!success) return false;

  if (!hostedEndUpdate()) {
    Serial.println("[JC8012/C6] Coprocessor OTA end failed; not retrying");
    return false;
  }
  if (activate_after_end) {
    const esp_err_t activate_err = esp_hosted_slave_ota_activate();
    if (activate_err != ESP_OK) {
      Serial.printf("[JC8012/C6] Coprocessor OTA activation failed: %s (%d)\n",
                    esp_err_to_name(activate_err),
                    static_cast<int>(activate_err));
      return false;
    }
  }
  return true;
}

}  // namespace

Result runIfPresent() {
  if (g_blocked) return Result::Blocked;

  const esp_partition_t* partition = recoveryPartition();
  if (!partition) {
    g_blocked = true;
    return Result::Blocked;
  }

  RecoveryPayloadHeader header{};
  if (!readHeader(partition, header)) {
    g_blocked = true;
    return Result::Blocked;
  }
  if (!headerIsPresent(header)) return Result::NoAction;
  if (!headerIsValid(header) || !validateImageHeader(partition, header) ||
      !validatePayloadHash(partition, header)) {
    g_blocked = true;
    return Result::Blocked;
  }

  const bool sd_was_mounted = Device::suspendSDCardForNetworkTransition();
  const auto finish = [sd_was_mounted](Result result) {
    if (result != Result::RestartRequired && sd_was_mounted &&
        !Device::resumeSDCardAfterNetworkTransition()) {
      Serial.println("[JC8012/C6] SD remount after recovery failed");
    }
    return result;
  };

  WiFi.persistent(false);
  if (!WiFi.mode(WIFI_STA)) {
    Serial.println("[JC8012/C6] Could not initialize ESP-Hosted for recovery");
    g_blocked = true;
    return finish(Result::Blocked);
  }

  uint32_t slave_major = 0;
  uint32_t slave_minor = 0;
  uint32_t slave_patch = 0;
  hostedGetSlaveVersion(&slave_major, &slave_minor, &slave_patch);
  Serial.printf("[JC8012/C6] Detected coprocessor firmware: %lu.%lu.%lu\n",
                static_cast<unsigned long>(slave_major),
                static_cast<unsigned long>(slave_minor),
                static_cast<unsigned long>(slave_patch));

  if (versionEquals(slave_major, slave_minor, slave_patch)) {
    if (!invalidatePayload(partition)) {
      g_blocked = true;
      return finish(Result::Blocked);
    }
    Serial.println("[JC8012/C6] Coprocessor firmware 2.11.6 confirmed");
    return finish(Result::NoAction);
  }

  const bool version_known =
      slave_major != 0 || slave_minor != 0 || slave_patch != 0;
  if (version_known &&
      versionIsNewer(slave_major, slave_minor, slave_patch)) {
    Serial.println(
        "[JC8012/C6] Newer coprocessor firmware detected; refusing downgrade");
    if (!invalidatePayload(partition)) g_blocked = true;
    return finish(g_blocked ? Result::Blocked : Result::NoAction);
  }

  const RecoveryState previous_state = readState();
  if (previous_state != RecoveryState::None) {
    Serial.printf(
        "[JC8012/C6] Previous recovery state %u is unconfirmed; automatic retry is blocked\n",
        static_cast<unsigned>(previous_state));
    g_blocked = true;
    return finish(Result::Blocked);
  }
  if (!writeState(RecoveryState::InProgress)) {
    Serial.println("[JC8012/C6] Could not persist the recovery attempt marker");
    g_blocked = true;
    return finish(Result::Blocked);
  }

  Serial.println(
      "[JC8012/C6] Starting isolated coprocessor update to ESP-Hosted 2.11.6");
  const bool activate_after_end =
      version_known &&
      versionSupportsActivate(slave_major, slave_minor, slave_patch);
  if (!sendPayload(partition, header, activate_after_end)) {
    writeState(RecoveryState::FailedUncertain);
    Serial.println(
        "[JC8012/C6] Recovery stopped safely; power-cycle before further diagnosis");
    g_blocked = true;
    return finish(Result::Blocked);
  }

  if (!writeState(RecoveryState::AwaitVerify)) {
    Serial.println(
        "[JC8012/C6] Could not persist the post-update verification marker");
  }

  Serial.println(
      "[JC8012/C6] Coprocessor image accepted; restarting P4 for version verification");
  return Result::RestartRequired;
}

bool isBlocked() {
  return g_blocked;
}

}  // namespace Jc8012C6Recovery

#else

namespace Jc8012C6Recovery {

Result runIfPresent() {
  return Result::NoAction;
}

bool isBlocked() {
  return false;
}

}  // namespace Jc8012C6Recovery

#endif

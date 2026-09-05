#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <cstring>

#if defined(DEVICE_ESP32_S3_RGB_480)
#include <nvs.h>
#endif

namespace BatchedNvsWrite {

#if defined(DEVICE_ESP32_S3_RGB_480)

// Arduino Preferences commits after every put/remove call. The RGB S3 profile
// uses one native NVS transaction instead, keeping the guarded flash window
// short. Even one NVS commit can stall the flash cache long enough to
// de-synchronise direct PSRAM RGB scanout, so the transaction must still use
// the display/storage guard. LittleFS and OTA use the same guard separately.
class Preferences {
 public:
  ~Preferences() {
    if (started_) nvs_close(handle_);
  }

  bool begin(const char* name, bool read_only) {
    if (started_ || !name || read_only) return false;
    namespace_name_ = name;
    started_ms_ = millis();
    const esp_err_t err = nvs_open(name, NVS_READWRITE, &handle_);
    if (err != ESP_OK) {
      first_error_ = err;
      Serial.printf("[S3Diag/NVS] phase=open namespace=%s result=%s(0x%X)\n",
                    name, esp_err_to_name(err), static_cast<unsigned>(err));
      return false;
    }
    started_ = true;
    return true;
  }

  size_t putString(const char* key, const char* value) {
    if (!value) return noteResult(ESP_ERR_INVALID_ARG, 0);
    return noteResult(nvs_set_str(handle_, key, value), strlen(value));
  }

  size_t putString(const char* key, const String& value) {
    return putString(key, value.c_str());
  }

  size_t putUChar(const char* key, uint8_t value) {
    return noteResult(nvs_set_u8(handle_, key, value), sizeof(value));
  }

  size_t putUShort(const char* key, uint16_t value) {
    return noteResult(nvs_set_u16(handle_, key, value), sizeof(value));
  }

  size_t putUInt(const char* key, uint32_t value) {
    return noteResult(nvs_set_u32(handle_, key, value), sizeof(value));
  }

  size_t putBool(const char* key, bool value) {
    return putUChar(key, value ? 1U : 0U);
  }

  size_t putBytes(const char* key, const void* value, size_t length) {
    if (!value && length != 0) return noteResult(ESP_ERR_INVALID_ARG, 0);
    return noteResult(nvs_set_blob(handle_, key, value, length), length);
  }

  bool remove(const char* key) {
    if (!started_) {
      noteResult(ESP_ERR_INVALID_STATE, 0);
      return false;
    }
    const esp_err_t err = nvs_erase_key(handle_, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) return false;
    return noteResult(err, 1) != 0;
  }

  bool clear() {
    return noteResult(nvs_erase_all(handle_), 1) != 0;
  }

  bool finish() {
    if (!started_) return false;

    const uint32_t commit_started_ms = millis();
    esp_err_t result = first_error_;
    if (result == ESP_OK && pending_writes_ != 0) {
      result = nvs_commit(handle_);
    }
    const uint32_t commit_ms = millis() - commit_started_ms;
    const uint32_t total_ms = millis() - started_ms_;
    nvs_close(handle_);
    started_ = false;
    handle_ = 0;

    Serial.printf(
        "[S3Diag/NVS] namespace=%s keys=%u commit_ms=%lu total_ms=%lu "
        "display_guard=1 result=%s(0x%X)\n",
        namespace_name_ ? namespace_name_ : "?",
        static_cast<unsigned>(pending_writes_),
        static_cast<unsigned long>(commit_ms),
        static_cast<unsigned long>(total_ms), esp_err_to_name(result),
        static_cast<unsigned>(result));
    return result == ESP_OK;
  }

 private:
  size_t noteResult(esp_err_t err, size_t success_size) {
    if (!started_) {
      if (first_error_ == ESP_OK) first_error_ = ESP_ERR_INVALID_STATE;
      return 0;
    }
    ++pending_writes_;
    if (err != ESP_OK && first_error_ == ESP_OK) first_error_ = err;
    return err == ESP_OK ? success_size : 0;
  }

  nvs_handle_t handle_ = 0;
  const char* namespace_name_ = nullptr;
  esp_err_t first_error_ = ESP_OK;
  uint16_t pending_writes_ = 0;
  uint32_t started_ms_ = 0;
  bool started_ = false;
};

inline bool finish(Preferences& prefs) {
  return prefs.finish();
}

inline constexpr bool kNeedsDisplayGuard = true;

#else

using Preferences = ::Preferences;

inline bool finish(Preferences& prefs) {
  prefs.end();
  return true;
}

inline constexpr bool kNeedsDisplayGuard = true;

#endif

}  // namespace BatchedNvsWrite

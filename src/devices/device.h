#pragma once

#include <FS.h>

#include "src/devices/active_device.h"
#include "src/devices/device_types.h"

namespace Device {

inline constexpr Profile kProfile = DeviceImpl::kProfile;
inline constexpr uint16_t kScreenWidth = kProfile.screen_width;
inline constexpr uint16_t kScreenHeight = kProfile.screen_height;
inline constexpr uint8_t kGridCols = kProfile.grid_cols;
inline constexpr uint8_t kGridRows = kProfile.grid_rows;
inline constexpr uint16_t kGridGap = kProfile.grid_gap;
inline constexpr uint16_t kGridPad = kProfile.grid_pad;
inline constexpr uint16_t kGridCellW = kProfile.grid_cell_w;
inline constexpr uint16_t kGridCellH = kProfile.grid_cell_h;
inline constexpr uint8_t kDisplayFlushBands = kProfile.display_flush_bands;
inline constexpr uint8_t kBacklightInputMin = kProfile.backlight_input_min;
inline constexpr RotationStepMode kRotationStepMode = kProfile.rotation_step_mode;
inline constexpr uint8_t kRotationDefault = kProfile.rotation_default;
inline constexpr uint8_t kRotationFlipped = kProfile.rotation_flipped;
inline constexpr Capabilities kCapabilities = kProfile.capabilities;

// Einheitliche Prozentwerte fuer normale Display-Helligkeit, Screensaver und
// MQTT. Gespeichert bleibt aus Kompatibilitaetsgruenden weiterhin der
// geraetespezifische Rohwert.
inline uint8_t backlightRawFromPercent(uint8_t percent) {
  if (percent < 1) percent = 1;
  if (percent > 100) percent = 100;
  const uint16_t span = static_cast<uint16_t>(255U - kBacklightInputMin);
  return static_cast<uint8_t>(
      kBacklightInputMin +
      (static_cast<uint32_t>(percent - 1U) * span + 49U) / 99U);
}

inline uint8_t backlightPercentFromRaw(uint8_t raw) {
  if (raw < kBacklightInputMin) raw = kBacklightInputMin;
  const uint16_t span = static_cast<uint16_t>(255U - kBacklightInputMin);
  if (span == 0) return 100;
  return static_cast<uint8_t>(
      1U + (static_cast<uint32_t>(raw - kBacklightInputMin) * 99U +
            (span / 2U)) /
               span);
}

const Profile& profile();
const char* displayName();

bool init();
void update();

void displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint16_t* data);
void displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                          const uint16_t* data);
// Gemeinsamer direkter Frame-Pfad fuer Kamera und Vollbildvorschau. Der
// Quell-Stride ist explizit, weil der P4-JPEG-Decoder Zeilen auf 16 Pixel
// ausrichtet. false bedeutet: Frame auslassen, niemals still per LVGL neu
// zeichnen.
bool displayTryFullFramePreview(int32_t x, int32_t y, int32_t w, int32_t h,
                                int32_t source_stride,
                                const uint16_t* data, size_t data_size,
                                bool byte_swap);
// Ends device-specific preview buffering. This is a no-op on devices whose
// preview path does not keep persistent display state.
void displayEndFullFramePreview();
bool ppaCooldownActive();
void displayWaitDMA();
// Best-effort wait for the beginning of a physical panel frame. Devices
// without a refresh callback implement this as a no-op.
void displayWaitFrameStart();
void displayFillScreen(uint16_t color);
void displaySetRotation(uint8_t rotation);

void setBrightness(uint8_t value);
uint8_t getBrightness();

bool getTouch(int16_t& x, int16_t& y);

void displaySleep();
void displayWake();
void displayWakeDark();
void displayPowerSaveOn();
void displayPowerSaveOff();
void displayWaitDisplay();
void prepareForRestart();

bool initSDCard();
bool storageReady();
fs::FS& storageFS();
// Bracket write-heavy storage transactions. Most devices implement this as a
// no-op; the ESP32-S3 RGB board uses it to hide unavoidable main-flash stalls
// until the continuously scanned panel has re-synchronised.
void storageWriteBegin();
void storageWriteEnd();

class ScopedStorageWrite {
 public:
  explicit ScopedStorageWrite(bool active = true) : active_(active) {
    if (active_) storageWriteBegin();
  }
  ~ScopedStorageWrite() {
    if (active_) storageWriteEnd();
  }

  ScopedStorageWrite(const ScopedStorageWrite&) = delete;
  ScopedStorageWrite& operator=(const ScopedStorageWrite&) = delete;

 private:
  bool active_;
};

bool sdReady();
fs::FS& sdFS();
bool suspendSDCardForNetworkTransition();
bool resumeSDCardAfterNetworkTransition();

bool initLittleFS();
void migrateStorageFromSD();

uint8_t normalizeRotationQuarterTurns(uint8_t rotation);
bool supportsQuarterTurnRotation();

}  // namespace Device

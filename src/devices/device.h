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
inline constexpr RotationStepMode kRotationStepMode = kProfile.rotation_step_mode;
inline constexpr uint8_t kRotationDefault = kProfile.rotation_default;
inline constexpr uint8_t kRotationFlipped = kProfile.rotation_flipped;
inline constexpr Capabilities kCapabilities = kProfile.capabilities;

const Profile& profile();

bool init();
void update();

void displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint16_t* data);
void displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                          const uint16_t* data);
void displayWaitDMA();
void displayFillScreen(uint16_t color);
void displaySetRotation(uint8_t rotation);

void setBrightness(uint8_t value);
uint8_t getBrightness();

bool getTouch(int16_t& x, int16_t& y);

void displaySleep();
void displayWake();
void displayPowerSaveOn();
void displayPowerSaveOff();
void displayWaitDisplay();

bool initSDCard();
bool storageReady();
fs::FS& storageFS();

uint8_t normalizeRotationQuarterTurns(uint8_t rotation);
bool supportsQuarterTurnRotation();

}  // namespace Device

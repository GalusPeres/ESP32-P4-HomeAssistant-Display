#pragma once

#include <FS.h>

#include "src/devices/device_types.h"

namespace DeviceM5StacksTab5 {

inline constexpr Device::Profile kProfile{
    "m5stacks_tab5",
    "M5Stacks Tab5",
    1280,
    720,
    7,
    4,
    16,
    4,
    168,
    166,
    Device::RotationStepMode::FlipOnly,
    0,
    2,
    Device::Capabilities{false, false, false, false},
};

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

}  // namespace DeviceM5StacksTab5

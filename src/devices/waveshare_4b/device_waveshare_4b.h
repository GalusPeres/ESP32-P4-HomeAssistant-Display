#pragma once

#include <FS.h>

#include "src/devices/device_types.h"

namespace DeviceWaveshare4B {

inline constexpr Device::Profile kProfile{
    "waveshare_4b",
    "Waveshare B4",
    720,
    720,
    4,
    4,
    16,
    4,
    166,
    166,
    Device::RotationStepMode::QuarterTurns,
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

}  // namespace DeviceWaveshare4B

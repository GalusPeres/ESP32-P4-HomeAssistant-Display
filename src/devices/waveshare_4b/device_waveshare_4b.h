#pragma once

#include <FS.h>

#include "src/devices/device_types.h"
#include "src/devices/waveshare_4b/hardware_io_profile.h"

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
    4,
    1,
    Device::RotationStepMode::QuarterTurns,
    0,
    2,
    // The same 4-inch firmware family is used by the B4 and the
    // ESP32-P4-86-Panel-ETH-2RO. The B4 exposes USB-OTG for an external
    // adapter; the 86-panel variant additionally has the native RMII PHY.
    Device::Capabilities{false, false, false, false, true, true},
    kHardwareIoProfile,
};

bool init();
void update();

void displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint16_t* data);
void displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                          const uint16_t* data);
bool displayTryFullFramePreview(int32_t x, int32_t y, int32_t w, int32_t h,
                                int32_t source_stride,
                                const uint16_t* data, size_t data_size,
                                bool byte_swap);
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
void prepareForRestart();

bool initSDCard();
bool storageReady();
fs::FS& storageFS();

bool sdReady();
fs::FS& sdFS();
bool suspendSDCardForNetworkTransition();
bool resumeSDCardAfterNetworkTransition();

bool initLittleFS();
void migrateStorageFromSD();

}  // namespace DeviceWaveshare4B

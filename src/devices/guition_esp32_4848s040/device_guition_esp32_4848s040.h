#pragma once

#include <FS.h>

#include "src/devices/device_types.h"
#include "src/devices/guition_esp32_4848s040/hardware_io_profile.h"

namespace DeviceGuitionESP324848S040 {

inline constexpr Device::Profile kProfile{
    "guition_esp32_4848s040",
    "GUITION ESP32-4848S040",
    480,
    480,
    4,
    4,
    10,
    3,
    111,
    111,
    4,
    1,
    Device::RotationStepMode::QuarterTurns,
    2,
    0,
    Device::Capabilities{false, false, false, false, false, false},
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
// Prepare the inactive RGB framebuffer for one tear-free full-screen redraw.
// Normal partial UI updates continue to use the active framebuffer directly.
bool displayBeginAtomicFrame(const char* reason);
void displayWaitDMA();
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
void storageWriteBegin();
void storageWriteEnd();

bool sdReady();
fs::FS& sdFS();
bool suspendSDCardForNetworkTransition();
bool resumeSDCardAfterNetworkTransition();

bool initLittleFS();
void migrateStorageFromSD();

}  // namespace DeviceGuitionESP324848S040

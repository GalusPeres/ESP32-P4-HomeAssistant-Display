#include "src/devices/device.h"

namespace Device {

const Profile& profile() {
  return kProfile;
}

const char* displayName() {
  return kProfile.display_name;
}

bool init() {
  return DeviceImpl::init();
}

void update() {
  DeviceImpl::update();
}

void displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                       const uint16_t* data) {
  DeviceImpl::displayPushPixels(x, y, w, h, data);
}

void displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                          const uint16_t* data) {
  DeviceImpl::displayPushPixelsDMA(x, y, w, h, data);
}

bool displayTryFullFramePreview(int32_t x, int32_t y, int32_t w, int32_t h,
                                const uint16_t* data, size_t data_size,
                                bool byte_swap) {
  return DeviceImpl::displayTryFullFramePreview(
      x, y, w, h, data, data_size, byte_swap);
}

bool ppaCooldownActive() {
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_GUITION_JC1060P470C)
  return DeviceImpl::ppaCooldownActive();
#else
  return false;
#endif
}

void displayWaitDMA() {
  DeviceImpl::displayWaitDMA();
}

void displayFillScreen(uint16_t color) {
  DeviceImpl::displayFillScreen(color);
}

void displaySetRotation(uint8_t rotation) {
  DeviceImpl::displaySetRotation(rotation);
}

void setBrightness(uint8_t value) {
  DeviceImpl::setBrightness(value);
}

uint8_t getBrightness() {
  return DeviceImpl::getBrightness();
}

bool getTouch(int16_t& x, int16_t& y) {
  return DeviceImpl::getTouch(x, y);
}

void displaySleep() {
  DeviceImpl::displaySleep();
}

void displayWake() {
  DeviceImpl::displayWake();
}

void displayWakeDark() {
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_M5STACKS_TAB5) || \
    defined(DEVICE_GUITION_JC8012P4A1) || \
    defined(DEVICE_GUITION_JC1060P470C) || \
    defined(DEVICE_GUITION_ESP32_4848S040)
  DeviceImpl::displayWakeDark();
#else
  DeviceImpl::displayWake();
  DeviceImpl::setBrightness(0);
#endif
}

void displayPowerSaveOn() {
  DeviceImpl::displayPowerSaveOn();
}

void displayPowerSaveOff() {
  DeviceImpl::displayPowerSaveOff();
}

void displayWaitDisplay() {
  DeviceImpl::displayWaitDisplay();
}

void prepareForRestart() {
  DeviceImpl::prepareForRestart();
}

bool initSDCard() {
  return DeviceImpl::initSDCard();
}

bool storageReady() {
  return DeviceImpl::storageReady();
}

fs::FS& storageFS() {
  return DeviceImpl::storageFS();
}

bool sdReady() {
  return DeviceImpl::sdReady();
}

fs::FS& sdFS() {
  return DeviceImpl::sdFS();
}

bool suspendSDCardForNetworkTransition() {
  return DeviceImpl::suspendSDCardForNetworkTransition();
}

bool resumeSDCardAfterNetworkTransition() {
  return DeviceImpl::resumeSDCardAfterNetworkTransition();
}

bool initLittleFS() {
  return DeviceImpl::initLittleFS();
}

void migrateStorageFromSD() {
  DeviceImpl::migrateStorageFromSD();
}

uint8_t normalizeRotationQuarterTurns(uint8_t rotation) {
  rotation &= 0x03;
  if (kRotationStepMode == RotationStepMode::FlipOnly) {
    return (rotation & 0x02);
  }
  return rotation;
}

bool supportsQuarterTurnRotation() {
  return kRotationStepMode == RotationStepMode::QuarterTurns;
}

}  // namespace Device

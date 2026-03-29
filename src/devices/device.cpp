#include "src/devices/device.h"

namespace Device {

const Profile& profile() {
  return kProfile;
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

void displayPowerSaveOn() {
  DeviceImpl::displayPowerSaveOn();
}

void displayPowerSaveOff() {
  DeviceImpl::displayPowerSaveOff();
}

void displayWaitDisplay() {
  DeviceImpl::displayWaitDisplay();
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

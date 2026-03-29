#include "src/core/board_hal.h"

#include "src/devices/device.h"

bool BoardHAL::init() {
  return Device::init();
}

void BoardHAL::update() {
  Device::update();
}

void BoardHAL::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                 const uint16_t* data) {
  Device::displayPushPixels(x, y, w, h, data);
}

void BoardHAL::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                    const uint16_t* data) {
  Device::displayPushPixelsDMA(x, y, w, h, data);
}

void BoardHAL::displayWaitDMA() {
  Device::displayWaitDMA();
}

void BoardHAL::displayFillScreen(uint16_t color) {
  Device::displayFillScreen(color);
}

void BoardHAL::displaySetRotation(uint8_t rotation) {
  Device::displaySetRotation(rotation);
}

void BoardHAL::setBrightness(uint8_t value) {
  Device::setBrightness(value);
}

uint8_t BoardHAL::getBrightness() {
  return Device::getBrightness();
}

bool BoardHAL::getTouch(TouchPoint* tp) {
  if (!tp) {
    return false;
  }
  return Device::getTouch(tp->x, tp->y);
}

void BoardHAL::displaySleep() {
  Device::displaySleep();
}

void BoardHAL::displayWake() {
  Device::displayWake();
}

void BoardHAL::displayPowerSaveOn() {
  Device::displayPowerSaveOn();
}

void BoardHAL::displayPowerSaveOff() {
  Device::displayPowerSaveOff();
}

void BoardHAL::displayWaitDisplay() {
  Device::displayWaitDisplay();
}

bool BoardHAL::initSDCard() {
  return Device::initSDCard();
}

#include "src/core/board_hal.h"

#include "src/devices/device.h"
#include "src/devices/device_select.h"

#if defined(DEVICE_WAVESHARE_4B)
#include <esp_private/system_internal.h>
#endif
#if defined(DEVICE_GUITION_ESP32_4848S040)
#include <esp_sleep.h>
#endif

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

void BoardHAL::displayWakeDark() {
  Device::displayWakeDark();
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

void BoardHAL::prepareForRestart() {
  Device::prepareForRestart();
}

void BoardHAL::restart() {
  Serial.flush();
#if defined(DEVICE_WAVESHARE_4B)
  Serial.println("[BoardHAL] Neustart via no-OS restart");
  Serial.flush();
  esp_restart_noos();
#elif defined(DEVICE_GUITION_ESP32_4848S040)
  // Some ESP32-S3 RGB boards do not reliably reset the panel/PSRAM path with
  // a software-only restart. A short timed deep sleep performs a clean
  // hardware reset and is also safe after OTA.
  Serial.println("[BoardHAL] Hardware reset via timed deep sleep");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(100000);
  esp_deep_sleep_start();
#else
  ESP.restart();
#endif
}

bool BoardHAL::initSDCard() {
  return Device::initSDCard();
}

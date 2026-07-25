#include "src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.h"

#if defined(DEVICE_GUITION_ESP32_4848S040)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_heap_caps.h>

#include <algorithm>
#include <cstring>

namespace {

constexpr int8_t kPanelCs = 39;
constexpr int8_t kPanelSck = 48;
constexpr int8_t kPanelMosi = 47;

constexpr int8_t kPanelDe = 18;
constexpr int8_t kPanelVsync = 17;
constexpr int8_t kPanelHsync = 16;
constexpr int8_t kPanelPclk = 21;
constexpr int8_t kPanelR0 = 11;
constexpr int8_t kPanelR1 = 12;
constexpr int8_t kPanelR2 = 13;
constexpr int8_t kPanelR3 = 14;
constexpr int8_t kPanelR4 = 0;
constexpr int8_t kPanelG0 = 8;
constexpr int8_t kPanelG1 = 20;
constexpr int8_t kPanelG2 = 3;
constexpr int8_t kPanelG3 = 46;
constexpr int8_t kPanelG4 = 9;
constexpr int8_t kPanelG5 = 10;
constexpr int8_t kPanelB0 = 4;
constexpr int8_t kPanelB1 = 5;
constexpr int8_t kPanelB2 = 6;
constexpr int8_t kPanelB3 = 7;
constexpr int8_t kPanelB4 = 15;

constexpr int8_t kBacklightPin = 38;
constexpr uint32_t kBacklightFrequency = 150;
constexpr uint8_t kBacklightResolution = 10;
constexpr uint16_t kBacklightMaxDuty = (1u << kBacklightResolution) - 1u;

constexpr int8_t kTouchSda = 19;
constexpr int8_t kTouchScl = 45;
constexpr uint32_t kTouchFrequency = 400000;
constexpr uint8_t kTouchAddressPrimary = 0x5D;
constexpr uint8_t kTouchAddressAlternate = 0x14;
constexpr uint16_t kTouchProductIdRegister = 0x8140;
constexpr uint16_t kTouchStatusRegister = 0x814E;
constexpr uint16_t kTouchPointRegister = 0x814F;

constexpr int8_t kSdSck = 48;
constexpr int8_t kSdMiso = 41;
constexpr int8_t kSdMosi = 47;
constexpr int8_t kSdCs = 42;
constexpr uint32_t kSdFrequency = 20000000;

constexpr uint32_t kSdRetryMs = 1500;
constexpr size_t kRgbBounceBufferPixels = 480 * 10;

Arduino_DataBus* g_panel_bus = nullptr;
Arduino_ESP32RGBPanel* g_rgb_panel = nullptr;
Arduino_RGB_Display* g_gfx = nullptr;
SPIClass g_sd_spi(FSPI);

bool g_display_ready = false;
bool g_backlight_ready = false;
bool g_touch_ready = false;
bool g_littlefs_ready = false;
bool g_sd_available = false;
bool g_sd_init_attempted = false;
uint32_t g_sd_retry_tick_ms = 0;
uint8_t g_brightness = 0;
uint8_t g_rotation = DeviceGuitionESP324848S040::kProfile.rotation_default;
uint8_t g_touch_address = 0;

void ensureStorageLayout() {
  if (!g_littlefs_ready) return;
  LittleFS.mkdir("/_tile_grids");
  LittleFS.mkdir("/_tile_links");
  LittleFS.mkdir("/icons");
}

bool writeTouchRegister(uint16_t reg, uint8_t value) {
  if (!g_touch_address) return false;
  Wire.beginTransmission(g_touch_address);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readTouchRegisters(uint16_t reg, uint8_t* data, size_t len) {
  if (!g_touch_address || !data || len == 0 || len > 32) return false;
  Wire.beginTransmission(g_touch_address);
  Wire.write(static_cast<uint8_t>(reg >> 8));
  Wire.write(static_cast<uint8_t>(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;
  const size_t received =
      Wire.requestFrom(static_cast<int>(g_touch_address), static_cast<int>(len));
  if (received != len) {
    while (Wire.available()) Wire.read();
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool probeTouchAddress(uint8_t address) {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() != 0) return false;
  g_touch_address = address;
  uint8_t product_id[4] = {};
  if (!readTouchRegisters(kTouchProductIdRegister, product_id,
                          sizeof(product_id))) {
    g_touch_address = 0;
    return false;
  }
  Serial.printf("[Device/GUITION ESP32-4848S040] GT911 at 0x%02X, id=%c%c%c%c\n",
                address, product_id[0], product_id[1], product_id[2],
                product_id[3]);
  return true;
}

bool initTouch() {
  if (g_touch_ready) return true;
  Wire.begin(kTouchSda, kTouchScl, kTouchFrequency);
  delay(20);

  if (!probeTouchAddress(kTouchAddressPrimary) &&
      !probeTouchAddress(kTouchAddressAlternate)) {
    Serial.println(
        "[Device/GUITION ESP32-4848S040] GT911 not found at 0x5D/0x14");
    return false;
  }

  writeTouchRegister(kTouchStatusRegister, 0);
  g_touch_ready = true;
  return true;
}

bool initBacklight() {
  if (g_backlight_ready) return true;
  pinMode(kBacklightPin, OUTPUT);
  digitalWrite(kBacklightPin, LOW);
  if (!ledcAttach(kBacklightPin, kBacklightFrequency,
                  kBacklightResolution)) {
    Serial.println(
        "[Device/GUITION ESP32-4848S040] Backlight PWM init failed");
    return false;
  }
  g_backlight_ready = true;
  ledcWrite(kBacklightPin, 0);
  return true;
}

void applyBrightness(uint8_t value, bool remember = true) {
  if (remember) g_brightness = value;
  if (!g_backlight_ready && !initBacklight()) return;
  const uint32_t duty =
      (static_cast<uint32_t>(value) * kBacklightMaxDuty + 127u) / 255u;
  ledcWrite(kBacklightPin, duty);
}

bool initDisplay() {
  if (g_display_ready) return true;

  g_panel_bus = new Arduino_SWSPI(
      GFX_NOT_DEFINED, kPanelCs, kPanelSck, kPanelMosi, GFX_NOT_DEFINED);
  g_rgb_panel = new Arduino_ESP32RGBPanel(
      kPanelDe, kPanelVsync, kPanelHsync, kPanelPclk,
      kPanelR0, kPanelR1, kPanelR2, kPanelR3, kPanelR4,
      kPanelG0, kPanelG1, kPanelG2, kPanelG3, kPanelG4, kPanelG5,
      kPanelB0, kPanelB1, kPanelB2, kPanelB3, kPanelB4,
      1, 10, 8, 50,
      1, 10, 8, 20,
      0, 10000000, false,
      0, 0, kRgbBounceBufferPixels);
  g_gfx = new Arduino_RGB_Display(
      480, 480, g_rgb_panel, g_rotation, true, g_panel_bus, GFX_NOT_DEFINED,
      st7701_type9_init_operations, sizeof(st7701_type9_init_operations));

  if (!g_panel_bus || !g_rgb_panel || !g_gfx || !g_gfx->begin()) {
    Serial.println(
        "[Device/GUITION ESP32-4848S040] ST7701 RGB display init failed");
    return false;
  }

  g_gfx->fillScreen(0x0000);
  g_display_ready = true;
  Serial.printf(
      "[Device/GUITION ESP32-4848S040] Display ready, PCLK=10 MHz, "
      "bounce=%u px, PSRAM free=%u KB\n",
      static_cast<unsigned>(kRgbBounceBufferPixels),
      static_cast<unsigned>(
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  return true;
}

void mapTouch(uint16_t raw_x, uint16_t raw_y, int16_t& x, int16_t& y) {
  constexpr int16_t kMax = 479;
  switch (g_rotation & 0x03) {
    case 1:
      x = static_cast<int16_t>(raw_y);
      y = kMax - static_cast<int16_t>(raw_x);
      break;
    case 2:
      x = kMax - static_cast<int16_t>(raw_x);
      y = kMax - static_cast<int16_t>(raw_y);
      break;
    case 3:
      x = kMax - static_cast<int16_t>(raw_y);
      y = static_cast<int16_t>(raw_x);
      break;
    default:
      x = static_cast<int16_t>(raw_x);
      y = static_cast<int16_t>(raw_y);
      break;
  }
  x = std::max<int16_t>(0, std::min<int16_t>(kMax, x));
  y = std::max<int16_t>(0, std::min<int16_t>(kMax, y));
}

bool copyFile(fs::FS& src_fs, fs::FS& dst_fs, const char* path) {
  File src = src_fs.open(path, FILE_READ);
  if (!src) return false;
  File dst = dst_fs.open(path, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buffer[512];
  while (src.available()) {
    const size_t count = src.read(buffer, sizeof(buffer));
    if (!count || dst.write(buffer, count) != count) break;
  }
  dst.close();
  src.close();
  return true;
}

void copyDirectory(fs::FS& src_fs, fs::FS& dst_fs, const char* dir_path) {
  File dir = src_fs.open(dir_path);
  if (!dir || !dir.isDirectory()) return;
  dst_fs.mkdir(dir_path);

  File entry = dir.openNextFile();
  while (entry) {
    const String path = String(dir_path) + "/" + entry.name();
    const bool directory = entry.isDirectory();
    entry.close();
    if (directory) {
      copyDirectory(src_fs, dst_fs, path.c_str());
    } else if (copyFile(src_fs, dst_fs, path.c_str())) {
      Serial.printf("[Storage] Migrated: %s\n", path.c_str());
    }
    entry = dir.openNextFile();
  }
  dir.close();
}

}  // namespace

bool DeviceGuitionESP324848S040::init() {
  Serial.println("[Device/GUITION ESP32-4848S040] Initialising board...");

  if (!psramFound()) {
    Serial.println(
        "[Device/GUITION ESP32-4848S040] ERROR: octal PSRAM not detected");
    return false;
  }
  Serial.printf(
      "[Device/GUITION ESP32-4848S040] Flash=%u MB, PSRAM=%u MB\n",
      static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)),
      static_cast<unsigned>(ESP.getPsramSize() / (1024 * 1024)));

  if (!initBacklight()) return false;
  applyBrightness(0, false);
  if (!initDisplay()) return false;

  if (!initTouch()) {
    Serial.println(
        "[Device/GUITION ESP32-4848S040] Touch unavailable; continuing");
  }
  initLittleFS();
  initSDCard();
  return true;
}

void DeviceGuitionESP324848S040::update() {}

void DeviceGuitionESP324848S040::displayPushPixels(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  if (!g_display_ready || !g_gfx || !data || w <= 0 || h <= 0) return;
  g_gfx->draw16bitRGBBitmap(
      static_cast<int16_t>(x), static_cast<int16_t>(y),
      const_cast<uint16_t*>(data), static_cast<int16_t>(w),
      static_cast<int16_t>(h));
}

void DeviceGuitionESP324848S040::displayPushPixelsDMA(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data) {
  displayPushPixels(x, y, w, h, data);
}

bool DeviceGuitionESP324848S040::displayTryFullFramePreview(
    int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* data,
    size_t data_size, bool byte_swap) {
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)data;
  (void)data_size;
  (void)byte_swap;
  return false;
}

void DeviceGuitionESP324848S040::displayWaitDMA() {}

void DeviceGuitionESP324848S040::displayFillScreen(uint16_t color) {
  if (g_display_ready && g_gfx) g_gfx->fillScreen(color);
}

void DeviceGuitionESP324848S040::displaySetRotation(uint8_t rotation) {
  g_rotation = rotation & 0x03;
  if (g_display_ready && g_gfx) g_gfx->setRotation(g_rotation);
}

void DeviceGuitionESP324848S040::setBrightness(uint8_t value) {
  applyBrightness(value);
}

uint8_t DeviceGuitionESP324848S040::getBrightness() {
  return g_brightness;
}

bool DeviceGuitionESP324848S040::getTouch(int16_t& x, int16_t& y) {
  if (!g_touch_ready && !initTouch()) return false;

  uint8_t status = 0;
  if (!readTouchRegisters(kTouchStatusRegister, &status, 1)) return false;
  if ((status & 0x80) == 0) return false;

  const uint8_t points = status & 0x0F;
  if (points == 0 || points > 5) {
    writeTouchRegister(kTouchStatusRegister, 0);
    return false;
  }

  uint8_t point[8] = {};
  const bool read_ok =
      readTouchRegisters(kTouchPointRegister, point, sizeof(point));
  writeTouchRegister(kTouchStatusRegister, 0);
  if (!read_ok) return false;

  const uint16_t raw_x =
      static_cast<uint16_t>(point[1] | (point[2] << 8));
  const uint16_t raw_y =
      static_cast<uint16_t>(point[3] | (point[4] << 8));
  if (raw_x >= 480 || raw_y >= 480) return false;

  mapTouch(raw_x, raw_y, x, y);
  return true;
}

void DeviceGuitionESP324848S040::displaySleep() {
  applyBrightness(0, false);
}

void DeviceGuitionESP324848S040::displayWake() {
  applyBrightness(g_brightness ? g_brightness : 160, false);
}

void DeviceGuitionESP324848S040::displayWakeDark() {
  applyBrightness(0, false);
}

void DeviceGuitionESP324848S040::displayPowerSaveOn() {
  displaySleep();
}

void DeviceGuitionESP324848S040::displayPowerSaveOff() {
  displayWake();
}

void DeviceGuitionESP324848S040::displayWaitDisplay() {}

void DeviceGuitionESP324848S040::prepareForRestart() {
  applyBrightness(0, false);
  if (g_display_ready && g_gfx) {
    g_gfx->fillScreen(0x0000);
    g_gfx->flush(true);
  }
  if (g_sd_available) {
    SD.end();
    g_sd_available = false;
  }
  delay(20);
}

bool DeviceGuitionESP324848S040::initSDCard() {
  if (g_sd_available && SD.cardType() != CARD_NONE) return true;

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < kSdRetryMs) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;
  SD.end();

  // The card and panel command bus share SCK/MOSI. The ST7701 is initialized
  // first and remains deselected on CS39 while the card uses CS42.
  g_sd_spi.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
  if (!SD.begin(kSdCs, g_sd_spi, kSdFrequency, "/sdcard", 5)) {
    g_sd_available = false;
    Serial.println(
        "[Device/GUITION ESP32-4848S040] SD card mount failed");
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    SD.end();
    g_sd_available = false;
    Serial.println("[Device/GUITION ESP32-4848S040] SD card absent");
    return false;
  }

  g_sd_available = true;
  Serial.printf(
      "[Device/GUITION ESP32-4848S040] SD card OK, size=%llu MB\n",
      static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  return true;
}

bool DeviceGuitionESP324848S040::storageReady() {
  return g_littlefs_ready;
}

fs::FS& DeviceGuitionESP324848S040::storageFS() {
  return LittleFS;
}

bool DeviceGuitionESP324848S040::sdReady() {
  return initSDCard();
}

fs::FS& DeviceGuitionESP324848S040::sdFS() {
  return SD;
}

bool DeviceGuitionESP324848S040::suspendSDCardForNetworkTransition() {
  // Native S3 WiFi does not share the P4 ESP-Hosted SDIO bus with the card.
  return false;
}

bool DeviceGuitionESP324848S040::resumeSDCardAfterNetworkTransition() {
  return initSDCard();
}

bool DeviceGuitionESP324848S040::initLittleFS() {
  if (g_littlefs_ready) return true;
  if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
    Serial.println(
        "[Device/GUITION ESP32-4848S040] LittleFS mount failed");
    return false;
  }
  g_littlefs_ready = true;
  ensureStorageLayout();
  Serial.printf(
      "[Device/GUITION ESP32-4848S040] LittleFS ready, total=%u, used=%u\n",
      static_cast<unsigned>(LittleFS.totalBytes()),
      static_cast<unsigned>(LittleFS.usedBytes()));
  return true;
}

void DeviceGuitionESP324848S040::migrateStorageFromSD() {
  if (!initLittleFS() || LittleFS.exists("/_migrated")) return;

  ensureStorageLayout();
  if (initSDCard()) {
    Serial.println("[Storage] Migrating data from SD to LittleFS...");
    copyDirectory(SD, LittleFS, "/_tile_grids");
    copyDirectory(SD, LittleFS, "/_tile_links");
    copyDirectory(SD, LittleFS, "/icons");
    Serial.println("[Storage] Migration complete");
  } else {
    Serial.println("[Storage] No SD card, starting fresh");
  }

  File flag = LittleFS.open("/_migrated", FILE_WRITE);
  if (flag) {
    flag.print("1");
    flag.close();
  }
}

#endif  // defined(DEVICE_GUITION_ESP32_4848S040)

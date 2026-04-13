#include "src/devices/m5stacks_tab5/device_m5stacks_tab5.h"

#include "src/devices/device_select.h"

#if defined(DEVICE_M5STACKS_TAB5)

#include <LittleFS.h>
#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

namespace {

constexpr int32_t kLogicalWidth = DeviceM5StacksTab5::kProfile.screen_width;
constexpr int32_t kLogicalHeight = DeviceM5StacksTab5::kProfile.screen_height;

constexpr int kSdClkPin = 43;
constexpr int kSdMisoPin = 39;
constexpr int kSdMosiPin = 44;
constexpr int kSdCsPin = 42;
constexpr uint32_t kSdClockHz = 25000000;

bool g_display_ready = false;
bool g_sd_init_attempted = false;
bool g_sd_available = false;
uint32_t g_sd_retry_tick_ms = 0;
bool g_littlefs_ready = false;
uint8_t g_brightness = 150;
uint8_t g_rotation = DeviceM5StacksTab5::kProfile.rotation_default;

uint8_t to_panel_rotation(uint8_t logical_rotation) {
  logical_rotation &= 0x03;
  return (logical_rotation & 0x02) ? 3 : 1;
}

bool init_display() {
  if (g_display_ready) {
    return true;
  }

  Serial.println("[Device/M5StacksTab5] Display: M5.begin()...");
  Serial.flush();

  auto cfg = M5.config();
  M5.begin(cfg);
  Wire.setClock(400000);

  if (M5.getBoard() == m5::board_t::board_unknown) {
    Serial.println("[Device/M5StacksTab5] M5.begin() failed to detect board");
    Serial.flush();
    return false;
  }

  M5.Display.setRotation(to_panel_rotation(g_rotation));
  M5.Display.setBrightness(0);
  M5.Display.fillScreen(0x0000);
  M5.Display.waitDMA();
  M5.Display.setBrightness(g_brightness);

  g_display_ready = true;

  Serial.printf("[Device/M5StacksTab5] Display ready: %dx%d rot=%u\n",
                M5.Display.width(), M5.Display.height(),
                static_cast<unsigned>(to_panel_rotation(g_rotation)));
  Serial.flush();
  return true;
}

bool rect_inside_logical_bounds(int32_t x, int32_t y, int32_t w, int32_t h) {
  return x >= 0 && y >= 0 && w > 0 && h > 0 &&
         (x + w) <= kLogicalWidth &&
         (y + h) <= kLogicalHeight;
}

}  // namespace

bool DeviceM5StacksTab5::init() {
  if (g_display_ready) {
    return true;
  }

  Serial.println("[Device/M5StacksTab5] Initialising board...");
  Serial.flush();

  if (!init_display()) {
    return false;
  }

  setBrightness(g_brightness);

  Serial.println("[Device/M5StacksTab5] Init complete");
  Serial.flush();
  return true;
}

void DeviceM5StacksTab5::update() {
  if (!g_display_ready) {
    return;
  }

  M5.update();
}

void DeviceM5StacksTab5::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                          const uint16_t* data) {
  if (!g_display_ready || !data || !rect_inside_logical_bounds(x, y, w, h)) {
    return;
  }

  M5.Display.pushImage(x, y, w, h, data);
}

void DeviceM5StacksTab5::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                             const uint16_t* data) {
  if (!g_display_ready || !data || !rect_inside_logical_bounds(x, y, w, h)) {
    return;
  }

  M5.Display.pushImageDMA(x, y, w, h, data);
}

void DeviceM5StacksTab5::displayWaitDMA() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
}

void DeviceM5StacksTab5::displayFillScreen(uint16_t color) {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
  M5.Display.fillScreen(color);
  M5.Display.waitDMA();
}

void DeviceM5StacksTab5::displaySetRotation(uint8_t rotation) {
  g_rotation = rotation & 0x03;

  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
  M5.Display.setRotation(to_panel_rotation(g_rotation));
}

void DeviceM5StacksTab5::setBrightness(uint8_t value) {
  g_brightness = value;

  if (!g_display_ready) {
    return;
  }

  M5.Display.setBrightness(value);
}

uint8_t DeviceM5StacksTab5::getBrightness() {
  return g_brightness;
}

bool DeviceM5StacksTab5::getTouch(int16_t& x, int16_t& y) {
  if (!g_display_ready) {
    return false;
  }

  m5gfx::touch_point_t tp;
  if (!M5.Display.getTouch(&tp, 1)) {
    return false;
  }

  int32_t mapped_x = tp.x;
  int32_t mapped_y = tp.y;
  if (mapped_x < 0) mapped_x = 0;
  if (mapped_y < 0) mapped_y = 0;
  if (mapped_x >= kLogicalWidth) mapped_x = kLogicalWidth - 1;
  if (mapped_y >= kLogicalHeight) mapped_y = kLogicalHeight - 1;

  x = static_cast<int16_t>(mapped_x);
  y = static_cast<int16_t>(mapped_y);
  return true;
}

void DeviceM5StacksTab5::displaySleep() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
  // Keep the Tab5 panel/touch controller awake. Full display sleep stops
  // touch wake on this device, so sleep mode only blanks the backlight.
  M5.Display.setBrightness(0);
}

void DeviceM5StacksTab5::displayWake() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.wakeup();
  M5.Display.setBrightness(g_brightness);
}

void DeviceM5StacksTab5::displayPowerSaveOn() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
  // Tab5 touch wake only works while the display controller stays awake.
  // Power save therefore means backlight off, not panel sleep/powerSaveOn.
  M5.Display.setBrightness(0);
  M5.Display.waitDMA();
}

void DeviceM5StacksTab5::displayPowerSaveOff() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
  M5.Display.setBrightness(g_brightness);
}

void DeviceM5StacksTab5::displayWaitDisplay() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
}

void DeviceM5StacksTab5::prepareForRestart() {
  if (!g_display_ready) {
    return;
  }

  M5.Display.waitDMA();
  M5.Display.fillScreen(0x0000);
  M5.Display.waitDMA();
  M5.Display.setBrightness(0);
  M5.Display.sleep();
}

bool DeviceM5StacksTab5::initSDCard() {
  if (g_sd_available && SD.cardType() != CARD_NONE) {
    return true;
  }

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < 1500) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;

  SD.end();
  SPI.begin(kSdClkPin, kSdMisoPin, kSdMosiPin, kSdCsPin);
  if (!SD.begin(kSdCsPin, SPI, kSdClockHz)) {
    g_sd_available = false;
    Serial.println("[Device/M5StacksTab5] SD card mount failed");
    Serial.flush();
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    g_sd_available = false;
    Serial.println("[Device/M5StacksTab5] SD card absent after mount");
    Serial.flush();
    SD.end();
    return false;
  }

  g_sd_available = true;
  Serial.printf("[Device/M5StacksTab5] SD card OK, type=%u, size=%llu MB\n",
                static_cast<unsigned>(SD.cardType()),
                static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  Serial.flush();
  return true;
}

bool DeviceM5StacksTab5::storageReady() {
  return g_littlefs_ready;
}

fs::FS& DeviceM5StacksTab5::storageFS() {
  return LittleFS;
}

bool DeviceM5StacksTab5::sdReady() {
  return initSDCard();
}

fs::FS& DeviceM5StacksTab5::sdFS() {
  return SD;
}

bool DeviceM5StacksTab5::initLittleFS() {
  if (g_littlefs_ready) {
    return true;
  }
  if (!LittleFS.begin(true, "/littlefs", 10, "spiffs")) {
    Serial.println("[Device/M5StacksTab5] LittleFS mount failed");
    return false;
  }
  g_littlefs_ready = true;
  Serial.printf("[Device/M5StacksTab5] LittleFS ready, total=%u, used=%u\n",
                static_cast<unsigned>(LittleFS.totalBytes()),
                static_cast<unsigned>(LittleFS.usedBytes()));
  return true;
}

static bool copyFile(fs::FS& srcFS, fs::FS& dstFS, const char* path) {
  File src = srcFS.open(path, FILE_READ);
  if (!src) {
    return false;
  }
  File dst = dstFS.open(path, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }
  uint8_t buf[512];
  while (src.available()) {
    const size_t n = src.read(buf, sizeof(buf));
    if (n == 0) break;
    dst.write(buf, n);
  }
  dst.close();
  src.close();
  return true;
}

static void copyDirectory(fs::FS& srcFS, fs::FS& dstFS, const char* dirPath) {
  File dir = srcFS.open(dirPath);
  if (!dir || !dir.isDirectory()) {
    return;
  }
  dstFS.mkdir(dirPath);
  File entry = dir.openNextFile();
  while (entry) {
    String fullPath = String(dirPath) + "/" + entry.name();
    if (entry.isDirectory()) {
      copyDirectory(srcFS, dstFS, fullPath.c_str());
    } else {
      copyFile(srcFS, dstFS, fullPath.c_str());
      Serial.printf("[Storage] Migrated: %s (%u bytes)\n", fullPath.c_str(),
                    static_cast<unsigned>(entry.size()));
    }
    entry = dir.openNextFile();
  }
}

void DeviceM5StacksTab5::migrateStorageFromSD() {
  if (!g_littlefs_ready) {
    return;
  }
  if (LittleFS.exists("/_migrated")) {
    return;
  }

  LittleFS.mkdir("/_tile_grids");
  LittleFS.mkdir("/_tile_links");
  LittleFS.mkdir("/icons");

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

#endif

#include "src/devices/waveshare_4b/device_waveshare_4b.h"

#include "src/devices/device_select.h"

#if defined(DEVICE_WAVESHARE_4B)

#include <Arduino_GFX_Library.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

#include "src/devices/waveshare_4b/waveshare_sdmmc.h"
#include "src/devices/waveshare_4b/vendor/displays_config.h"
#include "src/devices/waveshare_4b/vendor/gt911.h"

namespace {

constexpr gpio_num_t kBacklightPin = GPIO_NUM_26;
constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_t kBacklightTimer = LEDC_TIMER_1;
constexpr uint32_t kBacklightFreq = 5000;
constexpr ledc_timer_bit_t kBacklightBits = LEDC_TIMER_10_BIT;
constexpr bool kBacklightActiveLow = true;

Arduino_ESP32DSIPanel* g_dsi_panel = nullptr;
Arduino_DSI_Display* g_gfx = nullptr;
esp_lcd_touch_handle_t g_touch = nullptr;

uint8_t g_brightness = 150;
bool g_backlight_ready = false;
bool g_sd_init_attempted = false;
bool g_sd_available = false;
uint32_t g_sd_retry_tick_ms = 0;

void apply_backlight(uint8_t value) {
  if (!g_backlight_ready) {
    return;
  }

  constexpr uint32_t kMaxDuty = (1u << 10) - 1u;
  uint32_t duty = ((uint32_t)value * kMaxDuty + 127u) / 255u;
  if (kBacklightActiveLow) {
    duty = kMaxDuty - duty;
  }

  ledc_set_duty(LEDC_LOW_SPEED_MODE, kBacklightChannel, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, kBacklightChannel);
}

bool init_backlight() {
  if (g_backlight_ready) {
    return true;
  }

  ledc_timer_config_t timer_cfg = {};
  timer_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  timer_cfg.timer_num = kBacklightTimer;
  timer_cfg.duty_resolution = kBacklightBits;
  timer_cfg.freq_hz = kBacklightFreq;
  timer_cfg.clk_cfg = LEDC_AUTO_CLK;
  if (ledc_timer_config(&timer_cfg) != ESP_OK) {
    Serial.println("[Device/Waveshare4B] Backlight timer init failed");
    return false;
  }

  ledc_channel_config_t ch_cfg = {};
  ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
  ch_cfg.channel = kBacklightChannel;
  ch_cfg.timer_sel = kBacklightTimer;
  ch_cfg.gpio_num = kBacklightPin;
  ch_cfg.duty = kBacklightActiveLow ? ((1u << 10) - 1u) : 0;
  ch_cfg.hpoint = 0;
  if (ledc_channel_config(&ch_cfg) != ESP_OK) {
    Serial.println("[Device/Waveshare4B] Backlight channel init failed");
    return false;
  }

  g_backlight_ready = true;
  apply_backlight(0);
  return true;
}

bool init_display() {
  if (g_gfx) {
    return true;
  }

  g_dsi_panel = new Arduino_ESP32DSIPanel(
      display_cfg.hsync_pulse_width,
      display_cfg.hsync_back_porch,
      display_cfg.hsync_front_porch,
      display_cfg.vsync_pulse_width,
      display_cfg.vsync_back_porch,
      display_cfg.vsync_front_porch,
      display_cfg.prefer_speed,
      display_cfg.lane_bit_rate);
  if (!g_dsi_panel) {
    Serial.println("[Device/Waveshare4B] DSI panel allocation failed");
    return false;
  }

  g_gfx = new Arduino_DSI_Display(
      display_cfg.width,
      display_cfg.height,
      g_dsi_panel,
      display_cfg.rotation,
      display_cfg.auto_flush,
      display_cfg.lcd_rst,
      display_cfg.init_cmds,
      display_cfg.init_cmds_size);
  if (!g_gfx) {
    Serial.println("[Device/Waveshare4B] Display allocation failed");
    return false;
  }

  if (!g_gfx->begin()) {
    Serial.println("[Device/Waveshare4B] gfx->begin() failed");
    return false;
  }

  g_gfx->fillScreen(0x0000);
  g_gfx->flush();
  g_gfx->displayOn();
  return true;
}

bool init_touch() {
  if (g_touch) {
    return true;
  }

  DEV_I2C_Port port = DEV_I2C_Init();
  g_touch = touch_gt911_init(port);
  if (!g_touch) {
    Serial.println("[Device/Waveshare4B] Touch init failed");
    return false;
  }

  return true;
}

}  // namespace

bool DeviceWaveshare4B::init() {
  Serial.println("[Device/Waveshare4B] Initialising board...");

  if (!init_backlight()) {
    return false;
  }
  apply_backlight(0);

  if (!init_touch()) {
    return false;
  }
  Serial.println("[Device/Waveshare4B] Touch OK");

  if (!init_display()) {
    return false;
  }
  Serial.println("[Device/Waveshare4B] Display OK");

  setBrightness(g_brightness);
  Serial.println("[Device/Waveshare4B] Init complete");
  return true;
}

void DeviceWaveshare4B::update() {
}

void DeviceWaveshare4B::displayPushPixels(int32_t x, int32_t y, int32_t w, int32_t h,
                                          const uint16_t* data) {
  if (!g_gfx || !data) {
    return;
  }

  g_gfx->draw16bitRGBBitmap((int16_t)x, (int16_t)y,
                            const_cast<uint16_t*>(data),
                            (int16_t)w, (int16_t)h);
}

void DeviceWaveshare4B::displayPushPixelsDMA(int32_t x, int32_t y, int32_t w, int32_t h,
                                             const uint16_t* data) {
  displayPushPixels(x, y, w, h, data);
}

void DeviceWaveshare4B::displayWaitDMA() {
}

void DeviceWaveshare4B::displayFillScreen(uint16_t color) {
  if (!g_gfx) {
    return;
  }

  g_gfx->fillScreen(color);
  g_gfx->flush();
}

void DeviceWaveshare4B::displaySetRotation(uint8_t rotation) {
  if (!g_gfx) {
    return;
  }

  g_gfx->setRotation(rotation & 0x03);
  g_gfx->flush();
}

void DeviceWaveshare4B::setBrightness(uint8_t value) {
  g_brightness = value;
  apply_backlight(value);
}

uint8_t DeviceWaveshare4B::getBrightness() {
  return g_brightness;
}

bool DeviceWaveshare4B::getTouch(int16_t& x, int16_t& y) {
  if (!g_touch) {
    return false;
  }

  uint16_t px[1] = {0};
  uint16_t py[1] = {0};
  uint16_t strength[1] = {0};
  uint8_t count = 0;

  esp_lcd_touch_read_data(g_touch);
  const bool touched = esp_lcd_touch_get_coordinates(g_touch, px, py, strength, &count, 1);
  if (!touched || count == 0) {
    return false;
  }

  x = static_cast<int16_t>(px[0]);
  y = static_cast<int16_t>(py[0]);
  return true;
}

void DeviceWaveshare4B::displaySleep() {
  if (g_gfx) {
    g_gfx->displayOff();
  }
  apply_backlight(0);
}

void DeviceWaveshare4B::displayWake() {
  if (g_gfx) {
    g_gfx->displayOn();
    g_gfx->flush();
  }
  apply_backlight(g_brightness);
}

void DeviceWaveshare4B::displayPowerSaveOn() {
  apply_backlight(0);
}

void DeviceWaveshare4B::displayPowerSaveOff() {
  apply_backlight(g_brightness);
}

void DeviceWaveshare4B::displayWaitDisplay() {
  if (g_gfx) {
    g_gfx->flush();
  }
}

bool DeviceWaveshare4B::initSDCard() {
  if (g_sd_available && WaveshareSDMMC.cardType() != CARD_NONE) {
    return true;
  }

  const uint32_t now = millis();
  if (g_sd_init_attempted && (now - g_sd_retry_tick_ms) < 1500) {
    return false;
  }
  g_sd_init_attempted = true;
  g_sd_retry_tick_ms = now;

  WaveshareSDMMC.end();

  if (!WaveshareSDMMC.begin("/sdcard", false, SDMMC_FREQ_DEFAULT, 5)) {
    g_sd_available = false;
    Serial.println("[Device/Waveshare4B] SD card mount failed");
    return false;
  }

  const uint8_t card_type = WaveshareSDMMC.cardType();
  if (card_type == CARD_NONE) {
    g_sd_available = false;
    Serial.println("[Device/Waveshare4B] SD card absent after mount");
    WaveshareSDMMC.end();
    return false;
  }

  g_sd_available = true;
  Serial.printf("[Device/Waveshare4B] SD card OK, type=%u, size=%llu MB\n",
                static_cast<unsigned>(card_type),
                static_cast<unsigned long long>(WaveshareSDMMC.cardSize() / (1024ULL * 1024ULL)));
  return true;
}

bool DeviceWaveshare4B::storageReady() {
  return initSDCard();
}

fs::FS& DeviceWaveshare4B::storageFS() {
  return WaveshareSDMMC;
}

#endif  // defined(DEVICE_WAVESHARE_4B)

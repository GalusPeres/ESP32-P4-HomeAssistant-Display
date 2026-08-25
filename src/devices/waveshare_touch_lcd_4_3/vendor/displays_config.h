#pragma once

#include <stdint.h>

#include "src/devices/device_select.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_4_3)

struct DisplayConfig {
  const char* name;
  uint32_t hsync_pulse_width;
  uint32_t hsync_back_porch;
  uint32_t hsync_front_porch;
  uint32_t vsync_pulse_width;
  uint32_t vsync_back_porch;
  uint32_t vsync_front_porch;
  uint32_t prefer_speed;
  uint32_t lane_bit_rate;
  uint16_t width;
  uint16_t height;
  int8_t i2c_sda_pin;
  int8_t i2c_scl_pin;
  uint32_t i2c_clock_speed;
  int8_t lcd_rst;
};

// Waveshare BSP 1.0.1 timing for the native portrait ST7701 panel.
inline constexpr DisplayConfig SCREEN_DEFAULT{
    "ESP32-P4-WIFI6-Touch-LCD-4.3",
    12,
    42,
    42,
    8,
    2,
    60,
    30000000,
    500,
    480,
    800,
    7,
    8,
    400000,
    27,
};

inline constexpr const DisplayConfig& display_cfg = SCREEN_DEFAULT;

#endif  // defined(DEVICE_WAVESHARE_TOUCH_LCD_4_3)

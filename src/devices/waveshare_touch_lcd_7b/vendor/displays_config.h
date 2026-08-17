#pragma once
#ifndef WAVESHARE_TOUCH_LCD_7B_DISPLAYS_CONFIG_H
#define WAVESHARE_TOUCH_LCD_7B_DISPLAYS_CONFIG_H

#include <stdint.h>

#include "src/devices/device_select.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7B)

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

// Exact Waveshare 7B and 7B-C EK79007 hardware contract. Both product
// variants use this same display and touch profile; the 7B-C camera is not a
// HomeTiles feature.
static constexpr DisplayConfig SCREEN_DEFAULT = {
    "WAVESHARE-7B-EK79007",
    10,
    160,
    160,
    1,
    23,
    12,
    52000000,
    1000,
    1024,
    600,
    7,
    8,
    400000,
    33,
};

inline constexpr const DisplayConfig& display_cfg = SCREEN_DEFAULT;

#endif  // defined(DEVICE_WAVESHARE_TOUCH_LCD_7B)
#endif  // WAVESHARE_TOUCH_LCD_7B_DISPLAYS_CONFIG_H

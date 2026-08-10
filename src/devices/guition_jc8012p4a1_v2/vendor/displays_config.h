/*
 * SPDX-FileCopyrightText: 2026 HomeTiles contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#ifndef DISPLAYS_CONFIG_H
#define DISPLAYS_CONFIG_H

#include <stdint.h>

#include "src/devices/device_select.h"

#if defined(DEVICE_GUITION_JC8012P4A1_V2)

struct DisplayConfig {
  const char* name;

  uint32_t hsync_pulse_width;
  uint32_t hsync_back_porch;
  uint32_t hsync_front_porch;
  uint32_t vsync_pulse_width;
  uint32_t vsync_back_porch;
  uint32_t vsync_front_porch;
  uint32_t prefer_speed;   // DPI pixel clock in Hz
  uint32_t lane_bit_rate;  // DSI lane bit rate in Mbps

  uint16_t width;   // native panel width, portrait
  uint16_t height;  // native panel height, portrait

  int8_t i2c_sda_pin;
  int8_t i2c_scl_pin;
  uint32_t i2c_clock_speed;
  int8_t lcd_rst;
};

// Guition JC8012P4A1 V2 native panel timing.
// The app stays landscape and the board driver rotates flush areas into this native portrait buffer.
static constexpr DisplayConfig SCREEN_DEFAULT = {
    "JC8012P4A1 V2",
    20,     // hsync_pulse_width
    20,     // hsync_back_porch
    40,     // hsync_front_porch
    4,      // vsync_pulse_width
    10,     // vsync_back_porch
    30,     // vsync_front_porch
    80000000,
    1500,
    800,
    1280,
    7,
    8,
    400000,
    27,     // lcd_rst
};

inline constexpr const DisplayConfig& display_cfg = SCREEN_DEFAULT;

#endif  // defined(DEVICE_GUITION_JC8012P4A1_V2)
#endif  // DISPLAYS_CONFIG_H

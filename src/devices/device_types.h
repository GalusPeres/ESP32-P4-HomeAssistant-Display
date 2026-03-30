#pragma once

#include <Arduino.h>

namespace Device {

enum class RotationStepMode : uint8_t {
  QuarterTurns = 0,
  FlipOnly = 1,
};

struct Capabilities {
  bool has_battery;
  bool has_imu;
  bool supports_auto_rotation;
  bool supports_battery_sleep_profile;
};

struct Profile {
  const char* key;
  const char* display_name;
  uint16_t screen_width;
  uint16_t screen_height;
  uint8_t grid_cols;
  uint8_t grid_rows;
  uint16_t grid_gap;
  uint16_t grid_pad;
  uint16_t grid_cell_w;
  uint16_t grid_cell_h;
  RotationStepMode rotation_step_mode;
  uint8_t rotation_default;
  uint8_t rotation_flipped;
  Capabilities capabilities;
};

}  // namespace Device

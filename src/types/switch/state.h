#pragma once

#include <stdint.h>
#include <lvgl.h>

struct SwitchTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* switch_obj = nullptr;
};

struct SwitchState {
  bool available = true;
  bool has_state = false;
  bool is_on = false;
  bool has_color = false;
  uint32_t color = 0;
  bool has_hs = false;
  float hs_h = 0.0f;
  float hs_s = 0.0f;
  bool has_brightness = false;
  uint8_t brightness_pct = 100;
  bool has_color_temp = false;
  uint16_t color_temp_kelvin = 4000;
  uint16_t min_color_temp_kelvin = 2000;
  uint16_t max_color_temp_kelvin = 6535;
  bool supports_color = false;
  bool supports_brightness = false;
  bool supports_temperature = false;
  bool supported_modes_known = false;
  bool supported_onoff_only = false;
};

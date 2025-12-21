#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct LightPopupInit {
  String entity_id;
  String title;
  uint32_t color = 0;
  uint8_t brightness_pct = 100;
  bool supports_color = false;
  bool supports_brightness = false;
  bool is_light = true;
  bool is_on = true;
};

void show_light_popup(const LightPopupInit& init);

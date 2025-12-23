#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct LightPopupInit {
  String entity_id;
  String title;
  String icon_name;
  uint32_t color = 0;
  uint8_t brightness_pct = 100;
  bool has_color = false;
  bool has_brightness = false;
  bool has_state = false;
  bool has_hs = false;
  float hs_h = 0.0f;
  float hs_s = 0.0f;
  bool supports_color = false;
  bool supports_brightness = false;
  bool is_light = true;
  bool is_on = true;
};

void show_light_popup(const LightPopupInit& init);
void update_light_popup(const LightPopupInit& init);
void preload_light_popup();
void hide_light_popup();

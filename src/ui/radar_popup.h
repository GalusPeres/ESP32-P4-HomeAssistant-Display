#pragma once

#include <Arduino.h>

struct RadarPopupInit {
  String title;
  String icon_name;
  String preset;
  String bbox;
  uint8_t frame_count = 6;
  uint16_t frame_delay_ms = 450;
  uint16_t refresh_sec = 300;
  uint32_t bg_color = 0x2A2A2A;
};

void show_radar_popup(const RadarPopupInit& init);
void hide_radar_popup();
void radar_popup_service();

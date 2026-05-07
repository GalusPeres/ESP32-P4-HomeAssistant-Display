#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct MediaPopupInit {
  String entity_id;
  String title;
  String icon_name;
  String icon_char;
  String media_title;
  String media_subtitle;
  bool is_playing = false;
  bool has_volume = false;
  float volume_level = 0.0f;
  bool is_muted = false;
  uint32_t bg_color = 0;
  const lv_image_dsc_t* cover_dsc = nullptr;
  uint32_t cover_hash = 0;
};

void show_media_popup(const MediaPopupInit& init);
void preload_media_popup();
void update_media_popup(const MediaPopupInit& init);
void update_media_popup_cover(const char* entity_id, const lv_image_dsc_t* cover_dsc, uint32_t cover_hash);
void hide_media_popup();

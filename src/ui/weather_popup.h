#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct WeatherPopupInit {
  String entity_id;
  String title;
  uint32_t bg_color = 0;
};

void show_weather_popup(const WeatherPopupInit& init);
// True only when the hidden, prebuilt popup already represents the newest
// cached payload for this entity in the currently selected language.
bool weather_popup_has_current_cached_payload(const char* entity_id);
void preload_weather_popup();
void hide_weather_popup();
void weather_popup_refresh_language();

// Thread-safe queue helper (MQTT -> main loop).
void queue_weather_popup_payload(const char* entity_id, const char* payload);
void process_weather_popup_queue();

#pragma once

#include <Arduino.h>
#include <lvgl.h>

struct SensorPopupInit {
  String entity_id;
  String title;
  String icon_name;
  String value;
  String unit;
};

void show_sensor_popup(const SensorPopupInit& init);
void preload_sensor_popup();

// Thread-safe queue helpers (MQTT -> main loop).
void queue_sensor_popup_value(const char* entity_id, const char* value, const char* unit);
void queue_sensor_popup_history(const char* entity_id, const char* payload, size_t len);
void process_sensor_popup_queue();

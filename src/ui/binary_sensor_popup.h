#pragma once

#include <Arduino.h>
#include <stdint.h>

struct BinarySensorPopupInit {
  String entity_id;
  String title;
  String icon_name;
  String state;
  String device_class;
  uint64_t last_changed = 0;
  bool available = true;
  bool icon_override = false;
  uint32_t bg_color = 0;
};

// Binary sensors reuse the established Sensor popup surface and lifecycle.
// Only the body changes from a numeric chart to a state timeline and activity
// list, so no second full-screen overlay has to remain allocated.
void show_binary_sensor_popup(const BinarySensorPopupInit& init);

// Thread-safe queue helper for live MQTT updates. The main loop applies the
// newest update only when the matching binary sensor popup is visible.
void queue_binary_sensor_popup_state(const String& entity_id,
                                     const String& state,
                                     bool available,
                                     const String& device_class,
                                     uint64_t last_changed,
                                     const String& icon_name);

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>

enum class BinarySensorValue : uint8_t {
  Missing = 0,
  Off,
  On,
  Unknown,
  Unavailable
};

struct BinarySensorState {
  bool valid = false;
  bool available = false;
  bool has_available = false;
  bool has_device_class = false;
  bool has_last_changed = false;
  bool has_icon = false;
  BinarySensorValue value = BinarySensorValue::Missing;
  uint64_t last_changed = 0;
  char device_class[24] = {};
  char icon_name[40] = {};
};

static constexpr size_t BINARY_SENSOR_PAYLOAD_MAX = 512;

struct BinarySensorTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* state_label = nullptr;
  uint32_t last_payload_hash = 0;
  bool dynamic_icon = true;
};

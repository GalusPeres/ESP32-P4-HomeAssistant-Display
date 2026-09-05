#pragma once

#include <stdint.h>
#include <lvgl.h>

enum CoverFeature : uint8_t {
  COVER_FEATURE_OPEN = 1U << 0,
  COVER_FEATURE_CLOSE = 1U << 1,
  COVER_FEATURE_SET_POSITION = 1U << 2,
  COVER_FEATURE_STOP = 1U << 3,
  COVER_FEATURE_OPEN_TILT = 1U << 4,
  COVER_FEATURE_CLOSE_TILT = 1U << 5,
  COVER_FEATURE_STOP_TILT = 1U << 6,
  COVER_FEATURE_SET_TILT_POSITION = 1U << 7
};

struct CoverState {
  bool valid = false;
  bool available = false;
  bool assumed_state = false;
  bool has_position = false;
  bool has_tilt_position = false;
  uint8_t position = 0;
  uint8_t tilt_position = 0;
  uint8_t supported_features = 0;
  char state[12] = {};
  char device_class[12] = {};
};

struct CoverTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* state_label = nullptr;
  lv_obj_t* value_label = nullptr;
  uint32_t last_payload_hash = 0;
  bool dynamic_icon = true;
};

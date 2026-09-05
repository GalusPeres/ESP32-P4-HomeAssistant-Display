#pragma once

#include <stdint.h>
#include <lvgl.h>

#include "src/devices/device_select.h"

struct WeatherForecastWidgets {
  lv_obj_t* day_label = nullptr;
  lv_obj_t* sep_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* temp_label = nullptr;
  lv_obj_t* temp_high_label = nullptr;
  lv_obj_t* temp_high_unit_label = nullptr;
  lv_obj_t* temp_low_label = nullptr;
  lv_obj_t* temp_low_unit_label = nullptr;
};

static constexpr uint8_t WEATHER_FORECAST_MAX = 8;
#if defined(DEVICE_LAYOUT_1024X600)
static constexpr lv_coord_t WEATHER_FORECAST_COL_W = 125;
#elif defined(DEVICE_LAYOUT_480X480)
static constexpr lv_coord_t WEATHER_FORECAST_COL_W = 100;
#else
static constexpr lv_coord_t WEATHER_FORECAST_COL_W = 150;
#endif

// Map tile width (span_w) to number of forecast days shown
inline uint8_t weather_forecast_count(uint8_t span_w) {
  switch (span_w) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 4;
    case 4: return 5;
    case 5: return 6;
    case 6: return 8;
    default: return span_w >= 6 ? 8 : span_w;
  }
}

struct WeatherTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* temp_label = nullptr;
  lv_obj_t* condition_label = nullptr;
  lv_obj_t* condition_sep_label = nullptr;
  lv_obj_t* location_label = nullptr;
  WeatherForecastWidgets forecast[WEATHER_FORECAST_MAX];
  uint32_t last_payload_hash = 0;
};

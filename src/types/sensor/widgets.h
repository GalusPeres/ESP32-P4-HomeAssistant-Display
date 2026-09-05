#pragma once

#include <stdint.h>
#include <lvgl.h>

struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  lv_obj_t* unit_label = nullptr;
  lv_obj_t* gauge = nullptr;
  int32_t gauge_min = 0;
  int32_t gauge_max = 100;
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
};

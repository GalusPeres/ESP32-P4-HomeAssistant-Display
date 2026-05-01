#pragma once

#include <lvgl.h>
#include "src/tiles/tile_renderer.h"

static constexpr int32_t GAUGE_ARC_STEPS = 1000;

// Extern declarations for global widget arrays
extern SensorTileWidgets g_tab0_sensors[];
extern SensorTileWidgets g_tab1_sensors[];
extern SensorTileWidgets g_tab2_sensors[];

extern SwitchTileWidgets g_tab0_switches[];
extern SwitchTileWidgets g_tab1_switches[];
extern SwitchTileWidgets g_tab2_switches[];

extern WeatherTileWidgets g_tab0_weather[];
extern WeatherTileWidgets g_tab1_weather[];
extern WeatherTileWidgets g_tab2_weather[];

extern MediaTileWidgets g_tab0_media[];
extern MediaTileWidgets g_tab1_media[];
extern MediaTileWidgets g_tab2_media[];

extern SwitchState g_tab0_switch_states[];
extern SwitchState g_tab1_switch_states[];
extern SwitchState g_tab2_switch_states[];

void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f);
void set_tile_grid_cell(lv_obj_t* obj, uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h);

SensorTileWidgets* tile_renderer_get_sensor_widgets(GridType grid_type);
SwitchTileWidgets* tile_renderer_get_switch_widgets(GridType grid_type);
WeatherTileWidgets* tile_renderer_get_weather_widgets(GridType grid_type);
MediaTileWidgets* tile_renderer_get_media_widgets(GridType grid_type);
SwitchState* tile_renderer_get_switch_states(GridType grid_type);

bool is_light_entity_id(const String& entity_id);
void update_switch_tile_state(GridType grid_type, uint8_t grid_index, const char* payload);
void update_media_tile_state(GridType grid_type, uint8_t grid_index, const char* payload);

static inline uint32_t brighten_rgb_color(uint32_t color, uint8_t delta) {
  const uint16_t r = ((color >> 16) & 0xFFu) + delta;
  const uint16_t g = ((color >> 8) & 0xFFu) + delta;
  const uint16_t b = (color & 0xFFu) + delta;
  return ((r > 0xFFu ? 0xFFu : r) << 16) |
         ((g > 0xFFu ? 0xFFu : g) << 8) |
         (b > 0xFFu ? 0xFFu : b);
}

static inline void disable_pressed_button_animation(lv_obj_t* obj) {
  if (!obj) return;
  lv_obj_set_style_transform_width(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_translate_x(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_translate_y(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
}

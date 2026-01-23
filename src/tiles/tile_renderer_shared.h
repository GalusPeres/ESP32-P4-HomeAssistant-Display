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

extern SwitchState g_tab0_switch_states[];
extern SwitchState g_tab1_switch_states[];
extern SwitchState g_tab2_switch_states[];

void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f);
void set_tile_grid_cell(lv_obj_t* obj, uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h);

SensorTileWidgets* tile_renderer_get_sensor_widgets(GridType grid_type);
SwitchTileWidgets* tile_renderer_get_switch_widgets(GridType grid_type);
SwitchState* tile_renderer_get_switch_states(GridType grid_type);

bool is_light_entity_id(const String& entity_id);
void update_switch_tile_state(GridType grid_type, uint8_t grid_index, const char* payload);

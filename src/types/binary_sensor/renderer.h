#pragma once

#include "src/tiles/runtime/tile_renderer.h"

lv_obj_t* render_binary_sensor_tile(lv_obj_t* parent, int col, int row,
                                    const Tile& tile, uint8_t index,
                                    GridType grid_type);

BinarySensorState parse_binary_sensor_payload(const char* payload);
const char* binary_sensor_state_name(BinarySensorValue value);
String binary_sensor_visual_icon(const BinarySensorState& state,
                                 const String& entity_icon = String());
String binary_sensor_resolve_icon(const Tile& tile,
                                  const BinarySensorState& state,
                                  bool* dynamic_icon = nullptr);
uint32_t binary_sensor_visual_color(const BinarySensorState& state);

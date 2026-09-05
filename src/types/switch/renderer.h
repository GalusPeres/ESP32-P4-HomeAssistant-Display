#pragma once

#include "src/tiles/runtime/tile_renderer.h"

lv_obj_t* render_switch_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);

#pragma once

#include <lvgl.h>
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"

lv_obj_t* render_energy_tile(lv_obj_t* parent,
                             int col,
                             int row,
                             const Tile& tile,
                             uint8_t index,
                             GridType grid_type);


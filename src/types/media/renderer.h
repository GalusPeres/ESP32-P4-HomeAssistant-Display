#pragma once

#include "src/tiles/tile_renderer.h"

lv_obj_t* render_media_tile(lv_obj_t* parent,
                            int col,
                            int row,
                            const Tile& tile,
                            uint8_t index,
                            GridType grid_type);

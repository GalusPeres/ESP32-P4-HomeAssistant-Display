#pragma once

#include "src/tiles/tile_renderer.h"

lv_obj_t* render_image_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
bool image_tile_get_preview_path(const Tile& tile, String& out_path);

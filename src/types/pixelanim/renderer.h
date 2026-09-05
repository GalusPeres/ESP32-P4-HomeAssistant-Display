#pragma once

#include "src/tiles/runtime/tile_renderer.h"

// Renders an "Animation" tile: a low-res pixel-art animation loaded from a
// ".panim" file on the SD card (folder /animations). The selected file name is
// stored in tile.scene_alias (same no-side-effect string slot the text tile
// uses). Empty selection renders a transparent placeholder like an empty tile.
lv_obj_t* render_pixelanim_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);

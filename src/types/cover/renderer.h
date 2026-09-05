#pragma once

#include "src/tiles/runtime/tile_renderer.h"

lv_obj_t* render_cover_tile(lv_obj_t* parent, int col, int row,
                            const Tile& tile, uint8_t index,
                            GridType grid_type);

String cover_resolve_icon(const Tile& tile, const CoverState& state,
                          bool* dynamic_icon = nullptr);
void refresh_cover_popup_for_tile(GridType grid_type, uint8_t index);

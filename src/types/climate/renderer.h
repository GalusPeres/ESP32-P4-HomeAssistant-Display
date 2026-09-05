#pragma once

#include "src/tiles/runtime/tile_renderer.h"

lv_obj_t* render_climate_tile(lv_obj_t* parent,
                              int col,
                              int row,
                              const Tile& tile,
                              uint8_t index,
                              GridType grid_type);

void refresh_climate_tile_content(GridType grid_type,
                                  uint8_t index,
                                  const ClimateState& state);

// Keeps a locally adjusted mini-target visible while Home Assistant may still
// return an older climate state. Returns true when an optimistic target value
// was merged into the incoming state.
bool reconcile_climate_mini_target_state(GridType grid_type,
                                         uint8_t index,
                                         ClimateState& state);

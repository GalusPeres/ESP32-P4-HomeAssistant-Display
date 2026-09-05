#pragma once

#include <lvgl.h>

namespace ui_surface_style {

void apply_tile_border(lv_obj_t* obj, bool enabled);
void apply_global_tile_border(lv_obj_t* obj);

// Safe to call from the Web handler: only sets a flag. Apply the actual
// LVGL update later during the safe UI service pass.
void request_global_tile_border_refresh();
void process_pending_updates();

}  // namespace ui_surface_style

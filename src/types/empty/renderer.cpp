#include "src/types/empty/renderer.h"
#include "src/tiles/runtime/tile_renderer_shared.h"
#include <Arduino.h>

lv_obj_t* render_empty_tile(lv_obj_t* parent, int col, int row) {
  lv_obj_t* placeholder = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(placeholder, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(placeholder, 0, 0);
  set_tile_grid_cell(placeholder, col, row, 1, 1);
  return placeholder;
}



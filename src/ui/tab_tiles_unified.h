#ifndef TAB_TILES_UNIFIED_H
#define TAB_TILES_UNIFIED_H

#include <lvgl.h>
#include "src/tiles/tile_renderer.h"

// Unified tile tab functions - works for HOME, GAME, and WEATHER grids
void build_tiles_tab(lv_obj_t* parent, GridType grid_type, scene_publish_cb_t scene_cb);
void tiles_reload_layout(GridType grid_type);
void tiles_update_tile(GridType grid_type, uint8_t index);
void tiles_update_sensor_by_entity(GridType grid_type, const char* entity_id, const char* value);

#endif // TAB_TILES_UNIFIED_H

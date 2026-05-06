#ifndef TAB_TILES_UNIFIED_H
#define TAB_TILES_UNIFIED_H

#include <lvgl.h>
#include "src/tiles/tile_renderer.h"

// Unified tile tab functions - works for HOME, GAME, and WEATHER grids
void build_tiles_tab(lv_obj_t* parent, GridType grid_type, scene_publish_cb_t scene_cb);
void tiles_reload_layout(GridType grid_type);
void tiles_release_layout(GridType grid_type);
void tiles_release_all();
bool tiles_is_loaded(GridType grid_type);
void tiles_request_reload(GridType grid_type);
void tiles_request_reload_if_loaded(GridType grid_type);
void tiles_request_reload_all();
void tiles_request_icon_refresh();
void tiles_refresh_image_previews_for_key(GridType grid_type, const String& raw_key);
void tiles_request_release(GridType grid_type);
void tiles_request_release_all();
void tiles_process_reload_requests();
void tiles_refresh_visible_from_cache();
void tiles_request_visible_cache_refresh();
void tiles_process_visible_cache_refresh(bool allow_now);
void tiles_refresh_cache_from_bridge_values();
void tiles_request_bridge_cache_refresh();
void tiles_process_bridge_cache_refresh(bool allow_now);
void tiles_update_tile(GridType grid_type, uint8_t index);
void tiles_update_sensor_by_entity(GridType grid_type, const char* entity_id, const char* value);
void tiles_update_weather_by_entity(GridType grid_type, const char* entity_id, const char* payload);
bool tiles_get_cached_entity_payload(const char* entity_id, String& out);
void tiles_cache_entity_payload(const char* entity_id, const char* payload);
void tiles_switch_to_folder(uint16_t folder_id);
void tiles_invalidate_folder(uint16_t folder_id);

#endif // TAB_TILES_UNIFIED_H

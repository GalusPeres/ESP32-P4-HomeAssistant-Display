#ifndef TAB_TILES_WEATHER_H
#define TAB_TILES_WEATHER_H

#include <lvgl.h>
#include "src/tiles/tile_renderer.h"

void build_tiles_weather_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb);
void tiles_weather_reload_layout();
void tiles_weather_update_tile(uint8_t index);
void tiles_weather_update_sensor_by_entity(const char* entity_id, const char* value);

#endif // TAB_TILES_WEATHER_H

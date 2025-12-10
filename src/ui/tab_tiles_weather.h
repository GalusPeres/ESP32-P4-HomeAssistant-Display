#ifndef TAB_TILES_WEATHER_H
#define TAB_TILES_WEATHER_H

#include "tab_tiles_unified.h"

// Backward compatibility wrappers for Weather tab
inline void build_tiles_weather_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb) {
    build_tiles_tab(parent, GridType::WEATHER, scene_cb);
}

inline void tiles_weather_reload_layout() {
    tiles_reload_layout(GridType::WEATHER);
}

inline void tiles_weather_update_tile(uint8_t index) {
    tiles_update_tile(GridType::WEATHER, index);
}

inline void tiles_weather_update_sensor_by_entity(const char* entity_id, const char* value) {
    tiles_update_sensor_by_entity(GridType::WEATHER, entity_id, value);
}

#endif // TAB_TILES_WEATHER_H

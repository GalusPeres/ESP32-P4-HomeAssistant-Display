#ifndef TAB_TILES_HOME_H
#define TAB_TILES_HOME_H

#include "tab_tiles_unified.h"

// Backward compatibility wrappers for Home tab
inline void build_tiles_home_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb) {
    build_tiles_tab(parent, GridType::HOME, scene_cb);
}

inline void tiles_home_reload_layout() {
    tiles_reload_layout(GridType::HOME);
}

inline void tiles_home_update_tile(uint8_t index) {
    tiles_update_tile(GridType::HOME, index);
}

inline void tiles_home_update_sensor_by_entity(const char* entity_id, const char* value) {
    tiles_update_sensor_by_entity(GridType::HOME, entity_id, value);
}

#endif // TAB_TILES_HOME_H

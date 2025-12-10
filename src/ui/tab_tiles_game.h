#ifndef TAB_TILES_GAME_H
#define TAB_TILES_GAME_H

#include "tab_tiles_unified.h"

// Backward compatibility wrappers for Game tab
inline void build_tiles_game_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb) {
    build_tiles_tab(parent, GridType::GAME, scene_cb);
}

inline void tiles_game_reload_layout() {
    tiles_reload_layout(GridType::GAME);
}

inline void tiles_game_update_tile(uint8_t index) {
    tiles_update_tile(GridType::GAME, index);
}

inline void tiles_game_update_sensor_by_entity(const char* entity_id, const char* value) {
    tiles_update_sensor_by_entity(GridType::GAME, entity_id, value);
}

#endif // TAB_TILES_GAME_H

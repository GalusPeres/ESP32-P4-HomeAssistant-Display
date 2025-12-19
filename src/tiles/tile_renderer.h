#ifndef TILE_RENDERER_H
#define TILE_RENDERER_H

#include <lvgl.h>
#include "src/tiles/tile_config.h"

// Forward declarations
typedef void (*scene_publish_cb_t)(const char* scene_alias);

enum class GridType : uint8_t {
  TAB0 = 0,
  TAB1 = 1,
  TAB2 = 2
};

// Rendert ein komplettes Tile-Grid (12 Kacheln, 3×4)
void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type, scene_publish_cb_t scene_cb = nullptr);

// Rendert eine einzelne Kachel basierend auf Typ und liefert das erzeugte Objekt
lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb);

// Typ-spezifische Render-Funktionen
lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb);
lv_obj_t* render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_switch_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_empty_tile(lv_obj_t* parent, int col, int row);

// Update-Funktionen (für Sensoren)
void update_sensor_tile_value(GridType grid_type, uint8_t grid_index, const char* value, const char* unit = nullptr);
void reset_sensor_widget(GridType grid_type, uint8_t grid_index);
void reset_sensor_widgets(GridType grid_type);

// THREAD-SAFE: Queue für Sensor-Updates (MQTT Callback → Main Loop)
void queue_sensor_tile_update(GridType grid_type, uint8_t grid_index, const char* value, const char* unit = nullptr);
void process_sensor_update_queue();  // Im Main Loop VOR lv_timer_handler() aufrufen!

#endif // TILE_RENDERER_H

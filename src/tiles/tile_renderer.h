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

struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  lv_obj_t* unit_label = nullptr;
  lv_obj_t* gauge = nullptr;
  int32_t gauge_min = 0;
  int32_t gauge_max = 100;
};

struct SwitchTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* switch_obj = nullptr;
};

struct SwitchState {
  bool has_state = false;
  bool is_on = false;
  bool has_color = false;
  uint32_t color = 0;
  bool has_hs = false;
  float hs_h = 0.0f;
  float hs_s = 0.0f;
  bool has_brightness = false;
  uint8_t brightness_pct = 100;
  bool supports_color = false;
  bool supports_brightness = false;
  bool supported_modes_known = false;
  bool supported_onoff_only = false;
};

struct TileWidgetCache {
  SensorTileWidgets sensors[TILES_PER_GRID];
  SwitchTileWidgets switches[TILES_PER_GRID];
  SwitchState switch_states[TILES_PER_GRID];
};

// Rendert ein komplettes Tile-Grid (6x4)
void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type, scene_publish_cb_t scene_cb = nullptr);

// Rendert eine einzelne Kachel basierend auf Typ und liefert das erzeugte Objekt
lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb);

// Typ-spezifische Render-Funktionen
lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb);
lv_obj_t* render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_switch_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_image_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_clock_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_empty_tile(lv_obj_t* parent, int col, int row);

// Update-Funktionen (für Sensoren)
void update_sensor_tile_value(GridType grid_type, uint8_t grid_index, const char* value, const char* unit = nullptr);
void reset_sensor_widget(GridType grid_type, uint8_t grid_index);
void reset_sensor_widgets(GridType grid_type);

// THREAD-SAFE: Queue für Sensor-Updates (MQTT Callback → Main Loop)
void queue_sensor_tile_update(GridType grid_type, uint8_t grid_index, const char* value, const char* unit = nullptr);
void process_sensor_update_queue();  // Im Main Loop VOR lv_timer_handler() aufrufen!

// Update-Funktionen (fuer Switches)
void reset_switch_widget(GridType grid_type, uint8_t grid_index);
void reset_switch_widgets(GridType grid_type);

// THREAD-SAFE: Queue fuer Switch-Updates (MQTT Callback -> Main Loop)
void queue_switch_tile_update(GridType grid_type, uint8_t grid_index, const char* payload);
void process_switch_update_queue();  // Im Main Loop VOR lv_timer_handler() aufrufen!

void tile_renderer_snapshot_tab0(TileWidgetCache* out);
void tile_renderer_restore_tab0(const TileWidgetCache* in);

#endif // TILE_RENDERER_H

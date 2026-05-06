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
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
};

struct SwitchTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* switch_obj = nullptr;
};

struct WeatherForecastWidgets {
  lv_obj_t* day_label = nullptr;
  lv_obj_t* sep_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* temp_label = nullptr;
  lv_obj_t* temp_high_label = nullptr;
  lv_obj_t* temp_high_unit_label = nullptr;
  lv_obj_t* temp_low_label = nullptr;
  lv_obj_t* temp_low_unit_label = nullptr;
};

static constexpr uint8_t WEATHER_FORECAST_MAX = 8;
static constexpr lv_coord_t WEATHER_FORECAST_COL_W = 150;

// Map tile width (span_w) to number of forecast days shown
inline uint8_t weather_forecast_count(uint8_t span_w) {
  switch (span_w) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 4;
    case 4: return 5;
    case 5: return 6;
    case 6: return 8;
    default: return span_w >= 6 ? 8 : span_w;
  }
}

struct WeatherTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* temp_label = nullptr;
  lv_obj_t* condition_label = nullptr;
  lv_obj_t* condition_sep_label = nullptr;
  lv_obj_t* location_label = nullptr;
  WeatherForecastWidgets forecast[WEATHER_FORECAST_MAX];
  uint32_t last_payload_hash = 0;
};

struct MediaCoverRef {
  lv_image_dsc_t* dsc = nullptr;
  String source_url;
  uint32_t url_hash = 0;
  uint32_t requested_url_hash = 0;
  uint32_t failed_url_hash = 0;
  uint32_t failed_at_ms = 0;
};

struct MediaTileWidgets {
  lv_obj_t* cover_clip = nullptr;
  lv_obj_t* cover_image = nullptr;
  MediaCoverRef* cover_ref = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* previous_label = nullptr;
  lv_obj_t* play_pause_label = nullptr;
  lv_obj_t* next_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* media_title_label = nullptr;
  lv_obj_t* media_subtitle_label = nullptr;
  lv_obj_t* state_label = nullptr;
  uint32_t last_payload_hash = 0;
  uint32_t last_media_text_hash = 0;
  bool dynamic_icon = true;
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
  bool has_color_temp = false;
  uint16_t color_temp_kelvin = 4000;
  uint16_t min_color_temp_kelvin = 2200;
  uint16_t max_color_temp_kelvin = 6500;
  bool supports_color = false;
  bool supports_brightness = false;
  bool supports_temperature = false;
  bool supported_modes_known = false;
  bool supported_onoff_only = false;
};

struct TileWidgetCache {
  SensorTileWidgets sensors[TILES_PER_GRID];
  SwitchTileWidgets switches[TILES_PER_GRID];
  SwitchState switch_states[TILES_PER_GRID];
  WeatherTileWidgets weather[TILES_PER_GRID];
  MediaTileWidgets media[TILES_PER_GRID];
};

// Rendert ein komplettes Tile-Grid (4x4)
void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type,
                      scene_publish_cb_t scene_cb = nullptr, lv_obj_t** out_tile_objs = nullptr);

// Rendert eine einzelne Kachel basierend auf Typ und liefert das erzeugte Objekt
lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb);

// Typ-spezifische Render-Funktionen
lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb);
lv_obj_t* render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_switch_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_clock_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_text_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index);
lv_obj_t* render_counter_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_weather_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
lv_obj_t* render_media_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);
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

void reset_weather_widget(GridType grid_type, uint8_t grid_index);
void reset_weather_widgets(GridType grid_type);
void queue_weather_tile_update(GridType grid_type, uint8_t grid_index, const char* payload);
void process_weather_update_queue();  // Im Main Loop VOR lv_timer_handler() aufrufen!

void reset_media_widget(GridType grid_type, uint8_t grid_index);
void reset_media_widgets(GridType grid_type);
void queue_media_tile_update(GridType grid_type, uint8_t grid_index, const char* payload);
void process_media_update_queue();  // Im Main Loop VOR lv_timer_handler() aufrufen!

// THREAD-SAFE: Queue fuer Tile-Graph-History (MQTT Callback -> Main Loop)
void queue_tile_graph_history(const char* entity_id, const char* payload, size_t len);
void process_tile_graph_queue();  // Im Main Loop VOR lv_timer_handler() aufrufen!
void request_tile_graph_history(const char* entity_id);  // Fordert History fuer entity an

void tile_renderer_snapshot_tab0(TileWidgetCache* out);
void tile_renderer_restore_tab0(const TileWidgetCache* in);

#endif // TILE_RENDERER_H

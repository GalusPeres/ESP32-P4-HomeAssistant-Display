#include "src/ui/tab_tiles_unified.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include <Arduino.h>

/* === Layout-Konstanten === */
static const int GAP = 24;
static const int OUTER = 0;
static const int GRID_PAD = 24;

/* === Globale State (unified for all 3 grids) === */
static lv_obj_t* g_tiles_grids[3] = {nullptr};           // [TAB0, TAB1, TAB2]
static scene_publish_cb_t g_tiles_scene_cbs[3] = {nullptr};
static lv_obj_t* g_tiles_objs[3][TILES_PER_GRID] = {nullptr};
static bool g_tiles_loaded[3] = {false, false, false};
static bool g_tiles_reload_requested[3] = {false, false, false};
static bool g_tiles_reload_only_if_loaded[3] = {true, true, true};
static bool g_tiles_release_requested[3] = {false, false, false};

/* === Entity-State Cache (for lazy-loaded tabs) === */
struct EntityCacheEntry {
  String entity_id;
  String payload;
  bool valid = false;
};

static constexpr size_t kEntityCacheSize = TILES_PER_GRID * 3;
static EntityCacheEntry g_entity_cache[kEntityCacheSize];
static size_t g_entity_cache_cursor = 0;

static void cache_entity_payload(const char* entity_id, const char* payload) {
  if (!entity_id || !payload || entity_id[0] == '\0') return;

  for (size_t i = 0; i < kEntityCacheSize; ++i) {
    EntityCacheEntry& entry = g_entity_cache[i];
    if (entry.valid && entry.entity_id.equalsIgnoreCase(entity_id)) {
      entry.payload = payload;
      return;
    }
  }

  for (size_t i = 0; i < kEntityCacheSize; ++i) {
    if (!g_entity_cache[i].valid) {
      g_entity_cache[i].entity_id = entity_id;
      g_entity_cache[i].payload = payload;
      g_entity_cache[i].valid = true;
      return;
    }
  }

  size_t idx = g_entity_cache_cursor++ % kEntityCacheSize;
  g_entity_cache[idx].entity_id = entity_id;
  g_entity_cache[idx].payload = payload;
  g_entity_cache[idx].valid = true;
}

static bool get_cached_entity_payload(const char* entity_id, String& out) {
  if (!entity_id || entity_id[0] == '\0') return false;
  for (size_t i = 0; i < kEntityCacheSize; ++i) {
    const EntityCacheEntry& entry = g_entity_cache[i];
    if (entry.valid && entry.entity_id.equalsIgnoreCase(entity_id)) {
      out = entry.payload;
      return true;
    }
  }
  return false;
}

/* === Helper: Get grid config by type === */
static const TileGridConfig& getGridConfig(GridType type) {
  switch(type) {
    case GridType::TAB0:    return tileConfig.getTab0Grid();
    case GridType::TAB1:    return tileConfig.getTab1Grid();
    case GridType::TAB2:    return tileConfig.getTab2Grid();
    default:                return tileConfig.getTab0Grid();
  }
}

/* === Helper: Get grid name for logging === */
static const char* getGridName(GridType type) {
  switch(type) {
    case GridType::TAB0:    return "TilesTab0";
    case GridType::TAB1:    return "TilesTab1";
    case GridType::TAB2:    return "TilesTab2";
    default:                return "TilesUnknown";
  }
}

/* === Create tiles grid === */
static lv_obj_t* create_tiles_grid(lv_obj_t* parent) {
  if (!parent) return nullptr;
  lv_obj_t* grid = lv_obj_create(parent);
  lv_obj_set_style_bg_color(grid, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, GRID_PAD, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_column(grid, GAP, 0);
  lv_obj_set_style_pad_row(grid, GAP, 0);

  static lv_coord_t col_dsc[] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t row_dsc[] = {
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_CONTENT,
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  return grid;
}

static void apply_cached_states(GridType grid_type, const TileGridConfig& config) {
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type != TILE_SENSOR && tile.type != TILE_SWITCH) continue;
    if (tile.sensor_entity.length() == 0) continue;

    String payload;
    if (!get_cached_entity_payload(tile.sensor_entity.c_str(), payload)) continue;

    if (tile.type == TILE_SENSOR) {
      const char* unit = tile.sensor_unit.length() > 0 ? tile.sensor_unit.c_str() : nullptr;
      queue_sensor_tile_update(grid_type, i, payload.c_str(), unit);
    } else if (tile.type == TILE_SWITCH) {
      queue_switch_tile_update(grid_type, i, payload.c_str());
    }
  }
}

/* === Aufbau Tiles-Tab (unified) === */
void build_tiles_tab(lv_obj_t *parent, GridType grid_type, scene_publish_cb_t scene_cb) {
  uint8_t idx = (uint8_t)grid_type;
  g_tiles_scene_cbs[idx] = scene_cb;

  lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
  lv_obj_set_scroll_dir(parent, LV_DIR_VER);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_anim_duration(parent, 0, 0);
  lv_obj_set_style_pad_all(parent, OUTER, 0);

  g_tiles_grids[idx] = create_tiles_grid(parent);
  if (grid_type == GridType::TAB0) {
    tiles_reload_layout(grid_type);
  } else {
    g_tiles_loaded[idx] = false;
  }
}

/* === Reload layout (unified) === */
void tiles_reload_layout(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (!g_tiles_grids[idx]) return;

  reset_sensor_widgets(grid_type);
  reset_switch_widgets(grid_type);
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    g_tiles_objs[idx][i] = nullptr;
  }
  lv_obj_clean(g_tiles_grids[idx]);

  // Render tile grid using unified system
  const TileGridConfig& config = getGridConfig(grid_type);
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    int row = i / 3;
    int col = i % 3;
    g_tiles_objs[idx][i] = render_tile(g_tiles_grids[idx], col, row, config.tiles[i], i, grid_type, g_tiles_scene_cbs[idx]);
    if ((i % 3) == 2) {
      yield();
      delay(1);
    }
  }

  g_tiles_loaded[idx] = true;
  apply_cached_states(grid_type, config);
  Serial.printf("[%s] Layout neu geladen\n", getGridName(grid_type));
}

void tiles_release_layout(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (!g_tiles_grids[idx] || !g_tiles_loaded[idx]) return;

  reset_sensor_widgets(grid_type);
  reset_switch_widgets(grid_type);
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    g_tiles_objs[idx][i] = nullptr;
  }
  lv_obj_clean(g_tiles_grids[idx]);
  g_tiles_loaded[idx] = false;

  Serial.printf("[%s] Layout freigegeben\n", getGridName(grid_type));
}

void tiles_release_all() {
  tiles_release_layout(GridType::TAB0);
  tiles_release_layout(GridType::TAB1);
  tiles_release_layout(GridType::TAB2);
}

bool tiles_is_loaded(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (idx >= 3) return false;
  return g_tiles_loaded[idx];
}

void tiles_request_reload(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (idx >= 3) return;
  g_tiles_reload_requested[idx] = true;
  g_tiles_reload_only_if_loaded[idx] = false;
}

void tiles_request_reload_if_loaded(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (idx >= 3) return;
  if (!g_tiles_reload_requested[idx]) {
    g_tiles_reload_only_if_loaded[idx] = true;
  }
  g_tiles_reload_requested[idx] = true;
}

void tiles_request_reload_all() {
  for (uint8_t i = 0; i < 3; ++i) {
    if (!g_tiles_reload_requested[i]) {
      g_tiles_reload_only_if_loaded[i] = true;
    }
    g_tiles_reload_requested[i] = true;
  }
}

void tiles_request_release(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (idx >= 3) return;
  g_tiles_release_requested[idx] = true;
}

void tiles_request_release_all() {
  for (uint8_t i = 0; i < 3; ++i) {
    g_tiles_release_requested[i] = true;
  }
}

void tiles_process_reload_requests() {
  for (uint8_t i = 0; i < 3; ++i) {
    if (!g_tiles_release_requested[i]) continue;
    g_tiles_release_requested[i] = false;
    GridType grid_type = static_cast<GridType>(i);
    if (g_tiles_grids[i] && g_tiles_loaded[i]) {
      tiles_release_layout(grid_type);
    }
    return;  // nur ein Release pro Loop
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (!g_tiles_reload_requested[i]) continue;
    GridType grid_type = static_cast<GridType>(i);
    bool only_if_loaded = g_tiles_reload_only_if_loaded[i];
    if (only_if_loaded && !g_tiles_loaded[i]) {
      g_tiles_reload_requested[i] = false;
      g_tiles_reload_only_if_loaded[i] = true;
      continue;
    }
    g_tiles_reload_requested[i] = false;
    g_tiles_reload_only_if_loaded[i] = true;
    if (g_tiles_grids[i]) {
      tiles_reload_layout(grid_type);
    }
    break;  // nur ein Reload pro Loop
  }
}

/* === Update single tile (unified) === */
void tiles_update_tile(GridType grid_type, uint8_t index) {
  uint8_t idx = (uint8_t)grid_type;
  if (!g_tiles_grids[idx]) return;
  if (!g_tiles_loaded[idx]) return;
  if (index >= TILES_PER_GRID) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  const Tile& tile = config.tiles[index];
  reset_sensor_widget(grid_type, index);
  reset_switch_widget(grid_type, index);
  int row = index / 3;
  int col = index % 3;
  lv_obj_t* old_tile = g_tiles_objs[idx][index];
  lv_obj_t* new_tile = render_tile(g_tiles_grids[idx], col, row, tile, index, grid_type, g_tiles_scene_cbs[idx]);
  g_tiles_objs[idx][index] = new_tile;
  if (tile.type == TILE_SENSOR || tile.type == TILE_SWITCH) {
    String payload;
    if (get_cached_entity_payload(tile.sensor_entity.c_str(), payload)) {
      if (tile.type == TILE_SENSOR) {
        const char* unit = tile.sensor_unit.length() > 0 ? tile.sensor_unit.c_str() : nullptr;
        queue_sensor_tile_update(grid_type, index, payload.c_str(), unit);
      } else {
        queue_switch_tile_update(grid_type, index, payload.c_str());
      }
    }
  }
  if (old_tile) {
    lv_obj_del_async(old_tile);
  }
}

/* === Update sensor by entity (unified) === */
void tiles_update_sensor_by_entity(GridType grid_type, const char* entity_id, const char* value) {
  if (!entity_id || !value) return;

  cache_entity_payload(entity_id, value);
  if (!tiles_is_loaded(grid_type)) return;

  const TileGridConfig& config = getGridConfig(grid_type);

  // Find tile with matching sensor_entity
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_SENSOR && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      const char* unit = tile.sensor_unit.length() > 0 ? tile.sensor_unit.c_str() : nullptr;
      queue_sensor_tile_update(grid_type, i, value, unit);
      Serial.printf("[%s] Sensor %s@%u queued: %s %s\n", getGridName(grid_type), entity_id, i, value, unit ? unit : "");
    }
    if (tile.type == TILE_SWITCH && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      queue_switch_tile_update(grid_type, i, value);
      Serial.printf("[%s] Switch %s@%u queued: %s\n", getGridName(grid_type), entity_id, i, value);
    }
  }
}

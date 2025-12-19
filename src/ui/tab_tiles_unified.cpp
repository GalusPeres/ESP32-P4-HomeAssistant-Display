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
  tiles_reload_layout(grid_type);
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
  }

  Serial.printf("[%s] Layout neu geladen\n", getGridName(grid_type));
}

/* === Update single tile (unified) === */
void tiles_update_tile(GridType grid_type, uint8_t index) {
  uint8_t idx = (uint8_t)grid_type;
  if (!g_tiles_grids[idx]) return;
  if (index >= TILES_PER_GRID) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  reset_sensor_widget(grid_type, index);
  reset_switch_widget(grid_type, index);
  int row = index / 3;
  int col = index % 3;
  lv_obj_t* old_tile = g_tiles_objs[idx][index];
  lv_obj_t* new_tile = render_tile(g_tiles_grids[idx], col, row, config.tiles[index], index, grid_type, g_tiles_scene_cbs[idx]);
  g_tiles_objs[idx][index] = new_tile;
  if (old_tile) {
    lv_obj_del_async(old_tile);
  }
}

/* === Update sensor by entity (unified) === */
void tiles_update_sensor_by_entity(GridType grid_type, const char* entity_id, const char* value) {
  if (!entity_id || !value) return;

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

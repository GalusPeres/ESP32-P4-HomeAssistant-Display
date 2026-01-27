#include "src/ui/tab_tiles_unified.h"
#include "src/core/display_manager.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/ui/sensor_popup.h"
#include "src/network/ha_bridge_config.h"
#include "src/types/image/renderer.h"
#include <misc/cache/instance/lv_image_cache.h>
#include <Arduino.h>

/* === Layout-Konstanten === */
static const int GAP = GRID_GAP;
static const int OUTER = 0;
static const int GRID_PAD_PX = GRID_PAD;

/* === Globale State (unified for all 3 grids) === */
static lv_obj_t* g_tiles_grids[3] = {nullptr};           // [TAB0, TAB1, TAB2]
static lv_obj_t* g_tiles_roots[3] = {nullptr};
static scene_publish_cb_t g_tiles_scene_cbs[3] = {nullptr};
static lv_obj_t* g_tiles_objs[3][TILES_PER_GRID] = {nullptr};
static bool g_tiles_loaded[3] = {false, false, false};
static bool g_tiles_reload_requested[3] = {false, false, false};
static bool g_tiles_reload_only_if_loaded[3] = {true, true, true};
static bool g_tiles_release_requested[3] = {false, false, false};

static constexpr size_t kFolderCacheSize = 3;
static constexpr uint16_t kInvalidFolderId = 0xFFFF;

struct FolderCacheEntry {
  uint16_t folder_id = kInvalidFolderId;
  lv_obj_t* grid = nullptr;
  bool loaded = false;
  bool dirty = false;
  bool grid_loaded = false;
  bool widgets_valid = false;
  TileGridConfig grid_config{};
  TileWidgetCache widgets{};
  uint32_t last_used_ms = 0;
};

static FolderCacheEntry g_folder_cache[kFolderCacheSize];
static FolderCacheEntry* g_active_cache = nullptr;

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

static bool is_url_path_local(const String& value) {
  return value.startsWith("http://") || value.startsWith("https://");
}

static bool is_slideshow_token_local(const String& value) {
  return value.startsWith("__slideshow");
}

static String normalize_preview_key_local(String raw) {
  raw.trim();
  if (raw.startsWith("S:")) raw = raw.substring(2);
  if (!raw.length()) return raw;
  if (is_url_path_local(raw)) return raw;
  if (!raw.startsWith("/")) raw = "/" + raw;
  return raw;
}

static lv_obj_t* find_preview_image_child(lv_obj_t* parent) {
  if (!parent) return nullptr;
  uint32_t count = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!child) continue;
    if (lv_obj_has_flag(child, LV_OBJ_FLAG_USER_1)) return child;
    lv_obj_t* nested = find_preview_image_child(child);
    if (nested) return nested;
  }
  return nullptr;
}

static lv_obj_t* create_tiles_grid(lv_obj_t* parent);

/* === Deferred image preview loading === */
static lv_timer_t* g_preview_timer = nullptr;
static GridType g_preview_timer_grid = GridType::TAB0;
static constexpr uint32_t kPreviewDelayMs = 100;
static uint32_t g_preview_block_until_ms[3] = {0, 0, 0};

static void tiles_refresh_all_image_previews(GridType grid_type, bool only_missing);
static void hide_preview_images(GridType grid_type);

static void preview_timer_cb(lv_timer_t* timer) {
  (void)timer;
  g_preview_timer = nullptr;
  const uint8_t idx = static_cast<uint8_t>(g_preview_timer_grid);
  if (idx < 3) {
    g_preview_block_until_ms[idx] = 0;
  }
  tiles_refresh_all_image_previews(g_preview_timer_grid, true);
}

static void schedule_preview_load(GridType grid_type) {
  if (g_preview_timer) {
    lv_timer_del(g_preview_timer);
    g_preview_timer = nullptr;
  }
  const uint8_t idx = static_cast<uint8_t>(grid_type);
  if (idx < 3) {
    g_preview_block_until_ms[idx] = millis() + kPreviewDelayMs;
  }
  hide_preview_images(grid_type);
  g_preview_timer_grid = grid_type;
  g_preview_timer = lv_timer_create(preview_timer_cb, kPreviewDelayMs, nullptr);
  if (g_preview_timer) {
    lv_timer_set_repeat_count(g_preview_timer, 1);
  }
}

/* === Helper: Get grid config by type === */
static const TileGridConfig& getGridConfig(GridType type) {
  (void)type;
  return tileConfig.getActiveGrid();
}

/* === Helper: Get grid name for logging === */
static const char* getGridName(GridType type) {
  (void)type;
  return "TilesFolder";
}

static bool get_tile_layout(const Tile& tile, uint8_t& col, uint8_t& row, uint8_t& span_w, uint8_t& span_h) {
  if (tile.col >= GRID_COLS || tile.row >= GRID_ROWS) return false;
  col = tile.col;
  row = tile.row;
  span_w = tile.span_w < 1 ? 1 : tile.span_w;
  span_h = tile.span_h < 1 ? 1 : tile.span_h;
  if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
  if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;
  return true;
}

static void mark_occupied(bool occupied[GRID_ROWS][GRID_COLS], uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h) {
  for (uint8_t r = row; r < row + span_h; ++r) {
    for (uint8_t c = col; c < col + span_w; ++c) {
      if (r < GRID_ROWS && c < GRID_COLS) {
        occupied[r][c] = true;
      }
    }
  }
}

static FolderCacheEntry* find_folder_cache(uint16_t folder_id) {
  for (size_t i = 0; i < kFolderCacheSize; ++i) {
    if (g_folder_cache[i].folder_id == folder_id) {
      return &g_folder_cache[i];
    }
  }
  return nullptr;
}

static void clear_cache_entry(FolderCacheEntry& entry) {
  if (entry.grid) {
    lv_obj_del_async(entry.grid);
    entry.grid = nullptr;
  }
  entry.loaded = false;
  entry.dirty = false;
  entry.grid_loaded = false;
  entry.widgets_valid = false;
}

static void reset_cache_entry(FolderCacheEntry& entry) {
  clear_cache_entry(entry);
  entry.folder_id = kInvalidFolderId;
  entry.grid_config = TileGridConfig{};
  entry.last_used_ms = 0;
}

static FolderCacheEntry* allocate_folder_cache(uint16_t folder_id) {
  for (size_t i = 0; i < kFolderCacheSize; ++i) {
    if (g_folder_cache[i].folder_id == kInvalidFolderId) {
      g_folder_cache[i].folder_id = folder_id;
      return &g_folder_cache[i];
    }
  }

  FolderCacheEntry* lru = nullptr;
  for (size_t i = 0; i < kFolderCacheSize; ++i) {
    FolderCacheEntry* entry = &g_folder_cache[i];
    if (entry == g_active_cache) continue;
    if (!lru || entry->last_used_ms < lru->last_used_ms) {
      lru = entry;
    }
  }

  if (!lru) {
    lru = g_active_cache;
  }
  if (!lru) return nullptr;

  reset_cache_entry(*lru);
  lru->folder_id = folder_id;
  return lru;
}

static void snapshot_active_cache() {
  if (!g_active_cache) return;
  tile_renderer_snapshot_tab0(&g_active_cache->widgets);
  g_active_cache->widgets_valid = true;
  g_active_cache->last_used_ms = millis();
}

static void build_folder_cache_entry(FolderCacheEntry& entry, GridType grid_type) {
  const uint8_t idx = static_cast<uint8_t>(grid_type);
  if (!g_tiles_roots[idx]) return;

  clear_cache_entry(entry);

  if (!entry.grid_loaded || entry.dirty) {
    if (entry.folder_id == tileConfig.getActiveFolderId()) {
      entry.grid_config = tileConfig.getActiveGrid();
    } else {
      tileConfig.loadFolderGrid(entry.folder_id, entry.grid_config);
    }
    entry.grid_loaded = true;
  }
  TileGridConfig& config = entry.grid_config;

  TileWidgetCache saved{};
  tile_renderer_snapshot_tab0(&saved);

  entry.grid = create_tiles_grid(g_tiles_roots[idx]);
  if (!entry.grid) {
    tile_renderer_restore_tab0(&saved);
    return;
  }
  lv_obj_add_flag(entry.grid, LV_OBJ_FLAG_HIDDEN);

  render_tile_grid(entry.grid, config, grid_type, g_tiles_scene_cbs[idx]);

  tile_renderer_snapshot_tab0(&entry.widgets);
  entry.widgets_valid = true;
  tile_renderer_restore_tab0(&saved);

  entry.loaded = true;
  entry.dirty = false;
  entry.last_used_ms = millis();
}

/* === Create tiles grid === */
static lv_obj_t* create_tiles_grid(lv_obj_t* parent) {
  if (!parent) return nullptr;
  lv_obj_t* grid = lv_obj_create(parent);
  lv_obj_set_style_bg_color(grid, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, GRID_PAD_PX, 0);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(grid, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_column(grid, GAP, 0);
  lv_obj_set_style_pad_row(grid, GAP, 0);

  static lv_coord_t col_dsc[] = {
    GRID_CELL_W, GRID_CELL_W, GRID_CELL_W, GRID_CELL_W, GRID_CELL_W, GRID_CELL_W,
    LV_GRID_TEMPLATE_LAST
  };
  static lv_coord_t row_dsc[] = {
    GRID_CELL_H, GRID_CELL_H, GRID_CELL_H, GRID_CELL_H,
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

  g_tiles_roots[idx] = parent;
  g_tiles_grids[idx] = nullptr;
  g_tiles_loaded[idx] = false;

  if (grid_type == GridType::TAB0) {
    for (size_t i = 0; i < kFolderCacheSize; ++i) {
      reset_cache_entry(g_folder_cache[i]);
    }
    g_active_cache = nullptr;
    g_tiles_grids[idx] = create_tiles_grid(parent);
    g_tiles_loaded[idx] = (g_tiles_grids[idx] != nullptr);
    if (g_tiles_grids[idx]) {
      tiles_reload_layout(grid_type);
    }
  } else {
    g_tiles_grids[idx] = create_tiles_grid(parent);
    g_tiles_loaded[idx] = (g_tiles_grids[idx] != nullptr);
    if (g_tiles_grids[idx]) {
      tiles_reload_layout(grid_type);
    }
  }
}

/* === Reload layout (unified) === */
void tiles_reload_layout(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (grid_type == GridType::TAB0 && g_active_cache) {
    g_tiles_grids[idx] = g_active_cache->grid;
  }
  if (!g_tiles_grids[idx]) return;

  displayManager.debugFlushNext(40);

  lv_display_t* disp = lv_obj_get_display(g_tiles_grids[idx]);
  if (disp) {
    lv_display_enable_invalidation(disp, false);
  }

  reset_sensor_widgets(grid_type);
  reset_switch_widgets(grid_type);
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    g_tiles_objs[idx][i] = nullptr;
  }
  lv_obj_clean(g_tiles_grids[idx]);

  const TileGridConfig& config = getGridConfig(grid_type);
  bool occupied[GRID_ROWS][GRID_COLS] = {};
  struct TileLayout {
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t span_w = 1;
    uint8_t span_h = 1;
    bool valid = false;
  };
  TileLayout layouts[TILES_PER_GRID]{};

  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_EMPTY) continue;
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t span_w = 1;
    uint8_t span_h = 1;
    if (!get_tile_layout(tile, col, row, span_w, span_h)) continue;
    layouts[i] = {col, row, span_w, span_h, true};
    mark_occupied(occupied, col, row, span_w, span_h);
  }

  for (uint8_t r = 0; r < GRID_ROWS; ++r) {
    for (uint8_t c = 0; c < GRID_COLS; ++c) {
      if (!occupied[r][c]) {
        render_empty_tile(g_tiles_grids[idx], c, r);
      }
    }
    yield();
  }

  size_t render_count = 0;
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    if (!layouts[i].valid) continue;
    Tile layout_tile = config.tiles[i];
    layout_tile.col = layouts[i].col;
    layout_tile.row = layouts[i].row;
    layout_tile.span_w = layouts[i].span_w;
    layout_tile.span_h = layouts[i].span_h;
    g_tiles_objs[idx][i] = render_tile(g_tiles_grids[idx], layouts[i].col, layouts[i].row, layout_tile, i, grid_type, g_tiles_scene_cbs[idx]);
    if ((++render_count % GRID_COLS) == 0) {
      yield();
      delay(1);
    }
  }

  g_tiles_loaded[idx] = true;
  apply_cached_states(grid_type, config);
  if (disp) {
    lv_display_enable_invalidation(disp, true);
    lv_obj_invalidate(g_tiles_grids[idx]);
    lv_refr_now(disp);
  }
  if (grid_type == GridType::TAB0 && g_active_cache) {
    g_active_cache->grid_config = tileConfig.getActiveGrid();
    g_active_cache->grid_loaded = true;
    tile_renderer_snapshot_tab0(&g_active_cache->widgets);
    g_active_cache->widgets_valid = true;
    g_active_cache->dirty = false;
    g_active_cache->last_used_ms = millis();
  }
  Serial.printf("[%s] Layout neu geladen\n", getGridName(grid_type));
  schedule_preview_load(grid_type);
}

void tiles_release_layout(GridType grid_type) {
  uint8_t idx = (uint8_t)grid_type;
  if (!g_tiles_grids[idx] || !g_tiles_loaded[idx]) return;

  if (grid_type == GridType::TAB0 && g_active_cache) {
    clear_cache_entry(*g_active_cache);
    g_active_cache = nullptr;
    g_tiles_grids[idx] = nullptr;
    g_tiles_loaded[idx] = false;
    return;
  }

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
  for (size_t i = 0; i < kFolderCacheSize; ++i) {
    reset_cache_entry(g_folder_cache[i]);
  }
  g_active_cache = nullptr;
  g_tiles_grids[0] = nullptr;
  g_tiles_loaded[0] = false;
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

void tiles_switch_to_folder(uint16_t folder_id) {
  const uint8_t idx = static_cast<uint8_t>(GridType::TAB0);
  if (!g_tiles_roots[idx]) return;
  if (!tileConfig.folderExists(folder_id)) return;

  if (!tileConfig.setActiveFolder(folder_id)) return;

  if (!g_tiles_grids[idx]) {
    g_tiles_grids[idx] = create_tiles_grid(g_tiles_roots[idx]);
    g_tiles_loaded[idx] = (g_tiles_grids[idx] != nullptr);
  }
  if (!g_tiles_grids[idx]) return;

  lv_obj_clear_flag(g_tiles_grids[idx], LV_OBJ_FLAG_HIDDEN);
  tiles_reload_layout(GridType::TAB0);
}

void tiles_invalidate_folder(uint16_t folder_id) {
  FolderCacheEntry* entry = find_folder_cache(folder_id);
  if (!entry) return;
  entry->dirty = true;
  entry->grid_loaded = false;
  if (entry != g_active_cache) {
    clear_cache_entry(*entry);
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

static void tiles_refresh_all_image_previews(GridType grid_type, bool only_missing) {
  uint8_t idx = static_cast<uint8_t>(grid_type);
  if (!g_tiles_grids[idx]) return;
  if (!g_tiles_loaded[idx]) return;
  if (idx < 3) {
    uint32_t now = millis();
    if (g_preview_block_until_ms[idx] != 0 &&
        (int32_t)(now - g_preview_block_until_ms[idx]) < 0) {
      return;
    }
  }

  const TileGridConfig& config = getGridConfig(grid_type);
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type != TILE_IMAGE) continue;
    if (tile.sensor_display_mode == 0) continue;
    if (!tile.image_path.length()) continue;
    if (is_slideshow_token_local(tile.image_path)) continue;

    lv_obj_t* btn = g_tiles_objs[idx][i];
    if (!btn) continue;
    lv_obj_t* img = find_preview_image_child(btn);
    if (!img) continue;
    if (only_missing && lv_obj_has_flag(img, LV_OBJ_FLAG_USER_2)) continue;

    String preview_path;
    if (!image_tile_get_preview_path(tile, preview_path)) continue;

    String src = "S:" + preview_path;
    lv_image_cache_drop(src.c_str());
    lv_img_set_src(img, src.c_str());
    lv_obj_add_flag(img, LV_OBJ_FLAG_USER_2);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(img);
  }
}

void tiles_refresh_image_previews_for_key(GridType grid_type, const String& raw_key) {
  uint8_t idx = static_cast<uint8_t>(grid_type);
  if (!g_tiles_grids[idx]) return;
  if (!g_tiles_loaded[idx]) return;
  if (idx < 3) {
    uint32_t now = millis();
    if (g_preview_block_until_ms[idx] != 0 &&
        (int32_t)(now - g_preview_block_until_ms[idx]) < 0) {
      return;
    }
  }

  String key = normalize_preview_key_local(raw_key);
  if (!key.length()) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type != TILE_IMAGE) continue;
    if (tile.sensor_display_mode == 0) continue;
    if (!tile.image_path.length()) continue;
    if (is_slideshow_token_local(tile.image_path)) continue;

    String tile_key = normalize_preview_key_local(tile.image_path);
    if (tile_key != key) continue;

    String preview_path;
    if (!image_tile_get_preview_path(tile, preview_path)) continue;

    lv_obj_t* btn = g_tiles_objs[idx][i];
    if (!btn) continue;
    lv_obj_t* img = find_preview_image_child(btn);
    if (!img) continue;

    String src = "S:" + preview_path;
    lv_image_cache_drop(src.c_str());
    lv_img_set_src(img, src.c_str());
    lv_obj_add_flag(img, LV_OBJ_FLAG_USER_2);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(img);
  }
}

static void hide_preview_images(GridType grid_type) {
  uint8_t idx = static_cast<uint8_t>(grid_type);
  if (!g_tiles_grids[idx]) return;
  if (!g_tiles_loaded[idx]) return;

  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    lv_obj_t* btn = g_tiles_objs[idx][i];
    if (!btn) continue;
    lv_obj_t* img = find_preview_image_child(btn);
    if (!img) continue;
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_USER_2);
  }
}

/* === Update single tile (unified) === */
void tiles_update_tile(GridType grid_type, uint8_t index) {
  uint8_t idx = (uint8_t)grid_type;
  if (!g_tiles_grids[idx]) return;
  if (!g_tiles_loaded[idx]) return;
  if (index >= TILES_PER_GRID) return;

  // Layout changes (position/span) require a full rebuild to keep placeholders in sync.
  tiles_reload_layout(grid_type);
}

/* === Update sensor by entity (unified) === */
void tiles_update_sensor_by_entity(GridType grid_type, const char* entity_id, const char* value) {
  if (!entity_id || !value) return;

  cache_entity_payload(entity_id, value);
  if (!tiles_is_loaded(grid_type)) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  bool popup_queued = false;

  // Find tile with matching sensor_entity
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_SENSOR && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      const char* unit = tile.sensor_unit.length() > 0 ? tile.sensor_unit.c_str() : nullptr;
      queue_sensor_tile_update(grid_type, i, value, unit);
      Serial.printf("[%s] Sensor %s@%u queued: %s %s\n", getGridName(grid_type), entity_id, i, value, unit ? unit : "");
      if (!popup_queued) {
        String popup_unit = tile.sensor_unit;
        if (!popup_unit.length()) {
          popup_unit = haBridgeConfig.findSensorUnit(entity_id);
        }
        queue_sensor_popup_value(entity_id, value, popup_unit.length() ? popup_unit.c_str() : nullptr);
        popup_queued = true;
      }
    }
    if (tile.type == TILE_SWITCH && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      queue_switch_tile_update(grid_type, i, value);
      Serial.printf("[%s] Switch %s@%u queued: %s\n", getGridName(grid_type), entity_id, i, value);
    }
  }
}

#include "src/ui/tab_tiles_unified.h"
#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/weather_popup.h"
#include "src/network/ha_bridge_config.h"
#include "src/tiles/mdi_icons.h"
#include <misc/cache/instance/lv_image_cache.h>
#include <Arduino.h>
#include <cstring>
#include <deque>

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
static bool g_tiles_icon_refresh_requested = false;

static constexpr uint16_t kInvalidFolderId = 0xFFFF;

struct FolderCacheEntry {
  uint16_t folder_id = kInvalidFolderId;
  lv_obj_t* grid = nullptr;
  lv_obj_t* tile_objs[TILES_PER_GRID] = {};
  bool loaded = false;
  bool dirty = false;
  bool grid_loaded = false;
  bool widgets_valid = false;
  TileGridConfig grid_config{};
  TileWidgetCache widgets{};
  uint32_t last_used_ms = 0;
};

static std::deque<FolderCacheEntry> g_folder_cache;
static FolderCacheEntry* g_active_cache = nullptr;
static TileWidgetCache g_cache_build_saved_widgets;
static bool g_folder_switch_pending = false;
static uint16_t g_pending_folder_id = kInvalidFolderId;
static constexpr uint32_t kFolderPreloadMinHeapBytes = 384UL * 1024UL;
static constexpr uint32_t kFolderPreloadMinPsramBytes = 4UL * 1024UL * 1024UL;

static void log_folder_switch_memory(const char* phase, uint16_t folder_id) {
  Serial.printf("[Tiles] %s folder=%u | heap=%lu KB | heap_min=%lu KB | psram=%lu KB\n",
                phase ? phase : "switch",
                static_cast<unsigned>(folder_id),
                ESP.getFreeHeap() / 1024,
                ESP.getMinFreeHeap() / 1024,
                ESP.getFreePsram() / 1024);
}

static bool can_preload_more_folders() {
  const uint32_t free_heap = ESP.getFreeHeap();
  const uint32_t psram_total = ESP.getPsramSize();
  const uint32_t free_psram = ESP.getFreePsram();
  if (free_heap < kFolderPreloadMinHeapBytes) {
    return false;
  }
  if (psram_total > 0 && free_psram < kFolderPreloadMinPsramBytes) {
    return false;
  }
  return true;
}

static void build_grid_track_descriptors(lv_coord_t* dsc, uint8_t count, lv_coord_t cell_size) {
  if (!dsc) return;
  for (uint8_t i = 0; i < count; ++i) {
    dsc[i] = cell_size;
  }
  dsc[count] = LV_GRID_TEMPLATE_LAST;
}

/* === Entity-State Cache (for lazy-loaded tabs) === */
struct EntityCacheEntry {
  String entity_id;
  String payload;
  bool valid = false;
};

static constexpr size_t kEntityCacheSize = TILES_PER_GRID * 8;
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

bool tiles_get_cached_entity_payload(const char* entity_id, String& out) {
  return get_cached_entity_payload(entity_id, out);
}

void tiles_cache_entity_payload(const char* entity_id, const char* payload) {
  cache_entity_payload(entity_id, payload);
}

static bool get_cached_or_initial_payload(const Tile& tile, String& out) {
  if (!tile.sensor_entity.length()) return false;
  if (get_cached_entity_payload(tile.sensor_entity.c_str(), out)) return true;
  String initial = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
  if (!initial.length()) return false;
  out = initial;
  cache_entity_payload(tile.sensor_entity.c_str(), initial.c_str());
  return true;
}

static bool is_url_path_local(const String& value) {
  return value.startsWith("http://") || value.startsWith("https://");
}

static bool is_slideshow_token_local(const String& value) {
  return value.startsWith("__slideshow");
}

static bool is_disabled_token(const String& value) {
  if (!value.length()) return false;
  String t = value;
  t.trim();
  if (!t.length()) return true;
  t.toLowerCase();
  return t == "-" || t == "none" || t == "null" || t == "no" || t == "off";
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

static lv_obj_t* find_mdi_label_child(lv_obj_t* parent) {
  if (!parent) return nullptr;
  uint32_t count = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (!child) continue;
    const lv_font_t* font = lv_obj_get_style_text_font(child, LV_PART_MAIN);
    if (font == FONT_MDI_ICONS) return child;
    lv_obj_t* nested = find_mdi_label_child(child);
    if (nested) return nested;
  }
  return nullptr;
}

static lv_obj_t* create_tiles_grid(lv_obj_t* parent);
static void apply_cached_states(GridType grid_type, const TileGridConfig& config);

/* === Deferred image preview loading === */
static lv_timer_t* g_preview_timer = nullptr;
static GridType g_preview_timer_grid = GridType::TAB0;
static constexpr uint32_t kPreviewDelayMs = 30;
static constexpr uint32_t kPreviewStepMs = 12;
static constexpr uint8_t kPreviewLoadsPerStep = 1;
static uint32_t g_preview_block_until_ms[3] = {0, 0, 0};
static uint8_t g_preview_next_index = 0;
static bool g_preview_only_missing = true;

static void tiles_refresh_all_image_previews(GridType grid_type, bool only_missing);
static void hide_preview_images(GridType grid_type);
static void tiles_refresh_icons_for_grid(GridType grid_type);
static bool process_preview_step(GridType grid_type, bool only_missing, uint8_t max_loads);

static void stop_preview_timer() {
  if (g_preview_timer) {
    lv_timer_del(g_preview_timer);
    g_preview_timer = nullptr;
  }
}

static void preview_timer_cb(lv_timer_t* timer) {
  (void)timer;
  const uint8_t idx = static_cast<uint8_t>(g_preview_timer_grid);
  if (idx >= 3 || !g_tiles_grids[idx] || !g_tiles_loaded[idx]) {
    if (idx < 3) g_preview_block_until_ms[idx] = 0;
    stop_preview_timer();
    return;
  }

  const uint32_t now = millis();
  if (g_preview_block_until_ms[idx] != 0 &&
      (int32_t)(now - g_preview_block_until_ms[idx]) < 0) {
    return;
  }
  g_preview_block_until_ms[idx] = 0;

  if (process_preview_step(g_preview_timer_grid, g_preview_only_missing, kPreviewLoadsPerStep)) {
    stop_preview_timer();
  }
}

static void schedule_preview_load(GridType grid_type) {
  stop_preview_timer();
  const uint8_t idx = static_cast<uint8_t>(grid_type);
  if (idx < 3) {
    g_preview_block_until_ms[idx] = millis() + kPreviewDelayMs;
  }
  hide_preview_images(grid_type);
  g_preview_timer_grid = grid_type;
  g_preview_only_missing = true;
  g_preview_next_index = 0;
  g_preview_timer = lv_timer_create(preview_timer_cb, kPreviewStepMs, nullptr);
  if (!g_preview_timer) {
    if (idx < 3) g_preview_block_until_ms[idx] = 0;
    tiles_refresh_all_image_previews(grid_type, true);
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
  for (auto& entry : g_folder_cache) {
    if (entry.folder_id == folder_id) {
      return &entry;
    }
  }
  return nullptr;
}

static void clear_cache_entry(FolderCacheEntry& entry) {
  if (entry.grid) {
    lv_obj_del_async(entry.grid);
    entry.grid = nullptr;
  }
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    entry.tile_objs[i] = nullptr;
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
  if (FolderCacheEntry* existing = find_folder_cache(folder_id)) {
    return existing;
  }
  g_folder_cache.emplace_back();
  g_folder_cache.back().folder_id = folder_id;
  return &g_folder_cache.back();
}

static void rebuild_folder_cache_index() {
  for (auto& entry : g_folder_cache) {
    reset_cache_entry(entry);
  }
  g_folder_cache.clear();
  for (const auto& folder : tileConfig.getFolders()) {
    g_folder_cache.emplace_back();
    g_folder_cache.back().folder_id = folder.id;
  }
}

static void snapshot_active_cache() {
  if (!g_active_cache) return;
  tile_renderer_snapshot_tab0(&g_active_cache->widgets);
  memcpy(g_active_cache->tile_objs, g_tiles_objs[static_cast<uint8_t>(GridType::TAB0)], sizeof(g_active_cache->tile_objs));
  g_active_cache->widgets_valid = true;
  g_active_cache->grid = g_tiles_grids[static_cast<uint8_t>(GridType::TAB0)];
  g_active_cache->loaded = (g_active_cache->grid != nullptr) && g_tiles_loaded[static_cast<uint8_t>(GridType::TAB0)];
  g_active_cache->grid_config = tileConfig.getActiveGrid();
  g_active_cache->grid_loaded = true;
  g_active_cache->last_used_ms = millis();
}

static void restore_active_cache(FolderCacheEntry& entry) {
  const uint8_t idx = static_cast<uint8_t>(GridType::TAB0);
  g_active_cache = &entry;
  g_tiles_grids[idx] = entry.grid;
  g_tiles_loaded[idx] = entry.loaded && (entry.grid != nullptr);
  memcpy(g_tiles_objs[idx], entry.tile_objs, sizeof(entry.tile_objs));
  if (entry.widgets_valid) {
    tile_renderer_restore_tab0(&entry.widgets);
  }
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

  g_cache_build_saved_widgets = {};
  tile_renderer_snapshot_tab0(&g_cache_build_saved_widgets);

  entry.grid = create_tiles_grid(g_tiles_roots[idx]);
  if (!entry.grid) {
    tile_renderer_restore_tab0(&g_cache_build_saved_widgets);
    return;
  }
  lv_obj_add_flag(entry.grid, LV_OBJ_FLAG_HIDDEN);

  render_tile_grid(entry.grid, config, grid_type, g_tiles_scene_cbs[idx], entry.tile_objs);

  // Warm hidden folder caches with the latest known entity states so the
  // first visible folder switch behaves much closer to a second open.
  apply_cached_states(grid_type, config);
  process_sensor_update_queue();
  process_switch_update_queue();
  process_weather_update_queue();
  process_media_update_queue();
  lv_obj_update_layout(entry.grid);

  tile_renderer_snapshot_tab0(&entry.widgets);
  entry.widgets_valid = true;
  tile_renderer_restore_tab0(&g_cache_build_saved_widgets);

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

  static lv_coord_t col_dsc[GRID_COLS + 1];
  static lv_coord_t row_dsc[GRID_ROWS + 1];
  static bool dsc_ready = false;
  if (!dsc_ready) {
    build_grid_track_descriptors(col_dsc, GRID_COLS, GRID_CELL_W);
    build_grid_track_descriptors(row_dsc, GRID_ROWS, GRID_CELL_H);
    dsc_ready = true;
  }
  lv_obj_set_layout(grid, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
  return grid;
}

static void apply_cached_states(GridType grid_type, const TileGridConfig& config) {
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type != TILE_SENSOR && tile.type != TILE_SWITCH &&
        tile.type != TILE_WEATHER && tile.type != TILE_ENERGY &&
        tile.type != TILE_MEDIA) continue;
    if (tile.sensor_entity.length() == 0) continue;

    String payload;
    if (!get_cached_or_initial_payload(tile, payload)) continue;

    if (tile.type == TILE_SENSOR || tile.type == TILE_ENERGY) {
      String unit = tile.sensor_unit;
      if (is_disabled_token(unit)) {
        unit = "";
      } else if (!unit.length()) {
        unit = haBridgeConfig.findSensorUnit(tile.sensor_entity);
      }
      const char* unit_cstr = unit.length() > 0 ? unit.c_str() : nullptr;
      queue_sensor_tile_update(grid_type, i, payload.c_str(), unit_cstr);
    } else if (tile.type == TILE_SWITCH) {
      queue_switch_tile_update(grid_type, i, payload.c_str());
    } else if (tile.type == TILE_WEATHER) {
      queue_weather_tile_update(grid_type, i, payload.c_str());
    } else if (tile.type == TILE_MEDIA) {
      queue_media_tile_update(grid_type, i, payload.c_str());
    }
  }
}

static void apply_cached_state_for_index(GridType grid_type, const TileGridConfig& config, uint8_t index) {
  if (index >= TILES_PER_GRID) return;
  const Tile& tile = config.tiles[index];
  if (tile.type != TILE_SENSOR && tile.type != TILE_SWITCH &&
      tile.type != TILE_WEATHER && tile.type != TILE_ENERGY &&
      tile.type != TILE_MEDIA) return;
  if (tile.sensor_entity.length() == 0) return;

  String payload;
  if (!get_cached_or_initial_payload(tile, payload)) return;

  if (tile.type == TILE_SENSOR || tile.type == TILE_ENERGY) {
    String unit = tile.sensor_unit;
    if (is_disabled_token(unit)) {
      unit = "";
    } else if (!unit.length()) {
      unit = haBridgeConfig.findSensorUnit(tile.sensor_entity);
    }
    const char* unit_cstr = unit.length() > 0 ? unit.c_str() : nullptr;
    queue_sensor_tile_update(grid_type, index, payload.c_str(), unit_cstr);
  } else if (tile.type == TILE_SWITCH) {
    queue_switch_tile_update(grid_type, index, payload.c_str());
  } else if (tile.type == TILE_WEATHER) {
    queue_weather_tile_update(grid_type, index, payload.c_str());
  } else if (tile.type == TILE_MEDIA) {
    queue_media_tile_update(grid_type, index, payload.c_str());
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
    const uint32_t preload_heap_before = ESP.getFreeHeap();
    const uint32_t preload_psram_before = ESP.getFreePsram();
    size_t preloaded_folder_count = 0;
    rebuild_folder_cache_index();
    g_active_cache = allocate_folder_cache(tileConfig.getActiveFolderId());
    if (g_active_cache) {
      build_folder_cache_entry(*g_active_cache, grid_type);
      restore_active_cache(*g_active_cache);
      if (g_tiles_grids[idx]) {
        lv_obj_clear_flag(g_tiles_grids[idx], LV_OBJ_FLAG_HIDDEN);
        apply_cached_states(grid_type, g_active_cache->grid_config);
        process_sensor_update_queue();
        process_switch_update_queue();
        process_weather_update_queue();
        process_media_update_queue();
        g_active_cache->last_used_ms = millis();
        preloaded_folder_count = 1;
      }
    }
    for (auto& entry : g_folder_cache) {
      if (&entry == g_active_cache) continue;
      if (!can_preload_more_folders()) {
        Serial.printf("[Tiles] TAB0 folder preload stopped: heap=%lu KB, psram=%lu KB\n",
                      ESP.getFreeHeap() / 1024, ESP.getFreePsram() / 1024);
        break;
      }
      build_folder_cache_entry(entry, grid_type);
      if (entry.loaded) {
        ++preloaded_folder_count;
      }
    }
    Serial.printf("[Tiles] TAB0 folder preload cached %u/%u folders (heap %lu -> %lu KB, psram %lu -> %lu KB)\n",
                  static_cast<unsigned>(preloaded_folder_count),
                  static_cast<unsigned>(g_folder_cache.size()),
                  preload_heap_before / 1024, ESP.getFreeHeap() / 1024,
                  preload_psram_before / 1024, ESP.getFreePsram() / 1024);
    if (g_tiles_grids[idx]) {
      schedule_preview_load(grid_type);
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

  // displayManager.debugFlushNext(40);

  lv_display_t* disp = lv_obj_get_display(g_tiles_grids[idx]);
  if (disp) {
    lv_display_enable_invalidation(disp, false);
  }

  reset_sensor_widgets(grid_type);
  reset_switch_widgets(grid_type);
  reset_weather_widgets(grid_type);
  reset_weather_widgets(grid_type);
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
  process_sensor_update_queue();
  process_switch_update_queue();
  process_weather_update_queue();
  process_media_update_queue();
  if (disp) {
    lv_display_enable_invalidation(disp, true);
    lv_obj_invalidate(g_tiles_grids[idx]);
    lv_refr_now(disp);
  }
  if (grid_type == GridType::TAB0 && g_active_cache) {
    g_active_cache->grid = g_tiles_grids[idx];
    g_active_cache->loaded = g_tiles_loaded[idx];
    g_active_cache->grid_config = tileConfig.getActiveGrid();
    g_active_cache->grid_loaded = true;
    tile_renderer_snapshot_tab0(&g_active_cache->widgets);
    memcpy(g_active_cache->tile_objs, g_tiles_objs[idx], sizeof(g_active_cache->tile_objs));
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
    reset_media_widgets(grid_type);
    clear_cache_entry(*g_active_cache);
    g_active_cache = nullptr;
    g_tiles_grids[idx] = nullptr;
    g_tiles_loaded[idx] = false;
    return;
  }

  reset_sensor_widgets(grid_type);
  reset_switch_widgets(grid_type);
  reset_media_widgets(grid_type);
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    g_tiles_objs[idx][i] = nullptr;
  }
  lv_obj_clean(g_tiles_grids[idx]);
  g_tiles_loaded[idx] = false;

  Serial.printf("[%s] Layout freigegeben\n", getGridName(grid_type));
}

void tiles_release_all() {
  for (auto& entry : g_folder_cache) {
    reset_cache_entry(entry);
  }
  g_folder_cache.clear();
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

void tiles_request_icon_refresh() {
  g_tiles_icon_refresh_requested = true;
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

  // Wechsel nicht im LVGL-Event-Callback blockieren.
  stop_preview_timer();
  g_pending_folder_id = folder_id;
  g_folder_switch_pending = true;
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
  bool did_reload = false;

  if (g_folder_switch_pending) {
    g_folder_switch_pending = false;
    const uint16_t folder_id = g_pending_folder_id;
    g_pending_folder_id = kInvalidFolderId;
    const uint8_t idx = static_cast<uint8_t>(GridType::TAB0);

    if (g_tiles_roots[idx] && tileConfig.folderExists(folder_id)) {
      snapshot_active_cache();

      FolderCacheEntry* target = find_folder_cache(folder_id);
      if (!target) {
        target = allocate_folder_cache(folder_id);
      }
      if (!target) {
        return;
      }

      FolderCacheEntry* previous = g_active_cache;
      const bool can_reuse = target->grid && target->loaded && target->widgets_valid &&
                             target->grid_loaded && !target->dirty;

      if (can_reuse) {
        if (!tileConfig.setActiveFolderCached(folder_id, target->grid_config)) {
          return;
        }
        if (previous && previous != target && previous->grid) {
          lv_obj_add_flag(previous->grid, LV_OBJ_FLAG_HIDDEN);
        }
        restore_active_cache(*target);
        lv_obj_clear_flag(target->grid, LV_OBJ_FLAG_HIDDEN);
        apply_cached_states(GridType::TAB0, target->grid_config);
        process_sensor_update_queue();
        process_switch_update_queue();
        process_weather_update_queue();
        process_media_update_queue();
        lv_display_t* disp = lv_obj_get_display(target->grid);
        if (disp) {
          lv_obj_invalidate(target->grid);
          lv_refr_now(disp);
        }
        target->last_used_ms = millis();
        schedule_preview_load(GridType::TAB0);
        log_folder_switch_memory("folder-switch-cached", folder_id);
        return;
      }

      build_folder_cache_entry(*target, GridType::TAB0);
      if (!target->grid || !target->loaded || !target->widgets_valid) {
        if (previous && previous->grid) {
          restore_active_cache(*previous);
          lv_obj_clear_flag(previous->grid, LV_OBJ_FLAG_HIDDEN);
        }
        return;
      }
      if (!tileConfig.setActiveFolderCached(folder_id, target->grid_config)) {
        if (target->grid) {
          lv_obj_add_flag(target->grid, LV_OBJ_FLAG_HIDDEN);
        }
        if (previous && previous->grid) {
          restore_active_cache(*previous);
          lv_obj_clear_flag(previous->grid, LV_OBJ_FLAG_HIDDEN);
        }
        return;
      }

      if (previous && previous != target && previous->grid) {
        lv_obj_add_flag(previous->grid, LV_OBJ_FLAG_HIDDEN);
      }

      restore_active_cache(*target);
      if (target->grid) {
        lv_obj_clear_flag(target->grid, LV_OBJ_FLAG_HIDDEN);
        apply_cached_states(GridType::TAB0, target->grid_config);
        process_sensor_update_queue();
        process_switch_update_queue();
        process_weather_update_queue();
        process_media_update_queue();
        lv_display_t* disp = lv_obj_get_display(target->grid);
        if (disp) {
          lv_obj_invalidate(target->grid);
          lv_refr_now(disp);
        }
        target->last_used_ms = millis();
        schedule_preview_load(GridType::TAB0);
        log_folder_switch_memory("folder-switch-built", folder_id);
      } else if (previous && previous->grid) {
        restore_active_cache(*previous);
        lv_obj_clear_flag(previous->grid, LV_OBJ_FLAG_HIDDEN);
      }
    }
    return;
  }

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
    did_reload = true;
    break;  // nur ein Reload pro Loop
  }

  if (g_tiles_icon_refresh_requested && !did_reload) {
    g_tiles_icon_refresh_requested = false;
    for (uint8_t i = 0; i < 3; ++i) {
      GridType grid_type = static_cast<GridType>(i);
      if (!g_tiles_grids[i] || !g_tiles_loaded[i]) continue;
      tiles_refresh_icons_for_grid(grid_type);
    }
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

  g_preview_next_index = 0;
  while (!process_preview_step(grid_type, only_missing, TILES_PER_GRID)) {
    // continue until all previews in this grid are processed
  }
}

static bool apply_preview_for_tile(GridType, uint8_t, bool, bool) {
  return false;
}

static bool process_preview_step(GridType grid_type, bool only_missing, uint8_t max_loads) {
  if (max_loads < 1) max_loads = 1;
  uint8_t loaded = 0;
  while (g_preview_next_index < TILES_PER_GRID) {
    if (apply_preview_for_tile(grid_type, g_preview_next_index, only_missing, false)) {
      ++loaded;
      ++g_preview_next_index;
      if (loaded >= max_loads) {
        return false;
      }
      continue;
    }
    ++g_preview_next_index;
  }
  return true;
}

void tiles_refresh_image_previews_for_key(GridType, const String&) {
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

static void rebuild_tile_at_index(GridType grid_type, uint8_t index) {
  uint8_t idx = static_cast<uint8_t>(grid_type);
  if (!g_tiles_grids[idx] || !g_tiles_loaded[idx]) return;
  if (index >= TILES_PER_GRID) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  const Tile& tile = config.tiles[index];
  if (tile.type == TILE_EMPTY) return;

  uint8_t col = 0;
  uint8_t row = 0;
  uint8_t span_w = 1;
  uint8_t span_h = 1;
  if (!get_tile_layout(tile, col, row, span_w, span_h)) return;

  if (g_tiles_objs[idx][index]) {
    lv_obj_del(g_tiles_objs[idx][index]);
    g_tiles_objs[idx][index] = nullptr;
  }
  reset_sensor_widget(grid_type, index);
  reset_switch_widget(grid_type, index);
  reset_weather_widget(grid_type, index);

  Tile layout_tile = tile;
  layout_tile.col = col;
  layout_tile.row = row;
  layout_tile.span_w = span_w;
  layout_tile.span_h = span_h;
  g_tiles_objs[idx][index] = render_tile(g_tiles_grids[idx], col, row, layout_tile, index, grid_type, g_tiles_scene_cbs[idx]);

  apply_cached_state_for_index(grid_type, config, index);
}

static void tiles_refresh_icons_for_grid(GridType grid_type) {
  uint8_t idx = static_cast<uint8_t>(grid_type);
  if (!g_tiles_grids[idx] || !g_tiles_loaded[idx]) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type != TILE_SENSOR && tile.type != TILE_SWITCH &&
        tile.type != TILE_SCENE && tile.type != TILE_ENERGY &&
        tile.type != TILE_MEDIA) continue;
    lv_obj_t* tile_obj = g_tiles_objs[idx][i];
    if (!tile_obj) continue;

    String icon_name = tile.icon_name;
    bool icon_disabled = isMdiIconDisabled(icon_name);
    icon_name = normalizeMdiIconName(icon_name);
    if (!icon_disabled && !icon_name.length()) {
      if (tile.type == TILE_SCENE) {
        if (tile.scene_alias.length()) {
          String scene_entity = haBridgeConfig.findSceneEntity(tile.scene_alias);
          if (scene_entity.length()) {
            icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(scene_entity));
          }
        }
      } else if (tile.sensor_entity.length()) {
        icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
      }
    }

    String iconChar;
    if (icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
      iconChar = getMdiChar(icon_name);
    }

    lv_obj_t* icon_lbl = find_mdi_label_child(tile_obj);
    if (!icon_lbl) {
      if (!icon_disabled && iconChar.length()) {
        rebuild_tile_at_index(grid_type, i);
      }
      continue;
    }

    if (icon_disabled || !icon_name.length()) {
      lv_obj_add_flag(icon_lbl, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    if (!iconChar.length()) {
      lv_obj_add_flag(icon_lbl, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    lv_label_set_text(icon_lbl, iconChar.c_str());
    lv_obj_clear_flag(icon_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(icon_lbl);
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
  // Im Sleep nur den letzten Zustand cachen; UI-Queues bleiben leer.
  if (powerManager.isInSleep()) return;
  if (!tiles_is_loaded(grid_type)) return;

  const TileGridConfig& config = getGridConfig(grid_type);
  bool popup_queued = false;

  // Find tile with matching sensor_entity
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_SENSOR && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      String unit = tile.sensor_unit;
      if (is_disabled_token(unit)) {
        unit = "";
      } else if (!unit.length()) {
        unit = haBridgeConfig.findSensorUnit(entity_id);
      }
      const char* unit_cstr = unit.length() > 0 ? unit.c_str() : nullptr;
      queue_sensor_tile_update(grid_type, i, value, unit_cstr);
      Serial.printf("[%s] Sensor %s@%u queued: %s %s\n", getGridName(grid_type), entity_id, i, value, unit_cstr ? unit_cstr : "");
      if (!popup_queued) {
        queue_sensor_popup_value(entity_id, value, unit.length() ? unit.c_str() : nullptr, tile.sensor_decimals);
        popup_queued = true;
      }
    }
    if (tile.type == TILE_SWITCH && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      queue_switch_tile_update(grid_type, i, value);
      Serial.printf("[%s] Switch %s@%u queued: %s\n", getGridName(grid_type), entity_id, i, value);
    }
    if (tile.type == TILE_MEDIA && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      queue_media_tile_update(grid_type, i, value);
      Serial.printf("[%s] Media %s@%u queued\n", getGridName(grid_type), entity_id, i);
    }
  }
}

void tiles_update_weather_by_entity(GridType grid_type, const char* entity_id, const char* payload) {
  if (!entity_id || !payload) return;

  cache_entity_payload(entity_id, payload);
  // Im Sleep nur den letzten Zustand cachen; UI-Queues bleiben leer.
  if (powerManager.isInSleep()) return;
  if (!tiles_is_loaded(grid_type)) return;

  const TileGridConfig& config = getGridConfig(grid_type);

  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_WEATHER && tile.sensor_entity.equalsIgnoreCase(entity_id)) {
      queue_weather_tile_update(grid_type, i, payload);
      queue_weather_popup_payload(entity_id, payload);
      Serial.printf("[%s] Weather %s@%u queued\n", getGridName(grid_type), entity_id, i);
    }
  }
}

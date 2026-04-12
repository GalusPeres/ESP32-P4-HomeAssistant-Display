#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include <Arduino.h>
#include <vector>

#include "src/devices/device.h"

static constexpr uint8_t GRID_COLS = Device::kGridCols;
static constexpr uint8_t GRID_ROWS = Device::kGridRows;
static constexpr size_t TILES_PER_GRID = GRID_COLS * GRID_ROWS;

static constexpr int GRID_GAP = Device::kGridGap;
static constexpr int GRID_PAD = Device::kGridPad;
static constexpr int GRID_CELL_W = Device::kGridCellW;
static constexpr int GRID_CELL_H = Device::kGridCellH;

enum TileType : uint8_t {
  TILE_EMPTY = 0,
  TILE_SENSOR = 1,
  TILE_SCENE = 2,
  TILE_KEY = 3,
  TILE_FOLDER = 4,
  TILE_SWITCH = 5,
  TILE_IMAGE = 6,
  TILE_SETTINGS = 7,
  TILE_BACK = 8,
  TILE_CLOCK = 9,
  TILE_TEXT = 10,
  TILE_COUNTER = 11,
  TILE_WEATHER = 12,
  TILE_RADAR = 13,
  TILE_ENERGY = 14
};

enum TilePopupOpenMode : uint8_t {
  TILE_POPUP_OPEN_LONG_PRESS = 0,
  TILE_POPUP_OPEN_SHORT_PRESS = 1
};

enum SwitchPopupOpenModeStorage : uint8_t {
  TILE_SWITCH_POPUP_MODE_LEGACY = 0,
  TILE_SWITCH_POPUP_MODE_SHORT = 1,
  TILE_SWITCH_POPUP_MODE_LONG = 2
};

struct Tile {
  TileType type;
  String title;
  String icon_name;
  uint32_t bg_color;

  uint8_t col;
  uint8_t row;
  uint8_t span_w;
  uint8_t span_h;

  String sensor_entity;
  String sensor_unit;
  uint8_t sensor_decimals;
  uint8_t sensor_value_font;
  uint8_t sensor_display_mode;
  int32_t sensor_gauge_min;
  int32_t sensor_gauge_max;
  uint16_t sensor_gauge_arc;
  uint16_t sensor_gauge_size;
  int16_t sensor_gauge_y_offset;
  int16_t sensor_value_y_offset;
  uint16_t sensor_graph_height;
  uint8_t popup_open_mode;

  String scene_alias;

  String key_macro;
  uint8_t key_code;
  uint8_t key_modifier;

  String image_path;
  uint16_t image_slideshow_sec;

  Tile()
      : type(TILE_EMPTY),
        bg_color(0),
        col(0),
        row(0),
        span_w(1),
        span_h(1),
        sensor_decimals(0xFF),
        sensor_value_font(0),
        sensor_display_mode(0),
        sensor_gauge_min(0),
        sensor_gauge_max(100),
        sensor_gauge_arc(100),
        sensor_gauge_size(350),
        sensor_gauge_y_offset(12),
        sensor_value_y_offset(0),
        sensor_graph_height(60),
        popup_open_mode(TILE_POPUP_OPEN_LONG_PRESS),
        key_code(0),
        key_modifier(0),
        image_slideshow_sec(10) {}
};

static inline uint8_t getTilePopupOpenMode(const Tile& tile) {
  if (tile.type == TILE_SWITCH) {
    return (tile.key_code == TILE_SWITCH_POPUP_MODE_LONG)
               ? TILE_POPUP_OPEN_LONG_PRESS
               : TILE_POPUP_OPEN_SHORT_PRESS;
  }
  if (tile.type != TILE_SENSOR && tile.type != TILE_WEATHER && tile.type != TILE_ENERGY) {
    return TILE_POPUP_OPEN_LONG_PRESS;
  }
  return (tile.popup_open_mode == TILE_POPUP_OPEN_SHORT_PRESS)
             ? TILE_POPUP_OPEN_SHORT_PRESS
             : TILE_POPUP_OPEN_LONG_PRESS;
}

static inline void setTilePopupOpenMode(Tile& tile, uint8_t mode) {
  if (tile.type == TILE_SWITCH) {
    const uint8_t normalized =
        (mode == TILE_POPUP_OPEN_SHORT_PRESS)
            ? TILE_POPUP_OPEN_SHORT_PRESS
            : TILE_POPUP_OPEN_LONG_PRESS;
    tile.popup_open_mode = normalized;
    // Switch tiles previously had no configurable popup mode. Use the otherwise
    // unused key_code slot to distinguish legacy tiles (default to short press)
    // from an explicitly saved long-press configuration.
    tile.key_code = (normalized == TILE_POPUP_OPEN_SHORT_PRESS)
                        ? TILE_SWITCH_POPUP_MODE_SHORT
                        : TILE_SWITCH_POPUP_MODE_LONG;
    tile.key_modifier = 0;
    return;
  }
  if (tile.type != TILE_SENSOR && tile.type != TILE_WEATHER && tile.type != TILE_ENERGY) return;
  tile.popup_open_mode = (mode == TILE_POPUP_OPEN_SHORT_PRESS)
                             ? TILE_POPUP_OPEN_SHORT_PRESS
                             : TILE_POPUP_OPEN_LONG_PRESS;
}

struct TileGridConfig {
  Tile tiles[TILES_PER_GRID];
};

struct FolderEntry {
  uint16_t id;
  uint16_t parent_id;
  char name[32];
  char icon_name[32];
};

class TileConfig {
public:
  TileConfig();

  bool load();
  bool loadFolderGrid(uint16_t folder_id, TileGridConfig& out);
  bool saveFolderGrid(uint16_t folder_id, const TileGridConfig& grid);

  bool setActiveFolder(uint16_t folder_id);
  bool setActiveFolderCached(uint16_t folder_id, const TileGridConfig& grid);
  uint16_t getActiveFolderId() const { return active_folder_id; }
  const TileGridConfig& getActiveGrid() const { return active_grid; }
  TileGridConfig& getActiveGrid() { return active_grid; }

  const FolderEntry* getFolder(uint16_t folder_id) const;
  uint16_t getFolderParent(uint16_t folder_id) const;
  const std::vector<FolderEntry>& getFolders() const { return folders; }
  bool folderExists(uint16_t folder_id) const;
  bool createFolder(uint16_t parent_id, const String& name, const String& icon, uint16_t& out_id);
  bool updateFolder(uint16_t folder_id, const String& name, const String& icon);
  bool deleteFolder(uint16_t folder_id);

private:
  static constexpr uint16_t kRootFolderId = 0;
  static constexpr uint16_t kInvalidFolderId = 0xFFFF;

  TileGridConfig active_grid;
  uint16_t active_folder_id = kRootFolderId;
  std::vector<FolderEntry> folders;

  bool loadFolders();
  bool saveFolders() const;
  bool loadGrid(uint16_t folder_id, TileGridConfig& grid);
  bool saveGrid(uint16_t folder_id, const TileGridConfig& grid);
  uint16_t nextFolderId() const;
  void ensureRootFolder();
  bool ensureSettingsTile(TileGridConfig& grid);
  bool ensureBackTile(uint16_t folder_id, TileGridConfig& grid);
};

extern TileConfig tileConfig;

#endif // TILE_CONFIG_H

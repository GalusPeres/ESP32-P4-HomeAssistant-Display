#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include <Arduino.h>
#include <vector>

// Grid Layout: 6 columns x 4 rows = 24 tiles max
static constexpr uint8_t GRID_COLS = 6;
static constexpr uint8_t GRID_ROWS = 4;
static constexpr size_t TILES_PER_GRID = GRID_COLS * GRID_ROWS;

// Grid Dimensions (pixels)
static constexpr int GRID_GAP = 24;       // Gap between tiles
static constexpr int GRID_PAD = 16;       // Edge padding
static constexpr int GRID_CELL_W = 188;   // Single cell width
static constexpr int GRID_CELL_H = 154;   // Single cell height

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
  TILE_CLOCK = 9
};

struct Tile {
  TileType type;
  String title;              // Für alle Typen
  String icon_name;          // MDI Icon Name (z.B. "home", "thermometer")
  uint32_t bg_color;         // Hintergrundfarbe (0 = Standard)

  // Grid Position & Size
  uint8_t col;               // Column (0-5)
  uint8_t row;               // Row (0-3)
  uint8_t span_w;            // Width in cells (1-6)
  uint8_t span_h;            // Height in cells (1-4)

  // Sensor-spezifisch
  String sensor_entity;      // HA Entity ID (z.B. "sensor.temperature")
  String sensor_unit;        // Einheit (z.B. "°C")
  uint8_t sensor_decimals;   // Nachkommastellen (0xFF = unverändert)
  uint8_t sensor_value_font; // 0=Standard, 1=20, 2=24
  uint8_t sensor_display_mode; // 0=none, 1=gauge, 2=graph
  int32_t sensor_gauge_min;  // Gauge-Min
  int32_t sensor_gauge_max;  // Gauge-Max
  uint16_t sensor_gauge_arc; // Gauge Bogengrad (90-359, Default: 100)
  uint16_t sensor_gauge_size; // Gauge Größe in Pixel (100-800, Default: 350)
  int16_t sensor_gauge_y_offset; // Gauge Y-Offset (-100 bis 200, Default: 12)
  int16_t sensor_value_y_offset; // Wert Y-Offset (-100 bis 200, Default: 0)
  uint16_t sensor_graph_height;  // Graph Höhe in Pixel (20-200, Default: 60)

  // Scene-spezifisch
  String scene_alias;        // HA Scene Alias

  // Key-spezifisch
  String key_macro;          // Makro-String (z.B. "ctrl+g")
  uint8_t key_code;          // USB HID Scancode
  uint8_t key_modifier;      // Modifier bits (CTRL=0x01, SHIFT=0x02, ALT=0x04)

  // Image-spezifisch
  String image_path;         // SD-Karten Pfad (z.B. "/bild.bin")
  uint16_t image_slideshow_sec;  // Diashow-Intervall in Sekunden (Default 10)

  Tile()
      : type(TILE_EMPTY),
        bg_color(0),
        col(0),
        row(0),
        span_w(1),
        span_h(1),
        sensor_decimals(0xFF),  // 0xFF = keine Rundung, Originalwert anzeigen
        sensor_value_font(0),
        sensor_display_mode(0),
        sensor_gauge_min(0),
        sensor_gauge_max(100),
        sensor_gauge_arc(100),
        sensor_gauge_size(350),
        sensor_gauge_y_offset(12),
        sensor_value_y_offset(0),
        sensor_graph_height(60),
        key_code(0),
        key_modifier(0),
        image_slideshow_sec(10) {}
};

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

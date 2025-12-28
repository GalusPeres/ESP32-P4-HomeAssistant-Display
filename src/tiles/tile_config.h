#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include <Arduino.h>

static constexpr size_t TILES_PER_GRID = 12;

enum TileType : uint8_t {
  TILE_EMPTY = 0,
  TILE_SENSOR = 1,
  TILE_SCENE = 2,
  TILE_KEY = 3,
  TILE_NAVIGATE = 4,
  TILE_SWITCH = 5,
  TILE_IMAGE = 6
};

struct Tile {
  TileType type;
  String title;              // Für alle Typen
  String icon_name;          // MDI Icon Name (z.B. "home", "thermometer")
  uint32_t bg_color;         // Hintergrundfarbe (0 = Standard)

  // Sensor-spezifisch
  String sensor_entity;      // HA Entity ID (z.B. "sensor.temperature")
  String sensor_unit;        // Einheit (z.B. "°C")
  uint8_t sensor_decimals;   // Nachkommastellen (0xFF = unverändert)
  uint8_t sensor_value_font; // 0=Standard, 1=20, 2=24

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
        sensor_decimals(0xFF),  // 0xFF = keine Rundung, Originalwert anzeigen
        sensor_value_font(0),
        key_code(0),
        key_modifier(0),
        image_slideshow_sec(10) {}
};

struct TileGridConfig {
  Tile tiles[TILES_PER_GRID];
};

struct TabConfig {
  char name[32];      // Custom tab name (empty = use default)
  char icon_name[32]; // MDI Icon Name (empty = no icon)

  TabConfig() {
    name[0] = '\0';      // Empty = use default
    icon_name[0] = '\0'; // Empty = no icon
  }
};

class TileConfig {
public:
  TileConfig();

  bool load();
  bool save(const TileGridConfig& tab0, const TileGridConfig& tab1, const TileGridConfig& tab2);
  bool saveSingleGrid(const char* grid_name, const TileGridConfig& grid);

  const TileGridConfig& getTab0Grid() const { return tab0_grid; }
  const TileGridConfig& getTab1Grid() const { return tab1_grid; }
  const TileGridConfig& getTab2Grid() const { return tab2_grid; }

  TileGridConfig& getTab0Grid() { return tab0_grid; }
  TileGridConfig& getTab1Grid() { return tab1_grid; }
  TileGridConfig& getTab2Grid() { return tab2_grid; }

  // Tab names (configurable via web interface)
  const char* getTabName(uint8_t tab_index) const;
  void setTabName(uint8_t tab_index, const char* name);

  // Tab icons (configurable via web interface)
  const char* getTabIcon(uint8_t tab_index) const;
  void setTabIcon(uint8_t tab_index, const char* icon_name);

  bool loadTabNames();
  bool saveTabNames();

private:
  TileGridConfig tab0_grid;
  TileGridConfig tab1_grid;
  TileGridConfig tab2_grid;

  TabConfig tab_configs[4];  // [0]=Tab0, [1]=Tab1, [2]=Tab2, [3]=Tab3(Settings)

  bool loadGrid(const char* prefix, TileGridConfig& grid);
  bool saveGrid(const char* prefix, const TileGridConfig& grid);
};

extern TileConfig tileConfig;

#endif // TILE_CONFIG_H

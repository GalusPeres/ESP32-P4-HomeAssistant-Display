#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include <Arduino.h>

static constexpr size_t TILES_PER_GRID = 12;

enum TileType : uint8_t {
  TILE_EMPTY = 0,
  TILE_SENSOR = 1,
  TILE_SCENE = 2,
  TILE_KEY = 3
};

struct Tile {
  TileType type;
  String title;              // Für alle Typen
  uint32_t bg_color;         // Hintergrundfarbe (0 = Standard)

  // Sensor-spezifisch
  String sensor_entity;      // HA Entity ID (z.B. "sensor.temperature")
  String sensor_unit;        // Einheit (z.B. "°C")
  uint8_t sensor_decimals;   // Nachkommastellen (0xFF = unverändert)

  // Scene-spezifisch
  String scene_alias;        // HA Scene Alias

  // Key-spezifisch
  String key_macro;          // Makro-String (z.B. "ctrl+g")
  uint8_t key_code;          // USB HID Scancode
  uint8_t key_modifier;      // Modifier bits (CTRL=0x01, SHIFT=0x02, ALT=0x04)

  Tile()
      : type(TILE_EMPTY),
        bg_color(0),
        sensor_decimals(0xFF),  // 0xFF = keine Rundung, Originalwert anzeigen
        key_code(0),
        key_modifier(0) {}
};

struct TileGridConfig {
  Tile tiles[TILES_PER_GRID];
};

struct TabConfig {
  char name[32];  // Custom tab name (empty = use default)

  TabConfig() {
    name[0] = '\0';  // Empty = use default
  }
};

class TileConfig {
public:
  TileConfig();

  bool load();
  bool save(const TileGridConfig& home, const TileGridConfig& game, const TileGridConfig& weather);
  bool saveSingleGrid(const char* grid_name, const TileGridConfig& grid);

  const TileGridConfig& getHomeGrid() const { return home_grid; }
  const TileGridConfig& getGameGrid() const { return game_grid; }
  const TileGridConfig& getWeatherGrid() const { return weather_grid; }

  TileGridConfig& getHomeGrid() { return home_grid; }
  TileGridConfig& getGameGrid() { return game_grid; }
  TileGridConfig& getWeatherGrid() { return weather_grid; }

  // Tab names (configurable via web interface)
  const char* getTabName(uint8_t tab_index) const;
  void setTabName(uint8_t tab_index, const char* name);
  bool loadTabNames();
  bool saveTabNames();

private:
  TileGridConfig home_grid;
  TileGridConfig game_grid;
  TileGridConfig weather_grid;

  TabConfig tab_configs[3];  // [0]=Home, [1]=Game, [2]=Weather

  bool loadGrid(const char* prefix, TileGridConfig& grid);
  bool saveGrid(const char* prefix, const TileGridConfig& grid);
};

extern TileConfig tileConfig;

#endif // TILE_CONFIG_H

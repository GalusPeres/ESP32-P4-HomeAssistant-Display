#include "src/tiles/tile_config.h"
#include <Preferences.h>

static const char* PREF_NAMESPACE = "tab5_tiles";

TileConfig tileConfig;

TileConfig::TileConfig() = default;

bool TileConfig::load() {
  bool home_ok = loadGrid("home", home_grid);
  bool game_ok = loadGrid("game", game_grid);
  return home_ok && game_ok;
}

bool TileConfig::save(const TileGridConfig& home, const TileGridConfig& game) {
  bool home_ok = saveGrid("home", home);
  bool game_ok = saveGrid("game", game);

  if (home_ok && game_ok) {
    home_grid = home;
    game_grid = game;
    Serial.println("[TileConfig] Konfiguration gespeichert");
    return true;
  }

  return false;
}

bool TileConfig::saveSingleGrid(const char* grid_name, const TileGridConfig& grid) {
  if (!grid_name || !*grid_name) {
    return false;
  }

  bool ok = false;
  if (strcmp(grid_name, "home") == 0) {
    ok = saveGrid("home", grid);
    if (ok) home_grid = grid;
  } else if (strcmp(grid_name, "game") == 0) {
    ok = saveGrid("game", grid);
    if (ok) game_grid = grid;
  } else {
    return false;
  }

  if (ok) {
    Serial.printf("[TileConfig] Grid '%s' gespeichert (single)\n", grid_name);
  }
  return ok;
}

bool TileConfig::loadGrid(const char* prefix, TileGridConfig& grid) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    char key[32];

    // Typ
    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i));
    grid.tiles[i].type = static_cast<TileType>(prefs.getUChar(key, TILE_EMPTY));

    // Title
    snprintf(key, sizeof(key), "%s_t%u_title", prefix, static_cast<unsigned>(i));
    grid.tiles[i].title = prefs.getString(key, "");

    // Farbe
    snprintf(key, sizeof(key), "%s_t%u_color", prefix, static_cast<unsigned>(i));
    grid.tiles[i].bg_color = prefs.getUInt(key, 0);

    // Sensor-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_ent", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_entity = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_unit", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_unit = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_prec", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_decimals = prefs.getUChar(key, 0xFF);

    // Scene-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i));
    grid.tiles[i].scene_alias = prefs.getString(key, "");

    // Key-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_macro = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_code = prefs.getUChar(key, 0);

    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_modifier = prefs.getUChar(key, 0);
  }

  prefs.end();
  Serial.printf("[TileConfig] Grid '%s' geladen\n", prefix);
  return true;
}

bool TileConfig::saveGrid(const char* prefix, const TileGridConfig& grid) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    return false;
  }

  auto removeKey = [&](const char* key) {
    prefs.remove(key);
  };

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    char key[32];
    const Tile& tile = grid.tiles[i];

    // Typ
    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i));
    prefs.putUChar(key, static_cast<uint8_t>(tile.type));

    // Title + Farbe (nur bei Nicht-Empty relevant, sonst entfernen)
    snprintf(key, sizeof(key), "%s_t%u_title", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_EMPTY) {
      removeKey(key);
    } else {
      prefs.putString(key, tile.title);
    }

    snprintf(key, sizeof(key), "%s_t%u_color", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_EMPTY) {
      removeKey(key);
    } else {
      prefs.putUInt(key, tile.bg_color);
    }

    // Sensor-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_ent", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_SENSOR) {
      prefs.putString(key, tile.sensor_entity);
    } else {
      removeKey(key);
    }

    snprintf(key, sizeof(key), "%s_t%u_unit", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_SENSOR) {
      prefs.putString(key, tile.sensor_unit);
    } else {
      removeKey(key);
    }

    snprintf(key, sizeof(key), "%s_t%u_prec", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_SENSOR) {
      prefs.putUChar(key, tile.sensor_decimals);
    } else {
      removeKey(key);
    }

    // Scene-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_SCENE) {
      prefs.putString(key, tile.scene_alias);
    } else {
      removeKey(key);
    }

    // Key-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_KEY) {
      prefs.putString(key, tile.key_macro);
    } else {
      removeKey(key);
    }

    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_KEY) {
      prefs.putUChar(key, tile.key_code);
    } else {
      removeKey(key);
    }

    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i));
    if (tile.type == TILE_KEY) {
      prefs.putUChar(key, tile.key_modifier);
    } else {
      removeKey(key);
    }
  }

  prefs.end();
  Serial.printf("[TileConfig] Grid '%s' gespeichert\n", prefix);
  return true;
}

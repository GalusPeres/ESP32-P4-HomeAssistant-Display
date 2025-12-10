#include "src/tiles/tile_config.h"
#include <Preferences.h>
#include <string.h>

static const char* PREF_NAMESPACE = "tab5_tiles";
static constexpr uint8_t PACKED_GRID_VERSION = 1;

// Feste Längen für gepackte Strings (inkl. Nullterminator)
static constexpr size_t TITLE_MAX     = 32;
static constexpr size_t ENTITY_MAX    = 64;
static constexpr size_t UNIT_MAX      = 16;
static constexpr size_t SCENE_MAX     = 32;
static constexpr size_t MACRO_MAX     = 32;

struct PackedTile {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  char title[TITLE_MAX];
  char sensor_entity[ENTITY_MAX];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
};

struct PackedGrid {
  uint8_t version;
  uint8_t reserved[3];  // Alignment / future use
  PackedTile tiles[TILES_PER_GRID];
};

TileConfig tileConfig;

TileConfig::TileConfig() = default;

static void copyString(const String& src, char* dst, size_t max_len) {
  if (!dst || max_len == 0) return;
  memset(dst, 0, max_len);
  if (src.length() == 0) return;
  size_t n = src.length();
  if (n >= max_len) n = max_len - 1;
  memcpy(dst, src.c_str(), n);
}

static uint8_t clampDecimals(uint8_t val) {
  if (val == 0xFF) return 0xFF;
  if (val > 6) return 6;
  return val;
}

static void packTile(const Tile& in, PackedTile& out) {
  memset(&out, 0, sizeof(out));
  out.type = static_cast<uint8_t>(in.type);
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.bg_color = in.bg_color;
  copyString(in.title, out.title, sizeof(out.title));
  copyString(in.sensor_entity, out.sensor_entity, sizeof(out.sensor_entity));
  copyString(in.sensor_unit, out.sensor_unit, sizeof(out.sensor_unit));
  copyString(in.scene_alias, out.scene_alias, sizeof(out.scene_alias));
  copyString(in.key_macro, out.key_macro, sizeof(out.key_macro));
}

static void unpackTile(const PackedTile& in, Tile& out) {
  out.type = static_cast<TileType>(in.type);
  out.bg_color = in.bg_color;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.title = String(in.title);
  out.sensor_entity = String(in.sensor_entity);
  out.sensor_unit = String(in.sensor_unit);
  out.scene_alias = String(in.scene_alias);
  out.key_macro = String(in.key_macro);
}

static bool buildBlobKey(const char* prefix, char* out, size_t out_len) {
  if (!prefix || !out || out_len < 8) return false;
  int written = snprintf(out, out_len, "%s_blob", prefix);
  return written > 0 && static_cast<size_t>(written) < out_len;
}

// Legacy Loader (alte Key/Value-EintrÃ¤ge)
static bool loadGridLegacy(const char* prefix, TileGridConfig& grid) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    char key[32];

    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i));
    grid.tiles[i].type = static_cast<TileType>(prefs.getUChar(key, TILE_EMPTY));

    snprintf(key, sizeof(key), "%s_t%u_title", prefix, static_cast<unsigned>(i));
    grid.tiles[i].title = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_color", prefix, static_cast<unsigned>(i));
    grid.tiles[i].bg_color = prefs.getUInt(key, 0);

    snprintf(key, sizeof(key), "%s_t%u_ent", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_entity = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_unit", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_unit = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_prec", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_decimals = clampDecimals(prefs.getUChar(key, 0xFF));

    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i));
    grid.tiles[i].scene_alias = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_macro = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_code = prefs.getUChar(key, 0);

    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_modifier = prefs.getUChar(key, 0);
  }

  prefs.end();
  Serial.printf("[TileConfig] Grid '%s' geladen (legacy)\n", prefix);
  return true;
}

static void clearLegacyKeys(Preferences& prefs, const char* prefix) {
  char key[32];
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_title", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_color", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_ent", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_unit", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_prec", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i)); prefs.remove(key);
    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i)); prefs.remove(key);
  }
}

static void clearAllLegacyKeys() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    return;
  }
  clearLegacyKeys(prefs, "home");
  clearLegacyKeys(prefs, "game");
  clearLegacyKeys(prefs, "weather");  // Altlasten falls vorhanden
  prefs.end();
}

bool TileConfig::load() {
  clearAllLegacyKeys();  // AufrÃ¤umen von Altlasten (vorherige Key/Value-Layouts)
  bool home_ok = loadGrid("home", home_grid);
  bool game_ok = loadGrid("game", game_grid);
  bool weather_ok = loadGrid("weather", weather_grid);
  return home_ok && game_ok && weather_ok;
}

bool TileConfig::save(const TileGridConfig& home, const TileGridConfig& game, const TileGridConfig& weather) {
  bool home_ok = saveGrid("home", home);
  bool game_ok = saveGrid("game", game);
  bool weather_ok = saveGrid("weather", weather);

  if (home_ok && game_ok && weather_ok) {
    home_grid = home;
    game_grid = game;
    weather_grid = weather;
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
  } else if (strcmp(grid_name, "weather") == 0) {
    ok = saveGrid("weather", grid);
    if (ok) weather_grid = grid;
  } else {
    return false;
  }

  if (ok) {
    Serial.printf("[TileConfig] Grid '%s' gespeichert (single)\n", grid_name);
  }
  return ok;
}

bool TileConfig::loadGrid(const char* prefix, TileGridConfig& grid) {
  char blob_key[16];
  if (!buildBlobKey(prefix, blob_key, sizeof(blob_key))) {
    return false;
  }

  // Erst versuchen, gepacktes Grid zu laden
  {
    Preferences prefs;
    if (prefs.begin(PREF_NAMESPACE, true)) {
      size_t blob_len = prefs.getBytesLength(blob_key);
      if (blob_len >= sizeof(PackedGrid)) {
        PackedGrid packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        prefs.end();
        if (read == sizeof(packed) && packed.version == PACKED_GRID_VERSION) {
          for (size_t i = 0; i < TILES_PER_GRID; ++i) {
            unpackTile(packed.tiles[i], grid.tiles[i]);
          }
          Serial.printf("[TileConfig] Grid '%s' geladen (blob)\n", prefix);
          return true;
        }
      } else {
        prefs.end();
      }
    }
  }

  // Fallback: Legacy-Keys laden und sofort migrieren
  bool legacy_ok = loadGridLegacy(prefix, grid);
  if (legacy_ok) {
    saveGrid(prefix, grid);  // Migration: schreibt Blob
  }
  return legacy_ok;
}

bool TileConfig::saveGrid(const char* prefix, const TileGridConfig& grid) {
  char blob_key[16];
  if (!buildBlobKey(prefix, blob_key, sizeof(blob_key))) {
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    return false;
  }

  clearLegacyKeys(prefs, prefix);  // Alte Key/Value-EintrÃ¤ge freirÃ¤umen

  PackedGrid packed{};
  packed.version = PACKED_GRID_VERSION;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    packTile(grid.tiles[i], packed.tiles[i]);
  }

  size_t written = prefs.putBytes(blob_key, &packed, sizeof(packed));
  prefs.end();

  if (written != sizeof(packed)) {
    Serial.printf("[TileConfig] Fehler beim Speichern von Grid '%s' (geschrieben: %u)\n",
                  prefix, static_cast<unsigned>(written));
    return false;
  }

  Serial.printf("[TileConfig] Grid '%s' gespeichert (blob, %u bytes)\n",
                prefix, static_cast<unsigned>(written));
  return true;
}

#include "src/tiles/tile_config.h"
#include <Preferences.h>
#include <string.h>

static const char* PREF_NAMESPACE = "tab5_tiles";
static constexpr uint8_t PACKED_GRID_VERSION = 3;
static constexpr uint16_t IMAGE_SLIDESHOW_DEFAULT_SEC = 10;
static constexpr uint16_t IMAGE_SLIDESHOW_MAX_SEC = 3600;

// Feste Längen für gepackte Strings (inkl. Nullterminator)
static constexpr size_t TITLE_MAX     = 32;
static constexpr size_t ICON_MAX      = 32;  // MDI Icon Name (z.B. "thermometer")
static constexpr size_t ENTITY_MAX    = 64;
static constexpr size_t UNIT_MAX      = 16;
static constexpr size_t SCENE_MAX     = 32;
static constexpr size_t MACRO_MAX     = 32;

struct PackedTileV1 {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  char title[TITLE_MAX];
  char icon_name[ICON_MAX];        // MDI Icon Name
  char sensor_entity[ENTITY_MAX];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
};

struct PackedGridV1 {
  uint8_t version;
  uint8_t reserved[3];  // Alignment / future use
  PackedTileV1 tiles[TILES_PER_GRID];
};

struct PackedTileV2 {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  char title[TITLE_MAX];
  char icon_name[ICON_MAX];        // MDI Icon Name
  char sensor_entity[ENTITY_MAX];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
  uint8_t sensor_value_font;
  uint8_t reserved[3];             // Alignment / future use
};

struct PackedGridV2 {
  uint8_t version;
  uint8_t reserved[3];  // Alignment / future use
  PackedTileV2 tiles[TILES_PER_GRID];
};

struct PackedTile {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  char title[TITLE_MAX];
  char icon_name[ICON_MAX];        // MDI Icon Name
  char sensor_entity[ENTITY_MAX];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
  uint8_t sensor_value_font;
  uint16_t image_slideshow_sec;
  uint8_t reserved[1];             // Alignment / future use
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

static uint8_t clampSensorValueFont(uint8_t val) {
  if (val > 2) return 0;
  return val;
}

static uint16_t clampImageSlideshowSeconds(uint16_t val) {
  if (val == 0) return IMAGE_SLIDESHOW_DEFAULT_SEC;
  if (val > IMAGE_SLIDESHOW_MAX_SEC) return IMAGE_SLIDESHOW_MAX_SEC;
  return val;
}

static bool looksLikeImagePath(const String& value);

static void packTile(const Tile& in, PackedTile& out) {
  memset(&out, 0, sizeof(out));
  out.type = static_cast<uint8_t>(in.type);
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.bg_color = in.bg_color;
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
  copyString(in.title, out.title, sizeof(out.title));
  copyString(in.icon_name, out.icon_name, sizeof(out.icon_name));
  copyString(in.sensor_unit, out.sensor_unit, sizeof(out.sensor_unit));
  copyString(in.scene_alias, out.scene_alias, sizeof(out.scene_alias));
  if (in.type == TILE_IMAGE) {
    // Element-Pool: TILE_IMAGE nutzt sensor_entity für image_path (mehr Platz als key_macro).
    String image_path = in.image_path;
    if (image_path.length() == 0 && looksLikeImagePath(in.key_macro)) {
      image_path = in.key_macro;  // Legacy fallback
    }
    copyString(image_path, out.sensor_entity, sizeof(out.sensor_entity));
    out.key_macro[0] = '\0';
    Serial.printf("[TileConfig] packTile - TILE_IMAGE: image_path='%s', packed='%s'\n",
                  image_path.c_str(), out.sensor_entity);
  } else {
    copyString(in.sensor_entity, out.sensor_entity, sizeof(out.sensor_entity));
    copyString(in.key_macro, out.key_macro, sizeof(out.key_macro));
  }
}

static bool looksLikeImagePath(const String& value) {
  if (value.length() == 0) return false;
  if (value.startsWith("/") || value.startsWith("__")) return true;
  if (value.startsWith("http://") || value.startsWith("https://")) return true;
  return false;
}

static void unpackTile(const PackedTile& in, Tile& out) {
  out.type = static_cast<TileType>(in.type);
  out.bg_color = in.bg_color;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
  out.title = String(in.title);
  out.icon_name = String(in.icon_name);
  out.sensor_entity = String(in.sensor_entity);
  out.sensor_unit = String(in.sensor_unit);
  out.scene_alias = String(in.scene_alias);
  out.key_macro = String(in.key_macro);
  // Element-Pool: TILE_IMAGE nutzt sensor_entity für image_path (fallback: key_macro aus Altbestand).
  if (out.type == TILE_IMAGE) {
    if (looksLikeImagePath(out.sensor_entity)) {
      out.image_path = out.sensor_entity;
    } else {
      out.image_path = out.key_macro;
    }
    out.key_macro = "";
    out.sensor_entity = "";
    Serial.printf("[TileConfig] unpackTile - TILE_IMAGE: packed(sensor)='%s', packed(macro)='%s', image_path='%s'\n",
                  in.sensor_entity, in.key_macro, out.image_path.c_str());
  } else {
    out.image_path = "";
  }
}

static void unpackTileV2(const PackedTileV2& in, Tile& out) {
  out.type = static_cast<TileType>(in.type);
  out.bg_color = in.bg_color;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.image_slideshow_sec = IMAGE_SLIDESHOW_DEFAULT_SEC;
  out.title = String(in.title);
  out.icon_name = String(in.icon_name);
  out.sensor_entity = String(in.sensor_entity);
  out.sensor_unit = String(in.sensor_unit);
  out.scene_alias = String(in.scene_alias);
  out.key_macro = String(in.key_macro);
  // Element-Pool: TILE_IMAGE nutzt sensor_entity fuer image_path (fallback: key_macro aus Altbestand).
  if (out.type == TILE_IMAGE) {
    if (looksLikeImagePath(out.sensor_entity)) {
      out.image_path = out.sensor_entity;
    } else {
      out.image_path = out.key_macro;
    }
    out.key_macro = "";
    out.sensor_entity = "";
    Serial.printf("[TileConfig] unpackTile - TILE_IMAGE: packed(sensor)='%s', packed(macro)='%s', image_path='%s'\n",
                  in.sensor_entity, in.key_macro, out.image_path.c_str());
  } else {
    out.image_path = "";
  }
}

static void unpackTileV1(const PackedTileV1& in, Tile& out) {
  out.type = static_cast<TileType>(in.type);
  out.bg_color = in.bg_color;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  out.sensor_value_font = 0;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.image_slideshow_sec = IMAGE_SLIDESHOW_DEFAULT_SEC;
  out.title = String(in.title);
  out.icon_name = String(in.icon_name);
  out.sensor_entity = String(in.sensor_entity);
  out.sensor_unit = String(in.sensor_unit);
  out.scene_alias = String(in.scene_alias);
  out.key_macro = String(in.key_macro);
  // Element-Pool: TILE_IMAGE nutzt sensor_entity fuer image_path (fallback: key_macro aus Altbestand).
  if (out.type == TILE_IMAGE) {
    if (looksLikeImagePath(out.sensor_entity)) {
      out.image_path = out.sensor_entity;
    } else {
      out.image_path = out.key_macro;
    }
    out.key_macro = "";
    out.sensor_entity = "";
  } else {
    out.image_path = "";
  }
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
    grid.tiles[i].sensor_value_font = 0;

    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i));
    grid.tiles[i].scene_alias = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_macro = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_code = prefs.getUChar(key, 0);

    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_modifier = prefs.getUChar(key, 0);
    grid.tiles[i].image_slideshow_sec = IMAGE_SLIDESHOW_DEFAULT_SEC;
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
  clearLegacyKeys(prefs, "tab0");
  clearLegacyKeys(prefs, "tab1");
  clearLegacyKeys(prefs, "tab2");
  clearLegacyKeys(prefs, "home");   // Clean up old naming
  clearLegacyKeys(prefs, "game");
  clearLegacyKeys(prefs, "weather");
  prefs.end();
}

// Migrate old blob keys to new naming
static bool migrateOldBlobs() {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("[TileConfig] Fehler: Konnte NVS nicht öffnen für Migration");
    return false;
  }

  // Always clean up old blob keys to free space
  bool had_old_blobs = false;
  if (prefs.isKey("home_blob")) {
    had_old_blobs = true;
    prefs.remove("home_blob");
    Serial.println("[TileConfig] Removed old home_blob");
  }
  if (prefs.isKey("game_blob")) {
    had_old_blobs = true;
    prefs.remove("game_blob");
    Serial.println("[TileConfig] Removed old game_blob");
  }
  if (prefs.isKey("weather_blob")) {
    had_old_blobs = true;
    prefs.remove("weather_blob");
    Serial.println("[TileConfig] Removed old weather_blob");
  }

  prefs.end();

  if (had_old_blobs) {
    Serial.println("[TileConfig] Old blobs cleaned up - NVS space freed");
  }

  return had_old_blobs;
}

bool TileConfig::load() {
  migrateOldBlobs();  // Migrate old home/game/weather to tab0/tab1/tab2
  clearAllLegacyKeys();  // Aufräumen von Altlasten (vorherige Key/Value-Layouts)
  bool tab0_ok = loadGrid("tab0", tab0_grid);
  bool tab1_ok = loadGrid("tab1", tab1_grid);
  bool tab2_ok = loadGrid("tab2", tab2_grid);
  loadTabNames();  // Load custom tab names
  return tab0_ok && tab1_ok && tab2_ok;
}

bool TileConfig::save(const TileGridConfig& tab0, const TileGridConfig& tab1, const TileGridConfig& tab2) {
  bool tab0_ok = saveGrid("tab0", tab0);
  bool tab1_ok = saveGrid("tab1", tab1);
  bool tab2_ok = saveGrid("tab2", tab2);

  if (tab0_ok && tab1_ok && tab2_ok) {
    tab0_grid = tab0;
    tab1_grid = tab1;
    tab2_grid = tab2;
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
  if (strcmp(grid_name, "tab0") == 0) {
    ok = saveGrid("tab0", grid);
    if (ok) tab0_grid = grid;
  } else if (strcmp(grid_name, "tab1") == 0) {
    ok = saveGrid("tab1", grid);
    if (ok) tab1_grid = grid;
  } else if (strcmp(grid_name, "tab2") == 0) {
    ok = saveGrid("tab2", grid);
    if (ok) tab2_grid = grid;
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
        if (read == sizeof(packed) && packed.version == PACKED_GRID_VERSION) {
          for (size_t i = 0; i < TILES_PER_GRID; ++i) {
            unpackTile(packed.tiles[i], grid.tiles[i]);
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v%u)\n",
                        prefix, static_cast<unsigned>(packed.version));
          return true;
        }
      }

      if (blob_len >= sizeof(PackedGridV2)) {
        PackedGridV2 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 2) {
          for (size_t i = 0; i < TILES_PER_GRID; ++i) {
            unpackTileV2(packed.tiles[i], grid.tiles[i]);
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v2)\n", prefix);
          saveGrid(prefix, grid);  // Migration auf neues Format
          return true;
        }
      }

      if (blob_len >= sizeof(PackedGridV1)) {
        PackedGridV1 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 1) {
          for (size_t i = 0; i < TILES_PER_GRID; ++i) {
            unpackTileV1(packed.tiles[i], grid.tiles[i]);
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v1)\n", prefix);
          saveGrid(prefix, grid);  // Migration auf neues Format
          return true;
        }
      }

      prefs.end();
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
    Serial.printf("[TileConfig] Fehler beim Öffnen von NVS namespace '%s'\n", PREF_NAMESPACE);
    return false;
  }

  Serial.printf("[TileConfig] Versuche zu speichern: '%s' (key='%s', size=%u bytes)\n",
                prefix, blob_key, static_cast<unsigned>(sizeof(PackedGrid)));

  clearLegacyKeys(prefs, prefix);  // Alte Key/Value-EintrÃ¤ge freirÃ¤umen

  PackedGrid packed{};
  packed.version = PACKED_GRID_VERSION;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    packTile(grid.tiles[i], packed.tiles[i]);
  }

  size_t written = prefs.putBytes(blob_key, &packed, sizeof(packed));
  prefs.end();

  if (written != sizeof(packed)) {
    Serial.printf("[TileConfig] Fehler beim Speichern von Grid '%s' (geschrieben: %u, erwartet: %u)\n",
                  prefix, static_cast<unsigned>(written), static_cast<unsigned>(sizeof(packed)));
    return false;
  }

  Serial.printf("[TileConfig] Grid '%s' gespeichert (blob, %u bytes)\n",
                prefix, static_cast<unsigned>(written));
  return true;
}

// ========== Tab Names (configurable via web interface) ==========

const char* TileConfig::getTabName(uint8_t tab_index) const {
  if (tab_index >= 4) return "";

  // Return custom name (kann auch leer sein)
  return tab_configs[tab_index].name;
}

void TileConfig::setTabName(uint8_t tab_index, const char* name) {
  if (tab_index >= 4 || !name) return;

  size_t len = strlen(name);
  if (len >= sizeof(tab_configs[0].name)) {
    len = sizeof(tab_configs[0].name) - 1;
  }

  memcpy(tab_configs[tab_index].name, name, len);
  tab_configs[tab_index].name[len] = '\0';
}

const char* TileConfig::getTabIcon(uint8_t tab_index) const {
  if (tab_index >= 4) return "";
  return tab_configs[tab_index].icon_name;
}

void TileConfig::setTabIcon(uint8_t tab_index, const char* icon_name) {
  if (tab_index >= 4 || !icon_name) return;

  size_t len = strlen(icon_name);
  if (len >= sizeof(tab_configs[0].icon_name)) {
    len = sizeof(tab_configs[0].icon_name) - 1;
  }

  memcpy(tab_configs[tab_index].icon_name, icon_name, len);
  tab_configs[tab_index].icon_name[len] = '\0';
}

bool TileConfig::loadTabNames() {
  Preferences prefs;
  if (!prefs.begin("tab5_config", true)) {  // Read-only
    return false;
  }

  for (uint8_t i = 0; i < 4; i++) {
    char key[16];

    // Load tab name
    snprintf(key, sizeof(key), "tab_name_%u", i);
    String name = prefs.getString(key, "");
    if (name.length() > 0) {
      setTabName(i, name.c_str());
    }

    // Load tab icon
    snprintf(key, sizeof(key), "tab_icon_%u", i);
    String icon = prefs.getString(key, "");
    if (icon.length() > 0) {
      setTabIcon(i, icon.c_str());
    }
  }

  prefs.end();
  Serial.println("[TileConfig] Tab-Namen und Icons geladen");
  return true;
}

bool TileConfig::saveTabNames() {
  Preferences prefs;
  if (!prefs.begin("tab5_config", false)) {  // Read-write
    return false;
  }

  for (uint8_t i = 0; i < 4; i++) {
    char key[16];

    // Save tab name
    snprintf(key, sizeof(key), "tab_name_%u", i);
    if (tab_configs[i].name[0] != '\0') {
      prefs.putString(key, tab_configs[i].name);
    } else {
      prefs.remove(key);  // Remove if empty (use default)
    }

    // Save tab icon
    snprintf(key, sizeof(key), "tab_icon_%u", i);
    if (tab_configs[i].icon_name[0] != '\0') {
      prefs.putString(key, tab_configs[i].icon_name);
    } else {
      prefs.remove(key);  // Remove if empty (no icon)
    }
  }

  prefs.end();
  Serial.println("[TileConfig] Tab-Namen und Icons gespeichert");
  return true;
}

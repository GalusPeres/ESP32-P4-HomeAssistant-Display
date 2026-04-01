#include "src/tiles/tile_config.h"
#include "src/devices/device.h"
#include <Preferences.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <memory>
#include <new>

static const char* PREF_NAMESPACE = "tab5_tiles";
static constexpr uint8_t PACKED_GRID_VERSION = 7;
static constexpr uint16_t IMAGE_SLIDESHOW_DEFAULT_SEC = 10;
static constexpr uint16_t IMAGE_SLIDESHOW_MAX_SEC = 3600;
static constexpr size_t OLD_TILES_PER_GRID = 12;  // For V1-V5 migration
static constexpr uint8_t LEGACY_NAV_KIND_SETTINGS = 1;
static constexpr uint8_t LEGACY_NAV_KIND_BACK = 2;
static constexpr uint8_t LEGACY_TAB_SETTINGS = 3;

// Feste Längen für gepackte Strings (inkl. Nullterminator)
static constexpr size_t TITLE_MAX     = 32;
static constexpr size_t ICON_MAX      = 32;  // MDI Icon Name (z.B. "thermometer")
static constexpr size_t ENTITY_MAX    = 64;
static constexpr size_t ENTITY_MAX_V4 = 128;
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
  PackedTileV1 tiles[OLD_TILES_PER_GRID];
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
  PackedTileV2 tiles[OLD_TILES_PER_GRID];
};

struct PackedTileV4 {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  char title[TITLE_MAX];
  char icon_name[ICON_MAX];        // MDI Icon Name
  char sensor_entity[ENTITY_MAX_V4];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
  uint8_t sensor_value_font;
  uint16_t image_slideshow_sec;
  uint8_t reserved[1];             // Alignment / future use
};

struct PackedGridV4 {
  uint8_t version;
  uint8_t reserved[3];  // Alignment / future use
  PackedTileV4 tiles[OLD_TILES_PER_GRID];
};

struct PackedTileV3 {
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

struct PackedGridV3 {
  uint8_t version;
  uint8_t reserved[3];  // Alignment / future use
  PackedTileV3 tiles[OLD_TILES_PER_GRID];
};

struct PackedTileV5 {
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
  uint8_t sensor_gauge_enabled;
  int32_t sensor_gauge_min;
  int32_t sensor_gauge_max;
  uint8_t reserved[1];             // Alignment / future use
};

struct PackedGridV5 {
  uint8_t version;
  uint8_t reserved[3];  // Alignment / future use
  PackedTileV5 tiles[OLD_TILES_PER_GRID];
};

// V6: Added col/row/span_w/span_h for flexible grid layout
struct PackedTileV6 {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  uint8_t col;                       // Grid column (0-3)
  uint8_t row;                       // Grid row (0-3)
  uint8_t span_w;                    // Width in cells (1-4)
  uint8_t span_h;                    // Height in cells (1-4)
  char title[TITLE_MAX];
  char icon_name[ICON_MAX];
  char sensor_entity[ENTITY_MAX];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
  uint8_t sensor_value_font;
  uint16_t image_slideshow_sec;
  uint8_t sensor_gauge_enabled;
  int32_t sensor_gauge_min;
  int32_t sensor_gauge_max;
};

struct PackedTileV7 {
  uint8_t type;
  uint8_t sensor_decimals;
  uint8_t key_code;
  uint8_t key_modifier;
  uint32_t bg_color;
  uint8_t col;                       // Grid column (0-3)
  uint8_t row;                       // Grid row (0-3)
  uint8_t span_w;                    // Width in cells (1-4)
  uint8_t span_h;                    // Height in cells (1-4)
  char title[TITLE_MAX];
  char icon_name[ICON_MAX];
  char sensor_entity[ENTITY_MAX];
  char sensor_unit[UNIT_MAX];
  char scene_alias[SCENE_MAX];
  char key_macro[MACRO_MAX];
  uint8_t sensor_value_font;
  uint16_t image_slideshow_sec;
  uint8_t sensor_gauge_enabled;
  int32_t sensor_gauge_min;
  int32_t sensor_gauge_max;
  uint8_t popup_open_mode;
  uint8_t reserved[3];
};

// Split grid into 4 quarters to fit NVS size limit (~2KB per entry)
// Each quarter holds 4 tiles for the 4x4 layout.
static constexpr size_t TILES_PER_QUARTER = 4;
static constexpr size_t QUARTERS_PER_GRID = TILES_PER_GRID / TILES_PER_QUARTER;
static_assert(QUARTERS_PER_GRID * TILES_PER_QUARTER == TILES_PER_GRID,
              "TILES_PER_GRID must be divisible by TILES_PER_QUARTER");

struct PackedQuarterGridV6 {
  uint8_t version;
  uint8_t quarter_index;  // 0-3
  uint8_t reserved[2];
  PackedTileV6 tiles[TILES_PER_QUARTER];
};

struct PackedQuarterGridV7 {
  uint8_t version;
  uint8_t quarter_index;  // 0-3
  uint8_t reserved[2];
  PackedTileV7 tiles[TILES_PER_QUARTER];
};

struct FolderIndexHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
};

struct FolderEntryDisk {
  uint16_t id;
  uint16_t parent_id;
  char name[32];
  char icon_name[32];
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
  if (val > 4) return 0;
  return val;
}

static uint16_t clampImageSlideshowSeconds(uint16_t val) {
  if (val == 0) return IMAGE_SLIDESHOW_DEFAULT_SEC;
  if (val > IMAGE_SLIDESHOW_MAX_SEC) return IMAGE_SLIDESHOW_MAX_SEC;
  return val;
}

static uint16_t getNavigateTargetId(const Tile& tile) {
  return static_cast<uint16_t>((static_cast<uint16_t>(tile.key_modifier) << 8) | tile.key_code);
}

static void setNavigateTargetId(Tile& tile, uint16_t folder_id) {
  tile.key_code = static_cast<uint8_t>(folder_id & 0xFF);
  tile.key_modifier = static_cast<uint8_t>((folder_id >> 8) & 0xFF);
}

static void normalizeGaugeRange(int32_t& min_val, int32_t& max_val) {
  if (max_val <= min_val) {
    min_val = 0;
    max_val = 100;
  }
}

static bool shouldNormalizeGaugeRange(TileType type) {
  return type != TILE_CLOCK;
}

static bool looksLikeImagePath(const String& value);
static const char* kImagePathDir = "/_tile_links";
static const char* kTileGridDir = "/_tile_grids";
static const char* kFolderIndexFile = "/_tile_grids/folders.bin";
static constexpr uint32_t kFolderIndexMagic = 0x54464C44;  // 'TFLD'
static constexpr uint16_t kFolderIndexVersion = 1;

static fs::FS& storageFS() {
  return Device::storageFS();
}

static bool storageReady() {
  return Device::storageReady();
}

static bool ensureImagePathDir() {
  if (!storageReady()) return false;
  if (storageFS().exists(kImagePathDir)) return true;
  return storageFS().mkdir(kImagePathDir);
}

static bool ensureTileGridDir() {
  if (!storageReady()) return false;
  if (storageFS().exists(kTileGridDir)) return true;
  return storageFS().mkdir(kTileGridDir);
}

static bool ensureIconDir() {
  if (!storageReady()) return false;
  if (storageFS().exists("/icons")) return true;
  return storageFS().mkdir("/icons");
}

static String imagePathFile(uint16_t folder_id, size_t index) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s/f%u_%02u.url", kImagePathDir, static_cast<unsigned>(folder_id), static_cast<unsigned>(index));
  return String(buf);
}

static String imagePathFileLegacy(const char* prefix, size_t index) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s/%s_%02u.url", kImagePathDir, prefix, static_cast<unsigned>(index));
  return String(buf);
}

static String tileGridFile(uint16_t folder_id) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s/f%05u_v7.bin", kTileGridDir, static_cast<unsigned>(folder_id));
  return String(buf);
}

static String tileGridFileLegacyV6(uint16_t folder_id) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s/f%05u_v6.bin", kTileGridDir, static_cast<unsigned>(folder_id));
  return String(buf);
}

static String tileGridFileLegacy(const char* prefix) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s/%s_v6.bin", kTileGridDir, prefix);
  return String(buf);
}

static bool writeGridSd(uint16_t folder_id, const PackedQuarterGridV7* packed, size_t count) {
  if (!storageReady()) return false;
  if (!ensureTileGridDir()) return false;

  String filePath = tileGridFile(folder_id);
  if (storageFS().exists(filePath)) {
    storageFS().remove(filePath);
  }

  File f = storageFS().open(filePath, FILE_WRITE);
  if (!f) {
    return false;
  }

  const size_t expected = count * sizeof(PackedQuarterGridV7);
  const size_t written = f.write(reinterpret_cast<const uint8_t*>(packed), expected);
  f.flush();
  f.close();

  if (written != expected) {
    Serial.printf("[TileConfig] Storage short write: folder=%u written=%u expected=%u\n",
                  static_cast<unsigned>(folder_id),
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(expected));
    return false;
  }

  return true;
}

static bool readPackedGridFileV7(const String& filePath, PackedQuarterGridV7* packed, size_t count) {
  if (!storageFS().exists(filePath)) return false;
  File f = storageFS().open(filePath, FILE_READ);
  if (!f) return false;
  const size_t expected = count * sizeof(PackedQuarterGridV7);
  if (static_cast<size_t>(f.size()) < expected) {
    f.close();
    return false;
  }
  size_t read = f.read(reinterpret_cast<uint8_t*>(packed), expected);
  f.close();
  if (read != expected) return false;
  for (size_t q = 0; q < count; ++q) {
    if (packed[q].version != 7 || packed[q].quarter_index != static_cast<uint8_t>(q)) {
      return false;
    }
  }
  return true;
}

static bool readPackedGridFileV6(const String& filePath, PackedQuarterGridV6* packed, size_t count) {
  if (!storageFS().exists(filePath)) return false;
  File f = storageFS().open(filePath, FILE_READ);
  if (!f) return false;
  const size_t expected = count * sizeof(PackedQuarterGridV6);
  if (static_cast<size_t>(f.size()) < expected) {
    f.close();
    return false;
  }
  size_t read = f.read(reinterpret_cast<uint8_t*>(packed), expected);
  f.close();
  if (read != expected) return false;
  for (size_t q = 0; q < count; ++q) {
    if (packed[q].version != 6 || packed[q].quarter_index != static_cast<uint8_t>(q)) {
      return false;
    }
  }
  return true;
}

static bool readGridSd(uint16_t folder_id, PackedQuarterGridV7* packed, size_t count) {
  if (!storageReady()) return false;
  return readPackedGridFileV7(tileGridFile(folder_id), packed, count);
}

static bool readGridSdV6(uint16_t folder_id, PackedQuarterGridV6* packed, size_t count) {
  if (!storageReady()) return false;
  return readPackedGridFileV6(tileGridFileLegacyV6(folder_id), packed, count);
}

static bool writeImagePathSd(uint16_t folder_id, size_t index, const String& path) {
  if (!ensureImagePathDir()) return false;
  String filePath = imagePathFile(folder_id, index);
  if (path.length() == 0) {
    if (storageFS().exists(filePath)) storageFS().remove(filePath);
    return true;
  }
  if (storageFS().exists(filePath)) storageFS().remove(filePath);
  File f = storageFS().open(filePath, FILE_WRITE);
  if (!f) return false;
  f.print(path);
  f.close();
  return true;
}

static bool readImagePathSd(uint16_t folder_id, size_t index, String& out) {
  out = "";
  if (!storageReady()) return false;
  String filePath = imagePathFile(folder_id, index);
  if (!storageFS().exists(filePath) && folder_id == 0) {
    filePath = imagePathFileLegacy("tab0", index);
  }
  if (!storageFS().exists(filePath)) return false;
  File f = storageFS().open(filePath, FILE_READ);
  if (!f) return false;
  out = f.readString();
  f.close();
  out.trim();
  return out.length() > 0;
}

static void packTile(const Tile& in, PackedTileV7& out) {
  memset(&out, 0, sizeof(out));
  out.type = static_cast<uint8_t>(in.type);
  uint8_t decimals = clampDecimals(in.sensor_decimals);
  if (in.type == TILE_FOLDER || in.type == TILE_SETTINGS || in.type == TILE_BACK) {
    decimals = 0xFF;
  }
  out.sensor_decimals = decimals;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  out.bg_color = in.bg_color;
  out.col = (in.col < GRID_COLS) ? in.col : 0;
  out.row = (in.row < GRID_ROWS) ? in.row : 0;
  uint8_t span_w = (in.span_w < 1) ? 1 : ((in.span_w > GRID_COLS) ? GRID_COLS : in.span_w);
  uint8_t span_h = (in.span_h < 1) ? 1 : ((in.span_h > GRID_ROWS) ? GRID_ROWS : in.span_h);
  if (span_w > GRID_COLS - out.col) span_w = GRID_COLS - out.col;
  if (span_h > GRID_ROWS - out.row) span_h = GRID_ROWS - out.row;
  out.span_w = span_w;
  out.span_h = span_h;
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
  out.sensor_gauge_enabled = (in.sensor_display_mode <= 2) ? in.sensor_display_mode : 0;
  out.sensor_gauge_min = in.sensor_gauge_min;
  out.sensor_gauge_max = in.sensor_gauge_max;
  if (shouldNormalizeGaugeRange(in.type)) {
    normalizeGaugeRange(out.sensor_gauge_min, out.sensor_gauge_max);
  }
  out.popup_open_mode = ((in.type == TILE_SENSOR || in.type == TILE_WEATHER) &&
                         getTilePopupOpenMode(in) == TILE_POPUP_OPEN_SHORT_PRESS)
                            ? TILE_POPUP_OPEN_SHORT_PRESS
                            : TILE_POPUP_OPEN_LONG_PRESS;
  copyString(in.title, out.title, sizeof(out.title));
  copyString(in.icon_name, out.icon_name, sizeof(out.icon_name));
  copyString(in.sensor_unit, out.sensor_unit, sizeof(out.sensor_unit));
  // For TILE_SENSOR, store gauge appearance in scene_alias with magic prefix
  if (in.type == TILE_SENSOR) {
    memset(out.scene_alias, 0, sizeof(out.scene_alias));
    out.scene_alias[0] = 0x01;  // Magic byte
    // Clamp and store arc degrees (90-359, default 100)
    uint16_t arc = in.sensor_gauge_arc;
    if (arc < 90) arc = 90;
    if (arc > 359) arc = 359;
    out.scene_alias[1] = static_cast<char>(arc & 0xFF);
    out.scene_alias[2] = static_cast<char>((arc >> 8) & 0xFF);
    // Clamp and store gauge size (100-800, default 350)
    uint16_t size = in.sensor_gauge_size;
    if (size < 100) size = 100;
    if (size > 800) size = 800;
    out.scene_alias[3] = static_cast<char>(size & 0xFF);
    out.scene_alias[4] = static_cast<char>((size >> 8) & 0xFF);
    // Clamp and store y offset (-100 to 200, default 12)
    int16_t y_off = in.sensor_gauge_y_offset;
    if (y_off < -100) y_off = -100;
    if (y_off > 200) y_off = 200;
    out.scene_alias[5] = static_cast<char>(y_off & 0xFF);
    out.scene_alias[6] = static_cast<char>((y_off >> 8) & 0xFF);
    // Clamp and store value y offset (-100 to 200, default 0)
    int16_t val_y_off = in.sensor_value_y_offset;
    if (val_y_off < -100) val_y_off = -100;
    if (val_y_off > 200) val_y_off = 200;
    out.scene_alias[7] = static_cast<char>(val_y_off & 0xFF);
    out.scene_alias[8] = static_cast<char>((val_y_off >> 8) & 0xFF);
    // Clamp and store graph height (20-200, default 60)
    uint16_t graph_h = in.sensor_graph_height;
    if (graph_h < 20) graph_h = 20;
    if (graph_h > 200) graph_h = 200;
    out.scene_alias[9] = static_cast<char>(graph_h & 0xFF);
    out.scene_alias[10] = static_cast<char>((graph_h >> 8) & 0xFF);
  } else {
    copyString(in.scene_alias, out.scene_alias, sizeof(out.scene_alias));
  }
  if (in.type == TILE_IMAGE) {
    // TILE_IMAGE speichert image_path nicht im NVS (liegt im dateibasierten Storage).
    out.sensor_entity[0] = '\0';
    out.key_macro[0] = '\0';
    Serial.printf("[TileConfig] packTile - TILE_IMAGE: image_path='%s' (storage)\n",
                  in.image_path.c_str());
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

static void applyImagePathsFromSd(uint16_t folder_id, TileGridConfig& grid) {
  const bool have_storage = storageReady();
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    Tile& tile = grid.tiles[i];
    if (tile.type != TILE_IMAGE && tile.type != TILE_SCENE) continue;
    String sd_path;
    if (have_storage && readImagePathSd(folder_id, i, sd_path)) {
      tile.image_path = sd_path;
      Serial.printf("[TileConfig] applyImagePaths folder=%u idx=%u type=%d -> '%s'\n",
        folder_id, (unsigned)i, tile.type, sd_path.c_str());
      continue;
    }
    if (have_storage && tile.image_path.length() > 0) {
      writeImagePathSd(folder_id, i, tile.image_path);
    }
    if (!have_storage) {
      tile.image_path = "";
    }
  }
}

static void unpackTileV7(const PackedTileV7& in, Tile& out) {
  TileType type = static_cast<TileType>(in.type);
  if (type == TILE_FOLDER) {
    if (in.sensor_decimals == LEGACY_NAV_KIND_SETTINGS) {
      type = TILE_SETTINGS;
    } else if (in.sensor_decimals == LEGACY_NAV_KIND_BACK) {
      type = TILE_BACK;
    }
  }
  out.type = type;
  out.bg_color = in.bg_color;
  out.col = (in.col < GRID_COLS) ? in.col : 0;
  out.row = (in.row < GRID_ROWS) ? in.row : 0;
  uint8_t span_w = (in.span_w < 1) ? 1 : in.span_w;
  uint8_t span_h = (in.span_h < 1) ? 1 : in.span_h;
  if (span_w > GRID_COLS - out.col) span_w = GRID_COLS - out.col;
  if (span_h > GRID_ROWS - out.row) span_h = GRID_ROWS - out.row;
  out.span_w = span_w;
  out.span_h = span_h;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.sensor_display_mode = (in.sensor_gauge_enabled <= 2) ? in.sensor_gauge_enabled : 0;
  out.sensor_gauge_min = in.sensor_gauge_min;
  out.sensor_gauge_max = in.sensor_gauge_max;
  if (shouldNormalizeGaugeRange(out.type)) {
    normalizeGaugeRange(out.sensor_gauge_min, out.sensor_gauge_max);
  }
  out.sensor_gauge_arc = 100;
  out.sensor_gauge_size = 350;
  out.sensor_gauge_y_offset = 12;
  out.sensor_value_y_offset = 0;
  out.sensor_graph_height = 60;
  out.popup_open_mode = TILE_POPUP_OPEN_LONG_PRESS;
  if (out.type == TILE_SENSOR && in.scene_alias[0] == 0x01) {
    uint16_t arc = static_cast<uint8_t>(in.scene_alias[1]) |
                   (static_cast<uint8_t>(in.scene_alias[2]) << 8);
    if (arc >= 90 && arc <= 359) out.sensor_gauge_arc = arc;
    uint16_t size = static_cast<uint8_t>(in.scene_alias[3]) |
                    (static_cast<uint8_t>(in.scene_alias[4]) << 8);
    if (size >= 100 && size <= 800) out.sensor_gauge_size = size;
    int16_t y_off = static_cast<int16_t>(
        static_cast<uint8_t>(in.scene_alias[5]) |
        (static_cast<uint8_t>(in.scene_alias[6]) << 8));
    if (y_off >= -100 && y_off <= 200) out.sensor_gauge_y_offset = y_off;
    int16_t val_y_off = static_cast<int16_t>(
        static_cast<uint8_t>(in.scene_alias[7]) |
        (static_cast<uint8_t>(in.scene_alias[8]) << 8));
    if (val_y_off >= -100 && val_y_off <= 200) out.sensor_value_y_offset = val_y_off;
    uint16_t graph_h = static_cast<uint8_t>(in.scene_alias[9]) |
                       (static_cast<uint8_t>(in.scene_alias[10]) << 8);
    if (graph_h >= 20 && graph_h <= 200) out.sensor_graph_height = graph_h;
  }
  if ((out.type == TILE_SENSOR || out.type == TILE_WEATHER) &&
      in.popup_open_mode == TILE_POPUP_OPEN_SHORT_PRESS) {
    out.popup_open_mode = TILE_POPUP_OPEN_SHORT_PRESS;
  }
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SENSOR || out.type == TILE_WEATHER) {
    out.key_code = 0;
    out.key_modifier = 0;
  } else if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
  out.title = String(in.title);
  out.icon_name = String(in.icon_name);
  out.sensor_entity = String(in.sensor_entity);
  out.sensor_unit = String(in.sensor_unit);
  if (out.type == TILE_SENSOR) {
    out.scene_alias = "";
  } else {
    out.scene_alias = String(in.scene_alias);
  }
  out.key_macro = String(in.key_macro);
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

static void unpackTileV6(const PackedTileV6& in, Tile& out) {
  TileType type = static_cast<TileType>(in.type);
  if (type == TILE_FOLDER) {
    if (in.sensor_decimals == LEGACY_NAV_KIND_SETTINGS) {
      type = TILE_SETTINGS;
    } else if (in.sensor_decimals == LEGACY_NAV_KIND_BACK) {
      type = TILE_BACK;
    }
  }
  out.type = type;
  out.bg_color = in.bg_color;
  out.col = (in.col < GRID_COLS) ? in.col : 0;
  out.row = (in.row < GRID_ROWS) ? in.row : 0;
  uint8_t span_w = (in.span_w < 1) ? 1 : in.span_w;
  uint8_t span_h = (in.span_h < 1) ? 1 : in.span_h;
  if (span_w > GRID_COLS - out.col) span_w = GRID_COLS - out.col;
  if (span_h > GRID_ROWS - out.row) span_h = GRID_ROWS - out.row;
  out.span_w = span_w;
  out.span_h = span_h;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.sensor_display_mode = (in.sensor_gauge_enabled <= 2) ? in.sensor_gauge_enabled : 0;
  out.sensor_gauge_min = in.sensor_gauge_min;
  out.sensor_gauge_max = in.sensor_gauge_max;
  if (shouldNormalizeGaugeRange(out.type)) {
    normalizeGaugeRange(out.sensor_gauge_min, out.sensor_gauge_max);
  }
  // Read gauge appearance from scene_alias for TILE_SENSOR
  out.sensor_gauge_arc = 100;     // Default
  out.sensor_gauge_size = 350;    // Default
  out.sensor_gauge_y_offset = 12; // Default
  out.sensor_value_y_offset = 0;  // Default
  out.sensor_graph_height = 60;   // Default
  out.popup_open_mode = TILE_POPUP_OPEN_LONG_PRESS;
  if (out.type == TILE_SENSOR && in.scene_alias[0] == 0x01) {
    // Magic byte found, extract gauge appearance data
    uint16_t arc = static_cast<uint8_t>(in.scene_alias[1]) |
                   (static_cast<uint8_t>(in.scene_alias[2]) << 8);
    if (arc >= 90 && arc <= 359) out.sensor_gauge_arc = arc;
    uint16_t size = static_cast<uint8_t>(in.scene_alias[3]) |
                    (static_cast<uint8_t>(in.scene_alias[4]) << 8);
    if (size >= 100 && size <= 800) out.sensor_gauge_size = size;
    int16_t y_off = static_cast<int16_t>(
        static_cast<uint8_t>(in.scene_alias[5]) |
        (static_cast<uint8_t>(in.scene_alias[6]) << 8));
    if (y_off >= -100 && y_off <= 200) out.sensor_gauge_y_offset = y_off;
    int16_t val_y_off = static_cast<int16_t>(
        static_cast<uint8_t>(in.scene_alias[7]) |
        (static_cast<uint8_t>(in.scene_alias[8]) << 8));
    if (val_y_off >= -100 && val_y_off <= 200) out.sensor_value_y_offset = val_y_off;
    uint16_t graph_h = static_cast<uint8_t>(in.scene_alias[9]) |
                       (static_cast<uint8_t>(in.scene_alias[10]) << 8);
    if (graph_h >= 20 && graph_h <= 200) out.sensor_graph_height = graph_h;
  }
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SENSOR || out.type == TILE_WEATHER) {
    if (in.key_code == TILE_POPUP_OPEN_SHORT_PRESS ||
        in.key_modifier == TILE_POPUP_OPEN_SHORT_PRESS) {
      out.popup_open_mode = TILE_POPUP_OPEN_SHORT_PRESS;
    }
    out.key_code = 0;
    out.key_modifier = 0;
  } else if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
  out.title = String(in.title);
  out.icon_name = String(in.icon_name);
  out.sensor_entity = String(in.sensor_entity);
  out.sensor_unit = String(in.sensor_unit);
  // For TILE_SENSOR, scene_alias stores gauge appearance, not a scene alias
  if (out.type == TILE_SENSOR) {
    out.scene_alias = "";
  } else {
    out.scene_alias = String(in.scene_alias);
  }
  out.key_macro = String(in.key_macro);
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

static void unpackTileV5(const PackedTileV5& in, Tile& out, uint8_t index) {
  out.type = static_cast<TileType>(in.type);
  if (out.type == TILE_FOLDER && in.sensor_decimals == LEGACY_TAB_SETTINGS) {
    out.type = TILE_SETTINGS;
  }
  out.bg_color = in.bg_color;
  // V5 had no position - migrate to grid position based on index (3x4 grid -> 4x4)
  uint8_t old_col = index % 3;
  uint8_t old_row = index / 3;
  out.col = old_col;
  out.row = old_row;
  out.span_w = 1;
  out.span_h = 1;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.sensor_display_mode = (in.sensor_gauge_enabled != 0) ? 1 : 0;  // Legacy: map bool to mode
  out.sensor_gauge_min = in.sensor_gauge_min;
  out.sensor_gauge_max = in.sensor_gauge_max;
  if (shouldNormalizeGaugeRange(out.type)) {
    normalizeGaugeRange(out.sensor_gauge_min, out.sensor_gauge_max);
  }
  out.sensor_gauge_arc = 100;
  out.sensor_gauge_size = 350;
  out.sensor_gauge_y_offset = 12;
  out.sensor_value_y_offset = 0;
  out.sensor_graph_height = 60;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
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

static void unpackTileV3(const PackedTileV3& in, Tile& out, uint8_t index) {
  out.type = static_cast<TileType>(in.type);
  if (out.type == TILE_FOLDER && in.sensor_decimals == LEGACY_TAB_SETTINGS) {
    out.type = TILE_SETTINGS;
  }
  out.bg_color = in.bg_color;
  out.col = index % 3;
  out.row = index / 3;
  out.span_w = 1;
  out.span_h = 1;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.sensor_display_mode = 0;
  out.sensor_gauge_min = 0;
  out.sensor_gauge_max = 100;
  out.sensor_gauge_arc = 100;
  out.sensor_gauge_size = 350;
  out.sensor_gauge_y_offset = 12;
  out.sensor_value_y_offset = 0;
  out.sensor_graph_height = 60;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
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
    Serial.printf("[TileConfig] unpackTileV3 - TILE_IMAGE: packed(sensor)='%s', packed(macro)='%s', image_path='%s'\n",
                  in.sensor_entity, in.key_macro, out.image_path.c_str());
  } else {
    out.image_path = "";
  }
}

static void unpackTileV2(const PackedTileV2& in, Tile& out, uint8_t index) {
  out.type = static_cast<TileType>(in.type);
  if (out.type == TILE_FOLDER && in.sensor_decimals == LEGACY_TAB_SETTINGS) {
    out.type = TILE_SETTINGS;
  }
  out.bg_color = in.bg_color;
  out.col = index % 3;
  out.row = index / 3;
  out.span_w = 1;
  out.span_h = 1;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.sensor_display_mode = 0;
  out.sensor_gauge_min = 0;
  out.sensor_gauge_max = 100;
  out.sensor_gauge_arc = 100;
  out.sensor_gauge_size = 350;
  out.sensor_gauge_y_offset = 12;
  out.sensor_value_y_offset = 0;
  out.sensor_graph_height = 60;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
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

static void unpackTileV4(const PackedTileV4& in, Tile& out, uint8_t index) {
  out.type = static_cast<TileType>(in.type);
  if (out.type == TILE_FOLDER && in.sensor_decimals == LEGACY_TAB_SETTINGS) {
    out.type = TILE_SETTINGS;
  }
  out.bg_color = in.bg_color;
  out.col = index % 3;
  out.row = index / 3;
  out.span_w = 1;
  out.span_h = 1;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = clampSensorValueFont(in.sensor_value_font);
  out.sensor_display_mode = 0;
  out.sensor_gauge_min = 0;
  out.sensor_gauge_max = 100;
  out.sensor_gauge_arc = 100;
  out.sensor_gauge_size = 350;
  out.sensor_gauge_y_offset = 12;
  out.sensor_value_y_offset = 0;
  out.sensor_graph_height = 60;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
  out.image_slideshow_sec = clampImageSlideshowSeconds(in.image_slideshow_sec);
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

static void unpackTileV1(const PackedTileV1& in, Tile& out, uint8_t index) {
  out.type = static_cast<TileType>(in.type);
  if (out.type == TILE_FOLDER && in.sensor_decimals == LEGACY_TAB_SETTINGS) {
    out.type = TILE_SETTINGS;
  }
  out.bg_color = in.bg_color;
  out.col = index % 3;
  out.row = index / 3;
  out.span_w = 1;
  out.span_h = 1;
  out.sensor_decimals = clampDecimals(in.sensor_decimals);
  if (out.type == TILE_FOLDER || out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.sensor_decimals = 0xFF;
  }
  out.sensor_value_font = 0;
  out.sensor_display_mode = 0;
  out.sensor_gauge_min = 0;
  out.sensor_gauge_max = 100;
  out.sensor_gauge_arc = 100;
  out.sensor_gauge_size = 350;
  out.sensor_gauge_y_offset = 12;
  out.sensor_value_y_offset = 0;
  out.sensor_graph_height = 60;
  out.key_code = in.key_code;
  out.key_modifier = in.key_modifier;
  if (out.type == TILE_SETTINGS || out.type == TILE_BACK) {
    out.key_code = 0;
    out.key_modifier = 0;
  }
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
static bool get_tile_layout_clamped(const Tile& tile, uint8_t& col, uint8_t& row, uint8_t& span_w, uint8_t& span_h) {
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

static void initGridDefaults(TileGridConfig& grid) {
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    grid.tiles[i] = Tile();
    grid.tiles[i].col = i % GRID_COLS;
    grid.tiles[i].row = i / GRID_COLS;
    grid.tiles[i].span_w = 1;
    grid.tiles[i].span_h = 1;
  }
}

static bool find_free_cell_bottom_right(const TileGridConfig& grid, uint8_t& out_col, uint8_t& out_row) {
  bool occupied[GRID_ROWS][GRID_COLS] = {};
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type == TILE_EMPTY) continue;
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t span_w = 1;
    uint8_t span_h = 1;
    if (!get_tile_layout_clamped(tile, col, row, span_w, span_h)) continue;
    mark_occupied(occupied, col, row, span_w, span_h);
  }

  for (int r = GRID_ROWS - 1; r >= 0; --r) {
    for (int c = GRID_COLS - 1; c >= 0; --c) {
      if (!occupied[r][c]) {
        out_col = static_cast<uint8_t>(c);
        out_row = static_cast<uint8_t>(r);
        return true;
      }
    }
  }
  return false;
}

static bool find_free_cell_top_left(const TileGridConfig& grid, uint8_t& out_col, uint8_t& out_row) {
  bool occupied[GRID_ROWS][GRID_COLS] = {};
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type == TILE_EMPTY) continue;
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t span_w = 1;
    uint8_t span_h = 1;
    if (!get_tile_layout_clamped(tile, col, row, span_w, span_h)) continue;
    mark_occupied(occupied, col, row, span_w, span_h);
  }

  for (int r = 0; r < GRID_ROWS; ++r) {
    for (int c = 0; c < GRID_COLS; ++c) {
      if (!occupied[r][c]) {
        out_col = static_cast<uint8_t>(c);
        out_row = static_cast<uint8_t>(r);
        return true;
      }
    }
  }
  return false;
}

static void collectFolderSubtree(const std::vector<FolderEntry>& entries, uint16_t parent_id, std::vector<uint16_t>& out) {
  for (const auto& entry : entries) {
    if (entry.parent_id != parent_id) continue;
    if (entry.id == parent_id) continue;
    out.push_back(entry.id);
    collectFolderSubtree(entries, entry.id, out);
  }
}

bool TileConfig::ensureSettingsTile(TileGridConfig& grid) {
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type == TILE_SETTINGS) {
      return false;
    }
  }

  uint8_t col = 0;
  uint8_t row = 0;
  if (!find_free_cell_bottom_right(grid, col, row)) {
    return false;
  }

  size_t empty_index = TILES_PER_GRID;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (grid.tiles[i].type == TILE_EMPTY) {
      empty_index = i;
      break;
    }
  }
  if (empty_index >= TILES_PER_GRID) {
    return false;
  }

  Tile& tile = grid.tiles[empty_index];
  tile = Tile();
  tile.type = TILE_SETTINGS;
  tile.title = "Settings";
  tile.icon_name = "cog";
  tile.col = col;
  tile.row = row;
  tile.span_w = 1;
  tile.span_h = 1;
  return true;
}

bool TileConfig::ensureBackTile(uint16_t folder_id, TileGridConfig& grid) {
  if (folder_id == kRootFolderId) return false;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type == TILE_BACK) {
      return false;
    }
  }

  uint8_t col = 0;
  uint8_t row = 0;
  if (!find_free_cell_top_left(grid, col, row)) {
    return false;
  }

  size_t empty_index = TILES_PER_GRID;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (grid.tiles[i].type == TILE_EMPTY) {
      empty_index = i;
      break;
    }
  }
  if (empty_index >= TILES_PER_GRID) {
    return false;
  }

  Tile& tile = grid.tiles[empty_index];
  tile = Tile();
  tile.type = TILE_BACK;
  tile.title = "";
  tile.icon_name = "arrow-left";
  tile.col = col;
  tile.row = row;
  tile.span_w = 1;
  tile.span_h = 1;
  return true;
}

static bool buildBlobKey(const char* prefix, char* out, size_t out_len) {
  if (!prefix || !out || out_len < 8) return false;
  int written = snprintf(out, out_len, "%s_blob", prefix);
  return written > 0 && static_cast<size_t>(written) < out_len;
}

static bool buildQuarterBlobKey(const char* prefix, uint8_t quarter, char* out, size_t out_len) {
  if (!prefix || !out || out_len < 8) return false;
  int written = snprintf(out, out_len, "%s_q%u", prefix, static_cast<unsigned>(quarter));
  return written > 0 && static_cast<size_t>(written) < out_len;
}

// Legacy Loader (alte Key/Value-EintrÃ¤ge)
static bool loadGridLegacy(const char* prefix, TileGridConfig& grid) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
    char key[32];

    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i));
    grid.tiles[i].type = static_cast<TileType>(prefs.getUChar(key, TILE_EMPTY));

    // Migrate position from index (3x4 grid)
    grid.tiles[i].col = i % 3;
    grid.tiles[i].row = i / 3;
    grid.tiles[i].span_w = 1;
    grid.tiles[i].span_h = 1;

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
    grid.tiles[i].sensor_display_mode = 0;
    grid.tiles[i].sensor_gauge_min = 0;
    grid.tiles[i].sensor_gauge_max = 100;
    grid.tiles[i].sensor_gauge_arc = 100;
    grid.tiles[i].sensor_gauge_size = 350;
    grid.tiles[i].sensor_gauge_y_offset = 12;
    grid.tiles[i].sensor_value_y_offset = 0;
    grid.tiles[i].sensor_graph_height = 60;

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
  for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
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

static void clearGridStorage(const char* prefix) {
  if (!prefix || !*prefix) return;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("[TileConfig] WARN: NVS nicht verfuegbar zum Loeschen");
    return;
  }
  clearLegacyKeys(prefs, prefix);

  char key[16];
  for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
    if (buildQuarterBlobKey(prefix, static_cast<uint8_t>(q), key, sizeof(key))) {
      prefs.remove(key);
    }
  }
  if (buildBlobKey(prefix, key, sizeof(key))) {
    prefs.remove(key);
  }
  prefs.end();
  Serial.printf("[TileConfig] Storage fuer '%s' geloescht\n", prefix);
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

#if 0
bool TileConfig::load() {
  migrateOldBlobs();  // Migrate old home/game/weather to tab0/tab1/tab2
  clearAllLegacyKeys();  // Aufräumen von Altlasten (vorherige Key/Value-Layouts)
  bool tab0_ok = loadGrid("tab0", tab0_grid);
  bool tab1_ok = true;
  bool tab2_ok = true;
  if (ACTIVE_TILE_TABS > 1) {
    tab1_ok = loadGrid("tab1", tab1_grid);
  } else {
    initGridDefaults(tab1_grid);
    clearGridStorage("tab1");
  }
  if (ACTIVE_TILE_TABS > 2) {
    tab2_ok = loadGrid("tab2", tab2_grid);
  } else {
    initGridDefaults(tab2_grid);
    clearGridStorage("tab2");
  }
  loadTabNames();  // Load custom tab names
  if (add_settings_tile_if_missing(tab0_grid)) {
    if (saveGrid("tab0", tab0_grid)) {
      Serial.println("[TileConfig] Settings tile hinzugefuegt");
    } else {
      Serial.println("[TileConfig] WARN: Settings tile konnte nicht gespeichert werden");
    }
  }
  return tab0_ok && tab1_ok && tab2_ok;
}

bool TileConfig::save(const TileGridConfig& tab0, const TileGridConfig& tab1, const TileGridConfig& tab2) {
  bool tab0_ok = saveGrid("tab0", tab0);
  bool tab1_ok = true;
  bool tab2_ok = true;
  if (ACTIVE_TILE_TABS > 1) {
    tab1_ok = saveGrid("tab1", tab1);
  } else {
    clearGridStorage("tab1");
  }
  if (ACTIVE_TILE_TABS > 2) {
    tab2_ok = saveGrid("tab2", tab2);
  } else {
    clearGridStorage("tab2");
  }

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
    if (ACTIVE_TILE_TABS > 1) {
      ok = saveGrid("tab1", grid);
    } else {
      clearGridStorage("tab1");
      ok = true;
      Serial.println("[TileConfig] Tab1 deaktiviert - Speicher wird uebersprungen");
    }
    if (ok) tab1_grid = grid;
  } else if (strcmp(grid_name, "tab2") == 0) {
    if (ACTIVE_TILE_TABS > 2) {
      ok = saveGrid("tab2", grid);
    } else {
      clearGridStorage("tab2");
      ok = true;
      Serial.println("[TileConfig] Tab2 deaktiviert - Speicher wird uebersprungen");
    }
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
  // Initialize all tiles to empty with default positions
  initGridDefaults(grid);

  // Try file-based storage first
  {
    PackedQuarterGridV6 packed_sd[QUARTERS_PER_GRID]{};
    if (readGridSd(prefix, packed_sd, QUARTERS_PER_GRID)) {
      for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
        for (size_t i = 0; i < TILES_PER_QUARTER; ++i) {
          size_t grid_idx = q * TILES_PER_QUARTER + i;
          unpackTileV6(packed_sd[q].tiles[i], grid.tiles[grid_idx]);
        }
      }
      Serial.printf("[TileConfig] Grid '%s' geladen (storage v%u)\n",
                    prefix, static_cast<unsigned>(packed_sd[0].version));
      applyImagePathsFromSd(prefix, grid);
      return true;
    }
  }

  // Try V6 split format first (four quarter blobs)
  char quarter_keys[QUARTERS_PER_GRID][16];
  bool have_quarter_keys = true;
  for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
    if (!buildQuarterBlobKey(prefix, static_cast<uint8_t>(q), quarter_keys[q], sizeof(quarter_keys[q]))) {
      have_quarter_keys = false;
      break;
    }
  }

  if (have_quarter_keys) {
    Preferences prefs;
    if (prefs.begin(PREF_NAMESPACE, true)) {
      PackedQuarterGridV6 packed[QUARTERS_PER_GRID]{};
      bool ok = true;

      for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
        size_t len = prefs.getBytesLength(quarter_keys[q]);
        if (len < sizeof(PackedQuarterGridV6)) {
          ok = false;
          break;
        }
        size_t read = prefs.getBytes(quarter_keys[q], &packed[q], sizeof(packed[q]));
        if (read != sizeof(packed[q]) ||
            packed[q].version != PACKED_GRID_VERSION ||
            packed[q].quarter_index != static_cast<uint8_t>(q)) {
          ok = false;
          break;
        }
      }

      if (ok) {
        for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
          for (size_t i = 0; i < TILES_PER_QUARTER; ++i) {
            size_t grid_idx = q * TILES_PER_QUARTER + i;
            unpackTileV6(packed[q].tiles[i], grid.tiles[grid_idx]);
          }
        }
        prefs.end();
        Serial.printf("[TileConfig] Grid '%s' geladen (quarters v%u)\n",
                      prefix, static_cast<unsigned>(packed[0].version));
        applyImagePathsFromSd(prefix, grid);
        if (saveGrid(prefix, grid)) {
          clearGridStorage(prefix);
        }
        return true;
      }
      prefs.end();
    }
  }

  // Fallback: Try old single-blob formats (V1-V5) for migration
  char blob_key[16];
  if (!buildBlobKey(prefix, blob_key, sizeof(blob_key))) {
    return false;
  }

  {
    Preferences prefs;
    if (prefs.begin(PREF_NAMESPACE, true)) {
      size_t blob_len = prefs.getBytesLength(blob_key);

      // V5 migration (old 12-tile format)
      if (blob_len >= sizeof(PackedGridV5)) {
        PackedGridV5 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 5) {
          for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
            unpackTileV5(packed.tiles[i], grid.tiles[i], static_cast<uint8_t>(i));
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v5, migrating to v6 quarters)\n", prefix);
          applyImagePathsFromSd(prefix, grid);
          if (saveGrid(prefix, grid)) {
            clearGridStorage(prefix);
          }
          return true;
        }
      }

      if (blob_len >= sizeof(PackedGridV4)) {
        PackedGridV4 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 4) {
          for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
            unpackTileV4(packed.tiles[i], grid.tiles[i], static_cast<uint8_t>(i));
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v4, migrating)\n", prefix);
          applyImagePathsFromSd(prefix, grid);
          if (saveGrid(prefix, grid)) {
            clearGridStorage(prefix);
          }
          return true;
        }
      }

      if (blob_len >= sizeof(PackedGridV3)) {
        PackedGridV3 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 3) {
          for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
            unpackTileV3(packed.tiles[i], grid.tiles[i], static_cast<uint8_t>(i));
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v3, migrating)\n", prefix);
          applyImagePathsFromSd(prefix, grid);
          if (saveGrid(prefix, grid)) {
            clearGridStorage(prefix);
          }
          return true;
        }
      }

      if (blob_len >= sizeof(PackedGridV2)) {
        PackedGridV2 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 2) {
          for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
            unpackTileV2(packed.tiles[i], grid.tiles[i], static_cast<uint8_t>(i));
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v2, migrating)\n", prefix);
          applyImagePathsFromSd(prefix, grid);
          if (saveGrid(prefix, grid)) {
            clearGridStorage(prefix);
          }
          return true;
        }
      }

      if (blob_len >= sizeof(PackedGridV1)) {
        PackedGridV1 packed{};
        size_t read = prefs.getBytes(blob_key, &packed, sizeof(packed));
        if (read == sizeof(packed) && packed.version == 1) {
          for (size_t i = 0; i < OLD_TILES_PER_GRID; ++i) {
            unpackTileV1(packed.tiles[i], grid.tiles[i], static_cast<uint8_t>(i));
          }
          prefs.end();
          Serial.printf("[TileConfig] Grid '%s' geladen (blob v1, migrating)\n", prefix);
          applyImagePathsFromSd(prefix, grid);
          if (saveGrid(prefix, grid)) {
            clearGridStorage(prefix);
          }
          return true;
        }
      }

      prefs.end();
    }
  }

  // Fallback: Legacy-keys laden und sofort migrieren
  bool legacy_ok = loadGridLegacy(prefix, grid);
  if (legacy_ok) {
    applyImagePathsFromSd(prefix, grid);
    if (saveGrid(prefix, grid)) {
      clearGridStorage(prefix);
    }
  }
  return legacy_ok;
}

bool TileConfig::saveGrid(const char* prefix, const TileGridConfig& grid) {
  // V6 stored in file-based storage (single file with 4 quarters)
  if (!storageReady()) {
    Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, Grid kann nicht gespeichert werden");
    return false;
  }

  Serial.printf("[TileConfig] Speichere Grid '%s' (storage, %u x %u bytes)\n",
                prefix,
                static_cast<unsigned>(QUARTERS_PER_GRID),
                static_cast<unsigned>(sizeof(PackedQuarterGridV6)));

  static PackedQuarterGridV6 packed[QUARTERS_PER_GRID];
  memset(packed, 0, sizeof(packed));
  for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
    packed[q].version = PACKED_GRID_VERSION;
    packed[q].quarter_index = static_cast<uint8_t>(q);
    for (size_t i = 0; i < TILES_PER_QUARTER; ++i) {
      size_t grid_idx = q * TILES_PER_QUARTER + i;
      if (grid.tiles[grid_idx].type == TILE_IMAGE || grid.tiles[grid_idx].type == TILE_SCENE) {
        if (!storageReady()) {
          Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, image_path wird nicht gespeichert");
        } else if (!writeImagePathSd(prefix, grid_idx, grid.tiles[grid_idx].image_path)) {
          Serial.println("[TileConfig] WARN: image_path konnte nicht im Storage gespeichert werden");
        }
      }
      packTile(grid.tiles[grid_idx], packed[q].tiles[i]);
    }
  }

  if (!writeGridSd(prefix, packed, QUARTERS_PER_GRID)) {
    Serial.printf("[TileConfig] Fehler beim Speichern von Grid '%s' (storage write failed)\n", prefix);
    return false;
  }

  Serial.printf("[TileConfig] Grid '%s' gespeichert (storage, %u x %u bytes)\n",
                prefix,
                static_cast<unsigned>(QUARTERS_PER_GRID),
                static_cast<unsigned>(sizeof(PackedQuarterGridV6)));
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
#endif

static FolderEntry makeFolderEntry(uint16_t id, uint16_t parent_id, const String& name, const String& icon) {
  FolderEntry entry{};
  entry.id = id;
  entry.parent_id = parent_id;
  String safe_name = name;
  safe_name.trim();
  if (safe_name.length() == 0) safe_name = "Ordner";
  String safe_icon = icon;
  safe_icon.trim();
  copyString(safe_name, entry.name, sizeof(entry.name));
  copyString(safe_icon, entry.icon_name, sizeof(entry.icon_name));
  return entry;
}

bool TileConfig::load() {
  folders.clear();
  bool folders_ok = loadFolders();
  bool had_root = folderExists(kRootFolderId);
  ensureRootFolder();
  if (folders_ok && !had_root) {
    saveFolders();
  }

  active_folder_id = kRootFolderId;
  return loadGrid(active_folder_id, active_grid);
}

bool TileConfig::loadFolderGrid(uint16_t folder_id, TileGridConfig& out) {
  if (!folderExists(folder_id)) return false;
  bool ok = loadGrid(folder_id, out);
  if (ok && folder_id == active_folder_id) {
    active_grid = out;
  }
  return ok;
}

bool TileConfig::saveFolderGrid(uint16_t folder_id, const TileGridConfig& grid) {
  if (!folderExists(folder_id)) return false;
  bool ok = saveGrid(folder_id, grid);
  if (ok && folder_id == active_folder_id) {
    active_grid = grid;
  }
  return ok;
}

bool TileConfig::setActiveFolder(uint16_t folder_id) {
  if (!folderExists(folder_id)) return false;
  const uint16_t previous_folder_id = active_folder_id;
  if (!loadGrid(folder_id, active_grid)) {
    if (previous_folder_id != folder_id && folderExists(previous_folder_id)) {
      loadGrid(previous_folder_id, active_grid);
    }
    return false;
  }
  active_folder_id = folder_id;
  return true;
}

bool TileConfig::setActiveFolderCached(uint16_t folder_id, const TileGridConfig& grid) {
  if (!folderExists(folder_id)) return false;
  active_folder_id = folder_id;
  active_grid = grid;
  return true;
}

const FolderEntry* TileConfig::getFolder(uint16_t folder_id) const {
  for (const auto& entry : folders) {
    if (entry.id == folder_id) return &entry;
  }
  return nullptr;
}

uint16_t TileConfig::getFolderParent(uint16_t folder_id) const {
  const FolderEntry* entry = getFolder(folder_id);
  if (!entry) return kRootFolderId;
  return entry->parent_id;
}

bool TileConfig::folderExists(uint16_t folder_id) const {
  return getFolder(folder_id) != nullptr;
}

uint16_t TileConfig::nextFolderId() const {
  uint16_t max_id = kRootFolderId;
  for (const auto& entry : folders) {
    if (entry.id > max_id) max_id = entry.id;
  }
  if (max_id == 0xFFFF) return kInvalidFolderId;
  return static_cast<uint16_t>(max_id + 1);
}

void TileConfig::ensureRootFolder() {
  if (folderExists(kRootFolderId)) return;
  folders.insert(folders.begin(), makeFolderEntry(kRootFolderId, kRootFolderId, "Home", "home-analytics"));
}

bool TileConfig::loadFolders() {
  if (!storageReady()) {
    Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, Ordner-Liste kann nicht geladen werden");
    return false;
  }
  if (!storageFS().exists(kFolderIndexFile)) {
    return false;
  }
  File f = storageFS().open(kFolderIndexFile, FILE_READ);
  if (!f) return false;

  FolderIndexHeader header{};
  if (f.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }
  if (header.magic != kFolderIndexMagic || header.version != kFolderIndexVersion) {
    f.close();
    return false;
  }

  folders.clear();
  for (uint16_t i = 0; i < header.count; ++i) {
    FolderEntryDisk disk{};
    if (f.read(reinterpret_cast<uint8_t*>(&disk), sizeof(disk)) != sizeof(disk)) {
      break;
    }
    FolderEntry entry{};
    entry.id = disk.id;
    entry.parent_id = disk.parent_id;
    memcpy(entry.name, disk.name, sizeof(entry.name));
    entry.name[sizeof(entry.name) - 1] = '\0';
    memcpy(entry.icon_name, disk.icon_name, sizeof(entry.icon_name));
    entry.icon_name[sizeof(entry.icon_name) - 1] = '\0';
    folders.push_back(entry);
  }

  f.close();
  return true;
}

bool TileConfig::saveFolders() const {
  if (!storageReady()) {
    Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, Ordner-Liste kann nicht gespeichert werden");
    return false;
  }
  if (!ensureTileGridDir()) return false;
  if (storageFS().exists(kFolderIndexFile)) storageFS().remove(kFolderIndexFile);
  File f = storageFS().open(kFolderIndexFile, FILE_WRITE);
  if (!f) return false;

  FolderIndexHeader header{};
  header.magic = kFolderIndexMagic;
  header.version = kFolderIndexVersion;
  header.count = static_cast<uint16_t>(folders.size());
  if (f.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) != sizeof(header)) {
    f.close();
    return false;
  }

  for (const auto& entry : folders) {
    FolderEntryDisk disk{};
    disk.id = entry.id;
    disk.parent_id = entry.parent_id;
    memcpy(disk.name, entry.name, sizeof(disk.name));
    disk.name[sizeof(disk.name) - 1] = '\0';
    memcpy(disk.icon_name, entry.icon_name, sizeof(disk.icon_name));
    disk.icon_name[sizeof(disk.icon_name) - 1] = '\0';
    if (f.write(reinterpret_cast<const uint8_t*>(&disk), sizeof(disk)) != sizeof(disk)) {
      f.close();
      return false;
    }
  }

  f.close();
  return true;
}

bool TileConfig::createFolder(uint16_t parent_id, const String& name, const String& icon, uint16_t& out_id) {
  if (!storageReady()) return false;
  if (!folderExists(parent_id)) parent_id = kRootFolderId;
  uint16_t next_id = nextFolderId();
  if (next_id == kInvalidFolderId) return false;

  FolderEntry entry = makeFolderEntry(next_id, parent_id, name, icon);
  folders.push_back(entry);
  if (!saveFolders()) return false;

  std::unique_ptr<TileGridConfig> grid(new (std::nothrow) TileGridConfig{});
  if (!grid) return false;

  initGridDefaults(*grid);
  ensureBackTile(next_id, *grid);
  if (!saveGrid(next_id, *grid)) {
    return false;
  }

  out_id = next_id;
  return true;
}

bool TileConfig::updateFolder(uint16_t folder_id, const String& name, const String& icon) {
  for (auto& entry : folders) {
    if (entry.id != folder_id) continue;
    String safe_name = name;
    safe_name.trim();
    if (!safe_name.length()) safe_name = "Ordner";
    String safe_icon = icon;
    safe_icon.trim();
    copyString(safe_name, entry.name, sizeof(entry.name));
    copyString(safe_icon, entry.icon_name, sizeof(entry.icon_name));
    return saveFolders();
  }
  return false;
}

bool TileConfig::deleteFolder(uint16_t folder_id) {
  if (folder_id == kRootFolderId) return false;
  if (!folderExists(folder_id)) return false;
  if (!storageReady()) {
    Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, Ordner kann nicht geloescht werden");
    return false;
  }

  std::vector<uint16_t> to_delete;
  to_delete.push_back(folder_id);
  collectFolderSubtree(folders, folder_id, to_delete);

  const std::vector<FolderEntry> original = folders;
  auto should_delete = [&](const FolderEntry& entry) {
    for (uint16_t id : to_delete) {
      if (entry.id == id) return true;
    }
    return false;
  };
  folders.erase(std::remove_if(folders.begin(), folders.end(), should_delete), folders.end());

  if (!saveFolders()) {
    folders = original;
    Serial.println("[TileConfig] WARN: Ordner-Liste konnte nicht gespeichert werden");
    return false;
  }

  for (uint16_t id : to_delete) {
    String grid_path = tileGridFile(id);
    if (storageFS().exists(grid_path)) storageFS().remove(grid_path);
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      String link_path = imagePathFile(id, i);
      if (storageFS().exists(link_path)) storageFS().remove(link_path);
    }
  }

  for (uint16_t id : to_delete) {
    if (active_folder_id == id) {
      setActiveFolder(kRootFolderId);
      break;
    }
  }

  return true;
}

bool TileConfig::loadGrid(uint16_t folder_id, TileGridConfig& grid) {
  initGridDefaults(grid);

  bool ok = false;
  bool needs_migration_save = false;

  static PackedQuarterGridV7 packed_v7[QUARTERS_PER_GRID];
  memset(packed_v7, 0, sizeof(packed_v7));
  if (readGridSd(folder_id, packed_v7, QUARTERS_PER_GRID)) {
    ok = true;
    for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
      for (size_t i = 0; i < TILES_PER_QUARTER; ++i) {
        size_t grid_idx = q * TILES_PER_QUARTER + i;
        unpackTileV7(packed_v7[q].tiles[i], grid.tiles[grid_idx]);
      }
    }
    Serial.printf("[TileConfig] Grid %u geladen (storage v%u)\n",
                  static_cast<unsigned>(folder_id),
                  static_cast<unsigned>(packed_v7[0].version));
  } else {
    static PackedQuarterGridV6 packed_v6[QUARTERS_PER_GRID];
    memset(packed_v6, 0, sizeof(packed_v6));
    if (readGridSdV6(folder_id, packed_v6, QUARTERS_PER_GRID) ||
        (folder_id == kRootFolderId && storageReady() &&
         readPackedGridFileV6(tileGridFileLegacy("tab0"), packed_v6, QUARTERS_PER_GRID))) {
      ok = true;
      needs_migration_save = true;
      for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
        for (size_t i = 0; i < TILES_PER_QUARTER; ++i) {
          size_t grid_idx = q * TILES_PER_QUARTER + i;
          unpackTileV6(packed_v6[q].tiles[i], grid.tiles[grid_idx]);
        }
      }
      Serial.printf("[TileConfig] Grid %u geladen (storage v6)\n",
                    static_cast<unsigned>(folder_id));
    }
  }

  bool changed = false;
  if (folder_id == kRootFolderId) {
    changed = ensureSettingsTile(grid);
  } else {
    changed = ensureBackTile(folder_id, grid);
  }

  if (!ok) {
    Serial.printf("[TileConfig] WARN: Grid %u konnte nicht geladen werden, Storage-Inhalt bleibt unveraendert\n",
                  static_cast<unsigned>(folder_id));
    return false;
  }

  applyImagePathsFromSd(folder_id, grid);

  // Migration: removed tile types → TILE_EMPTY
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (grid.tiles[i].type == TILE_IMAGE || grid.tiles[i].type == TILE_RADAR) {
      grid.tiles[i] = Tile{};
      changed = true;
    }
  }

  if (needs_migration_save || changed) {
    saveGrid(folder_id, grid);
  }
  return true;
}

bool TileConfig::saveGrid(uint16_t folder_id, const TileGridConfig& grid) {
  if (!storageReady()) {
    Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, Grid kann nicht gespeichert werden");
    return false;
  }

  TileGridConfig working = grid;
  if (folder_id == kRootFolderId) {
    ensureSettingsTile(working);
  } else {
    ensureBackTile(folder_id, working);
  }

  Serial.printf("[TileConfig] Speichere Grid %u (storage, %u x %u bytes)\n",
                static_cast<unsigned>(folder_id),
                static_cast<unsigned>(QUARTERS_PER_GRID),
                static_cast<unsigned>(sizeof(PackedQuarterGridV7)));

  static PackedQuarterGridV7 packed[QUARTERS_PER_GRID];
  memset(packed, 0, sizeof(packed));
  for (size_t q = 0; q < QUARTERS_PER_GRID; ++q) {
    packed[q].version = PACKED_GRID_VERSION;
    packed[q].quarter_index = static_cast<uint8_t>(q);
    for (size_t i = 0; i < TILES_PER_QUARTER; ++i) {
      size_t grid_idx = q * TILES_PER_QUARTER + i;
      if (working.tiles[grid_idx].type == TILE_IMAGE || working.tiles[grid_idx].type == TILE_SCENE) {
        if (!storageReady()) {
          Serial.println("[TileConfig] WARN: Storage nicht verfuegbar, image_path wird nicht gespeichert");
        } else if (!writeImagePathSd(folder_id, grid_idx, working.tiles[grid_idx].image_path)) {
          Serial.println("[TileConfig] WARN: image_path konnte nicht im Storage gespeichert werden");
        }
      }
      packTile(working.tiles[grid_idx], packed[q].tiles[i]);
    }
  }

  if (!writeGridSd(folder_id, packed, QUARTERS_PER_GRID)) {
    Serial.printf("[TileConfig] Fehler beim Speichern von Grid %u (storage write failed)\n",
                  static_cast<unsigned>(folder_id));
    return false;
  }

  String legacy_v6 = tileGridFileLegacyV6(folder_id);
  if (storageFS().exists(legacy_v6)) {
    storageFS().remove(legacy_v6);
  }
  if (folder_id == kRootFolderId) {
    String legacy_root = tileGridFileLegacy("tab0");
    if (storageFS().exists(legacy_root)) {
      storageFS().remove(legacy_root);
    }
  }

  Serial.printf("[TileConfig] Grid %u gespeichert (storage, %u x %u bytes)\n",
                static_cast<unsigned>(folder_id),
                static_cast<unsigned>(QUARTERS_PER_GRID),
                static_cast<unsigned>(sizeof(PackedQuarterGridV7)));
  return true;
}

#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include "src/network/network_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/ui/tab_settings.h"
#include "src/game/game_controls_config.h"
#include "src/game/key_parsing.h"
#include "src/tiles/tile_config.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/ui_manager.h"
#include "src/web/web_admin_tile_helpers.h"
#include "src/types/types_registry.h"
#include <algorithm>
#include <vector>
#include <SD.h>
#include <libs/tjpgd/tjpgd.h>
#include <stdlib.h>
#include <string.h>

namespace {

bool endsWithIgnoreCase(const String& value, const char* suffix) {
  if (!suffix) return false;
  String v = value;
  v.toLowerCase();
  String s = suffix;
  s.toLowerCase();
  return v.endsWith(s);
}

String joinPath(const String& dir, const String& name) {
  if (name.startsWith("/")) return name;
  if (dir == "/") return String("/") + name;
  return dir + "/" + name;
}

void collectImageFiles(const String& dir, std::vector<String>& out, size_t max_entries, uint8_t depth, bool allow_bin, bool allow_jpeg, bool allow_png) {
  if (out.size() >= max_entries) return;
  File root = SD.open(dir);
  if (!root) return;

  File file = root.openNextFile();
  while (file) {
    if (out.size() >= max_entries) break;
    const char* name_c = file.name();
    String name = name_c ? String(name_c) : String();
    if (file.isDirectory()) {
      if (depth > 0 && name.length()) {
        collectImageFiles(joinPath(dir, name), out, max_entries, depth - 1, allow_bin, allow_jpeg, allow_png);
      }
    } else if (name.length()) {
      const bool is_bin = endsWithIgnoreCase(name, ".bin");
      const bool is_jpeg = endsWithIgnoreCase(name, ".jpg") || endsWithIgnoreCase(name, ".jpeg");
      const bool is_png = endsWithIgnoreCase(name, ".png");
      if ((allow_bin && is_bin) || (allow_jpeg && is_jpeg) || (allow_png && is_png)) {
        out.push_back(joinPath(dir, name));
      }
    }
    file = root.openNextFile();
  }
}

void appendJsonEscaped(String& out, const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (c == '\"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
}

void appendKeyValueMapJson(String& out, const String& map) {
  out += "{";
  bool first = true;
  int start = 0;

  while (start < map.length()) {
    int eqPos = map.indexOf('=', start);
    if (eqPos < 0) break;

    int endPos = map.indexOf('\n', eqPos);
    if (endPos < 0) endPos = map.length();

    String key = map.substring(start, eqPos);
    String value = map.substring(eqPos + 1, endPos);

    key.trim();
    value.trim();

    if (key.length() > 0 && value.length() > 0) {
      if (!first) out += ",";
      out += "\"";
      appendJsonEscaped(out, key);
      out += "\":\"";
      appendJsonEscaped(out, value);
      out += "\"";
      first = false;
    }

    start = endPos + 1;
  }

  out += "}";
}

struct IconFileInfo {
  String path;
  uint32_t size = 0;
  uint16_t width = 0;
  uint16_t height = 0;
};

struct JpegInfoCtx {
  File* file = nullptr;
};

static size_t jpeg_info_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  JpegInfoCtx* ctx = static_cast<JpegInfoCtx*>(jd->device);
  if (!ctx || !ctx->file) return 0;
  if (buff) return ctx->file->read(buff, ndata);
  ctx->file->seek(ctx->file->position() + ndata);
  return ndata;
}

static bool read_jpeg_dimensions(const String& path, uint16_t& w, uint16_t& h) {
  w = 0;
  h = 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  uint8_t* work = static_cast<uint8_t*>(malloc(4096));
  if (!work) {
    f.close();
    return false;
  }
  JDEC jd;
  JpegInfoCtx ctx{};
  ctx.file = &f;
  JRESULT rc = jd_prepare(&jd, jpeg_info_input, work, 4096, &ctx);
  if (rc == JDR_OK) {
    w = jd.width;
    h = jd.height;
  }
  free(work);
  f.close();
  return rc == JDR_OK;
}

static bool read_png_dimensions(const String& path, uint16_t& w, uint16_t& h) {
  w = 0;
  h = 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  uint8_t buf[24] = {0};
  if (f.read(buf, sizeof(buf)) != sizeof(buf)) {
    f.close();
    return false;
  }
  f.close();
  const uint8_t sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (memcmp(buf, sig, sizeof(sig)) != 0) return false;
  if (memcmp(buf + 12, "IHDR", 4) != 0) return false;
  w = static_cast<uint16_t>((buf[16] << 24) | (buf[17] << 16) | (buf[18] << 8) | buf[19]);
  h = static_cast<uint16_t>((buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23]);
  return (w > 0 && h > 0);
}

struct TileRect {
  uint8_t col;
  uint8_t row;
  uint8_t span_w;
  uint8_t span_h;
};

static bool buildTileRect(uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h, TileRect& out) {
  if (col >= GRID_COLS || row >= GRID_ROWS) return false;
  if (span_w < 1 || span_h < 1) return false;
  if (span_w > GRID_COLS - col) return false;
  if (span_h > GRID_ROWS - row) return false;
  out = TileRect{col, row, span_w, span_h};
  return true;
}

static bool getTileRect(const Tile& tile, TileRect& out) {
  uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;
  return buildTileRect(tile.col, tile.row, span_w, span_h, out);
}

static bool rectsOverlap(const TileRect& a, const TileRect& b) {
  return !(a.col + a.span_w <= b.col ||
           b.col + b.span_w <= a.col ||
           a.row + a.span_h <= b.row ||
           b.row + b.span_h <= a.row);
}

static bool placementOverlaps(const TileGridConfig& grid, size_t self_index, const TileRect& rect, size_t ignore_index = static_cast<size_t>(-1)) {
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (i == self_index || i == ignore_index) continue;
    const Tile& other = grid.tiles[i];
    if (other.type == TILE_EMPTY) continue;
    TileRect other_rect{};
    if (!getTileRect(other, other_rect)) continue;
    if (rectsOverlap(rect, other_rect)) return true;
  }
  return false;
}

static bool indexInList(size_t value, const std::vector<size_t>& values) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

static bool placementOverlapsAny(
    const TileGridConfig& grid,
    size_t self_index,
    const TileRect& rect,
    const std::vector<size_t>& ignore_indices) {
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (i == self_index || indexInList(i, ignore_indices)) continue;
    const Tile& other = grid.tiles[i];
    if (other.type == TILE_EMPTY) continue;
    TileRect other_rect{};
    if (!getTileRect(other, other_rect)) continue;
    if (rectsOverlap(rect, other_rect)) return true;
  }
  return false;
}

struct TilePosSnapshot {
  size_t index;
  uint8_t col;
  uint8_t row;
};

static uint16_t tileArea(const Tile& tile) {
  const uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  const uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;
  return static_cast<uint16_t>(span_w) * static_cast<uint16_t>(span_h);
}

static bool findPlacementForTile(
    TileGridConfig& grid,
    size_t tile_index,
    int preferred_col,
    int preferred_row,
    const std::vector<size_t>& floating_indices) {
  if (tile_index >= TILES_PER_GRID) return false;
  Tile& tile = grid.tiles[tile_index];
  const uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  const uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;

  auto can_place = [&](uint8_t col, uint8_t row) -> bool {
    TileRect rect{};
    if (!buildTileRect(col, row, span_w, span_h, rect)) return false;
    return !placementOverlapsAny(grid, tile_index, rect, floating_indices);
  };

  if (preferred_col >= 0 && preferred_row >= 0) {
    if (can_place(static_cast<uint8_t>(preferred_col), static_cast<uint8_t>(preferred_row))) {
      tile.col = static_cast<uint8_t>(preferred_col);
      tile.row = static_cast<uint8_t>(preferred_row);
      return true;
    }
  }

  for (uint8_t row = 0; row < GRID_ROWS; ++row) {
    for (uint8_t col = 0; col < GRID_COLS; ++col) {
      if (!can_place(col, row)) continue;
      tile.col = col;
      tile.row = row;
      return true;
    }
  }

  return false;
}

static bool applySmartReorder(
    TileGridConfig& grid,
    size_t from_index,
    uint8_t target_col,
    uint8_t target_row) {
  if (from_index >= TILES_PER_GRID) return false;
  Tile& moving_tile = grid.tiles[from_index];
  if (moving_tile.type == TILE_EMPTY) return false;

  const uint8_t from_col = moving_tile.col;
  const uint8_t from_row = moving_tile.row;
  const uint8_t span_w = moving_tile.span_w < 1 ? 1 : moving_tile.span_w;
  const uint8_t span_h = moving_tile.span_h < 1 ? 1 : moving_tile.span_h;

  TileRect target_rect{};
  if (!buildTileRect(target_col, target_row, span_w, span_h, target_rect)) return false;

  std::vector<size_t> displaced_indices;
  std::vector<TilePosSnapshot> snapshots;
  snapshots.push_back(TilePosSnapshot{from_index, from_col, from_row});

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    if (i == from_index) continue;
    const Tile& other = grid.tiles[i];
    if (other.type == TILE_EMPTY) continue;
    TileRect other_rect{};
    if (!getTileRect(other, other_rect)) continue;
    if (!rectsOverlap(target_rect, other_rect)) continue;
    displaced_indices.push_back(i);
    snapshots.push_back(TilePosSnapshot{i, other.col, other.row});
  }

  moving_tile.col = target_col;
  moving_tile.row = target_row;

  std::sort(displaced_indices.begin(), displaced_indices.end(), [&](size_t a, size_t b) {
    const uint16_t area_a = tileArea(grid.tiles[a]);
    const uint16_t area_b = tileArea(grid.tiles[b]);
    if (area_a != area_b) return area_a > area_b;
    return a < b;
  });

  std::vector<size_t> floating_indices = displaced_indices;
  for (size_t displaced_index : displaced_indices) {
    auto it = std::find(floating_indices.begin(), floating_indices.end(), displaced_index);
    if (it != floating_indices.end()) floating_indices.erase(it);

    const int preferred_col = (displaced_index == displaced_indices.front()) ? from_col : grid.tiles[displaced_index].col;
    const int preferred_row = (displaced_index == displaced_indices.front()) ? from_row : grid.tiles[displaced_index].row;
    if (findPlacementForTile(grid, displaced_index, preferred_col, preferred_row, floating_indices)) {
      continue;
    }

    for (const TilePosSnapshot& snapshot : snapshots) {
      if (snapshot.index >= TILES_PER_GRID) continue;
      grid.tiles[snapshot.index].col = snapshot.col;
      grid.tiles[snapshot.index].row = snapshot.row;
    }
    return false;
  }

  return true;
}


static bool parseFolderIdArg(WebServer& server, uint16_t& out) {
  String raw;
  if (server.hasArg("folder")) raw = server.arg("folder");
  else if (server.hasArg("folder_id")) raw = server.arg("folder_id");
  else if (server.hasArg("tab")) {
    String tab = server.arg("tab");
    tab.toLowerCase();
    if (tab == "home" || tab == "tab0") raw = "0";
  }
  raw.trim();
  if (!raw.length()) return false;
  int v = raw.toInt();
  if (v < 0 || v > 0xFFFF) return false;
  out = static_cast<uint16_t>(v);
  return true;
}

}  // namespace

void WebAdminServer::handleSaveMQTT() {
  DeviceConfig cfg{};
  if (configManager.isConfigured()) {
    cfg = configManager.getConfig();
  } else {
    cfg.mqtt_port = 1883;
    strncpy(cfg.mqtt_base_topic, "tab5", CONFIG_MQTT_BASE_MAX - 1);
    strncpy(cfg.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);
  }

  if (server.hasArg("mqtt_host")) {
    copyToBuffer(cfg.mqtt_host, sizeof(cfg.mqtt_host), server.arg("mqtt_host"));
  }
  if (server.hasArg("mqtt_port")) {
    cfg.mqtt_port = server.arg("mqtt_port").toInt();
  }
  if (server.hasArg("mqtt_user")) {
    copyToBuffer(cfg.mqtt_user, sizeof(cfg.mqtt_user), server.arg("mqtt_user"));
  }
  if (server.hasArg("mqtt_pass")) {
    copyToBuffer(cfg.mqtt_pass, sizeof(cfg.mqtt_pass), server.arg("mqtt_pass"));
  }
  if (server.hasArg("mqtt_base")) {
    String base = server.arg("mqtt_base");
    base.trim();
    while (base.endsWith("/")) base.remove(base.length() - 1);
    if (base.isEmpty()) base = "tab5";
    copyToBuffer(cfg.mqtt_base_topic, sizeof(cfg.mqtt_base_topic), base);
  }
  if (server.hasArg("ha_prefix")) {
    String prefix = server.arg("ha_prefix");
    prefix.trim();
    while (prefix.endsWith("/")) prefix.remove(prefix.length() - 1);
    if (prefix.isEmpty()) prefix = "ha/statestream";
    copyToBuffer(cfg.ha_prefix, sizeof(cfg.ha_prefix), prefix);
  }

  if (!cfg.mqtt_host[0]) {
    server.send(400, "text/html", "<h1>Fehler: MQTT-Host ist erforderlich</h1>");
    return;
  }

  if (configManager.save(cfg)) {
    settings_show_mqtt_warning(false);
    // Reload grids im Loop (nicht im Web-Handler)
    tiles_request_reload_all();
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  } else {
    server.send(500, "text/html", "<h1>Speichern fehlgeschlagen</h1>");
  }
}

void WebAdminServer::handleSaveBridge() {
  HaBridgeConfigData updated = haBridgeConfig.get();
  const auto sensors = parseSensorList(updated.sensors_text);
  const auto scenes = parseSceneList(updated.scene_alias_text);
  bool changed = false;

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    String field = "sensor_slot";
    field += static_cast<int>(i);
    String value = server.hasArg(field) ? server.arg(field) : "";
    value = normalizeSensorSelection(value, sensors);
    if (updated.sensor_slots[i] != value) {
      updated.sensor_slots[i] = value;
      changed = true;
    }
    String label_field = "sensor_label";
    label_field += static_cast<int>(i);
    String title = server.hasArg(label_field) ? server.arg(label_field) : "";
    title.trim();
    if (updated.sensor_titles[i] != title) {
      updated.sensor_titles[i] = title;
      changed = true;
    }
    String unit_field = "sensor_unit";
    unit_field += static_cast<int>(i);
    String unit = server.hasArg(unit_field) ? server.arg(unit_field) : "";
    unit.trim();
    if (value.isEmpty()) {
      unit = "";
    }
    if (updated.sensor_custom_units[i] != unit) {
      updated.sensor_custom_units[i] = unit;
      changed = true;
    }

    // Farbe parsen (z.B. "#2A2A2A" → 0x2A2A2A)
    String color_field = "sensor_color";
    color_field += static_cast<int>(i);
    String colorStr = server.hasArg(color_field) ? server.arg(color_field) : "";
    colorStr.trim();

    uint32_t color = 0;
    if (colorStr.length() > 0 && colorStr[0] == '#') {
      colorStr = colorStr.substring(1); // "#" entfernen
      color = strtoul(colorStr.c_str(), nullptr, 16);
    }

    if (updated.sensor_colors[i] != color) {
      updated.sensor_colors[i] = color;
      changed = true;
    }
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    String field = "scene_slot";
    field += static_cast<int>(i);
    String value = server.hasArg(field) ? server.arg(field) : "";
    value = normalizeSceneSelection(value, scenes);
    if (updated.scene_slots[i] != value) {
      updated.scene_slots[i] = value;
      changed = true;
    }
    String label_field = "scene_label";
    label_field += static_cast<int>(i);
    String title = server.hasArg(label_field) ? server.arg(label_field) : "";
    title.trim();
    if (updated.scene_titles[i] != title) {
      updated.scene_titles[i] = title;
      changed = true;
    }

    // Farbe parsen (z.B. "#353535" → 0x353535)
    String color_field = "scene_color";
    color_field += static_cast<int>(i);
    String colorStr = server.hasArg(color_field) ? server.arg(color_field) : "";
    colorStr.trim();

    uint32_t color = 0;
    if (colorStr.length() > 0 && colorStr[0] == '#') {
      colorStr = colorStr.substring(1); // "#" entfernen
      color = strtoul(colorStr.c_str(), nullptr, 16);
    }

    if (updated.scene_colors[i] != color) {
      updated.scene_colors[i] = color;
      changed = true;
    }
  }

  if (!changed) {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
    return;
  }

  if (haBridgeConfig.save(updated)) {
    // Reload grids im Loop (nicht im Web-Handler)
    tiles_request_reload_all();
    mqttReloadDynamicSlots();
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  } else {
    server.send(500, "text/html", "<h1>Speichern fehlgeschlagen</h1>");
  }
}

void WebAdminServer::handleSaveGameControls() {
  GameControlsConfigData updated = gameControlsConfig.get();
  bool changed = false;

  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    // Name
    String name_field = "game_name";
    name_field += String((int)i);
    String name = server.hasArg(name_field) ? server.arg(name_field) : "";
    name.trim();
    if (updated.buttons[i].name != name) {
      updated.buttons[i].name = name;
      changed = true;
    }

    // Makro-String parsen (z.B. "g" oder "ctrl+g" oder "ctrl+shift+a")
    String macro_field = "game_macro";
    macro_field += String((int)i);
    String macro = server.hasArg(macro_field) ? server.arg(macro_field) : "";
    macro.trim();
    macro.toLowerCase();

    // Parse Makro → key_code + modifier
    uint8_t key_code = 0;
    uint8_t modifier = 0;

    parseKeyMacro(macro, key_code, modifier);

    if (updated.buttons[i].key_code != key_code) {
      updated.buttons[i].key_code = key_code;
      changed = true;
    }

    if (updated.buttons[i].modifier != modifier) {
      updated.buttons[i].modifier = modifier;
      changed = true;
    }

    // Farbe parsen (z.B. "#353535" → 0x353535)
    String color_field = "game_color";
    color_field += String((int)i);
    String colorStr = server.hasArg(color_field) ? server.arg(color_field) : "";
    colorStr.trim();

    uint32_t color = 0;
    if (colorStr.length() > 0 && colorStr[0] == '#') {
      colorStr = colorStr.substring(1); // "#" entfernen
      color = strtoul(colorStr.c_str(), nullptr, 16);
    }

    if (updated.buttons[i].color != color) {
      updated.buttons[i].color = color;
      changed = true;
    }
  }

  if (!changed) {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
    return;
  }

  if (gameControlsConfig.save(updated)) {
    // Reload im Loop (nicht im Web-Handler)
    tiles_request_reload_if_loaded(GridType::TAB1);
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
  } else {
    server.send(500, "text/html", "<h1>Speichern fehlgeschlagen</h1>");
  }
}

void WebAdminServer::handleBridgeRefresh() {
  if (!networkManager.isMqttConnected()) {
    server.send(503, "text/html",
                "<h1>MQTT ist nicht verbunden - bitte spaeter erneut versuchen.</h1>");
    return;
  }
  networkManager.publishBridgeRequest();
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
}

void WebAdminServer::handleStatus() {
  server.send(200, "application/json", getStatusJSON());
}

void WebAdminServer::handleRestart() {
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "");
  delay(200);
  ESP.restart();
}

void WebAdminServer::handleGetTiles() {
  // GET /api/tiles?folder=<id>[&index=0-23]
  if (!server.hasArg("tab") && !server.hasArg("folder") && !server.hasArg("folder_id")) {
    server.send(400, "application/json", "{\"error\":\"Missing folder parameter\"}");
    return;
  }

  uint16_t folder_id = 0;
  if (!parseFolderIdArg(server, folder_id) || !tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"error\":\"Folder not found\"}");
    return;
  }

  TileGridConfig grid{};
  tileConfig.loadFolderGrid(folder_id, grid);

  auto appendTileJson = [&](String& out, const Tile& tile) {
    out += "{\"type\":";
    out += String(static_cast<int>(tile.type));
    out += ",\"title\":\"";
    appendJsonEscaped(out, tile.title);
    out += "\",\"icon_name\":\"";
    appendJsonEscaped(out, tile.icon_name);
    out += "\",\"bg_color\":";
    out += String(tile.bg_color);
    out += ",\"col\":";
    out += String(tile.col);
    out += ",\"row\":";
    out += String(tile.row);
    out += ",\"span_w\":";
    out += String(tile.span_w);
    out += ",\"span_h\":";
    out += String(tile.span_h);
    out += ",\"sensor_entity\":\"";
    appendJsonEscaped(out, tile.sensor_entity);
    out += "\",\"sensor_unit\":\"";
    appendJsonEscaped(out, tile.sensor_unit);
    out += "\",\"sensor_decimals\":";
    out += String(tile.sensor_decimals == 0xFF ? -1 : static_cast<int>(tile.sensor_decimals));
    out += ",\"sensor_value_font\":";
    out += String(tile.sensor_value_font);
    out += ",\"sensor_display_mode\":";
    out += String(tile.sensor_display_mode);
    out += ",\"sensor_gauge_min\":";
    out += String(tile.sensor_gauge_min);
    out += ",\"sensor_gauge_max\":";
    out += String(tile.sensor_gauge_max);
    out += ",\"sensor_gauge_arc\":";
    out += String(tile.sensor_gauge_arc);
    out += ",\"sensor_gauge_size\":";
    out += String(tile.sensor_gauge_size);
    out += ",\"sensor_gauge_y_offset\":";
    out += String(tile.sensor_gauge_y_offset);
    out += ",\"sensor_value_y_offset\":";
    out += String(tile.sensor_value_y_offset);
    out += ",\"sensor_graph_height\":";
    out += String(tile.sensor_graph_height);
    out += ",\"scene_alias\":\"";
    appendJsonEscaped(out, tile.scene_alias);
    out += "\",\"key_macro\":\"";
    appendJsonEscaped(out, tile.key_macro);
    out += "\",\"key_code\":";
    out += String(tile.key_code);
    out += ",\"key_modifier\":";
    out += String(tile.key_modifier);
    out += ",\"switch_style\":";
    out += String((tile.type == TILE_SWITCH && tile.sensor_decimals == 1) ? 1 : 0);
    out += ",\"image_path\":\"";
    appendJsonEscaped(out, tile.image_path);
    out += "\",\"image_slideshow_sec\":";
    out += String(tile.image_slideshow_sec);
    out += ",\"navigate_target\":";
    out += String((tile.type == TILE_FOLDER) ? getNavigateTargetId(tile) : 0);
    out += "}";
  };

  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    if (index < 0 || index >= TILES_PER_GRID) {
      server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
      return;
    }

    String json;
    appendTileJson(json, grid.tiles[index]);
    server.send(200, "application/json", json);
    return;
  }

  String json = "[";
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    if (i > 0) json += ",";
    appendTileJson(json, grid.tiles[i]);
  }
  json += "]";

  server.send(200, "application/json", json);
}


void WebAdminServer::handleSaveTiles() {
  // POST /api/tiles
  if (!server.hasArg("index") || !server.hasArg("type")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    return;
  }

  uint16_t folder_id = 0;
  if (!parseFolderIdArg(server, folder_id) || !tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }

  int index = server.arg("index").toInt();
  int type = server.arg("type").toInt();

  if (index < 0 || index >= TILES_PER_GRID) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid parameters\"}");
    return;
  }

  TileGridConfig grid{};
  tileConfig.loadFolderGrid(folder_id, grid);

  Tile& tile = grid.tiles[index];
  Tile previous_tile = tile;
  const bool is_root = (folder_id == 0);
  const bool was_settings_tile = is_root && previous_tile.type == TILE_SETTINGS;
  const bool was_back_tile = (!is_root) && previous_tile.type == TILE_BACK;
  const bool force_settings_tile = was_settings_tile;
  const bool force_back_tile = was_back_tile;

  if (force_settings_tile) type = TILE_SETTINGS;
  if (force_back_tile) type = TILE_BACK;

  if (type == TILE_SETTINGS && !is_root) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Settings tile only allowed in Home\"}");
    return;
  }
  if (type == TILE_BACK && is_root) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Back tile only allowed in folders\"}");
    return;
  }
  if (type == TILE_SETTINGS && !force_settings_tile) {
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      if (i == static_cast<size_t>(index)) continue;
      if (grid.tiles[i].type == TILE_SETTINGS) {
        server.send(409, "application/json", "{\"success\":false,\"error\":\"Settings tile already exists\"}");
        return;
      }
    }
  }
  if (type == TILE_BACK && !force_back_tile) {
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      if (i == static_cast<size_t>(index)) continue;
      if (grid.tiles[i].type == TILE_BACK) {
        server.send(409, "application/json", "{\"success\":false,\"error\":\"Back tile already exists\"}");
        return;
      }
    }
  }

  const bool deleting_folder = (previous_tile.type == TILE_FOLDER && type == TILE_EMPTY);

  // Update tile data
  tile.type = static_cast<TileType>(type);
  tile.title = server.hasArg("title") ? server.arg("title") : "";
  tile.icon_name = server.hasArg("icon_name") ? server.arg("icon_name") : "";
  if (server.hasArg("image_slideshow_sec")) {
    int raw = server.arg("image_slideshow_sec").toInt();
    if (raw <= 0) raw = 10;
    if (raw > 3600) raw = 3600;
    tile.image_slideshow_sec = static_cast<uint16_t>(raw);
  }

  // Parse color
  if (server.hasArg("bg_color")) {
    tile.bg_color = server.arg("bg_color").toInt();
  }

  // Parse layout (0-based col/row, span >= 1)
  uint8_t col = tile.col;
  uint8_t row = tile.row;
  uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;

  if (server.hasArg("col")) {
    int raw = server.arg("col").toInt();
    if (raw < 0) raw = 0;
    if (raw >= GRID_COLS) raw = GRID_COLS - 1;
    col = static_cast<uint8_t>(raw);
  }
  if (server.hasArg("row")) {
    int raw = server.arg("row").toInt();
    if (raw < 0) raw = 0;
    if (raw >= GRID_ROWS) raw = GRID_ROWS - 1;
    row = static_cast<uint8_t>(raw);
  }
  if (server.hasArg("span_w")) {
    int raw = server.arg("span_w").toInt();
    if (raw < 1) raw = 1;
    if (raw > GRID_COLS) raw = GRID_COLS;
    span_w = static_cast<uint8_t>(raw);
  }
  if (server.hasArg("span_h")) {
    int raw = server.arg("span_h").toInt();
    if (raw < 1) raw = 1;
    if (raw > GRID_ROWS) raw = GRID_ROWS;
    span_h = static_cast<uint8_t>(raw);
  }

  if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
  if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;

  tile.col = col;
  tile.row = row;
  tile.span_w = span_w;
  tile.span_h = span_h;

  // Type-specific fields
  String error_message;
  TileTypeApplyContext apply_ctx;
  apply_ctx.folder_id = folder_id;
  apply_ctx.tile_config = &tileConfig;
  apply_ctx.error_message = &error_message;
  const TileTypeDescriptor* desc = get_tile_type_descriptor(tile.type);
  if (desc && desc->apply) {
    if (!desc->apply(server, tile, apply_ctx)) {
      tile = previous_tile;
      String err = error_message;
      if (!err.length()) {
        err = (type == TILE_FOLDER) ? "Folder create failed" : "Tile apply failed";
      }
      server.send(500, "application/json", String("{\"success\":false,\"error\":\"") + err + "\"}");
      return;
    }
  }

  if (deleting_folder) {
    const uint16_t target_id = getNavigateTargetId(previous_tile);
    if (target_id != 0) {
      if (!tileConfig.deleteFolder(target_id)) {
        tile = previous_tile;
        server.send(500, "application/json", "{\"success\":false,\"error\":\"Folder delete failed\"}");
        return;
      }
      tiles_invalidate_folder(target_id);
    }
  }

  if (tile.type != TILE_EMPTY) {
    TileRect rect{};
    if (!buildTileRect(col, row, span_w, span_h, rect)) {
      tile = previous_tile;
      server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid layout\"}");
      return;
    }
    if (placementOverlaps(grid, index, rect)) {
      tile = previous_tile;
      server.send(409, "application/json", "{\"success\":false,\"error\":\"Tile overlaps\"}");
      return;
    }
  }

  bool success = tileConfig.saveFolderGrid(folder_id, grid);
  if (success) {
    Serial.printf("[WebAdmin] Tile folder %u[%d] gespeichert - Type: %d\n", static_cast<unsigned>(folder_id), index, type);

    // Rebuild MQTT dynamic routes for new sensor entities
    mqttReloadDynamicSlots();
    Serial.println("[WebAdmin] MQTT Routes neu aufgebaut");

    tiles_invalidate_folder(folder_id);
    if (tileConfig.getActiveFolderId() == folder_id) {
      tiles_request_reload_if_loaded(GridType::TAB0);
    }

    server.send(200, "application/json", "{\"success\":true}");
  } else {
    Serial.printf("[WebAdmin] Fehler beim Speichern von Tile folder %u[%d]\n", static_cast<unsigned>(folder_id), index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Save failed\"}");
  }
}


void WebAdminServer::handleReorderTiles() {
  if (!server.hasArg("from") || !server.hasArg("to")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    return;
  }

  uint16_t folder_id = 0;
  if (!parseFolderIdArg(server, folder_id) || !tileConfig.folderExists(folder_id)) {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Folder not found\"}");
    return;
  }

  int from = server.arg("from").toInt();
  int to = server.arg("to").toInt();

  if (from < 0 || from >= TILES_PER_GRID || to < 0 || to >= TILES_PER_GRID) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid parameters\"}");
    return;
  }

  TileGridConfig grid{};
  tileConfig.loadFolderGrid(folder_id, grid);

  Tile& tile_to = grid.tiles[to];

  int target_col_raw = server.hasArg("target_col") ? server.arg("target_col").toInt() : -1;
  int target_row_raw = server.hasArg("target_row") ? server.arg("target_row").toInt() : -1;
  uint8_t target_col = (target_col_raw >= 0 && target_col_raw < GRID_COLS) ? static_cast<uint8_t>(target_col_raw) : tile_to.col;
  uint8_t target_row = (target_row_raw >= 0 && target_row_raw < GRID_ROWS) ? static_cast<uint8_t>(target_row_raw) : tile_to.row;

  if (target_col >= GRID_COLS || target_row >= GRID_ROWS) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid target\"}");
    return;
  }

  if (!applySmartReorder(grid, static_cast<size_t>(from), target_col, target_row)) {
    server.send(409, "application/json", "{\"success\":false,\"error\":\"Tile overlaps\"}");
    return;
  }

  bool success = tileConfig.saveFolderGrid(folder_id, grid);
  if (success) {
    mqttReloadDynamicSlots();
    tiles_invalidate_folder(folder_id);
    if (tileConfig.getActiveFolderId() == folder_id) {
      tiles_request_reload_if_loaded(GridType::TAB0);
    }
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Save failed\"}");
  }
}

void WebAdminServer::handleGetSensorValues() {
  const HaBridgeConfigData& ha = haBridgeConfig.get();

  Serial.println("[WebAdmin] /api/sensor_values Request");
  Serial.print("[WebAdmin] sensor_values_map: ");
  Serial.println(ha.sensor_values_map);

  // Build JSON response with values + meta
  String json = "{";
  json += "\"values\":";
  appendKeyValueMapJson(json, ha.sensor_values_map);
  json += ",\"units\":";
  appendKeyValueMapJson(json, ha.sensor_units_map);
  json += ",\"icons\":";
  appendKeyValueMapJson(json, ha.entity_icons_map);
  json += ",\"names\":";
  appendKeyValueMapJson(json, ha.sensor_names_map);
  json += "}";
  Serial.print("[WebAdmin] Sending JSON: ");
  Serial.println(json);
  server.send(200, "application/json", json);
}

void WebAdminServer::handleGetSdImages() {
  std::vector<String> files;
  collectImageFiles("/", files, 200, 3, true, true, false);

  String json = "[";
  for (size_t i = 0; i < files.size(); ++i) {
    if (i > 0) json += ",";
    json += "\"";
    appendJsonEscaped(json, files[i]);
    json += "\"";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void WebAdminServer::handleGetSdIcons() {
  if (!SD.exists("/icons")) SD.mkdir("/icons");
  std::vector<IconFileInfo> files;
  std::vector<String> paths;
  collectImageFiles("/icons", paths, 100, 1, false, true, true);
  for (const auto& path : paths) {
    IconFileInfo info;
    info.path = path;
    File f = SD.open(path, FILE_READ);
    if (f) {
      info.size = static_cast<uint32_t>(f.size());
      f.close();
    }
    if (endsWithIgnoreCase(path, ".png")) {
      read_png_dimensions(path, info.width, info.height);
    } else {
      read_jpeg_dimensions(path, info.width, info.height);
    }
    files.push_back(info);
  }

  String json = "[";
  for (size_t i = 0; i < files.size(); ++i) {
    if (i > 0) json += ",";
    json += "{\"path\":\"";
    appendJsonEscaped(json, files[i].path);
    json += "\",\"size\":";
    json += String(files[i].size);
    json += ",\"w\":";
    json += String(files[i].width);
    json += ",\"h\":";
    json += String(files[i].height);
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void WebAdminServer::handleUploadIcon() {
  HTTPUpload& upload = server.upload();
  static File uploadFile;

  if (upload.status == UPLOAD_FILE_START) {
    if (!SD.exists("/icons")) SD.mkdir("/icons");
    String filename = upload.filename;
    if (filename.indexOf('/') < 0) filename = "/icons/" + filename;
    if (SD.exists(filename)) SD.remove(filename);
    uploadFile = SD.open(filename, FILE_WRITE);
    if (!uploadFile) {
      Serial.println("[Icons] Upload open failed");
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("[Icons] Uploaded %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
    }
  }
}

void WebAdminServer::handleUploadIconDone() {
  HTTPUpload& upload = server.upload();
  String path = "/icons/" + upload.filename;
  String json = "{\"ok\":true,\"path\":\"";
  appendJsonEscaped(json, path);
  json += "\"}";
  server.send(200, "application/json", json);
}

// ========== Folder API ==========

void WebAdminServer::handleGetFolders() {
  const auto& folders = tileConfig.getFolders();
  String json = "[";
  for (size_t i = 0; i < folders.size(); ++i) {
    const auto& entry = folders[i];
    if (i > 0) json += ",";
    json += "{\"id\":";
    json += String(entry.id);
    json += ",\"parent_id\":";
    json += String(entry.parent_id);
    json += ",\"name\":\"";
    appendJsonEscaped(json, entry.name);
    json += "\",\"icon_name\":\"";
    appendJsonEscaped(json, entry.icon_name);
    json += "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

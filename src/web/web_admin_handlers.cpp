#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include "src/network/network_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/ui/tab_settings.h"
#include "src/game/game_controls_config.h"
#include "src/tiles/tile_config.h"
#include "src/ui/tab_tiles_unified.h"
#include <algorithm>

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
    // Reload all tile grids after MQTT config change
    tiles_reload_layout(GridType::HOME);
    tiles_reload_layout(GridType::GAME);
    tiles_reload_layout(GridType::WEATHER);
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
    // Reload all tile grids after bridge config change
    tiles_reload_layout(GridType::HOME);
    tiles_reload_layout(GridType::GAME);
    tiles_reload_layout(GridType::WEATHER);
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

    if (macro.length() > 0) {
      // Modifier extrahieren
      if (macro.indexOf("ctrl+") >= 0) { modifier |= 0x01; macro.replace("ctrl+", ""); }
      if (macro.indexOf("shift+") >= 0) { modifier |= 0x02; macro.replace("shift+", ""); }
      if (macro.indexOf("alt+") >= 0) { modifier |= 0x04; macro.replace("alt+", ""); }

      // Taste zu Scancode konvertieren
      macro.trim();
      if (macro.length() == 1 && macro[0] >= 'a' && macro[0] <= 'z') {
        key_code = 0x04 + (macro[0] - 'a');  // a=0x04, b=0x05, ..., z=0x1D
      } else if (macro.length() == 1 && macro[0] >= '0' && macro[0] <= '9') {
        key_code = 0x1E + (macro[0] - '0');  // 0=0x27, 1=0x1E, ..., 9=0x26
      } else if (macro == "space") key_code = 0x2C;
      else if (macro == "enter") key_code = 0x28;
      else if (macro == "backspace") key_code = 0x2A;
      else if (macro == "tab") key_code = 0x2B;
      else if (macro == "esc" || macro == "escape") key_code = 0x29;
      // sonst 0 (keine Taste)
    }

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
    // Reload game tile grid after game controls change
    tiles_reload_layout(GridType::GAME);
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
  // GET /api/tiles?tab=home|game|weather[&index=0-11]
  // If index is omitted, return all tiles as array
  if (!server.hasArg("tab")) {
    server.send(400, "application/json", "{\"error\":\"Missing tab parameter\"}");
    return;
  }

  String tab = server.arg("tab");
  if (tab != "home" && tab != "game" && tab != "weather" && tab != "tab0" && tab != "tab1" && tab != "tab2") {
    server.send(400, "application/json", "{\"error\":\"Invalid tab\"}");
    return;
  }

  const TileGridConfig& grid = (tab == "home" || tab == "tab0") ? tileConfig.getTab0Grid() : ((tab == "game" || tab == "tab1") ? tileConfig.getTab1Grid() : tileConfig.getTab2Grid());

  // Single tile request
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    if (index < 0 || index >= TILES_PER_GRID) {
      server.send(400, "application/json", "{\"error\":\"Invalid index\"}");
      return;
    }

    const Tile& tile = grid.tiles[index];

    // Build JSON response for single tile
    String json = "{";
    json += "\"type\":" + String((int)tile.type) + ",";
    json += "\"title\":\"";
    json += tile.title;
    json += "\",\"bg_color\":" + String(tile.bg_color) + ",";
    json += "\"sensor_entity\":\"";
    json += tile.sensor_entity;
    json += "\",\"sensor_unit\":\"";
    json += tile.sensor_unit;
    json += "\",\"sensor_decimals\":";
    json += String(tile.sensor_decimals == 0xFF ? -1 : (int)tile.sensor_decimals);
    json += ",\"scene_alias\":\"";
    json += tile.scene_alias;
    json += "\",\"key_macro\":\"";
    json += tile.key_macro;
    json += "\",\"key_code\":" + String(tile.key_code) + ",";
    json += "\"key_modifier\":" + String(tile.key_modifier);
    json += "}";

    server.send(200, "application/json", json);
    return;
  }

  // All tiles request - return array
  String json = "[";
  for (uint8_t i = 0; i < TILES_PER_GRID; i++) {
    const Tile& tile = grid.tiles[i];

    if (i > 0) json += ",";
    json += "{";
    json += "\"type\":" + String((int)tile.type) + ",";
    json += "\"title\":\"";
    json += tile.title;
    json += "\",\"bg_color\":" + String(tile.bg_color) + ",";
    json += "\"sensor_entity\":\"";
    json += tile.sensor_entity;
    json += "\",\"sensor_unit\":\"";
    json += tile.sensor_unit;
    json += "\",\"sensor_decimals\":";
    json += String(tile.sensor_decimals == 0xFF ? -1 : (int)tile.sensor_decimals);
    json += ",\"scene_alias\":\"";
    json += tile.scene_alias;
    json += "\",\"key_macro\":\"";
    json += tile.key_macro;
    json += "\",\"key_code\":" + String(tile.key_code) + ",";
    json += "\"key_modifier\":" + String(tile.key_modifier);
    json += "}";
  }
  json += "]";

  server.send(200, "application/json", json);
}

void WebAdminServer::handleSaveTiles() {
  // POST /api/tiles
  if (!server.hasArg("tab") || !server.hasArg("index") || !server.hasArg("type")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    return;
  }

  String tab = server.arg("tab");
  int index = server.arg("index").toInt();
  int type = server.arg("type").toInt();

  if ((tab != "home" && tab != "game" && tab != "weather" && tab != "tab0" && tab != "tab1" && tab != "tab2") || index < 0 || index >= TILES_PER_GRID) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid parameters\"}");
    return;
  }

  // Get current grid (mutable reference)
  TileGridConfig& grid = (tab == "home" || tab == "tab0") ? tileConfig.getTab0Grid() : ((tab == "game" || tab == "tab1") ? tileConfig.getTab1Grid() : tileConfig.getTab2Grid());
  Tile& tile = grid.tiles[index];

  // Update tile data
  tile.type = static_cast<TileType>(type);
  tile.title = server.hasArg("title") ? server.arg("title") : "";

  // Parse color
  if (server.hasArg("bg_color")) {
    tile.bg_color = server.arg("bg_color").toInt();
  }

  // Type-specific fields
  if (type == TILE_SENSOR) {
    tile.sensor_entity = server.hasArg("sensor_entity") ? server.arg("sensor_entity") : "";
    tile.sensor_unit = server.hasArg("sensor_unit") ? server.arg("sensor_unit") : "";

    uint8_t decimals = 0xFF;
    if (server.hasArg("sensor_decimals")) {
      String decStr = server.arg("sensor_decimals");
      decStr.trim();
      if (decStr.length() > 0) {
        int dec = decStr.toInt();
        if (dec < 0) dec = 0;
        if (dec > 6) dec = 6;
        decimals = static_cast<uint8_t>(dec);
      }
    }
    tile.sensor_decimals = decimals;
  } else if (type == TILE_SCENE) {
    tile.scene_alias = server.hasArg("scene_alias") ? server.arg("scene_alias") : "";
    tile.sensor_decimals = 0xFF;
  } else if (type == TILE_KEY) {
    tile.key_macro = server.hasArg("key_macro") ? server.arg("key_macro") : "";
    tile.sensor_decimals = 0xFF;

    // Parse macro to key_code and modifier
    String macro = tile.key_macro;
    macro.toLowerCase();

    uint8_t modifier = 0;
    uint8_t key_code = 0;

    // Parse modifiers
    if (macro.indexOf("ctrl+") >= 0) { modifier |= 0x01; macro.replace("ctrl+", ""); }
    if (macro.indexOf("shift+") >= 0) { modifier |= 0x02; macro.replace("shift+", ""); }
    if (macro.indexOf("alt+") >= 0) { modifier |= 0x04; macro.replace("alt+", ""); }

    // Parse key
    macro.trim();
    if (macro.length() == 1 && macro[0] >= 'a' && macro[0] <= 'z') {
      key_code = 0x04 + (macro[0] - 'a');
    } else if (macro.length() == 1 && macro[0] >= '0' && macro[0] <= '9') {
      key_code = 0x1E + (macro[0] - '0');
    } else if (macro == "space") key_code = 0x2C;
    else if (macro == "enter") key_code = 0x28;
    else if (macro == "backspace") key_code = 0x2A;
    else if (macro == "tab") key_code = 0x2B;
    else if (macro == "esc" || macro == "escape") key_code = 0x29;

    tile.key_code = key_code;
    tile.key_modifier = modifier;
  } else {
    tile.sensor_decimals = 0xFF;
  }

  // Save to NVS (nur das betroffene Grid)
  const char* grid_name = (tab == "home" || tab == "tab0") ? "tab0" : ((tab == "game" || tab == "tab1") ? "tab1" : "tab2");
  bool success = tileConfig.saveSingleGrid(grid_name, grid);

  if (success) {
    Serial.printf("[WebAdmin] Tile %s[%d] gespeichert - Type: %d\n", tab.c_str(), index, type);

    // Rebuild MQTT dynamic routes for new sensor entities
    mqttReloadDynamicSlots();
    Serial.println("[WebAdmin] MQTT Routes neu aufgebaut");

    // Update only the changed tile on display to avoid flicker
    GridType gridType = (tab == "home" || tab == "tab0") ? GridType::TAB0 : ((tab == "game" || tab == "tab1") ? GridType::TAB1 : GridType::TAB2);
    tiles_update_tile(gridType, static_cast<uint8_t>(index));
    Serial.printf("[WebAdmin] Tile %s[%d] aktualisiert\n", tab.c_str(), index);

    server.send(200, "application/json", "{\"success\":true}");
  } else {
    Serial.printf("[WebAdmin] Fehler beim Speichern von Tile %s[%d]\n", tab.c_str(), index);
    server.send(500, "application/json", "{\"success\":false,\"error\":\"Save failed\"}");
  }
}

void WebAdminServer::handleReorderTiles() {
  // POST /api/tiles/reorder with tab=home|game|weather, from, to
  if (!server.hasArg("tab") || !server.hasArg("from") || !server.hasArg("to")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing parameters\"}");
    return;
  }

  String tab = server.arg("tab");
  int from = server.arg("from").toInt();
  int to = server.arg("to").toInt();

  if ((tab != "home" && tab != "game" && tab != "weather" && tab != "tab0" && tab != "tab1" && tab != "tab2") || from < 0 || from >= TILES_PER_GRID || to < 0 || to >= TILES_PER_GRID) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid parameters\"}");
    return;
  }

  TileGridConfig& grid = (tab == "home" || tab == "tab0") ? tileConfig.getTab0Grid() : ((tab == "game" || tab == "tab1") ? tileConfig.getTab1Grid() : tileConfig.getTab2Grid());
  std::swap(grid.tiles[from], grid.tiles[to]);

  const char* grid_name = (tab == "home" || tab == "tab0") ? "tab0" : ((tab == "game" || tab == "tab1") ? "tab1" : "tab2");
  bool success = tileConfig.saveSingleGrid(grid_name, grid);
  if (success) {
    mqttReloadDynamicSlots();
    GridType gridType = (tab == "home" || tab == "tab0") ? GridType::TAB0 : ((tab == "game" || tab == "tab1") ? GridType::TAB1 : GridType::TAB2);
    tiles_update_tile(gridType, static_cast<uint8_t>(from));
    tiles_update_tile(gridType, static_cast<uint8_t>(to));
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

  // Build JSON response with sensor values
  String json = "{";
  bool first = true;

  // Parse sensor_values_map (format: "entity1=value1\nentity2=value2\n...")
  const String& valuesMap = ha.sensor_values_map;
  int start = 0;

  while (start < valuesMap.length()) {
    int eqPos = valuesMap.indexOf('=', start);
    if (eqPos < 0) break;

    int endPos = valuesMap.indexOf('\n', eqPos);
    if (endPos < 0) endPos = valuesMap.length();

    String entity = valuesMap.substring(start, eqPos);
    String value = valuesMap.substring(eqPos + 1, endPos);

    entity.trim();
    value.trim();

    if (entity.length() > 0 && value.length() > 0) {
      if (!first) json += ",";
      json += "\"";
      json += entity;
      json += "\":\"";

      // Escape quotes in value
      for (int i = 0; i < value.length(); i++) {
        char c = value.charAt(i);
        if (c == '"' || c == '\\') json += '\\';
        json += c;
      }

      json += "\"";
      first = false;
    }

    start = endPos + 1;
  }

  json += "}";
  Serial.print("[WebAdmin] Sending JSON: ");
  Serial.println(json);
  server.send(200, "application/json", json);
}

// ========== Tab Names API ==========

void WebAdminServer::handleGetTabs() {
  String json = "{\"tabs\":[";
  json += "{\"id\":0,\"name\":\"";
  json += tileConfig.getTabName(0);
  json += "\",\"type\":\"tab0\"},";
  json += "{\"id\":1,\"name\":\"";
  json += tileConfig.getTabName(1);
  json += "\",\"type\":\"tab1\"},";
  json += "{\"id\":2,\"name\":\"";
  json += tileConfig.getTabName(2);
  json += "\",\"type\":\"tab2\"}";
  json += "]}";
  server.send(200, "application/json", json);
  Serial.println("[WebAdmin] Tab names sent");
}

void WebAdminServer::handleRenameTab() {
  if (!server.hasArg("tab") || !server.hasArg("name")) {
    server.send(400, "application/json", "{\"error\":\"Missing tab or name parameter\"}");
    return;
  }

  String tab = server.arg("tab");
  String name = server.arg("name");

  uint8_t tab_index = 255;
  if (tab == "home" || tab == "0" || tab == "tab0") {
    tab_index = 0;
  } else if (tab == "game" || tab == "1" || tab == "tab1") {
    tab_index = 1;
  } else if (tab == "weather" || tab == "2" || tab == "tab2") {
    tab_index = 2;
  }

  if (tab_index == 255) {
    server.send(400, "application/json", "{\"error\":\"Invalid tab\"}");
    return;
  }

  tileConfig.setTabName(tab_index, name.c_str());
  tileConfig.saveTabNames();

  server.send(200, "application/json", "{\"success\":true}");
  Serial.printf("[WebAdmin] Tab %u renamed to: %s\n", tab_index, name.c_str());
}

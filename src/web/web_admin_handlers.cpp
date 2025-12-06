#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include "src/network/network_manager.h"
#include "src/ui/tab_home.h"
#include "src/network/mqtt_handlers.h"
#include "src/ui/tab_settings.h"
#include "src/game/game_controls_config.h"

// Forward declaration - kein Include von tab_game.h nötig
extern void game_reload_layout();

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
    home_reload_layout();
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
  }

  if (!changed) {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
    return;
  }

  if (haBridgeConfig.save(updated)) {
    home_reload_layout();
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
  }

  if (!changed) {
    server.sendHeader("Location", "/");
    server.send(303, "text/plain", "");
    return;
  }

  if (gameControlsConfig.save(updated)) {
    game_reload_layout();
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

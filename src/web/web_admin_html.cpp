#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include <WiFi.h>
#include <math.h>
#include <stdlib.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "src/core/config_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/game/game_controls_config.h"
#include "src/web/web_admin_scripts.h"
#include "src/web/web_admin_styles.h"
#include "src/tiles/tile_config.h"

String WebAdminServer::getAdminPage() {
  const DeviceConfig& cfg = configManager.getConfig();
  const HaBridgeConfigData& ha = haBridgeConfig.get();
  const auto sensorOptions = parseSensorList(ha.sensors_text);
  const auto sceneOptions = parseSceneList(ha.scene_alias_text);
  auto formatSensorValue = [](const String& raw, uint8_t decimals) -> String {
    String v = raw;
    v.trim();
    if (!v.length()) return String("--");
    String lower = v;
    lower.toLowerCase();
    if (lower == "unavailable") return String("--");
    if (decimals == 0xFF) return v;  // Keine Rundung gewünscht
    String normalized = v;
    normalized.replace(",", ".");
    char* end = nullptr;
    float f = strtof(normalized.c_str(), &end);
    if (!end || end == normalized.c_str()) return v;  // Nicht numerisch
    if (isnan(f) || isinf(f)) return v;
    uint8_t d = decimals > 6 ? 6 : decimals;
    return String(f, static_cast<unsigned int>(d));
  };

  String html;
  html.reserve(12000);
  html += R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Tab5 Admin</title>
)html";

  appendAdminStyles(html);
  appendAdminScripts(html);

  html += R"html(
</head>
<body>
  <div class="wrapper">
    <div class="card">
      <h1>Tab5 Admin-Panel</h1>
      <p class="subtitle">Konfiguration &amp; Uebersicht</p>

      <!-- WiFi Status at top -->
      <div class="status">
        <div>
          <div class="status-label">WiFi Status</div>
          <div class="status-value">)html";
  html += (WiFi.status() == WL_CONNECTED) ? "Verbunden" : "Getrennt";
  html += R"html(</div>
        </div>
        <div>
          <div class="status-label">SSID</div>
          <div class="status-value">)html";
  html += WiFi.SSID();
  html += R"html(</div>
        </div>
        <div>
          <div class="status-label">IP-Adresse</div>
          <div class="status-value">)html";
  html += WiFi.localIP().toString();
  html += R"html(</div>
        </div>
      </div>

      <!-- Tab Navigation -->
      <div class="tab-nav">
        <button class="tab-btn" onclick="switchTab('tab-network')">Network</button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-home')">Tiles Home</button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-game')">Tiles Game</button>
      </div>

      <!-- Tab 1: Network (MQTT Configuration) -->
      <div id="tab-network" class="tab-content">
        <form action="/mqtt" method="POST">
          <div>
            <label for="mqtt_host">MQTT Host / IP</label>
            <input type="text" id="mqtt_host" name="mqtt_host" required value=")html";
  html += cfg.mqtt_host;
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_port">Port</label>
            <input type="number" id="mqtt_port" name="mqtt_port" value=")html";
  html += String(cfg.mqtt_port ? cfg.mqtt_port : 1883);
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_user">Benutzername</label>
            <input type="text" id="mqtt_user" name="mqtt_user" value=")html";
  html += cfg.mqtt_user;
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_pass">Passwort</label>
            <input type="password" id="mqtt_pass" name="mqtt_pass" value=")html";
  html += cfg.mqtt_pass;
  html += R"html(">
          </div>
          <div>
            <label for="mqtt_base">Ger&auml;te-Topic Basis</label>
            <input type="text" id="mqtt_base" name="mqtt_base" value=")html";
  html += cfg.mqtt_base_topic;
  html += R"html(">
          </div>
          <div>
            <label for="ha_prefix">Home Assistant Prefix</label>
            <input type="text" id="ha_prefix" name="ha_prefix" value=")html";
  html += cfg.ha_prefix;
  html += R"html(">
          </div>
          <button class="btn" type="submit">Speichern</button>
        </form>
      </div>

      <!-- Tab 2: Tiles Home Editor -->
      <div id="tab-tiles-home" class="tab-content">
        <p class="hint">Klicke auf eine Kachel, um sie zu bearbeiten. WÃƒÂ¤hle den Typ (Sensor/Szene/Key) und passe die Einstellungen an.</p>
        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate 12 tiles for Home
  for (int i = 0; i < 12; i++) {
    const Tile& tile = tileConfig.getHomeGrid().tiles[i];

    // CSS-Klassen und Farben wie im Display
    String cssClass = "tile";
    String tileStyle = "";

    if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
      // Empty: kein background (transparent)
    } else if (tile.type == TILE_SENSOR) {
      cssClass += " sensor";
      // Sensor: Standard 0x2A2A2A
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#2A2A2A";
      }
    } else if (tile.type == TILE_SCENE) {
      cssClass += " scene";
      // Scene: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_KEY) {
      cssClass += " key";
      // Key: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    }

    html += "<div class=\"";
    html += cssClass;
    html += "\" data-index=\"";
    html += String(i);
    html += "\" draggable=\"true\" id=\"home-tile-";
    html += String(i);
    html += "\" style=\"";
    html += tileStyle;
    html += "\" onclick=\"selectTile(parseInt(this.dataset.index), 'home')\">";

    // Title - nur wenn nicht EMPTY
    if (tile.type != TILE_EMPTY) {
      html += "<div class=\"tile-title\" id=\"home-tile-";
      html += String(i);
      html += "-title\">";
      if (tile.title.length()) {
        appendHtmlEscaped(html, tile.title);
      } else if (tile.type == TILE_SENSOR) {
        html += "Sensor";
      } else if (tile.type == TILE_SCENE) {
        html += "Szene";
      } else if (tile.type == TILE_KEY) {
        html += "Key";
      }
      html += "</div>";
    }

    // Value (for sensors)
    if (tile.type == TILE_SENSOR) {
      html += "<div class=\"tile-value\" id=\"home-tile-";
      html += String(i);
      html += "-value\">";

      // Get actual sensor value from map
      String sensorValue = "--";
      if (tile.sensor_entity.length()) {
        sensorValue = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
        sensorValue = formatSensorValue(sensorValue, tile.sensor_decimals);
        Serial.printf("[WebAdmin] Home Tile %d: Entity=%s, Value=%s (dec=%u)\n",
                      i, tile.sensor_entity.c_str(),
                      sensorValue.length() ? sensorValue.c_str() : "(empty)",
                      static_cast<unsigned>(tile.sensor_decimals));
        if (sensorValue.length() == 0) {
          sensorValue = "--";
        }
      }
      appendHtmlEscaped(html, sensorValue);

      if (tile.sensor_unit.length()) {
        html += "<span class=\"tile-unit\">";
        appendHtmlEscaped(html, tile.sensor_unit);
        html += "</span>";
      }
      html += "</div>";
    }

    html += "</div>";
  }

  html += R"html(
          </div>

          <!-- Settings Panel -->
          <div class="tile-settings hidden" id="homeSettings">
            <h3 style="margin-top:0;">Kachel Einstellungen</h3>

            <label>Typ</label>
            <select id="home_tile_type" onchange="updateTileType('home')">
              <option value="0">Leer</option>
              <option value="1">Sensor</option>
              <option value="2">Szene</option>
              <option value="3">Key</option>
            </select>

            <label>Titel</label>
            <input type="text" id="home_tile_title" placeholder="Kachel-Titel">

            <label>Farbe</label>
            <input type="color" id="home_tile_color" value="#2A2A2A" style="height:40px;">

            <!-- Sensor Fields -->
            <div id="home_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id="home_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

  // Add sensor options
  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Einheit</label>
              <input type="text" id="home_sensor_unit" placeholder="z.B. Ã‚Â°C">
              <label>Nachkommastellen (leer = Originalwert)</label>
              <input type="number" id="home_sensor_decimals" min="0" max="6" step="1" placeholder="z.B. 1">
            </div>

            <!-- Scene Fields -->
            <div id="home_scene_fields" class="type-fields">
              <label>Szene</label>
              <select id="home_scene_alias">
                <option value="">Keine Auswahl</option>
)html";

  // Add scene options
  for (const auto& opt : sceneOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt.alias);
    html += "\">";
    String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
            </div>

            <!-- Key Fields -->
            <div id="home_key_fields" class="type-fields">
              <label>Makro</label>
              <input type="text" id="home_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Beispiele: g, ctrl+g, ctrl+shift+a</div>
            </div>
            <div style="display:flex;justify-content:space-between;align-items:center;margin-top:8px;font-size:12px;color:#64748b;">
              <span>Änderungen werden automatisch gespeichert.</span>
              <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:90px;" onclick="resetTile('home')">Löschen</button>
            </div>
          </div>
        </div>
      </div>

      <!-- Tab 5: Tiles Game Editor -->
      <div id="tab-tiles-game" class="tab-content">
        <p class="hint">Klicke auf eine Kachel, um sie zu bearbeiten. WÃƒÂ¤hle den Typ (Sensor/Szene/Key) und passe die Einstellungen an.</p>
        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate 12 tiles for Game
  for (int i = 0; i < 12; i++) {
    const Tile& tile = tileConfig.getGameGrid().tiles[i];

    // CSS-Klassen und Farben wie im Display
    String cssClass = "tile";
    String tileStyle = "";

    if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
      // Empty: kein background (transparent)
    } else if (tile.type == TILE_SENSOR) {
      cssClass += " sensor";
      // Sensor: Standard 0x2A2A2A
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#2A2A2A";
      }
    } else if (tile.type == TILE_SCENE) {
      cssClass += " scene";
      // Scene: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_KEY) {
      cssClass += " key";
      // Key: Standard 0x353535
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    }

    html += "<div class=\"";
    html += cssClass;
    html += "\" data-index=\"";
    html += String(i);
    html += "\" draggable=\"true\" id=\"game-tile-";
    html += String(i);
    html += "\" style=\"";
    html += tileStyle;
    html += "\" onclick=\"selectTile(parseInt(this.dataset.index), 'game')\">";

    // Title - nur wenn nicht EMPTY
    if (tile.type != TILE_EMPTY) {
      html += "<div class=\"tile-title\" id=\"game-tile-";
      html += String(i);
      html += "-title\">";
      if (tile.title.length()) {
        appendHtmlEscaped(html, tile.title);
      } else if (tile.type == TILE_SENSOR) {
        html += "Sensor";
      } else if (tile.type == TILE_SCENE) {
        html += "Szene";
      } else if (tile.type == TILE_KEY) {
        html += "Key";
      }
      html += "</div>";
    }

    // Value (for sensors)
    if (tile.type == TILE_SENSOR) {
      html += "<div class=\"tile-value\" id=\"game-tile-";
      html += String(i);
      html += "-value\">";

      // Get actual sensor value from map
      String sensorValue = "--";
      if (tile.sensor_entity.length()) {
        sensorValue = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
        sensorValue = formatSensorValue(sensorValue, tile.sensor_decimals);
        Serial.printf("[WebAdmin] Game Tile %d: Entity=%s, Value=%s (dec=%u)\n",
                      i, tile.sensor_entity.c_str(),
                      sensorValue.length() ? sensorValue.c_str() : "(empty)",
                      static_cast<unsigned>(tile.sensor_decimals));
        if (sensorValue.length() == 0) {
          sensorValue = "--";
        }
      }
      appendHtmlEscaped(html, sensorValue);

      if (tile.sensor_unit.length()) {
        html += "<span class=\"tile-unit\">";
        appendHtmlEscaped(html, tile.sensor_unit);
        html += "</span>";
      }
      html += "</div>";
    }

    html += "</div>";
  }

  html += R"html(
          </div>

          <!-- Settings Panel -->
          <div class="tile-settings hidden" id="gameSettings">
            <h3 style="margin-top:0;">Kachel Einstellungen</h3>

            <label>Typ</label>
            <select id="game_tile_type" onchange="updateTileType('game')">
              <option value="0">Leer</option>
              <option value="1">Sensor</option>
              <option value="2">Szene</option>
              <option value="3">Key</option>
            </select>

            <label>Titel</label>
            <input type="text" id="game_tile_title" placeholder="Kachel-Titel">

            <label>Farbe</label>
            <input type="color" id="game_tile_color" value="#2A2A2A" style="height:40px;">

            <!-- Sensor Fields -->
            <div id="game_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id="game_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

  // Add sensor options for game
  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Einheit</label>
              <input type="text" id="game_sensor_unit" placeholder="z.B. Ã‚Â°C">
              <label>Nachkommastellen (leer = Originalwert)</label>
              <input type="number" id="game_sensor_decimals" min="0" max="6" step="1" placeholder="z.B. 1">
            </div>

            <!-- Scene Fields -->
            <div id="game_scene_fields" class="type-fields">
              <label>Szene</label>
              <select id="game_scene_alias">
                <option value="">Keine Auswahl</option>
)html";

  // Add scene options for game
  for (const auto& opt : sceneOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt.alias);
    html += "\">";
    String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
            </div>

            <!-- Key Fields -->
            <div id="game_key_fields" class="type-fields">
              <label>Makro</label>
              <input type="text" id="game_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Beispiele: g, ctrl+g, ctrl+shift+a</div>
            </div>
            <div style="display:flex;justify-content:space-between;align-items:center;margin-top:8px;font-size:12px;color:#64748b;">
              <span>Änderungen werden automatisch gespeichert.</span>
              <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:90px;" onclick="resetTile('game')">Löschen</button>
            </div>
          </div>
        </div>
      </div>

      <!-- Restart button at bottom (always visible) -->
      <form action="/restart" method="POST" onsubmit="return confirm('Geraet wirklich neu starten?');" style="margin-top:32px;">
        <button class="btn btn-secondary" type="submit">Geraet neu starten</button>
      </form>
    </div>
  </div>

  <!-- Notification Toast -->
  <div id="notification" class="notification"></div>
</body>
</html>
)html";

  return html;
}

String WebAdminServer::getSuccessPage() {
  return R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <title>Gespeichert</title>
  <style>
    body { font-family: Arial, sans-serif; background:#eef2ff; height:100vh; margin:0; display:flex; align-items:center; justify-content:center; }
    .box { background:#fff; padding:30px; border-radius:12px; box-shadow:0 15px 35px rgba(0,0,0,.2); text-align:center; }
    h1 { margin:0 0 10px; color:#1f2937; }
    p { margin:0; color:#4b5563; }
  </style>
  <script>setTimeout(function(){window.location.href='/'},1500);</script>
</head>
<body>
  <div class="box">
    <h1>MQTT-Konfiguration gespeichert</h1>
    <p>Das Geraet verbindet sich neu ...</p>
  </div>
</body>
</html>)html";
}

String WebAdminServer::getBridgeSuccessPage() {
  return R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <title>Bridge gespeichert</title>
  <style>
    body { font-family: Arial, sans-serif; background:#eef2ff; height:100vh; margin:0; display:flex; align-items:center; justify-content:center; }
    .box { background:#fff; padding:30px; border-radius:12px; box-shadow:0 15px 35px rgba(0,0,0,.2); text-align:center; }
    h1 { margin:0 0 10px; color:#1f2937; }
    p { margin:0; color:#4b5563; }
  </style>
  <script>setTimeout(function(){window.location.href='/'},1500);</script>
</head>
<body>
  <div class="box">
    <h1>Bridge-Konfiguration gespeichert</h1>
    <p>Die Daten wurden per MQTT uebertragen.</p>
  </div>
</body>
</html>)html";
}

String WebAdminServer::getStatusJSON() {
  const DeviceConfig& cfg = configManager.getConfig();
  nvs_stats_t stats{};
  bool stats_ok = (nvs_get_stats(nullptr, &stats) == ESP_OK);

  String json = "{";
  json += "\"wifi_connected\":";
  json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  json += ",\"wifi_ssid\":\"" + String(cfg.wifi_ssid) + "\"";
  json += ",\"wifi_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"mqtt_host\":\"" + String(cfg.mqtt_host) + "\"";
  json += ",\"mqtt_port\":" + String(cfg.mqtt_port);
  json += ",\"mqtt_base\":\"" + String(cfg.mqtt_base_topic) + "\"";
  json += ",\"ha_prefix\":\"" + String(cfg.ha_prefix) + "\"";
  json += ",\"bridge_configured\":" + String(haBridgeConfig.hasData() ? "true" : "false");
  json += ",\"free_heap\":" + String(ESP.getFreeHeap());
  json += ",\"nvs_used_entries\":" + String(stats_ok ? stats.used_entries : -1);
  json += ",\"nvs_free_entries\":" + String(stats_ok ? stats.free_entries : -1);
  json += ",\"nvs_namespace_count\":" + String(stats_ok ? stats.namespace_count : -1);
  json += "}";
  return json;
}




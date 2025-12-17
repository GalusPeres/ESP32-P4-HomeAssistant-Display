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

// Helper function to generate tile tab HTML (unified for all 3 tabs)
static void appendTileTabHTML(
    String& html,
    uint8_t tab_index,
    const TileGridConfig& grid,
    const std::vector<String>& sensorOptions,
    const std::vector<SceneOption>& sceneOptions,
    const std::function<String(const String&, uint8_t)>& formatSensorValue
) {
  String tab_id = "tab" + String(tab_index);
  String tab_label = "Tab " + String(tab_index + 1);

  html += R"html(
      <!-- Tile Tab )html";
  html += String(tab_index);
  html += R"html( -->
      <div id="tab-tiles-)html";
  html += tab_id;
  html += R"html(" class="tab-content">
        <p class="hint">Klicke auf eine Kachel, um sie zu bearbeiten. Wähle den Typ (Sensor/Szene/Key) und passe die Einstellungen an.</p>

        <!-- Tab Settings (Above Grid) -->
        <div class="tab-settings-top">
          <h3 style="margin:0 0 12px;font-size:14px;color:#64748b;text-transform:uppercase;letter-spacing:0.1em;">Tab Einstellungen</h3>
          <label style="font-size:13px;font-weight:600;color:#475569;display:block;margin-bottom:6px;">Tab-Name</label>
          <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tab_name" placeholder=")html";
  html += tab_label;
  html += R"html(" value=")html";
  html += tileConfig.getTabName(tab_index);
  html += R"html(" onchange="saveTabName()html";
  html += String(tab_index);
  html += R"html(, this.value)" style="width:100%;max-width:300px;padding:10px;border:1px solid #cbd5e1;border-radius:8px;font-size:14px;box-sizing:border-box;">
        </div>

        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate 12 tiles
  for (int i = 0; i < 12; i++) {
    const Tile& tile = grid.tiles[i];
    String cssClass = "tile";
    String tileStyle = "";

    if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
    } else if (tile.type == TILE_SENSOR) {
      cssClass += " sensor";
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
    html += "\" draggable=\"true\" id=\"";
    html += tab_id;
    html += "-tile-";
    html += String(i);
    html += "\" style=\"";
    html += tileStyle;
    html += "\" onclick=\"selectTile(parseInt(this.dataset.index), '";
    html += tab_id;
    html += "')\">";

    if (tile.type != TILE_EMPTY) {
      // Icon (optional) - normalize icon name (lowercase, trim, remove mdi: prefix)
      String iconName = tile.icon_name;
      iconName.toLowerCase();
      iconName.trim();
      if (iconName.startsWith("mdi:")) iconName.remove(0, 4);
      else if (iconName.startsWith("mdi-")) iconName.remove(0, 4);

      bool hasIcon = iconName.length() > 0;

      if (hasIcon) {
        html += "<i class=\"mdi mdi-";
        appendHtmlEscaped(html, iconName);
        html += " tile-icon\"></i>";
      }

      html += "<div class=\"tile-title";
      if (hasIcon && tile.type == TILE_SENSOR) {
        html += " with-icon";
      }
      html += "\" id=\"";
      html += tab_id;
      html += "-tile-";
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

    if (tile.type == TILE_SENSOR) {
      html += "<div class=\"tile-value\" id=\"";
      html += tab_id;
      html += "-tile-";
      html += String(i);
      html += "-value\">";

      String sensorValue = "--";
      if (tile.sensor_entity.length()) {
        sensorValue = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
        sensorValue = formatSensorValue(sensorValue, tile.sensor_decimals);
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
          <div class="tile-settings" id=")html";
  html += tab_id;
  html += R"html(Settings">
            <!-- Tile Settings (Visible only when tile selected) -->
            <div class="tile-specific-settings hidden">
              <h3 style="margin-top:0;">Kachel Einstellungen</h3>

            <label>Typ</label>
            <select id=")html";
  html += tab_id;
  html += R"html(_tile_type" onchange="updateTileType(')html";
  html += tab_id;
  html += R"html(')">
              <option value="0">Leer</option>
              <option value="1">Sensor</option>
              <option value="2">Szene</option>
              <option value="3">Key</option>
            </select>

            <label>Titel</label>
            <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tile_title" placeholder="Kachel-Titel">

            <label>Icon (MDI)</label>
            <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tile_icon" placeholder="z.B. home, thermometer, lightbulb">
            <div style="font-size:11px;color:#64748b;margin-top:4px;">
              Material Design Icons: <a href="https://pictogrammers.com/library/mdi/" target="_blank" style="color:#3b82f6;">Icon-Liste anzeigen</a>
            </div>

            <label>Farbe</label>
            <input type="color" id=")html";
  html += tab_id;
  html += R"html(_tile_color" value="#2A2A2A" style="height:40px;">

            <!-- Sensor Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

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
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_sensor_unit" placeholder="z.B. °C">
              <label>Nachkommastellen (leer = Originalwert)</label>
              <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_decimals" min="0" max="6" step="1" placeholder="z.B. 1">
            </div>

            <!-- Scene Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_scene_fields" class="type-fields">
              <label>Szene</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_scene_alias">
                <option value="">Keine Auswahl</option>
)html";

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
            <div id=")html";
  html += tab_id;
  html += R"html(_key_fields" class="type-fields">
              <label>Makro</label>
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Beispiele: g, ctrl+g, ctrl+shift+a</div>
            </div>
            <div style="display:flex;justify-content:space-between;align-items:center;margin-top:8px;font-size:12px;color:#64748b;">
              <span>Änderungen werden automatisch gespeichert.</span>
              <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:90px;" onclick="resetTile(')html";
  html += tab_id;
  html += R"html(')">Löschen</button>
            </div>
            </div><!-- /tile-specific-settings -->
          </div>
        </div>
      </div>
)html";
}

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

      <!-- Tab Navigation -->
      <div class="tab-nav">
        <button class="tab-btn" onclick="switchTab('tab-network')">Network</button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-tab0')">
          <span id="tab-name-0">)html";
  html += tileConfig.getTabName(0);
  html += R"html(</span>
        </button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-tab1')">
          <span id="tab-name-1">)html";
  html += tileConfig.getTabName(1);
  html += R"html(</span>
        </button>
        <button class="tab-btn" onclick="switchTab('tab-tiles-tab2')">
          <span id="tab-name-2">)html";
  html += tileConfig.getTabName(2);
  html += R"html(</span>
        </button>
      </div>
)html";

  // Generate three unified tile tabs
  appendTileTabHTML(html, 0, tileConfig.getTab0Grid(), sensorOptions, sceneOptions, formatSensorValue);
  appendTileTabHTML(html, 1, tileConfig.getTab1Grid(), sensorOptions, sceneOptions, formatSensorValue);
  appendTileTabHTML(html, 2, tileConfig.getTab2Grid(), sensorOptions, sceneOptions, formatSensorValue);

  html += R"html(
      <!-- Tab 1: Network (MQTT Configuration) -->
      <div id="tab-network" class="tab-content">
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

  auto get_ns_used = [](const char* ns) -> size_t {
    if (!ns) return static_cast<size_t>(-1);
    nvs_handle_t h = 0;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return static_cast<size_t>(-1);
    size_t used = 0;
    esp_err_t err = nvs_get_used_entry_count(h, &used);
    nvs_close(h);
    return (err == ESP_OK) ? used : static_cast<size_t>(-1);
  };

  size_t tiles_used = get_ns_used("tab5_tiles");
  size_t config_used = get_ns_used("tab5_config");

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
  json += ",\"nvs_tab5_tiles_used\":" + String(tiles_used);
  json += ",\"nvs_tab5_config_used\":" + String(config_used);
  json += "}";
  return json;
}

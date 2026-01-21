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
#include "src/types/sensor/web_html.h"
#include "src/types/scene/web_html.h"
#include "src/types/key/web_html.h"
#include "src/types/navigate/web_html.h"
#include "src/types/switch/web_html.h"
#include "src/types/image/web_html.h"
#include "src/types/clock/web_html.h"

// Helper function to generate tile tab HTML (unified for all folders)
static void appendTileTabHTML(
    String& html,
    uint16_t folder_id,
    const FolderEntry& folder,
    const TileGridConfig& grid,
    const std::vector<String>& sensorOptions,
    const std::vector<SceneOption>& sceneOptions,
    const std::vector<String>& switchOptions,
    const std::function<String(const String&, uint8_t)>& formatSensorValue,
    const String& navigateOptionsHtml
) {
  String tab_id = "folder" + String(folder_id);

  html += R"html(
      <!-- Tile Folder -->
      <div id="tab-tiles-)html";
  html += tab_id;
  html += R"html(" class="tab-content tile-tab" data-tab-id=")html";
  html += tab_id;
  html += R"html(" data-folder-id=")html";
  html += String(folder_id);
  html += R"html(" data-folder-parent=")html";
  html += String(folder.parent_id);
  html += R"html(" data-folder-name=")html";
  appendHtmlEscaped(html, folder.name);
  html += R"html(" data-folder-icon=")html";
  appendHtmlEscaped(html, folder.icon_name);
  html += R"html(">
        <p class="hint">Klicke auf eine Kachel, um sie zu bearbeiten. Waehle den Typ (Sensor/Szene/Key/Ordner/Settings/Switch/Bild) und passe die Einstellungen an.</p>

        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate tiles
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    String cssClass = "tile";
    String tileStyle = "";
    uint8_t col = (tile.col < GRID_COLS) ? tile.col : 0;
    uint8_t row = (tile.row < GRID_ROWS) ? tile.row : 0;
    uint8_t span_w = (tile.span_w < 1) ? 1 : tile.span_w;
    uint8_t span_h = (tile.span_h < 1) ? 1 : tile.span_h;
    if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
    if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;

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
    } else if (tile.type == TILE_FOLDER || tile.type == TILE_SETTINGS || tile.type == TILE_BACK) {
      cssClass += " navigate";
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_SWITCH) {
      cssClass += " switch";
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_IMAGE) {
      cssClass += " image";
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    } else if (tile.type == TILE_CLOCK) {
      cssClass += " clock";
      if (tile.bg_color != 0) {
        char colorHex[8];
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
        tileStyle = "background:";
        tileStyle += colorHex;
      } else {
        tileStyle = "background:#353535";
      }
    }

    tileStyle += ";grid-column:";
    tileStyle += String(static_cast<unsigned>(col + 1));
    tileStyle += " / span ";
    tileStyle += String(static_cast<unsigned>(span_w));
    tileStyle += ";grid-row:";
    tileStyle += String(static_cast<unsigned>(row + 1));
    tileStyle += " / span ";
    tileStyle += String(static_cast<unsigned>(span_h));
    tileStyle += ";";

    html += "<div class=\"";
    html += cssClass;
    html += "\" data-index=\"";
    html += String(i);
    html += "\" data-col=\"";
    html += String(static_cast<unsigned>(col));
    html += "\" data-row=\"";
    html += String(static_cast<unsigned>(row));
    html += "\" data-span-w=\"";
    html += String(static_cast<unsigned>(span_w));
    html += "\" data-span-h=\"";
    html += String(static_cast<unsigned>(span_h));
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
        html += iconName;  // Direkt hinzufügen (CSS-Klasse darf nicht escaped werden!)
        html += " tile-icon\"></i>";
      }

      // Title nur anzeigen wenn vorhanden
      if (tile.title.length()) {
        html += "<div class=\"tile-title\" id=\"";
        html += tab_id;
        html += "-tile-";
        html += String(i);
        html += "-title\">";
        appendHtmlEscaped(html, tile.title);
        html += "</div>";
      }
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

    if (tile.type == TILE_CLOCK) {
      html += "<div class=\"tile-clock-time\">--:--</div>";
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
              <option value="4">Ordner</option>
              <option value="5">Schalter</option>
              <option value="6">Bild</option>
              <option value="9">Uhr</option>
              <option value="7">Settings</option>
              <option value="8">Zurueck</option>
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

            <div class="tile-layout">
              <div class="layout-field">
                <label>Spalte (1-6)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_col" min="1" max="6" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>Zeile (1-4)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_row" min="1" max="4" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>Breite (Zellen)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_span_w" min="1" max="6" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>Hoehe (Zellen)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_span_h" min="1" max="4" step="1" value="1">
              </div>
            </div>

)html";

            append_sensor_fields_html(html, tab_id, sensorOptions);
            append_scene_fields_html(html, tab_id, sceneOptions);
            append_key_fields_html(html, tab_id);
            append_navigate_fields_html(html, tab_id, navigateOptionsHtml);
            append_switch_fields_html(html, tab_id, switchOptions);
            append_image_fields_html(html, tab_id);
            append_clock_fields_html(html, tab_id);

  html += R"html(
<div style="display:flex;justify-content:space-between;align-items:center;margin-top:8px;font-size:12px;color:#64748b;gap:10px;">
              <span>Aenderungen werden automatisch gespeichert.</span>
              <div style="display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end;">
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:90px;" onclick="copyTile(')html";
  html += tab_id;
  html += R"html(')">Kopieren</button>
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:90px;" onclick="pasteTile(')html";
  html += tab_id;
  html += R"html(')">Einfuegen</button>
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:90px;" onclick="resetTile(')html";
  html += tab_id;
  html += R"html(')">Loeschen</button>
              </div>
            </div>
            <div style="margin-top:12px;border-top:1px solid #e2e8f0;padding-top:10px;">
              <div style="font-size:12px;color:#64748b;margin-bottom:6px;">Import / Export (alle Ordner & Kacheln)</div>
              <div style="display:flex;gap:8px;flex-wrap:wrap;">
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:110px;" onclick="exportTilesConfig()">Export</button>
                <input type="file" id=")html";
  html += tab_id;
  html += R"html(_tile_import" accept="application/json" style="display:none" onchange="importTilesConfig(')html";
  html += tab_id;
  html += R"html(', this.files)">
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:110px;" onclick="triggerTilesImport(')html";
  html += tab_id;
  html += R"html(')">Import</button>
              </div>
              <div style="font-size:11px;color:#94a3b8;margin-top:6px;">Import ueberschreibt alle Kacheln der vorhandenen Ordner.</div>
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
  const auto lightOptions = parseSensorList(ha.lights_text);
  const auto switchOptionsRaw = parseSensorList(ha.switches_text);
  std::vector<String> switchOptions;
  switchOptions.reserve(lightOptions.size() + switchOptionsRaw.size());
  auto addSwitchOption = [&](const String& entry) {
    if (!entry.length()) return;
    for (const auto& existing : switchOptions) {
      if (existing.equalsIgnoreCase(entry)) {
        return;
      }
    }
    switchOptions.push_back(entry);
  };
  for (const auto& opt : lightOptions) {
    addSwitchOption(opt);
  }
  for (const auto& opt : switchOptionsRaw) {
    addSwitchOption(opt);
  }
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

  const auto& folders = tileConfig.getFolders();
  String navigateOptionsHtml;
  for (const auto& entry : folders) {
    if (entry.id == 0) continue;
    String label = String(entry.name);
    label.trim();
    if (!label.length()) {
      label = "Ordner ";
      label += String(entry.id);
    }
    navigateOptionsHtml += "<option value=\"";
    navigateOptionsHtml += String(entry.id);
    navigateOptionsHtml += "\">";
    appendHtmlEscaped(navigateOptionsHtml, label);
    navigateOptionsHtml += "</option>\n";
  }

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
)html";

  for (const auto& entry : folders) {
    String tab_id = "folder" + String(entry.id);
    String icon = String(entry.icon_name);
    String name = String(entry.name);
    icon.trim();
    icon.toLowerCase();
    if (icon.startsWith("mdi:")) icon = icon.substring(4);
    else if (icon.startsWith("mdi-")) icon = icon.substring(4);
    name.trim();
    if (!name.length()) {
      name = (entry.id == 0) ? "Home" : String("Ordner ") + String(entry.id);
    }

    html += R"html(
        <button class="tab-btn" onclick="switchTab('tab-tiles-)html";
    html += tab_id;
    html += R"html(')">)html";
    if (icon.length()) {
      html += R"html(
          <i class="mdi mdi-)html";
      html += icon;
      html += R"html(" style="font-size:24px;"></i>)html";
    }
    html += R"html(
          <span style="font-size:14px;font-weight:600;">)html";
    appendHtmlEscaped(html, name);
    html += R"html(</span>
        </button>
)html";
  }

  html += R"html(
        <button class="tab-btn" onclick="switchTab('tab-network')">
          <i class="mdi mdi-cog" style="font-size:24px;"></i>
          <span style="font-size:14px;font-weight:600;">Settings</span>
        </button>
      </div>
)html";

  // Generate folder tile tabs
  for (const auto& entry : folders) {
    TileGridConfig grid{};
    tileConfig.loadFolderGrid(entry.id, grid);
    appendTileTabHTML(html, entry.id, entry, grid, sensorOptions, sceneOptions, switchOptions, formatSensorValue, navigateOptionsHtml);
  }

  html += R"html(
      <!-- Tab 3: Settings (Network/MQTT Configuration) -->
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
  json += ",\"heap_total\":" + String(ESP.getHeapSize());
  json += ",\"heap_min_free\":" + String(ESP.getMinFreeHeap());
  json += ",\"psram_free\":" + String(ESP.getFreePsram());
  json += ",\"psram_total\":" + String(ESP.getPsramSize());
  json += ",\"nvs_used_entries\":" + String(stats_ok ? stats.used_entries : -1);
  json += ",\"nvs_free_entries\":" + String(stats_ok ? stats.free_entries : -1);
  json += ",\"nvs_namespace_count\":" + String(stats_ok ? stats.namespace_count : -1);
  json += ",\"nvs_tab5_tiles_used\":" + String(tiles_used);
  json += ",\"nvs_tab5_config_used\":" + String(config_used);
  json += "}";
  return json;
}

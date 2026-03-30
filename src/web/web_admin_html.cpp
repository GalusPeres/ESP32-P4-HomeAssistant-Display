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
#include "src/types/types_registry.h"
#include "src/core/device_entities.h"
#include "src/core/i18n.h"
#include "src/devices/device.h"
#include <cstring>

// Helper function to generate tile tab HTML (unified for all folders)
static void appendTileTabHTML(
    String& html,
    uint16_t folder_id,
    const FolderEntry& folder,
    const TileGridConfig& grid,
    const std::vector<String>& sensorOptions,
    const std::vector<String>& weatherOptions,
    const std::vector<SceneOption>& sceneOptions,
    const std::vector<String>& switchOptions,
    const std::function<String(const String&, uint8_t)>& formatSensorValue,
    const String& navigateOptionsHtml
) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
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
        <p class="hint">)html";
  html += tr.admin_tile_hint;
  html += R"html(</p>
)html";
  if (folder_id != 0) {
    html += R"html(        <div style="margin-bottom:12px;">
          <button type="button" class="btn btn-danger" style="padding:8px 16px;font-size:13px;" onclick="deleteFolder(')html";
    html += tab_id;
    html += R"html(')">)html";
    html += tr.admin_delete_folder_tab;
    html += R"html(</button>
        </div>
)html";
  }
  html += R"html(
        <div class="tile-editor">
          <!-- Grid Preview -->
          <div class="tile-grid">
)html";

  // Generate tiles
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    String cssClass = "tile";
    String tileStyle = "";
    const TileTypeDescriptor* type_desc = get_tile_type_descriptor(tile.type);
    const char* type_css = type_desc ? type_desc->css_class : nullptr;
    uint8_t col = (tile.col < GRID_COLS) ? tile.col : 0;
    uint8_t row = (tile.row < GRID_ROWS) ? tile.row : 0;
    uint8_t span_w = (tile.span_w < 1) ? 1 : tile.span_w;
    uint8_t span_h = (tile.span_h < 1) ? 1 : tile.span_h;
    if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
    if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;

    if (type_css && type_css[0]) {
      cssClass += " ";
      cssClass += type_css;
    } else if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
    }

    if (tile.type != TILE_EMPTY) {
      uint32_t bg_color = tile.bg_color;
      if (bg_color == 0 && type_desc) bg_color = type_desc->default_bg_color;
      if (bg_color == 0) bg_color = 0x353535;
      char colorHex[8];
      snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)bg_color);
      tileStyle = "background:";
      tileStyle += colorHex;
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
    html += "\" data-type=\"";
    html += String(static_cast<unsigned>(tile.type));
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

    const char* preview_kind = get_tile_type_preview_kind(tile.type);
    if (preview_kind && strcmp(preview_kind, "sensor") == 0) {
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

    if (preview_kind && strcmp(preview_kind, "clock") == 0) {
      uint8_t flags = tile.sensor_decimals;
      if (flags == 0xFF) flags = 1;
      flags &= 0x03;
      if (flags == 0) flags = 1;
      if (flags & 1) {
        html += "<div class=\"tile-clock-time\">--:--</div>";
      }
      if (flags & 2) {
        html += "<div class=\"tile-clock-date\">--.--.----</div>";
      }
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
              <h3 style="margin-top:0;">)html";
  html += tr.admin_tile_settings;
  html += R"html(</h3>

            <label>)html";
  html += tr.admin_type;
  html += R"html(</label>
            <select id=")html";
  html += tab_id;
  html += R"html(_tile_type" onchange="updateTileType(')html";
  html += tab_id;
  html += R"html(')">
            )html";
  append_tile_type_select_options(html);
  html += R"html(
            </select>

            <label>)html";
  html += tr.admin_title;
  html += R"html(</label>
            <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tile_title" placeholder=")html";
  html += tr.admin_tile_title_placeholder;
  html += R"html(">

            <label>)html";
  html += tr.admin_icon_label;
  html += R"html(</label>
            <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tile_icon" placeholder=")html";
  html += tr.admin_icon_placeholder;
  html += R"html(">
            <div style="font-size:11px;color:#64748b;margin-top:4px;">
              Material Design Icons: <a href="https://pictogrammers.com/library/mdi/" target="_blank" style="color:#3b82f6;">)html";
  html += tr.admin_icon_list;
  html += R"html(</a>
            </div>

            <label>)html";
  html += tr.admin_color;
  html += R"html(</label>
            <input type="color" id=")html";
  html += tab_id;
  html += R"html(_tile_color" value="#2A2A2A" style="height:40px;">

            <div class="tile-layout">
              <div class="layout-field">
                <label>)html";
  html += tr.admin_column;
  html += R"html( (1-)html";
  html += String(GRID_COLS);
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_col" min="1" max=")html";
  html += String(GRID_COLS);
  html += R"html(" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>)html";
  html += tr.admin_row;
  html += R"html( (1-)html";
  html += String(GRID_ROWS);
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_row" min="1" max=")html";
  html += String(GRID_ROWS);
  html += R"html(" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>)html";
  html += tr.admin_width_cells;
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_span_w" min="1" max=")html";
  html += String(GRID_COLS);
  html += R"html(" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>)html";
  html += tr.admin_height_cells;
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_span_h" min="1" max=")html";
  html += String(GRID_ROWS);
  html += R"html(" step="1" value="1">
              </div>
            </div>

)html";

            TileTypeWebContext type_ctx;
            type_ctx.tab_id = &tab_id;
            type_ctx.sensor_options = &sensorOptions;
            type_ctx.weather_options = &weatherOptions;
            type_ctx.scene_options = &sceneOptions;
            type_ctx.switch_options = &switchOptions;
            type_ctx.navigate_options_html = &navigateOptionsHtml;
            append_tile_type_fields_html(html, type_ctx);

  html += R"html(
<div class="tile-actions">
                <button type="button" class="btn" onclick="copyTile(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_copy;
  html += R"html(</button>
                <button type="button" class="btn" onclick="pasteTile(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_paste;
  html += R"html(</button>
                <button type="button" class="btn" onclick="resetTile(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_delete;
  html += R"html(</button>
            </div>
            <div style="margin-top:12px;border-top:1px solid #e2e8f0;padding-top:10px;">
              <div style="font-size:12px;color:#64748b;margin-bottom:6px;">)html";
  html += tr.admin_import_export;
  html += R"html(</div>
              <div style="display:flex;gap:8px;flex-wrap:wrap;">
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:110px;" onclick="exportTilesConfig()">)html";
  html += tr.admin_export;
  html += R"html(</button>
                <input type="file" id=")html";
  html += tab_id;
  html += R"html(_tile_import" accept="application/json" style="display:none" onchange="importTilesConfig(')html";
  html += tab_id;
  html += R"html(', this.files)">
                <button type="button" class="btn" style="padding:8px 12px;font-size:12px;min-width:110px;" onclick="triggerTilesImport(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_import;
  html += R"html(</button>
              </div>
              <div style="font-size:11px;color:#94a3b8;margin-top:6px;">)html";
  html += tr.admin_import_overwrite;
  html += R"html(</div>
            </div>
            </div><!-- /tile-specific-settings -->
          </div>
        </div>
      </div>
)html";
}

String WebAdminServer::getAdminPage() {
  const DeviceConfig& cfg = configManager.getConfig();
  const auto& tr = i18n::strings(cfg.language);
  const bool is_german = strcmp(tr.html_lang, "de") == 0;
  const String admin_panel_title =
      String(Device::displayName()) + (is_german ? " Admin-Panel" : " Admin Panel");
  const HaBridgeConfigData& ha = haBridgeConfig.get();
  const auto sensorOptions = parseSensorList(ha.sensors_text);
  const auto weatherOptions = parseSensorList(ha.weathers_text);
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
  addSwitchOption(kEntityDisplayBrightness);
  addSwitchOption(kEntityDisplayRotate);
  addSwitchOption(kEntityDisplaySleep);
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
      label = tr.folder_prefix;
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
  html += "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>)html";
  html += admin_panel_title;
  html += R"html(</title>
)html";

  appendAdminStyles(html);
  appendAdminScripts(html);

  html += R"html(
</head>
<body>
  <div class="wrapper">
    <div class="card">
      <h1>)html";
  html += admin_panel_title;
  html += R"html(</h1>
      
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
      name = (entry.id == 0) ? String(tr.home) : String(tr.folder_prefix) + String(entry.id);
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
          <span style="font-size:14px;font-weight:600;">)html";
  html += tr.tile_type_settings;
  html += R"html(</span>
        </button>
      </div>
)html";

  // Generate folder tile tabs
  for (const auto& entry : folders) {
    TileGridConfig grid{};
    tileConfig.loadFolderGrid(entry.id, grid);
    appendTileTabHTML(html, entry.id, entry, grid, sensorOptions, weatherOptions, sceneOptions, switchOptions, formatSensorValue, navigateOptionsHtml);
  }

  html += R"html(
      <!-- Tab 3: Settings (Network/MQTT Configuration) -->
      <div id="tab-network" class="tab-content">
        <div class="status">
          <div>
            <div class="status-label">)html";
  html += tr.wifi_status;
  html += R"html(</div>
            <div class="status-value">)html";
  html += (WiFi.status() == WL_CONNECTED) ? tr.wifi_connected : tr.wifi_disconnected;
  html += R"html(</div>
          </div>
          <div>
            <div class="status-label">)html";
  html += tr.ssid_label;
  html += R"html(</div>
            <div class="status-value">)html";
  html += WiFi.SSID();
  html += R"html(</div>
          </div>
          <div>
            <div class="status-label">)html";
  html += tr.ip_label;
  html += R"html(</div>
            <div class="status-value">)html";
  html += WiFi.localIP().toString();
  html += R"html(</div>
          </div>
        </div>

        <form action="/mqtt" method="POST">
          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_wifi;
  html += R"html(</div>
            <div class="settings-grid">
              <div>
                <label for="wifi_ssid">)html";
  html += tr.ssid_label;
  html += R"html(</label>
                <input type="text" id="wifi_ssid" name="wifi_ssid" value=")html";
  html += cfg.wifi_ssid;
  html += R"html(">
              </div>
              <div>
                <label for="wifi_pass">)html";
  html += tr.wifi_password_label;
  html += R"html(</label>
                <input type="password" id="wifi_pass" name="wifi_pass" value=")html";
  html += cfg.wifi_pass;
  html += R"html(">
              </div>
              <div>
                <label for="wifi_static_ip">)html";
  html += tr.wifi_static_ip_label;
  html += R"html(</label>
                <input type="text" id="wifi_static_ip" name="wifi_static_ip" value=")html";
  html += cfg.wifi_static_ip;
  html += R"html(" placeholder="192.168.1.50">
              </div>
              <div>
                <label for="wifi_gateway">)html";
  html += tr.wifi_gateway_label;
  html += R"html(</label>
                <input type="text" id="wifi_gateway" name="wifi_gateway" value=")html";
  html += cfg.wifi_gateway;
  html += R"html(" placeholder="192.168.1.1">
              </div>
              <div>
                <label for="wifi_subnet">)html";
  html += tr.wifi_subnet_label;
  html += R"html(</label>
                <input type="text" id="wifi_subnet" name="wifi_subnet" value=")html";
  html += cfg.wifi_subnet;
  html += R"html(" placeholder="255.255.255.0">
              </div>
              <div>
                <label for="wifi_dns">)html";
  html += tr.wifi_dns_label;
  html += R"html(</label>
                <input type="text" id="wifi_dns" name="wifi_dns" value=")html";
  html += cfg.wifi_dns;
  html += R"html(" placeholder="192.168.1.1">
              </div>
              <div class="settings-note settings-full">)html";
  html += tr.wifi_dhcp_hint;
  html += R"html(</div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_mqtt;
  html += R"html(</div>
            <div class="settings-grid">
              <div>
                <label for="mqtt_host">)html";
  html += tr.mqtt_host;
  html += R"html(</label>
                <input type="text" id="mqtt_host" name="mqtt_host" value=")html";
  html += cfg.mqtt_host;
  html += R"html(">
              </div>
              <div>
                <label for="mqtt_port">)html";
  html += tr.mqtt_port;
  html += R"html(</label>
                <input type="number" id="mqtt_port" name="mqtt_port" value=")html";
  html += String(cfg.mqtt_port ? cfg.mqtt_port : 1883);
  html += R"html(">
              </div>
              <div>
                <label for="mqtt_user">)html";
  html += tr.mqtt_username;
  html += R"html(</label>
                <input type="text" id="mqtt_user" name="mqtt_user" value=")html";
  html += cfg.mqtt_user;
  html += R"html(">
              </div>
              <div>
                <label for="mqtt_pass">)html";
  html += tr.mqtt_password;
  html += R"html(</label>
                <input type="password" id="mqtt_pass" name="mqtt_pass" value=")html";
  html += cfg.mqtt_pass;
  html += R"html(">
              </div>
              <div class="settings-full">
                <label for="mqtt_client_id">)html";
  html += tr.mqtt_client_id;
  html += R"html(</label>
                <input type="text" id="mqtt_client_id" name="mqtt_client_id" placeholder=")html";
  html += tr.mqtt_client_id_placeholder;
  html += R"html(" value=")html";
  html += cfg.mqtt_client_id;
  html += R"html(">
                <div class="settings-note">)html";
  html += tr.mqtt_client_id_hint;
  html += R"html(</div>
              </div>
              <div>
                <label for="mqtt_base">)html";
  html += tr.mqtt_base_topic;
  html += R"html(</label>
                <input type="text" id="mqtt_base" name="mqtt_base" value=")html";
  html += cfg.mqtt_base_topic;
  html += R"html(">
              </div>
              <div>
                <label for="ha_prefix">)html";
  html += tr.ha_prefix;
  html += R"html(</label>
                <input type="text" id="ha_prefix" name="ha_prefix" value=")html";
  html += cfg.ha_prefix;
  html += R"html(">
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_language;
  html += R"html(</div>
            <div class="settings-grid">
              <div>
                <label for="language">)html";
  html += tr.language_label;
  html += R"html(</label>
                <select id="language" name="language">)html";
  html += i18n::build_language_options_html(cfg.language);
  html += R"html(</select>
              </div>
            </div>
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

  html.replace(">Speichern</button>", String(">") + tr.save + "</button>");
  html.replace("return confirm('Geraet wirklich neu starten?');", String("return confirm('") + tr.restart_confirm + "');");
  html.replace(">Geraet neu starten</button>", String(">") + tr.restart_button + "</button>");

  return html;
}

String WebAdminServer::getSuccessPage() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String html = "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="utf-8">
  <title>)html";
  html += tr.save;
  html += R"html(</title>
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
    <h1>)html";
  html += tr.mqtt_saved_title;
  html += R"html(</h1>
    <p>)html";
  html += tr.mqtt_saved_message;
  html += R"html(</p>
  </div>
</body>
</html>)html";
  return html;
}

String WebAdminServer::getBridgeSuccessPage() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String html = "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="utf-8">
  <title>)html";
  html += tr.bridge_saved_title;
  html += R"html(</title>
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
    <h1>)html";
  html += tr.bridge_saved_title;
  html += R"html(</h1>
    <p>)html";
  html += tr.bridge_saved_message;
  html += R"html(</p>
  </div>
</body>
</html>)html";
  return html;
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
  json += ",\"mqtt_client_id\":\"" + String(cfg.mqtt_client_id) + "\"";
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

#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include <WiFi.h>
#include "src/core/config_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/game/game_controls_config.h"

String WebAdminServer::getAdminPage() {
  const DeviceConfig& cfg = configManager.getConfig();
  const HaBridgeConfigData& ha = haBridgeConfig.get();
  const auto sensorOptions = parseSensorList(ha.sensors_text);
  const auto sceneOptions = parseSceneList(ha.scene_alias_text);

  auto appendList = [&](String& target, const String& raw) {
    if (!raw.length()) {
      target += "<p class=\"hint\">Keine Eintraege.</p>";
      return;
    }
    target += "<ul class=\"list\">";
    int start = 0;
    while (start < raw.length()) {
      int end = raw.indexOf('\n', start);
      if (end < 0) end = raw.length();
      String line = raw.substring(start, end);
      line.trim();
      if (line.length()) {
        target += "<li>";
        appendHtmlEscaped(target, line);
        target += "</li>";
      }
      start = end + 1;
    }
    target += "</ul>";
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
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; background:#eef2ff; margin:0; padding:0; }
    .wrapper { max-width:820px; margin:20px auto; padding:20px; }
    .card { background:#fff; border-radius:16px; box-shadow:0 20px 45px rgba(15,23,42,0.15); padding:32px; }
    h1 { margin:0 0 8px; font-size:28px; color:#1e293b; }
    .subtitle { color:#475569; margin-bottom:24px; }
    .status { display:grid; grid-template-columns:repeat(auto-fit,minmax(220px,1fr)); gap:12px; margin-bottom:28px; }
    .status div { background:#f8fafc; border-radius:12px; padding:14px; border:1px solid #e2e8f0; }
    .status-label { font-size:12px; text-transform:uppercase; color:#64748b; letter-spacing:.08em; }
    .status-value { font-size:16px; color:#0f172a; font-weight:600; }

    /* Tab Navigation */
    .tab-nav { display:flex; gap:8px; margin-bottom:24px; border-bottom:2px solid #e2e8f0; }
    .tab-btn { padding:12px 24px; border:none; background:transparent; color:#64748b; font-size:15px; font-weight:600; cursor:pointer; border-bottom:3px solid transparent; transition:all 0.3s; }
    .tab-btn:hover { color:#4f46e5; background:#f8fafc; }
    .tab-btn.active { color:#4f46e5; border-bottom-color:#4f46e5; }
    .tab-content { display:none; }
    .tab-content.active { display:block; }

    form { display:grid; gap:16px; margin-bottom:32px; }
    label { font-size:13px; font-weight:600; color:#475569; }
    input { width:100%; padding:12px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; box-sizing:border-box; }
    .btn { padding:12px 18px; border:none; border-radius:10px; background:#4f46e5; color:#fff; font-size:16px; cursor:pointer; }
    .btn-secondary { background:#94a3b8; margin-top:12px; width:100%; }
    .section-title { margin:32px 0 12px; text-transform:uppercase; font-size:12px; letter-spacing:.1em; color:#a1a1aa; }
    .hint { color:#64748b; font-size:14px; margin:8px 0 16px; }
    .list-block { background:#f8fafc; border-radius:12px; padding:16px; border:1px solid #e2e8f0; }
    .list-block strong { display:block; margin:12px 0 6px; color:#1e293b; }
    .list { list-style:none; padding-left:18px; margin:0; }
    .list li { padding:4px 0; font-family:monospace; color:#0f172a; }
    .layout-grid { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:16px; }
    .slot { background:#f8fafc; border:1px solid #e2e8f0; border-radius:12px; padding:12px; }
    .slot-scene { background:#fff7ed; border-color:#fed7aa; }
    .slot-label { font-size:13px; font-weight:600; color:#475569; margin-bottom:8px; }
    .slot select, .slot input { width:100%; box-sizing:border-box; }
    .slot select { padding:10px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; background:#fff; margin-bottom:8px; }
    .slot input { padding:9px; border:1px solid #d6defa; border-radius:10px; font-size:13px; margin-bottom:6px; }
  </style>
  <script>
    function switchTab(tabName) {
      // Hide all tabs
      const tabs = document.querySelectorAll('.tab-content');
      tabs.forEach(tab => tab.classList.remove('active'));

      // Remove active class from all buttons
      const btns = document.querySelectorAll('.tab-btn');
      btns.forEach(btn => btn.classList.remove('active'));

      // Show selected tab
      document.getElementById(tabName).classList.add('active');

      // Highlight active button
      event.target.classList.add('active');
    }

    window.onload = function() {
      // Activate first tab by default
      document.querySelector('.tab-btn').click();
    };
  </script>
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
        <button class="tab-btn" onclick="switchTab('tab-home')">Home</button>
        <button class="tab-btn" onclick="switchTab('tab-game')">Game Controls</button>
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

      <!-- Tab 2: Home (6 Sensors + 6 Scenes) -->
      <div id="tab-home" class="tab-content">
        <p class="hint">Ordne hier die 3x4 Kacheln zu. Die oberen zwei Reihen zeigen Sensoren, die unteren zwei Reihen Szenen. Auswahl &quot;Keine&quot; blendet eine Kachel aus.</p>
        <form action="/bridge" method="POST">
          <div class="layout-grid">
)html";

  auto appendSlot = [&](bool sensor, size_t index, const String& current) {
    const char* labels_sensor[] = {
      "Sensor 1 (oben links)",
      "Sensor 2",
      "Sensor 3",
      "Sensor 4",
      "Sensor 5",
      "Sensor 6"
    };
    const char* labels_scene[] = {
      "Szene 1",
      "Szene 2",
      "Szene 3",
      "Szene 4",
      "Szene 5",
      "Szene 6"
    };
    String field = sensor ? "sensor_slot" : "scene_slot";
    field += static_cast<int>(index);
    html += "<div class=\"slot ";
    html += sensor ? "slot-sensor" : "slot-scene";
    html += "\"><div class=\"slot-label\">";
    html += sensor ? labels_sensor[index] : labels_scene[index];
    html += "</div><select name=\"";
    html += field;
    html += "\"><option value=\"\"";
    if (!current.length()) html += " selected";
    html += ">Keine</option>";

    if (sensor) {
      for (const auto& opt : sensorOptions) {
        bool selected = current.equalsIgnoreCase(opt);
        html += "<option value=\"";
        appendHtmlEscaped(html, opt);
        html += "\"";
        if (selected) html += " selected";
        html += ">";
        String label = humanizeIdentifier(opt, true) + " - " + opt;
        appendHtmlEscaped(html, label);
        html += "</option>";
      }
    } else {
      for (const auto& opt : sceneOptions) {
        bool selected = current.equalsIgnoreCase(opt.alias);
        html += "<option value=\"";
        appendHtmlEscaped(html, opt.alias);
        html += "\"";
        if (selected) html += " selected";
        html += ">";
        String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
        appendHtmlEscaped(html, label);
        html += "</option>";
      }
    }
    html += "</select>";

    String custom_value = sensor ? ha.sensor_titles[index] : ha.scene_titles[index];
    String placeholder;
    if (sensor && current.length()) {
      placeholder = lookupKeyValue(ha.sensor_names_map, current);
      if (!placeholder.length()) {
        placeholder = humanizeIdentifier(current, true);
      }
    } else if (!sensor && current.length()) {
      placeholder = humanizeIdentifier(current, false);
    }
    String input_name = sensor ? "sensor_label" : "scene_label";
    input_name += static_cast<int>(index);
      html += "<input type=\"text\" name=\"";
      html += input_name;
      html += "\" placeholder=\"";
      if (placeholder.length()) {
        appendHtmlEscaped(html, String("Standard: ") + placeholder);
      } else {
      html += "Eigener Titel";
    }
    html += "\" value=\"";
    appendHtmlEscaped(html, custom_value);
    html += "\">";

    if (sensor) {
      String unit_input = "sensor_unit";
      unit_input += static_cast<int>(index);
      String unit_value = ha.sensor_custom_units[index];
      html += "<input type=\"text\" name=\"";
      html += unit_input;
      html += "\" maxlength=\"10\" placeholder=\"Einheit z.B. &deg;C\" value=\"";
      appendHtmlEscaped(html, unit_value);
      html += "\">";
    }

    html += "</div>";
  };

  for (size_t i = 0; i < HA_SENSOR_SLOT_COUNT; ++i) {
    appendSlot(true, i, ha.sensor_slots[i]);
  }
  for (size_t i = 0; i < HA_SCENE_SLOT_COUNT; ++i) {
    appendSlot(false, i, ha.scene_slots[i]);
  }

  html += R"html(
          </div>
          <button class="btn" type="submit">Layout speichern</button>
        </form>

        <div class="section-title">Home Assistant Bridge</div>
        <p class="hint">Konfiguration erfolgt in Home Assistant - diese Liste dient nur zur Anzeige.</p>
        <div class="list-block">
          <strong>Sensoren</strong>)html";
  appendList(html, ha.sensors_text);
  html += R"html(
          <strong>Szenen</strong>)html";
  appendList(html, ha.scene_alias_text);
  html += R"html(
        </div>
      </div>

      <!-- Tab 3: Game Controls (12 Buttons) -->
      <div id="tab-game" class="tab-content">
        <p class="hint">Konfiguriere 12 Buttons für USB-Tastatur-Makros (z.B. Star Citizen). Gerät muss per USB am PC angeschlossen sein.</p>
        <form action="/game_controls" method="POST">
          <div class="layout-grid">
)html";

  // Game Controls - 12 Buttons
  const GameControlsConfigData& gameData = gameControlsConfig.get();
  for (size_t i = 0; i < GAME_BUTTON_COUNT; ++i) {
    html += "<div class=\"slot\" style=\"background:#f0fdf4;border-color:#bbf7d0;\"><div class=\"slot-label\">Button ";
    html += String((int)i + 1);
    html += "</div><input type=\"text\" name=\"game_name";
    html += String((int)i);
    html += "\" placeholder=\"z.B. Landing Gear\" value=\"";
    appendHtmlEscaped(html, gameData.buttons[i].name);
    html += "\" style=\"margin-bottom:8px;\"><input type=\"text\" name=\"game_macro";
    html += String((int)i);
    html += "\" placeholder=\"z.B. g oder ctrl+g oder ctrl+shift+a\" value=\"";

    // Aktuelles Makro anzeigen (aus key_code + modifier rekonstruieren)
    String currentMacro = "";
    if (gameData.buttons[i].key_code != 0) {
      // Modifier hinzufügen
      if (gameData.buttons[i].modifier & 0x01) currentMacro += "ctrl+";
      if (gameData.buttons[i].modifier & 0x02) currentMacro += "shift+";
      if (gameData.buttons[i].modifier & 0x04) currentMacro += "alt+";

      // Taste hinzufügen (Scancode zu Buchstabe konvertieren)
      uint8_t code = gameData.buttons[i].key_code;
      if (code >= 0x04 && code <= 0x1D) currentMacro += (char)('a' + (code - 0x04));
      else if (code >= 0x1E && code <= 0x27) currentMacro += (char)('1' + (code - 0x1E));
      else if (code == 0x2C) currentMacro += "space";
      else if (code == 0x28) currentMacro += "enter";
      else if (code == 0x2A) currentMacro += "backspace";
      else if (code == 0x2B) currentMacro += "tab";
      else if (code == 0x29) currentMacro += "esc";
      else currentMacro += "?";
    }

    appendHtmlEscaped(html, currentMacro);
    html += "\"><div style=\"margin-top:6px;font-size:11px;color:#64748b;\">Beispiele: g, ctrl+g, ctrl+shift+a, space, enter</div></div>";
  }

  html += R"html(
          </div>
          <button class="btn" type="submit">Game Controls speichern</button>
        </form>
      </div>

      <!-- Restart button at bottom (always visible) -->
      <form action="/restart" method="POST" onsubmit="return confirm('Geraet wirklich neu starten?');" style="margin-top:32px;">
        <button class="btn btn-secondary" type="submit">Geraet neu starten</button>
      </form>
    </div>
  </div>
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
  json += "}";
  return json;
}

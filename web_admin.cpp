#include "web_admin.h"

#include <WiFi.h>
#include <algorithm>
#include <vector>
#include <ctype.h>

#include "network_manager.h"
#include "tab_home.h"
#include "mqtt_handlers.h"
#include "tab_settings.h"

WebAdminServer webAdminServer;

WebAdminServer::WebAdminServer() : server(80), running(false) {}

bool WebAdminServer::start() {
  if (running) {
    Serial.println("[WebAdmin] Server laeuft bereits");
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WebAdmin] Start abgebrochen - kein WiFi");
    return false;
  }

  server.on("/", [this]() { this->handleRoot(); });
  server.on("/mqtt", HTTP_POST, [this]() { this->handleSaveMQTT(); });
  server.on("/status", [this]() { this->handleStatus(); });
  server.on("/bridge_refresh", HTTP_POST, [this]() { this->handleBridgeRefresh(); });
  server.on("/bridge", HTTP_POST, [this]() { this->handleSaveBridge(); });
  server.on("/restart", HTTP_POST, [this]() { this->handleRestart(); });

  server.begin();
  running = true;
  IPAddress ip = WiFi.localIP();
  Serial.printf("[WebAdmin] erreichbar unter http://%s\n", ip.toString().c_str());
  return true;
}

void WebAdminServer::stop() {
  if (!running) return;
  server.stop();
  running = false;
  Serial.println("[WebAdmin] Server gestoppt");
}

void WebAdminServer::handle() {
  if (!running) return;
  server.handleClient();
}

void WebAdminServer::handleRoot() {
  server.send(200, "text/html", getAdminPage());
}

struct SceneOption {
  String alias;
  String entity;
};

static void copyToBuffer(char* dest, size_t max_len, const String& value) {
  if (!dest || !max_len) return;
  size_t copy_len = std::min(value.length(), max_len - 1);
  memcpy(dest, value.c_str(), copy_len);
  dest[copy_len] = '\0';
}

static void appendHtmlEscaped(String& out, const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c; break;
    }
  }
}

static String humanizeIdentifier(const String& raw, bool strip_domain) {
  if (!raw.length()) return String("--");
  String text = raw;
  if (strip_domain) {
    int dot = text.indexOf('.');
    if (dot >= 0) text = text.substring(dot + 1);
  }
  text.replace('_', ' ');
  bool new_word = true;
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text.charAt(i);
    if (isalpha(static_cast<unsigned char>(c))) {
      char mapped = new_word ? toupper(static_cast<unsigned char>(c))
                             : tolower(static_cast<unsigned char>(c));
      text.setCharAt(i, mapped);
      new_word = false;
    } else {
      new_word = (c == ' ' || c == '-' || c == '/');
    }
  }
  text.trim();
  return text.length() ? text : raw;
}

static std::vector<String> parseSensorList(const String& raw) {
  std::vector<String> out;
  int start = 0;
  while (start < raw.length()) {
    int end = raw.indexOf('\n', start);
    if (end < 0) end = raw.length();
    String line = raw.substring(start, end);
    line.trim();
    if (line.length()) out.push_back(line);
    start = end + 1;
  }
  return out;
}

static std::vector<SceneOption> parseSceneList(const String& raw) {
  std::vector<SceneOption> out;
  int start = 0;
  while (start < raw.length()) {
    int end = raw.indexOf('\n', start);
    if (end < 0) end = raw.length();
    String line = raw.substring(start, end);
    int eq = line.indexOf('=');
    if (eq > 0) {
      SceneOption opt;
      opt.alias = line.substring(0, eq);
      opt.alias.trim();
      opt.alias.toLowerCase();
      opt.entity = line.substring(eq + 1);
      opt.entity.trim();
      if (opt.alias.length() && opt.entity.length()) {
        out.push_back(opt);
      }
    }
    start = end + 1;
  }
  return out;
}

static String normalizeSensorSelection(const String& selection,
                                       const std::vector<String>& options) {
  String trimmed = selection;
  trimmed.trim();
  for (const auto& opt : options) {
    if (opt.equalsIgnoreCase(trimmed)) {
      return opt;
    }
  }
  return "";
}

static String normalizeSceneSelection(const String& selection,
                                      const std::vector<SceneOption>& options) {
  String trimmed = selection;
  trimmed.trim();
  trimmed.toLowerCase();
  for (const auto& opt : options) {
    if (opt.alias == trimmed) {
      return opt.alias;
    }
  }
  return "";
}

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

static String lookupKeyValue(const String& text, const String& key) {
  if (!key.length()) return "";
  int start = 0;
  while (start < text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    String line = text.substring(start, end);
    int eq = line.indexOf('=');
    if (eq > 0) {
      String lhs = line.substring(0, eq);
      lhs.trim();
      if (lhs.equalsIgnoreCase(key)) {
        String rhs = line.substring(eq + 1);
        rhs.trim();
        return rhs;
      }
    }
    start = end + 1;
  }
  return "";
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
  html.reserve(9000);
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
    form { display:grid; gap:16px; margin-bottom:32px; }
    label { font-size:13px; font-weight:600; color:#475569; }
    input { width:100%; padding:12px; border:1px solid #cbd5f5; border-radius:10px; font-size:15px; }
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
</head>
<body>
  <div class="wrapper">
    <div class="card">
      <h1>Tab5 Admin-Panel</h1>
      <p class="subtitle">MQTT-Konfiguration &amp; Uebersicht</p>
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

      <div class="section-title">Home-Layout</div>
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

      <form action="/restart" method="POST" onsubmit="return confirm('Geraet wirklich neu starten?');">
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

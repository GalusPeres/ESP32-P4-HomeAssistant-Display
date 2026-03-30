#include "src/web/web_config.h"
#include "src/devices/device_select.h"
#include "src/core/i18n.h"
#include <WiFi.h>

// Globale Instanz
WebConfigServer webConfigServer;

// Hotspot Einstellungen
static const char* AP_SSID = "WS_P4_Config";
static const char* AP_PASS = "12345678";  // Mindestens 8 Zeichen für WPA2
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

static void restoreStaModeAfterAp() {
#if defined(DEVICE_M5STACK_TAB5)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
#endif
}

WebConfigServer::WebConfigServer() : server(80), running(false), config_saved(false) {
}

bool WebConfigServer::start() {
  if (running) {
    Serial.println("⚠️ WebConfigServer läuft bereits");
    return true;
  }

  Serial.println("\n🌐 Starte WiFi-Konfigurationsmodus...");

  // Stoppe bisherige WiFi-Verbindung (hilft beim Captive Portal)
  // Stoppe bisherige WiFi-Verbindung (hilft beim Captive Portal)
  WiFi.disconnect();
  delay(100);

  // AP + STA wie im alten Tab5_LVGL-Pfad.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  // Starte AP mit expliziten Einstellungen
  // Channel 1, SSID nicht versteckt, max 4 Verbindungen
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  if (!ap_ok) {
    restoreStaModeAfterAp();
  }
  if (!ap_ok) {
    Serial.println("❌ Access Point konnte nicht gestartet werden!");
    return false;
  }

  delay(500);

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("✓ Access Point gestartet: %s\n", AP_SSID);
  Serial.printf("  Passwort: %s\n", AP_PASS);
  Serial.printf("  IP-Adresse: %s\n", ip.toString().c_str());

  // DNS Server für Captive Portal (alle DNS-Anfragen zu uns umleiten)
  if (dnsServer.start(53, "*", AP_IP)) {
    Serial.println("✓ DNS Server gestartet (Port 53, alle Domains → 192.168.4.1)");
  } else {
    Serial.println("⚠️ DNS Server konnte nicht gestartet werden!");
  }

  // Webserver Routes
  server.on("/", [this]() { this->handleRoot(); });
  auto captive_handler = [this]() { this->handleCaptivePortal(); };
  server.on("/generate_204", captive_handler);
  server.on("/gen_204", captive_handler);
  server.on("/hotspot-detect.html", captive_handler);
  server.on("/library/test/success.html", captive_handler);
  server.on("/success.txt", captive_handler);
  server.on("/ncsi.txt", captive_handler);
  server.on("/connecttest.txt", captive_handler);
  server.on("/redirect", captive_handler);
  server.on("/wpad.dat", captive_handler);
  server.on("/favicon.ico", captive_handler);
  server.on("/save", HTTP_POST, [this]() { this->handleSave(); });
  server.onNotFound([this]() { this->handleNotFound(); });

  server.begin();
  Serial.println("✓ Webserver gestartet auf http://192.168.4.1");
  Serial.println("  Verbinde dich mit WiFi 'WS_P4_Config' und öffne Browser");

  running = true;
  config_saved = false;
  return true;
}

void WebConfigServer::stop() {
  if (!running) return;

  Serial.println("🛑 Stoppe WebConfigServer...");

  dnsServer.stop();
  Serial.println("  ✓ DNS Server gestoppt");

  server.stop();
  Serial.println("  ✓ Webserver gestoppt");

  WiFi.softAPdisconnect(true);
  restoreStaModeAfterAp();
  Serial.println("  ✓ AP getrennt");

  Serial.println("  ✓ WiFi-Modus: AP/STA");

  running = false;
  Serial.println("✓ WebConfigServer gestoppt - bereit für normale WiFi-Verbindung");
}

void WebConfigServer::handle() {
  if (!running) return;
  dnsServer.processNextRequest();
  server.handleClient();
}

void WebConfigServer::handleRoot() {
  Serial.println("📄 Config-Seite angefordert");
  server.send(200, "text/html", getConfigPage());
}

void WebConfigServer::handleCaptivePortal() {
  String path = server.uri();
  Serial.printf("🌐 Captive Portal Request: %s (Host: %s)\n", path.c_str(), server.hostHeader().c_str());

  // FÜR ALLE CAPTIVE PORTAL CHECKS: Zeige direkt die Config-Seite!
  // Das ist die robusteste Lösung für moderne Smartphones
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.send(200, "text/html", getConfigPage());
  Serial.println("  ✓ Config-Seite direkt gesendet");
}

void WebConfigServer::handleSave() {
  Serial.println("💾 Speichere WiFi-Konfiguration...");

  // Lese POST-Parameter (nur WiFi im AP-Modus!)
  DeviceConfig cfg = configManager.getConfig();  // enthaelt Defaultwerte (Display, Sleep, MQTT)
  if (!configManager.isConfigured()) {
    cfg.mqtt_port = 1883;
    if (cfg.display_brightness < 121 || cfg.display_brightness > 255) {
      cfg.display_brightness = 200;  // Sicherstellen, dass das Display nicht dunkel bleibt
    }
    if (cfg.auto_sleep_seconds == 0) {
      cfg.auto_sleep_seconds = 60;
    }
    if (cfg.auto_sleep_battery_seconds == 0) {
      cfg.auto_sleep_battery_seconds = cfg.auto_sleep_seconds;
    }
    cfg.auto_sleep_battery_enabled = cfg.auto_sleep_enabled;
    if (cfg.mqtt_base_topic[0] == '\0') {
      strncpy(cfg.mqtt_base_topic, "tab5", CONFIG_MQTT_BASE_MAX - 1);
    }
    if (cfg.ha_prefix[0] == '\0') {
      strncpy(cfg.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);
    }
  }

  // Überschreibe nur WiFi-Daten
  if (server.hasArg("wifi_ssid")) {
    String ssid = server.arg("wifi_ssid");
    strncpy(cfg.wifi_ssid, ssid.c_str(), CONFIG_WIFI_SSID_MAX - 1);
  }

  if (server.hasArg("wifi_pass")) {
    String pass = server.arg("wifi_pass");
    strncpy(cfg.wifi_pass, pass.c_str(), CONFIG_WIFI_PASS_MAX - 1);
  }

  // Validierung
  if (strlen(cfg.wifi_ssid) == 0) {
    const auto& tr = i18n::strings(cfg.language);
    server.send(400, "text/html", String("<h1>") + tr.ap_wifi_required + "</h1>");
    return;
  }

  // Speichere Konfiguration (MQTT-Daten bleiben erhalten!)
  if (configManager.save(cfg)) {
    Serial.println("✓ WiFi-Konfiguration erfolgreich gespeichert");
    config_saved = true;
    server.send(200, "text/html", getSuccessPage());
  } else {
    Serial.println("❌ Fehler beim Speichern der Konfiguration");
    const auto& tr = i18n::strings(cfg.language);
    server.send(500, "text/html", String("<h1>") + tr.save_failed + "</h1>");
  }
}

void WebConfigServer::handleNotFound() {
  String path = server.uri();
  Serial.printf("❓ Not Found: %s (Host: %s)\n", path.c_str(), server.hostHeader().c_str());

  // Zeige für ALLE nicht gefundenen Pfade die Config-Seite
  // Das sorgt dafür, dass Captive Portal Detection auf jeden Fall funktioniert
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.send(200, "text/html", getConfigPage());
  Serial.println("  ✓ Config-Seite gesendet (Not Found → Config Page)");
}

String WebConfigServer::getConfigPage() {
  // Lade bestehende Konfiguration wenn vorhanden
  const DeviceConfig& cfg = configManager.getConfig();
  const auto& tr = i18n::strings(cfg.language);

  String html = "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>)html";
  html += tr.ap_window_title;
  html += R"html(</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 500px;
      width: 100%;
      padding: 40px;
    }
    h1 {
      color: #2d3748;
      font-size: 28px;
      margin-bottom: 8px;
      text-align: center;
    }
    .subtitle {
      color: #718096;
      text-align: center;
      margin-bottom: 32px;
      font-size: 14px;
    }
    .section {
      margin-bottom: 24px;
    }
    .section-title {
      color: #667eea;
      font-size: 14px;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      margin-bottom: 16px;
      padding-bottom: 8px;
      border-bottom: 2px solid #e2e8f0;
    }
    .form-group {
      margin-bottom: 16px;
    }
    label {
      display: block;
      color: #4a5568;
      font-size: 14px;
      font-weight: 500;
      margin-bottom: 6px;
    }
    input {
      width: 100%;
      padding: 12px 16px;
      border: 2px solid #e2e8f0;
      border-radius: 8px;
      font-size: 15px;
      transition: all 0.2s;
      font-family: inherit;
    }
    input:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
    }
    input::placeholder {
      color: #a0aec0;
    }
    .hint {
      color: #a0aec0;
      font-size: 12px;
      margin-top: 4px;
    }
    .btn {
      width: 100%;
      padding: 14px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: transform 0.2s, box-shadow 0.2s;
      margin-top: 8px;
    }
    .btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
    }
    .btn:active {
      transform: translateY(0);
    }
    .icon {
      font-size: 48px;
      text-align: center;
      margin-bottom: 16px;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="icon">📶</div>
    <h1>)html";
  html += tr.ap_heading;
  html += R"html(</h1>
    <p class="subtitle">)html";
  html += tr.ap_subtitle;
  html += R"html(</p>

    <form action="/save" method="POST">
      <div class="section">
        <div class="section-title">📶 WiFi-Verbindung</div>
        <div class="form-group">
          <label for="wifi_ssid">SSID (Netzwerkname)</label>
          <input type="text" id="wifi_ssid" name="wifi_ssid" placeholder="Mein WiFi" value=")html" + String(cfg.wifi_ssid) + R"html(" required>
        </div>
        <div class="form-group">
          <label for="wifi_pass">Passwort</label>
          <input type="password" id="wifi_pass" name="wifi_pass" placeholder="Passwort" value=")html" + String(cfg.wifi_pass) + R"html(">
          <div class="hint">Leer lassen für offenes Netzwerk</div>
        </div>
      </div>

      <div class="info" style="background: #fff3cd; border-left: 4px solid #ffc107; color: #856404; margin-bottom: 20px;">
        ℹ️ <strong>Hinweis:</strong> Nach erfolgreicher WLAN-Verbindung kannst du über das Webinterface im normalen Netzwerk die MQTT-Einstellungen konfigurieren.
      </div>

      <button type="submit" class="btn">💾 Speichern & Verbinden</button>
    </form>
  </div>
</body>
</html>
)html";

  html.replace("WiFi-Verbindung", tr.ap_wifi_section);
  html.replace("SSID (Netzwerkname)", tr.ap_wifi_ssid_label);
  html.replace("placeholder=\"Mein WiFi\"", String("placeholder=\"") + tr.ap_wifi_ssid_placeholder + "\"");
  html.replace(">Passwort</label>", String(">") + tr.ap_wifi_password_label + "</label>");
  html.replace("placeholder=\"Passwort\"", String("placeholder=\"") + tr.ap_wifi_password_placeholder + "\"");
  html.replace("Leer lassen fÃ¼r offenes Netzwerk", tr.ap_wifi_open_hint);
  html.replace("Hinweis:", tr.ap_info_notice);
  html.replace("Nach erfolgreicher WLAN-Verbindung kannst du Ã¼ber das Webinterface im normalen Netzwerk die MQTT-Einstellungen konfigurieren.", tr.ap_info_message);
  html.replace("Speichern & Verbinden", tr.ap_save_connect);

  return html;
}

String WebConfigServer::getSuccessPage() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String html = R"html(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Konfiguration gespeichert</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 16px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      max-width: 500px;
      width: 100%;
      padding: 40px;
      text-align: center;
    }
    .icon {
      font-size: 72px;
      margin-bottom: 24px;
      animation: bounce 0.6s;
    }
    @keyframes bounce {
      0%, 100% { transform: translateY(0); }
      50% { transform: translateY(-20px); }
    }
    h1 {
      color: #2d3748;
      font-size: 28px;
      margin-bottom: 16px;
    }
    p {
      color: #718096;
      font-size: 16px;
      line-height: 1.6;
      margin-bottom: 24px;
    }
    .info {
      background: #f7fafc;
      border-left: 4px solid #667eea;
      padding: 16px;
      border-radius: 8px;
      text-align: left;
      color: #4a5568;
      font-size: 14px;
    }
  </style>
  <script>
    setTimeout(function() {
      window.location.href = '/';
    }, 10000);
  </script>
</head>
<body>
  <div class="container">
    <div class="icon">✅</div>
    <h1>Erfolgreich gespeichert!</h1>
    <p>Die Konfiguration wurde erfolgreich gespeichert.<br>Das Gerät wird jetzt neu gestartet und versucht sich mit dem WiFi zu verbinden.</p>
    <div class="info">
      ℹ️ Du wirst in 10 Sekunden automatisch weitergeleitet.<br>
      Falls die Verbindung nicht klappt, aktiviere den Hotspot-Modus erneut über die Einstellungen.
    </div>
  </div>
</body>
</html>
)html";

  html.replace("<html lang=\"de\">", String("<html lang=\"") + tr.html_lang + "\">");
  html.replace("<title>Konfiguration gespeichert</title>", String("<title>") + tr.ap_success_title + "</title>");
  html.replace("Erfolgreich gespeichert!", tr.ap_success_title);
  html.replace("Die Konfiguration wurde erfolgreich gespeichert.<br>Das GerÃ¤t wird jetzt neu gestartet und versucht sich mit dem WiFi zu verbinden.", tr.ap_success_message);
  html.replace("â„¹ï¸ Du wirst in 10 Sekunden automatisch weitergeleitet.<br>\r\n      Falls die Verbindung nicht klappt, aktiviere den Hotspot-Modus erneut Ã¼ber die Einstellungen.", String("â„¹ï¸ ") + tr.ap_success_notice);
  html.replace("â„¹ï¸ Du wirst in 10 Sekunden automatisch weitergeleitet.<br>\n      Falls die Verbindung nicht klappt, aktiviere den Hotspot-Modus erneut Ã¼ber die Einstellungen.", String("â„¹ï¸ ") + tr.ap_success_notice);

  return html;
}

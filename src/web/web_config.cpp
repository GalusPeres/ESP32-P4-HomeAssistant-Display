#include "src/web/web_config.h"
#include "src/devices/device_select.h"
#include "src/devices/device.h"
#include "src/web/web_admin_utils.h"
#include "src/web/web_admin_fonts.h"
#include <WiFi.h>

// Globale Instanz
WebConfigServer webConfigServer;

// Hotspot Einstellungen
static const char* AP_PASS = "12345678";  // Mindestens 8 Zeichen für WPA2
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);

namespace {

const char* apSsidForDevice() {
#if defined(DEVICE_M5STACKS_TAB5)
  return "M5Stacks_Tab5_Config";
#elif defined(DEVICE_GUITION_JC8012P4A1)
  return "Guition_JC8012P4A1_Config";
#elif defined(DEVICE_GUITION_JC1060P470C)
  return "Guition_JC1060P470C_Config";
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_7)
  return "Waveshare_LCD7_Config";
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
  return "Waveshare_LCD8_Config";
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1)
  return "Waveshare_LCD10_1_Config";
#elif defined(DEVICE_LAYOUT_TEST_1024X600)
  return "HomeTiles_1024x600_Test";
#elif defined(DEVICE_LAYOUT_TEST_480X480)
  return "HomeTiles_480x480_Test";
#elif defined(DEVICE_WAVESHARE_4B)
  return "Waveshare_B4_Config";
#else
  return "ESP32_P4_Config";
#endif
}

}  // namespace

const char* webConfigApSsid() {
  return apSsidForDevice();
}

const char* webConfigApPassword() {
  return AP_PASS;
}

static void restoreStaModeAfterAp() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
}

WebConfigServer::WebConfigServer()
    : server(80), running(false), config_saved(false),
      routes_registered(false) {}

bool WebConfigServer::start() {
  if (running) {
    Serial.println("⚠️ WebConfigServer läuft bereits");
    return true;
  }

  Serial.println("\n🌐 Starte WiFi-Konfigurationsmodus...");

  // Stoppe bisherige WiFi-Verbindung (hilft beim Captive Portal)
  WiFi.disconnect();
  delay(100);

  // AP + STA wie im alten Tab5_LVGL-Pfad.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  // Starte AP mit expliziten Einstellungen
  // Channel 1, SSID nicht versteckt, max 4 Verbindungen
  const char* ap_ssid = webConfigApSsid();
  bool ap_ok = WiFi.softAP(ap_ssid, AP_PASS, 1, 0, 4);
  if (!ap_ok) {
    restoreStaModeAfterAp();
  }
  if (!ap_ok) {
    Serial.println("❌ Access Point konnte nicht gestartet werden!");
    return false;
  }

  delay(500);

  IPAddress ip = WiFi.softAPIP();
  Serial.printf("✓ Access Point gestartet: %s\n", ap_ssid);
  Serial.printf("  Passwort: %s\n", AP_PASS);
  Serial.printf("  IP-Adresse: %s\n", ip.toString().c_str());

  // DNS Server für Captive Portal (alle DNS-Anfragen zu uns umleiten)
  if (dnsServer.start(53, "*", AP_IP)) {
    Serial.println("✓ DNS Server gestartet (Port 53, alle Domains → 192.168.4.1)");
  } else {
    Serial.println("⚠️ DNS Server konnte nicht gestartet werden!");
  }

  // WebServer::stop() schliesst nur den Socket und behaelt alle Handler.
  // Daher die Routen pro Objekt genau einmal registrieren; andernfalls
  // kostet jeder AP-Start dauerhaft internen Heap.
  if (!routes_registered) {
    server.on("/", [this]() { this->handleRoot(); });
    server.on("/assets/inter-4.1-regular.woff2", HTTP_GET,
              [this]() { sendWebFontRegular(this->server); });
    server.on("/assets/inter-4.1-semibold.woff2", HTTP_GET,
              [this]() { sendWebFontSemibold(this->server); });
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
    routes_registered = true;
  }

  server.begin();
  Serial.println("✓ Webserver gestartet auf http://192.168.4.1");
  Serial.printf("  Verbinde dich mit WiFi '%s' und öffne Browser\n", ap_ssid);

  running = true;
  config_saved = false;
  return true;
}

void WebConfigServer::stop() {
  if (!running)
    return;

  Serial.println("🛑 Stoppe WebConfigServer...");

  dnsServer.stop();
  Serial.println("  ✓ DNS Server gestoppt");

  server.stop();
  Serial.println("  ✓ Webserver gestoppt");

  WiFi.softAPdisconnect(true);
  restoreStaModeAfterAp();
  Serial.println("  ✓ AP getrennt");

  Serial.println("  ✓ WiFi-Modus: STA");

  running = false;
  Serial.println(
      "✓ WebConfigServer gestoppt - bereit für normale WiFi-Verbindung");
}

void WebConfigServer::handle() {
  if (!running) return;
  dnsServer.processNextRequest();
  server.handleClient();
}

void WebConfigServer::handleRoot() {
  Serial.println("📄 Config-Seite angefordert");
  sendChunkedResponse(server, 200, "text/html", getConfigPage());
}

void WebConfigServer::handleCaptivePortal() {
  String path = server.uri();
  Serial.printf("🌐 Captive Portal Request: %s (Host: %s)\n", path.c_str(), server.hostHeader().c_str());

  // FÜR ALLE CAPTIVE PORTAL CHECKS: Zeige direkt die Config-Seite!
  // Das ist die robusteste Lösung für moderne Smartphones
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  sendChunkedResponse(server, 200, "text/html", getConfigPage());
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
      strncpy(cfg.mqtt_base_topic, "hometiles", CONFIG_MQTT_BASE_MAX - 1);
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

  // Captive portal always resets WiFi addressing back to DHCP.
  // This prevents stale static IP settings from locking the device out
  // when the user only wants to reconnect it to a network.
  cfg.wifi_static_enabled = false;
  cfg.wifi_static_ip[0] = '\0';
  cfg.wifi_gateway[0] = '\0';
  cfg.wifi_subnet[0] = '\0';
  cfg.wifi_dns[0] = '\0';

  // Validierung
  if (strlen(cfg.wifi_ssid) == 0) {
    server.send(400, "text/html", "<h1>WiFi SSID is required</h1>");
    return;
  }

  // Speichere Konfiguration (MQTT-Daten bleiben erhalten!)
  if (configManager.save(cfg)) {
    Serial.println("✓ WiFi-Konfiguration erfolgreich gespeichert");
    config_saved = true;
    sendChunkedResponse(server, 200, "text/html", getSuccessPage());
  } else {
    Serial.println("❌ Fehler beim Speichern der Konfiguration");
    server.send(500, "text/html", "<h1>Saving the configuration failed</h1>");
  }
}

void WebConfigServer::handleNotFound() {
  String path = server.uri();
  Serial.printf("❓ Not Found: %s (Host: %s)\n", path.c_str(), server.hostHeader().c_str());

  // Zeige für ALLE nicht gefundenen Pfade die Config-Seite
  // Das sorgt dafür, dass Captive Portal Detection auf jeden Fall funktioniert
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  sendChunkedResponse(server, 200, "text/html", getConfigPage());
  Serial.println("  ✓ Config-Seite gesendet (Not Found → Config Page)");
}

String WebConfigServer::getConfigPage() {
  // Lade bestehende Konfiguration wenn vorhanden
  const DeviceConfig& cfg = configManager.getConfig();
  const String ap_page_title = String(Device::displayName()) + " WiFi Configuration";

  String html = "<!DOCTYPE html>\n<html lang=\"en\">\n";
  html += R"html(<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>)html";
  html += ap_page_title;
  html += R"html(</title>
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 48 48'%3E%3Crect width='48' height='48' rx='10' fill='%2316181c'/%3E%3Crect x='4' y='4' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Crect x='27' y='4' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Crect x='4' y='27' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Cpath d='M33 26h5v6.5h6.5v5H38V44h-5v-6.5h-6.5v-5H33z' fill='%2326a69a'/%3E%3C/svg%3E">
)html";
  appendWebFontFaceStyles(html);
  html += R"html(
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'HomeTiles Inter', sans-serif;
      background: #0a0a0a;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: #1c1c1c;
      border: 1px solid #2a2a2a;
      border-radius: 22px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.5);
      max-width: 420px;
      width: 100%;
      padding: 32px;
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 14px;
      margin-bottom: 28px;
    }
    .brand h1 {
      color: #ffffff;
      font-size: 22px;
    }
    .brand .device {
      color: #8a8a8a;
      font-size: 13px;
      margin-top: 2px;
    }
    .form-group {
      margin-bottom: 16px;
    }
    label {
      display: block;
      color: #b8b8b8;
      font-size: 14px;
      font-weight: 500;
      margin-bottom: 6px;
    }
    input {
      width: 100%;
      padding: 12px 16px;
      border: 1px solid #333333;
      border-radius: 12px;
      background: #141414;
      color: #ffffff;
      font-size: 15px;
      transition: border-color 0.2s;
      font-family: inherit;
    }
    input:focus {
      outline: none;
      border-color: #26a69a;
    }
    input::placeholder {
      color: #666666;
    }
    .password-field {
      display: flex;
      gap: 8px;
      align-items: center;
    }
    .password-field input {
      flex: 1 1 auto;
    }
    .password-toggle {
      flex: 0 0 auto;
      padding: 12px 14px;
      border: 1px solid #333333;
      border-radius: 12px;
      background: #141414;
      color: #b8b8b8;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
    }
    .hint {
      color: #666666;
      font-size: 12px;
      margin-top: 4px;
    }
    .btn {
      width: 100%;
      padding: 14px;
      /* Gruen wie alle "Los"-Aktionen (Verbinden/Speichern/Update) - Tuerkis
         bleibt Akzentfarbe fuer Fokus-Ringe und Logo. */
      background: #2e7d32;
      color: white;
      border: none;
      border-radius: 12px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      margin-top: 8px;
    }
    .btn:active {
      background: #1b5e20;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="brand">
      <svg width="48" height="48" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg" style="flex-shrink:0;" aria-hidden="true">
        <rect x="4" y="4" width="17" height="17" rx="4" fill="#ffffff"/>
        <rect x="27" y="4" width="17" height="17" rx="4" fill="#ffffff"/>
        <rect x="4" y="27" width="17" height="17" rx="4" fill="#ffffff"/>
        <path d="M33 26h5v6.5h6.5v5H38V44h-5v-6.5h-6.5v-5H33z" fill="#26a69a"/>
      </svg>
      <div>
        <h1>HomeTiles</h1>
        <div class="device">)html";
  html += String(Device::displayName());
  html += R"html(</div>
      </div>
    </div>

    <form action="/save" method="POST">
      <div class="form-group">
        <label for="wifi_ssid">Network</label>
        <input type="text" id="wifi_ssid" name="wifi_ssid" placeholder="My WiFi" value=")html";
  html += String(cfg.wifi_ssid);
  html += R"html(" required>
      </div>
      <div class="form-group">
        <label for="wifi_pass">Password</label>
        <div class="password-field">
          <input type="password" id="wifi_pass" name="wifi_pass" placeholder="Password" value=")html";
  html += String(cfg.wifi_pass);
  html += R"html(">
          <button type="button" class="password-toggle" onclick="togglePasswordVisibility('wifi_pass', this)">Show</button>
        </div>
        <div class="hint">Leave empty for an open network</div>
      </div>

      <button type="submit" class="btn">Connect</button>
    </form>
  </div>
  <script>
    function togglePasswordVisibility(inputId, buttonEl) {
      const input = document.getElementById(inputId);
      if (!input || !buttonEl) return;
      const isHidden = input.type === 'password';
      input.type = isHidden ? 'text' : 'password';
      buttonEl.textContent = isHidden ? 'Hide' : 'Show';
    }
  </script>
</body>
</html>
)html";

  return html;
}

String WebConfigServer::getSuccessPage() {
  String html = R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Connecting...</title>
  <link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 48 48'%3E%3Crect width='48' height='48' rx='10' fill='%2316181c'/%3E%3Crect x='4' y='4' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Crect x='27' y='4' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Crect x='4' y='27' width='17' height='17' rx='4' fill='%23ffffff'/%3E%3Cpath d='M33 26h5v6.5h6.5v5H38V44h-5v-6.5h-6.5v-5H33z' fill='%2326a69a'/%3E%3C/svg%3E">
)html";
  appendWebFontFaceStyles(html);
  html += R"html(
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'HomeTiles Inter', sans-serif;
      background: #0a0a0a;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: #1c1c1c;
      border: 1px solid #2a2a2a;
      border-radius: 22px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.5);
      max-width: 420px;
      width: 100%;
      padding: 32px;
      text-align: center;
    }
    .logo {
      margin: 0 auto 20px;
      width: 56px;
      height: 56px;
    }
    h1 {
      color: #ffffff;
      font-size: 22px;
      margin-bottom: 10px;
    }
    p {
      color: #8a8a8a;
      font-size: 14px;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <div class="container">
    <svg class="logo" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
      <rect x="4" y="4" width="17" height="17" rx="4" fill="#ffffff"/>
      <rect x="27" y="4" width="17" height="17" rx="4" fill="#ffffff"/>
      <rect x="4" y="27" width="17" height="17" rx="4" fill="#ffffff"/>
      <path d="M33 26h5v6.5h6.5v5H38V44h-5v-6.5h-6.5v-5H33z" fill="#26a69a"/>
    </svg>
    <h1>Connecting...</h1>
    <p>HomeTiles is joining your network now.<br>This page will lose its connection - you can close it.</p>
  </div>
</body>
</html>
)html";
  return html;
}

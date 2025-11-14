// ========================================
// Tab5 LVGL - M5Stack Dashboard
// Refaktorierte Version mit modularer Architektur
// ========================================

#include <M5Unified.h>
#include <WiFi.h>
#include <esp32-hal-cpu.h>

// Core-Module
#include "display_manager.h"
#include "power_manager.h"
#include "ui_manager.h"

// Netzwerk-Module
#include "network_manager.h"
#include "mqtt_handlers.h"
#include "ha_bridge_config.h"
#include "mqtt_topics.h"

// Konfigurations-Module
#include "config_manager.h"
#include "web_config.h"
#include "web_admin.h"

// Tab-Module
#include "tab_settings.h"

// ========================================
// Globale Variablen
// ========================================
static uint32_t last_status_update = 0;

// ========================================
// Hotspot-Modus starten (wird von Settings-Tab verwendet)
// ========================================
static void start_hotspot_mode() {
  Serial.println("\n🌐 Starte Hotspot-Modus für WiFi-Konfiguration...");

  // Stoppe MQTT und WiFi
  if (networkManager.isMqttConnected()) {
    networkManager.getMqttClient().disconnect();
  }
  WiFi.disconnect();

  // Starte Webserver für Konfiguration
  if (webConfigServer.start()) {
    Serial.println("✓ Hotspot-Modus aktiv!");
    Serial.println("  1. Verbinde dich mit WiFi: 'Tab5_Config'");
    Serial.println("  2. Passwort: 12345678");
    Serial.println("  3. Öffne Browser: http://192.168.4.1");
  } else {
    Serial.println("❌ Hotspot-Modus konnte nicht gestartet werden!");
  }
}

// ========================================
// Setup
// ========================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n🚀 M5Stack Tab5 Power Management System");
  Serial.println("═══════════════════════════════════════\n");

  setCpuFrequencyMhz(240);

  // M5Stack initialisieren
  auto cfg = M5.config();
  M5.begin(cfg);

  // Display Manager initialisieren
  if (!displayManager.init()) {
    Serial.println("❌ Display-Initialisierung fehlgeschlagen!");
    while(1) delay(1000);
  }

  // Power Manager initialisieren
  powerManager.init();

  // Initial: Ultra-Performance (60 FPS)
#if LVGL_VERSION_MAJOR >= 9
  if (lv_display_get_refr_timer) {
    lv_timer_t *rt = lv_display_get_refr_timer(displayManager.getDisplay());
    if (rt) lv_timer_set_period(rt, 1000 / FPS_HIGH);
  }
#endif

  // --- Konfiguration laden (MUSS VOR UI-Aufbau passieren!) ---
  Serial.println("\n📋 Lade Konfiguration...");
  bool has_config = configManager.load();
  haBridgeConfig.load();

  // Display-Helligkeit anwenden
  {
    const DeviceConfig& cfg = configManager.getConfig();
    M5.Display.setBrightness(cfg.display_brightness);
    Serial.printf("✓ Display-Helligkeit: %d\n", cfg.display_brightness);
  }

  // UI aufbauen (mit Scene-Callback und Hotspot-Callback)
  // Config ist jetzt geladen, Settings-Tab bekommt die richtigen Werte!
  uiManager.buildUI(mqttPublishScene, start_hotspot_mode);
  uiManager.updateStatusbar();

  TopicSettings topicSettings;
  if (has_config) {
    const DeviceConfig& cfg = configManager.getConfig();
    topicSettings.device_base = cfg.mqtt_base_topic;
    topicSettings.ha_prefix = cfg.ha_prefix;
  }
  mqttTopics.begin(topicSettings);

  if (!has_config) {
    Serial.println("⚠️ Keine WiFi/MQTT-Konfiguration gefunden!");
    Serial.println("ℹ️ Öffne Einstellungen-Tab und klicke 'WiFi Konfiguration'");
  } else {
    // Network Manager initialisieren
    networkManager.init();

    // NTP-Sync planen wenn WiFi verbunden
    if (WiFi.status() == WL_CONNECTED) {
      uiManager.scheduleNtpSync(0);
    }

    Serial.println("✓ Konfiguration geladen - starte Verbindung...");
  }

  Serial.println("\n🚀 System bereit - Power-Management aktiviert!");
}

// ========================================
// Loop
// ========================================
void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();

  // LVGL Tick
  lv_tick_inc(now - last);
  last = now;

  // Power Management Update (Idle-Detection)
  powerManager.update(displayManager.getLastActivityTime());

  // Wenn im Display-Sleep, nur minimal weiter laufen
  if (powerManager.isInSleep()) {
    delay(100);  // Langsam pollen
    // Touch trotzdem checken für Wakeup - aber nur Touch, nicht M5.update()
    lgfx::touch_point_t tp;
    if (M5.Display.getTouch(&tp)) {
      powerManager.wakeFromDisplaySleep();
    }
    return;  // Skip Rest der Loop
  }

  // M5.update() - Touch, Buttons, etc. (nur wenn NICHT im Sleep)
  M5.update();

  // LVGL Handler
  lv_timer_handler();

  // Kurzer Delay für responsive Touch
  delay(2);

  // --- WebConfig Server Handler (falls im Hotspot-Modus) ---
  if (webConfigServer.isRunning()) {
    webConfigServer.handle();

    // Prüfe ob neue Konfiguration gespeichert wurde
    if (webConfigServer.hasNewConfig()) {
      Serial.println("\n✓ Neue WiFi-Konfiguration gespeichert - Neustart in 3 Sekunden...");
      delay(3000);
      ESP.restart();
    }
    return;  // Skip WiFi/MQTT wenn im Hotspot-Modus
  }

  // --- WebAdmin Server Handler (im normalen Netzwerk) ---
  if (webAdminServer.isRunning()) {
    webAdminServer.handle();
  }

  // --- Network Manager Update ---
  if (configManager.isConfigured()) {
    networkManager.update();
    uiManager.serviceNtpSync();

    // UI Status-Update alle 2 Sekunden
    if (now - last_status_update > 2000UL) {
      last_status_update = now;

      const DeviceConfig& cfg = configManager.getConfig();
      if (networkManager.isWifiConnected()) {
        String ip = WiFi.localIP().toString();
        settings_update_wifi_status(true, cfg.wifi_ssid, ip.c_str());
      } else {
        settings_update_wifi_status(false, nullptr, nullptr);
      }

      // Power Status Update
      settings_update_power_status();

      // Power Mode Update (WiFi Power Saving basierend auf Netzteil/Batterie)
      powerManager.updatePowerMode();

      uiManager.updateStatusbar();
    }
  } else {
    // Keine Config: Status update dass nicht konfiguriert
    if (now - last_status_update > 2000UL) {
      last_status_update = now;
      settings_update_wifi_status(false, nullptr, nullptr);
      uiManager.updateStatusbar();
    }
  }
}

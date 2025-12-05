#include <M5Unified.h>
#include <WiFi.h>
#include <Wire.h> // Wichtig
#include <SPI.h>  // Wichtig für M5GFX

#include "display_manager.h"
#include "power_manager.h"
#include "ui_manager.h"
#include "network_manager.h"
#include "mqtt_handlers.h"
#include "ha_bridge_config.h"
#include "mqtt_topics.h"
#include "config_manager.h"
#include "web_config.h"
#include "web_admin.h"
#include "tab_settings.h"
#include "game_controls_config.h"
#include "game_ws_server.h"

static uint32_t last_status_update = 0;

static void start_hotspot_mode() {
  if (networkManager.isMqttConnected()) networkManager.getMqttClient().disconnect();
  WiFi.disconnect();
  webConfigServer.start();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== TAB5 STARTUP ===");
  Serial.flush();

  Serial.println("[Setup] M5.begin()...");
  Serial.flush();
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.println("[Setup] M5.begin() OK");
  Serial.flush();

  Serial.println("[Setup] Wire.setClock(400000)...");
  Serial.flush();
  Wire.setClock(400000);
  Serial.println("[Setup] Wire OK");
  Serial.flush();

  Serial.println("[Setup] displayManager.init()...");
  Serial.flush();
  if (!displayManager.init()) {
    Serial.println("[Setup] Display FEHLER!");
    while(1) delay(1000);
  }
  Serial.println("[Setup] Display OK");
  Serial.flush();

  Serial.println("[Setup] powerManager.init()...");
  Serial.flush();
  powerManager.init();
  Serial.println("[Setup] Power OK");
  Serial.flush();

  Serial.println("[Setup] Loading configs...");
  Serial.flush();
  bool has_config = configManager.load();
  haBridgeConfig.load();
  gameControlsConfig.load();
  Serial.println("[Setup] Configs OK");
  Serial.flush();

  Serial.println("[Setup] Setting brightness...");
  Serial.flush();
  {
    const DeviceConfig& dcfg = configManager.getConfig();
    M5.Display.setBrightness(dcfg.display_brightness);
  }
  Serial.println("[Setup] Brightness OK");
  Serial.flush();

  Serial.println("[Setup] Building UI...");
  Serial.flush();
  uiManager.buildUI(mqttPublishScene, start_hotspot_mode);
  Serial.println("[Setup] UI built");
  Serial.flush();

  uiManager.updateStatusbar();
  Serial.println("[Setup] Statusbar updated");
  Serial.flush();

  Serial.println("[Setup] MQTT Topics...");
  Serial.flush();
  TopicSettings ts;
  if (has_config) {
    const DeviceConfig& dcfg = configManager.getConfig();
    ts.device_base = dcfg.mqtt_base_topic;
    ts.ha_prefix = dcfg.ha_prefix;
  }
  mqttTopics.begin(ts);
  Serial.println("[Setup] MQTT Topics OK");
  Serial.flush();

  if (has_config) {
    Serial.println("[Setup] Network init...");
    Serial.flush();
    networkManager.init();
    if (WiFi.status() == WL_CONNECTED) uiManager.scheduleNtpSync(0);
    Serial.println("[Setup] Network OK");
    Serial.flush();
  }

  // WebSocket Server für Game Controls starten (nach Network Init!)
  Serial.println("[Setup] Game WebSocket Server...");
  Serial.flush();
  gameWSServer.init(8081);  // Port 8081
  Serial.println("[Setup] Game WebSocket OK");
  Serial.flush();

  Serial.println("\n=== SETUP COMPLETE ===\n");
  Serial.flush();
}

void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();
  
  lv_tick_inc(now - last);
  last = now;

  powerManager.update(displayManager.getLastActivityTime());

  // --- SLEEP ---
  if (powerManager.isInSleep()) {
    if (configManager.isConfigured()) networkManager.update();
    lgfx::touch_point_t tp;
    if (M5.Display.getTouch(&tp)) {
      powerManager.wakeFromDisplaySleep();
      return; 
    }
    delay(150); 
    return;
  }

  // --- ACTIVE ---
  M5.update();
  lv_timer_handler();
  
  // Nur 1ms Pause für maximale FPS
  delay(1);

  if (webConfigServer.isRunning()) {
    webConfigServer.handle();
    if (webConfigServer.hasNewConfig()) { delay(1000); ESP.restart(); }
    return;
  }

  if (webAdminServer.isRunning()) webAdminServer.handle();

  // WebSocket Server für Game Controls
  gameWSServer.handle();

  if (configManager.isConfigured()) {
    static uint8_t net_tick = 0;
    if (++net_tick % 5 == 0) networkManager.update();
    
    if (now - last_status_update > 2000UL) {
      uiManager.serviceNtpSync();
      last_status_update = now;
      const DeviceConfig& c = configManager.getConfig();
      if (networkManager.isWifiConnected()) {
        settings_update_wifi_status(true, c.wifi_ssid, WiFi.localIP().toString().c_str());
      } else {
        settings_update_wifi_status(false, nullptr, nullptr);
      }
      settings_update_power_status();
      uiManager.updateStatusbar();
    }
  }
}
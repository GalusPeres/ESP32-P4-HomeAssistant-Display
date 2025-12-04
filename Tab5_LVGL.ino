#include <M5Unified.h>
#include <WiFi.h>
#include <Wire.h> // Wichtig

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

static uint32_t last_status_update = 0;

static void start_hotspot_mode() {
  if (networkManager.isMqttConnected()) networkManager.getMqttClient().disconnect();
  WiFi.disconnect();
  webConfigServer.start();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  auto cfg = M5.config();
  M5.begin(cfg);

  // WICHTIG: Touch I2C auf 400kHz beschleunigen (verhindert Ruckeln)
  Wire.setClock(400000); 

  if (!displayManager.init()) { while(1) delay(1000); }
  powerManager.init();

  bool has_config = configManager.load();
  haBridgeConfig.load();

  {
    const DeviceConfig& dcfg = configManager.getConfig();
    M5.Display.setBrightness(dcfg.display_brightness);
  }

  uiManager.buildUI(mqttPublishScene, start_hotspot_mode);
  uiManager.updateStatusbar();

  TopicSettings ts;
  if (has_config) {
    const DeviceConfig& dcfg = configManager.getConfig();
    ts.device_base = dcfg.mqtt_base_topic;
    ts.ha_prefix = dcfg.ha_prefix;
  }
  mqttTopics.begin(ts);

  if (has_config) {
    networkManager.init();
    if (WiFi.status() == WL_CONNECTED) uiManager.scheduleNtpSync(0);
  }
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
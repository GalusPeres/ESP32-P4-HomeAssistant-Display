#include <M5Unified.h>
#include <WiFi.h>
#include <Wire.h> // Wichtig
#include <SPI.h>  // Wichtig fÃ¼r M5GFX

#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/ui/ui_manager.h"
#include "src/network/network_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/web/web_config.h"
#include "src/web/web_admin.h"
#include "src/ui/tab_settings.h"
#include "src/game/game_controls_config.h"
#include "src/game/game_ws_server.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"  // Für process_sensor_update_queue()

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
  tileConfig.load();
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

  // Systemzeit initial aus RTC übernehmen (falls vorhanden)
  uiManager.syncSystemTimeFromRtc();

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

    // WebSocket Server für Game Controls nur starten, wenn Konfiguration vorhanden ist
    Serial.println("[Setup] Game WebSocket Server...");
    Serial.flush();
    gameWSServer.init(8081);  // Port 8081
    Serial.println("[Setup] Game WebSocket OK");
    Serial.flush();
  } else {
    Serial.println("[Setup] Ueberspringe Network/Game WS (keine Config)");
  }

  Serial.println("\n=== SETUP COMPLETE ===\n");
  Serial.flush();
}

void loop() {
  static bool first_run = true;
  static bool was_asleep = false;
  if (first_run) {
    Serial.println("[Loop] ERSTE ITERATION!");
    Serial.flush();
  }

  if (first_run) Serial.println("[Loop] millis()...");
  static uint32_t last = millis();
  uint32_t now = millis();

  if (first_run) Serial.println("[Loop] lv_tick_inc()...");
  lv_tick_inc(now - last);
  last = now;

  if (first_run) Serial.println("[Loop] powerManager.update()...");
  powerManager.update(displayManager.getLastActivityTime());

  if (first_run) Serial.println("[Loop] Nach powerManager.update()!");

  // --- SLEEP ---
  if (powerManager.isInSleep()) {
    if (!was_asleep) {
      Serial.println("[Loop] SLEEP MODE AKTIV!");
      was_asleep = true;
    }
    if (configManager.isConfigured()) networkManager.update();
    lgfx::touch_point_t tp;
    if (M5.Display.getTouch(&tp)) {
      powerManager.wakeFromDisplaySleep();
      was_asleep = false;
      return; 
    }
    delay(150); 
    return;
  }
  // Zurück im aktiven Modus
  was_asleep = false;

  // --- ACTIVE ---
  if (first_run) Serial.println("[Loop] M5.update()...");
  M5.update();

  if (first_run) Serial.println("[Loop] process_sensor_update_queue()...");
  process_sensor_update_queue();  // WICHTIG: VOR lv_timer_handler()!

  if (first_run) {
    Serial.println("[Loop] lv_timer_handler()...");
    Serial.flush();
  }
  yield();  // Watchdog füttern
  lv_timer_handler();
  yield();  // Watchdog füttern
  if (first_run) {
    Serial.println("[Loop] lv_timer_handler() KOMPLETT!");
    Serial.flush();
  }

  // Nur 1ms Pause fÃ¼r maximale FPS
  delay(1);

  if (first_run) Serial.println("[Loop] webConfigServer check...");
  if (webConfigServer.isRunning()) {
    webConfigServer.handle();
    if (webConfigServer.hasNewConfig()) { delay(1000); ESP.restart(); }
    return;
  }

  if (first_run) Serial.println("[Loop] webAdminServer.handle()...");
  if (webAdminServer.isRunning()) webAdminServer.handle();

  if (first_run) Serial.println("[Loop] gameWSServer.handle()...");
  // WebSocket Server fÃ¼r Game Controls
  gameWSServer.handle();

  if (first_run) Serial.println("[Loop] Network check...");
  if (configManager.isConfigured()) {
    static uint8_t net_tick = 0;
    if (++net_tick % 5 == 0) {
      if (first_run) Serial.println("[Loop] networkManager.update()...");
      networkManager.update();
    }
    
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

  if (first_run) {
    Serial.println("[Loop] === ERSTE ITERATION KOMPLETT ===");
    Serial.flush();
    first_run = false;  // Erst ganz am Ende!
  }
}


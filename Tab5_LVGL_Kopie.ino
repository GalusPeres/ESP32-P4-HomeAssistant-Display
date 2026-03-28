#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include "src/core/waveshare_sdmmc.h"
#include <OneWire.h>           // Erzwingt Arduino-Library-Discovery fuer DS18x20 Support
#include <DallasTemperature.h> // Erzwingt Arduino-Library-Discovery fuer DS18x20 Support
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_err.h>

#include "src/core/board_hal.h"
#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/core/config_manager.h"
#include "src/ui/ui_manager.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/image_popup.h"
#include "src/ui/weather_popup.h"
#include "src/ui/radar_popup.h"
#include "src/network/network_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/web/web_config.h"
#include "src/web/web_admin.h"
#include "src/ui/tab_settings.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/game/game_controls_config.h"
#include "src/game/game_ws_server.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"  // Für process_sensor_update_queue()
#include "src/tiles/mdi_icons.h"      // MDI Icon Mapping

// MDI Icons Font (48px, 4bpp) - definiert in src/fonts/mdi_icons_48.c
LV_FONT_DECLARE(mdi_icons_48);

// Mehr Stack fuer loopTask (verhindert Stack-Overflow bei lv_timer_handler).
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

static uint32_t last_status_update = 0;
static uint32_t ap_mode_started_at = 0;
static const uint32_t AP_MODE_TIMEOUT_MS = 10UL * 60UL * 1000UL;
static uint32_t ap_mode_disable_block_until = 0;
static TaskHandle_t ui_build_waiter = nullptr;
static scene_publish_cb_t ui_scene_cb = nullptr;
static hotspot_start_cb_t ui_hotspot_cb = nullptr;

static void build_ui_task(void* param) {
  (void)param;
  uiManager.buildUI(ui_scene_cb, ui_hotspot_cb);
  if (ui_build_waiter) {
    xTaskNotifyGive(ui_build_waiter);
  }
  vTaskDelete(nullptr);
}

static void set_hotspot_mode(bool enable) {
  if (enable) {
    if (webConfigServer.isRunning()) {
      settings_update_ap_mode(true);
      return;
    }
    if (networkManager.isMqttConnected()) networkManager.getMqttClient().disconnect();
    if (webAdminServer.isRunning()) webAdminServer.stop();
    settings_update_ap_mode(true);
    if (webConfigServer.start()) {
      ap_mode_started_at = millis();
      ap_mode_disable_block_until = ap_mode_started_at + 1500;
    } else {
      ap_mode_started_at = 0;
      ap_mode_disable_block_until = 0;
    }
    return;
  }

  if (ap_mode_disable_block_until != 0 &&
      (int32_t)(millis() - ap_mode_disable_block_until) < 0) {
    settings_update_ap_mode(true);
    return;
  }

  if (!webConfigServer.isRunning()) {
    ap_mode_started_at = 0;
    ap_mode_disable_block_until = 0;
    settings_update_ap_mode(false);
    return;
  }

  webConfigServer.stop();
  ap_mode_started_at = 0;
  ap_mode_disable_block_until = 0;
  settings_update_ap_mode(false);
  if (configManager.isConfigured()) {
    if (WiFi.status() != WL_CONNECTED) {
      networkManager.connectWifi();
    }
  }
}

static bool init_nvs() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.printf("[Setup] NVS init failed: %s (%d)\n", esp_err_to_name(err), err);
    return false;
  }
  Serial.println("[Setup] NVS ready");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== WAVESHARE P4 STARTUP ===");
  Serial.flush();

  // Board-HAL initialisiert I2C, Display (MIPI-DSI HX8394), GT911 Touch, Backlight
  Serial.println("[Setup] BoardHAL::init()...");
  Serial.flush();
  if (!BoardHAL::init()) {
    Serial.println("[Setup] BoardHAL init FAILED!");
    while(1) delay(1000);
  }
  Serial.println("[Setup] BoardHAL OK");
  Serial.flush();

  // SD Card (SDMMC 4-bit)
  Serial.println("[Setup] SD Card init (SDMMC)...");
  Serial.flush();
  BoardHAL::initSDCard();
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

  Serial.println("[Setup] NVS init...");
  Serial.flush();
  init_nvs();
  Serial.flush();

  Serial.println("[Setup] Loading configs...");
  Serial.flush();
  bool has_config = configManager.load();
  haBridgeConfig.load();
  gameControlsConfig.load();
  tileConfig.load();
  Serial.println("[Setup] Configs OK");
  Serial.flush();

  // Waveshare 720×720: Square display, no rotation needed.
  // Skip auto-rotation detection (no IMU).
  Serial.println("[Setup] Display rotation: fixed (square display)");
  Serial.flush();

  Serial.println("[Setup] Setting brightness...");
  Serial.flush();
  {
    const DeviceConfig& dcfg = configManager.getConfig();
    BoardHAL::setBrightness(dcfg.display_brightness);
  }
  Serial.println("[Setup] Brightness OK");
  Serial.flush();

  Serial.println("[Setup] Building UI...");
  Serial.flush();
  ui_scene_cb = mqttPublishScene;
  ui_hotspot_cb = set_hotspot_mode;
  ui_build_waiter = xTaskGetCurrentTaskHandle();
  xTaskCreatePinnedToCore(build_ui_task, "buildUI", 24576, nullptr, 2, nullptr, ARDUINO_RUNNING_CORE);
  if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10000)) == 0) {
    Serial.println("[Setup] WARNUNG: UI Build Timeout!");
  }
  ui_build_waiter = nullptr;
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

    Serial.println("[Setup] Game WebSocket Server...");
    Serial.flush();
    gameWSServer.init(8081);
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
    // Touch-Wake: abfragen ob Touch aktiv
    BoardHAL::TouchPoint tp;
    if (BoardHAL::getTouch(&tp)) {
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
  // Lokale Sensoren (z. B. externer OneWire-Temperatursensor)
  mqttServiceLocalSensors();

  if (first_run) Serial.println("[Loop] process_sensor_update_queue()...");
  // Im Idle nur alle 2s Queues verarbeiten (spart CPU bei 10 FPS)
  {
    static uint32_t last_queue_ms = 0;
    bool idle = !powerManager.isHighPerformance();
    if (!idle || (millis() - last_queue_ms >= 2000)) {
      process_sensor_update_queue();  // WICHTIG: VOR lv_timer_handler()!
      process_switch_update_queue();
      process_weather_update_queue();
      process_sensor_popup_queue();
      process_weather_popup_queue();
      process_tile_graph_queue();
      last_queue_ms = millis();
    }
  }
  tiles_process_reload_requests();

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

  // Nur 1ms Pause für maximale FPS
  delay(1);

  if (first_run) Serial.println("[Loop] webConfigServer check...");
  if (webConfigServer.isRunning()) {
    if (webAdminServer.isRunning()) webAdminServer.stop();
    webConfigServer.handle();
    settings_update_ap_mode(true);
    settings_update_wifi_status_ap("WS_P4_Config", "12345678");
    settings_update_power_status();

    if (webConfigServer.hasNewConfig()) { delay(1000); ESP.restart(); }

    if (ap_mode_started_at != 0 && (uint32_t)(now - ap_mode_started_at) > AP_MODE_TIMEOUT_MS) {
      set_hotspot_mode(false);
    }

    if (webConfigServer.isRunning()) {
      return;
    }
  }

  if (first_run) Serial.println("[Loop] webAdminServer.handle()...");
  if (webAdminServer.isRunning()) webAdminServer.handle();

  if (first_run) Serial.println("[Loop] gameWSServer.handle()...");
  gameWSServer.handle();

  if (first_run) Serial.println("[Loop] Network check...");
  if (configManager.isConfigured()) {
    static uint8_t net_tick = 0;
    if (++net_tick % 5 == 0) {
      if (first_run) Serial.println("[Loop] networkManager.update()...");
      networkManager.update();
    }
  }

  image_popup_service_url_cache();
  radar_popup_service();

  if (now - last_status_update > 2000UL) {
    last_status_update = now;
    settings_update_power_status();
    if (configManager.isConfigured()) {
      uiManager.serviceNtpSync();
      const DeviceConfig& c = configManager.getConfig();
      if (networkManager.isWifiConnected()) {
        settings_update_wifi_status(true, c.wifi_ssid, WiFi.localIP().toString().c_str());
      } else {
        settings_update_wifi_status(false, nullptr, nullptr);
      }
      uiManager.updateStatusbar();
    }
  }

  if (first_run) {
    Serial.println("[Loop] === ERSTE ITERATION KOMPLETT ===");
    Serial.flush();
    first_run = false;
  }
}

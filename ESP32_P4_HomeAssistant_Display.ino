
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <esp_err.h>
#include <esp_ota_ops.h>

#include "src/core/board_hal.h"
#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/core/config_manager.h"
#include "src/core/firmware_version.h"
#include "src/ui/ui_manager.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/weather_popup.h"
#include "src/ui/energy_popup.h"
#include "src/types/energy/energy_data.h"
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
static bool hotspot_mode_change_pending = false;
static bool hotspot_mode_requested = false;
static bool ota_display_suspended = false;
static TaskHandle_t ui_build_waiter = nullptr;
static scene_publish_cb_t ui_scene_cb = nullptr;
static hotspot_start_cb_t ui_hotspot_cb = nullptr;

static void log_memory_status(const char* tag) {
  Serial.printf("[Mem] %s | Heap free=%u KB | Heap min=%u KB | PSRAM free=%u KB | PSRAM total=%u KB\n",
                tag ? tag : "?",
                ESP.getFreeHeap() / 1024,
                ESP.getMinFreeHeap() / 1024,
                ESP.getFreePsram() / 1024,
                ESP.getPsramSize() / 1024);
}

static void confirm_running_ota_if_needed() {
  const esp_partition_t* running = esp_ota_get_running_partition();
  if (!running) {
    Serial.println("[OTA] Running partition lookup failed");
    return;
  }

  esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
  const esp_err_t state_err = esp_ota_get_state_partition(running, &ota_state);
  if (state_err != ESP_OK) {
    Serial.printf("[OTA] Could not read running OTA state: %s (%d)\n", esp_err_to_name(state_err), state_err);
    return;
  }

  Serial.printf("[OTA] Running partition state: %d\n", static_cast<int>(ota_state));

  if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
    return;
  }

  const esp_err_t mark_err = esp_ota_mark_app_valid_cancel_rollback();
  if (mark_err == ESP_OK) {
    Serial.println("[OTA] Running app marked valid; rollback cancelled");
  } else {
    Serial.printf("[OTA] Failed to mark running app valid: %s (%d)\n", esp_err_to_name(mark_err), mark_err);
  }
}

static void restore_display_after_ota_pause() {
  BoardHAL::displayPowerSaveOff();
  displayManager.setInputEnabled(true);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(displayManager.getDisplay());
}

static void build_ui_task(void* param) {
  (void)param;
  uiManager.buildUI(ui_scene_cb, ui_hotspot_cb);
  if (ui_build_waiter) {
    xTaskNotifyGive(ui_build_waiter);
  }
  vTaskDelete(nullptr);
}

static void apply_hotspot_mode(bool enable) {
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

static void set_hotspot_mode(bool enable) {
  hotspot_mode_requested = enable;
  hotspot_mode_change_pending = true;
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

static constexpr uint32_t BACKGROUND_STATE_REFRESH_MS = 60UL * 1000UL;
static uint32_t g_last_bridge_state_refresh_ms = 0;
static bool g_bridge_state_refresh_pending = false;

static void mark_background_state_refresh_sent() {
  g_last_bridge_state_refresh_ms = millis();
  g_bridge_state_refresh_pending = false;
}

static void service_background_state_refresh(bool allow_now) {
  if (!configManager.isConfigured()) return;
  if (!networkManager.isMqttConnected()) return;

  const uint32_t now = millis();
  if (g_last_bridge_state_refresh_ms == 0) {
    g_last_bridge_state_refresh_ms = now;
    return;
  }
  if ((uint32_t)(now - g_last_bridge_state_refresh_ms) >= BACKGROUND_STATE_REFRESH_MS) {
    g_bridge_state_refresh_pending = true;
  }
  if (!g_bridge_state_refresh_pending || !allow_now) return;

  networkManager.publishBridgeRequest();
  mark_background_state_refresh_sent();
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== WAVESHARE P4 STARTUP ===");
  Serial.printf("[Setup] Firmware: esp32-p4-homeassistant-display-%s-%s\n", FW_VERSION, Device::profile().key);
  confirm_running_ota_if_needed();
  log_memory_status("boot-start");
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

  // LittleFS (primary storage on flash)
  Serial.println("[Setup] LittleFS init...");
  Serial.flush();
  if (!Device::initLittleFS()) {
    Serial.println("[Setup] LittleFS FAILED!");
    while(1) delay(1000);
  }
  Serial.println("[Setup] LittleFS OK");
  Serial.flush();

  // SD Card (optional, for screenshots)
  Serial.println("[Setup] SD Card init...");
  Serial.flush();
  BoardHAL::initSDCard();
  Serial.flush();

  // Migrate tile data from SD to LittleFS on first boot
  Serial.println("[Setup] Storage migration check...");
  Serial.flush();
  Device::migrateStorageFromSD();
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
  if (has_config) {
    displayManager.setRotation(configManager.getConfig().display_rotation_quarters);
  }
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

  BoardHAL::displayWake();
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayWaitDisplay();
  delay(20);
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayWaitDisplay();
  Serial.println("[Setup] Display wake OK");
  log_memory_status("after-ui-build");
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
  log_memory_status("setup-complete");
  Serial.flush();
}

void loop() {
  static bool first_run = true;
  static bool was_asleep = false;
  static bool wake_cache_refresh_pending = false;
  static bool wake_bridge_request_pending = false;
  static uint32_t wake_bridge_request_until_ms = 0;
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

  if (hotspot_mode_change_pending) {
    hotspot_mode_change_pending = false;
    apply_hotspot_mode(hotspot_mode_requested);
  }

  const bool ota_in_progress = webAdminOtaInProgress();

  if (ota_in_progress) {
    if (!ota_display_suspended) {
      displayManager.setInputEnabled(false);
      BoardHAL::displayPowerSaveOn();
      ota_display_suspended = true;
    }
    displayManager.resetActivityTimer();
    if (webAdminServer.isRunning()) webAdminServer.handle();
    delay(1);
    if (first_run) {
      Serial.println("[Loop] OTA mode active - display suspended");
      Serial.flush();
      first_run = false;
    }
    return;
  }

  if (ota_display_suspended) {
    restore_display_after_ota_pause();
    ota_display_suspended = false;
  }

  if (first_run) Serial.println("[Loop] BoardHAL::update()...");
  BoardHAL::update();

  if (webConfigServer.isRunning()) {
    if (first_run) Serial.println("[Loop] AP mode active...");
    if (webAdminServer.isRunning()) webAdminServer.stop();

    if (first_run) {
      Serial.println("[Loop] lv_timer_handler()...");
      Serial.flush();
    }
    yield();
    lv_timer_handler();
    yield();
    if (first_run) {
      Serial.println("[Loop] lv_timer_handler() KOMPLETT!");
      Serial.flush();
    }

    webConfigServer.handle();
    settings_update_ap_mode(true);
    settings_update_wifi_status_ap(webConfigApSsid(), webConfigApPassword());
    settings_update_power_status();

    if (webConfigServer.hasNewConfig()) {
      displayManager.setInputEnabled(false);
      BoardHAL::prepareForRestart();
      delay(200);
      ESP.restart();
    }

    if (ap_mode_started_at != 0 && (uint32_t)(now - ap_mode_started_at) > AP_MODE_TIMEOUT_MS) {
      set_hotspot_mode(false);
    }

    delay(1);

    if (first_run) {
      Serial.println("[Loop] === ERSTE ITERATION KOMPLETT ===");
      Serial.flush();
      first_run = false;
    }
    return;
  }

  if (first_run) Serial.println("[Loop] powerManager.update()...");
  powerManager.update(displayManager.getLastActivityTime());

  if (first_run) Serial.println("[Loop] Nach powerManager.update()!");

  // --- SLEEP ---
  if (powerManager.isInSleep()) {
    if (!was_asleep) {
      Serial.println("[Loop] SLEEP MODE AKTIV!");
      was_asleep = true;
    }
    if (configManager.isConfigured()) {
      networkManager.update();
      tiles_process_bridge_cache_refresh(true);
      service_background_state_refresh(true);
      process_energy_response_queue();
      energy_service_periodic();
    }
    // Touch-Wake: abfragen ob Touch aktiv
    BoardHAL::TouchPoint tp;
    if (BoardHAL::getTouch(&tp)) {
      powerManager.wakeFromDisplaySleep();
      was_asleep = false;
      wake_cache_refresh_pending = true;
      wake_bridge_request_pending = true;
      wake_bridge_request_until_ms = millis() + 15000;
      return;
    }
    delay(150);
    return;
  }
  // Zurück im aktiven Modus
  was_asleep = false;

  if (wake_cache_refresh_pending) {
    wake_cache_refresh_pending = false;
    tiles_refresh_visible_from_cache();
  }
  if (wake_bridge_request_pending) {
    if (networkManager.isMqttConnected()) {
      networkManager.publishBridgeRequest();
      mark_background_state_refresh_sent();
      energy_request_day_for_tiles(true);
      wake_bridge_request_pending = false;
    } else if ((int32_t)(millis() - wake_bridge_request_until_ms) >= 0) {
      wake_bridge_request_pending = false;
    }
  }
  const bool ui_idle_for_background_refresh = !powerManager.isHighPerformance();
  service_background_state_refresh(ui_idle_for_background_refresh);
  tiles_process_bridge_cache_refresh(ui_idle_for_background_refresh);
  tiles_process_visible_cache_refresh(ui_idle_for_background_refresh);

  // --- ACTIVE ---
  // Lokale Sensoren (z. B. externer OneWire-Temperatursensor)
  mqttServiceLocalSensors();

  if (first_run) Serial.println("[Loop] process_sensor_update_queue()...");
  // Popup-Queues immer sofort verarbeiten (User wartet auf Inhalt)
  process_sensor_popup_queue();
  process_weather_popup_queue();
  process_energy_response_queue();
  process_energy_popup_queue();

  // Im Idle nur alle 2s tile/sensor Queues verarbeiten (spart CPU bei 10 FPS)
  {
    static uint32_t last_queue_ms = 0;
    bool idle = !powerManager.isHighPerformance();
    if (!idle || (millis() - last_queue_ms >= 2000)) {
      process_sensor_update_queue();  // WICHTIG: VOR lv_timer_handler()!
      process_switch_update_queue();
      process_weather_update_queue();
      process_media_update_queue();
      process_tile_graph_queue();
      if (idle) energy_service_periodic();
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

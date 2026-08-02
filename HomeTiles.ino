
#include <WiFi.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>  // xTaskCreatePinnedToCoreWithCaps (PSRAM-Task-Stack)
#include <nvs_flash.h>
#include <Preferences.h>  // GitHub-OTA-Auto-Retry ueber den Neustart hinweg
#include <esp_err.h>
#include <esp_wifi.h>  // esp_wifi_scan_stop (AP-Wechsel bricht laufenden Scan ab)
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_system.h>  // esp_reset_reason (Tab5-Brownout-Drossel)

#include "src/core/board_hal.h"
#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/core/config_manager.h"
#include "src/core/crash_log.h"
#include "src/core/firmware_version.h"
#include "src/core/github_update.h"
#include "src/core/lvgl_tick_service.h"
#include "src/ui/ui_manager.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/weather_popup.h"
#include "src/ui/energy_popup.h"
#include "src/ui/camera_popup.h"
#include "src/types/energy/energy_data.h"
#include "src/network/network_manager.h"
#include "src/network/network_transport.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/web/web_config.h"
#include "src/web/web_admin.h"
#include "src/ui/tab_settings.h"
#include "src/ui/boot_splash.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/image_screensaver.h"
#include "src/ui/screensaver_config.h"
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

#if defined(DEVICE_M5STACKS_TAB5)
// Brownout-Schutz (User-Log 2026-07-06): volles Backlight + Funk-Lastspitze
// (AP-Start oder STA-Verbinden) loest den Brownout-Detektor aus - inklusive
// Bootschleife, weil die gespeicherte Helligkeit VOR dem WLAN-Start gesetzt
// wird. Backlight in diesen Phasen deckeln; der Config-Wert (Slider) bleibt
// unberuehrt, der Loop stellt ihn nach dem Verbinden wieder her.
static constexpr uint8_t kTab5SafeBrightness = 140;
static constexpr uint32_t kTab5BrightnessRestoreTimeoutMs = 30000;
static bool tab5_brightness_capped = false;
static uint32_t tab5_brightness_cap_wait_since = 0;
#endif

// Splash-Screen bleibt mindestens so lange stehen, dass Version/Geraet
// tatsaechlich lesbar sind, auch wenn der restliche Boot schneller fertig ist.
static constexpr uint32_t kBootSplashMinVisibleMs = 2500;
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_M5STACKS_TAB5) || \
    defined(DEVICE_GUITION_JC8012P4A1)
static constexpr uint32_t kBootBlackWarmupMs = 90;
static constexpr uint32_t kBootBlackGapMs = 60;
#endif

static void log_memory_status(const char* tag) {
  const uint32_t heap_free = ESP.getFreeHeap();
  const uint32_t heap_min = ESP.getMinFreeHeap();
  const uint32_t int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const uint32_t int_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  const uint32_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  const uint32_t psram_free = ESP.getFreePsram();
  const uint32_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  Serial.printf("[Mem] %s | Heap free=%u KB | Heap min=%u KB | Int free=%u KB | Int largest=%u KB | DMA free=%u KB | DMA largest=%u KB | PSRAM free=%u KB | PSRAM largest=%u KB | PSRAM total=%u KB\n",
                tag ? tag : "?",
                heap_free / 1024,
                heap_min / 1024,
                int_free / 1024,
                int_largest / 1024,
                dma_free / 1024,
                dma_largest / 1024,
                psram_free / 1024,
                psram_largest / 1024,
                ESP.getPsramSize() / 1024);
  if (lv_is_initialized()) {
    lv_mem_monitor_t lv_mem{};
    lv_mem_monitor(&lv_mem);
    Serial.printf(
        "[LVGL/Mem] %s | Total=%u KB | Used=%u KB (%u%%) | "
        "Max used=%u KB | Free=%u KB | Largest=%u KB | Frag=%u%%\n",
        tag ? tag : "?",
        static_cast<unsigned>(lv_mem.total_size / 1024),
        static_cast<unsigned>((lv_mem.total_size - lv_mem.free_size) / 1024),
        static_cast<unsigned>(lv_mem.used_pct),
        static_cast<unsigned>(lv_mem.max_used / 1024),
        static_cast<unsigned>(lv_mem.free_size / 1024),
        static_cast<unsigned>(lv_mem.free_biggest_size / 1024),
        static_cast<unsigned>(lv_mem.frag_pct));
  }
  Serial.flush();
}

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_M5STACKS_TAB5) || \
    defined(DEVICE_GUITION_JC8012P4A1)
static void boot_black_warmup(const char* label) {
  Serial.printf("[Boot] Black display warmup: %s\n", label ? label : "?");
  Serial.flush();

  BoardHAL::displayFillScreen(0x0000);
  BoardHAL::displayWaitDisplay();
  BoardHAL::displayWakeDark();
  delay(kBootBlackWarmupMs);
  BoardHAL::displaySleep();
  delay(kBootBlackGapMs);
  BoardHAL::displayFillScreen(0x0000);
  BoardHAL::displayWaitDisplay();
}
#endif

#if defined(DEVICE_M5STACKS_TAB5)
static void tab5_timed_refresh_now(const char* label) {
  const uint32_t start_ms = millis();
  lv_refr_now(displayManager.getDisplay());
  Serial.printf("[Tab5/Setup] %s lv_refr_now=%lu ms\n",
                label ? label : "?",
                static_cast<unsigned long>(millis() - start_ms));
  Serial.flush();
}

static void tab5_timed_display_wait(const char* label) {
  const uint32_t start_ms = millis();
  BoardHAL::displayWaitDisplay();
  Serial.printf("[Tab5/Setup] %s displayWait=%lu ms\n",
                label ? label : "?",
                static_cast<unsigned long>(millis() - start_ms));
  Serial.flush();
}
#endif


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
    // Den Worker vor dem Disconnect sperren. Sonst verbindet er sich waehrend
    // der AP-Initialisierung erneut, solange die STA-Verbindung noch kurz als
    // aktiv gemeldet wird.
    networkManager.deferMqttReconnect(AP_MODE_TIMEOUT_MS + 10000UL);
    if (networkManager.isMqttConnected())
      networkManager.disconnectMqtt();
    if (webAdminServer.isRunning())
      webAdminServer.stop();
    networkManager.stopMdns();
    settings_update_ap_mode(true);
#if defined(DEVICE_M5STACKS_TAB5)
    // Vor dem Funk-Moduswechsel deckeln: AP-Start bei vollem Backlight
    // reisst die Versorgung in den Brownout.
    if (BoardHAL::getBrightness() > kTab5SafeBrightness) {
      BoardHAL::setBrightness(kTab5SafeBrightness);
      tab5_brightness_capped = true;
    }
    tab5_brightness_cap_wait_since = millis();
#endif
    // Laufender Async-Scan (WLAN-Popup) wuerde den Moduswechsel stoeren
    esp_wifi_scan_stop();
    if (webConfigServer.start()) {
      ap_mode_started_at = millis();
      ap_mode_disable_block_until = ap_mode_started_at + 1500;
    } else {
      ap_mode_started_at = 0;
      ap_mode_disable_block_until = 0;
      networkManager.deferMqttReconnect(1000);
      if (configManager.isConfigured())
        networkManager.connectWifi();
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
#if defined(DEVICE_M5STACKS_TAB5)
  // Deckel NICHT sofort aufheben - der Reconnect-Burst gleich unten wuerde
  // sonst wieder bei Volllast zuschlagen. Der Loop stellt die Helligkeit
  // wieder her, sobald das WLAN steht (oder nach Timeout).
  if (tab5_brightness_capped) tab5_brightness_cap_wait_since = millis();
#endif
  if (configManager.isConfigured()) {
    networkManager.deferMqttReconnect(6000);
    if (WiFi.status() != WL_CONNECTED) {
      // WiFi.begin() laeuft ins Leere, solange noch ein Scan aktiv ist -
      // genau deshalb verband sich das Geraet nach "AP beenden" nicht mehr.
      esp_wifi_scan_stop();
      networkManager.connectWifi();
    }
  }
}

static void set_hotspot_mode(bool enable) {
  hotspot_mode_requested = enable;
  hotspot_mode_change_pending = true;
}

// WLAN-Reconnect mit neuen Zugangsdaten (WLAN-Popup "Verbinden"): wie beim
// Hotspot-Wechsel nur ein Flag setzen - die eigentliche Netzwerkarbeit laeuft
// im Hauptloop statt im LVGL-Event-Callback.
static bool wifi_reconnect_pending = false;

static void request_wifi_reconnect() {
  wifi_reconnect_pending = true;
}

static void apply_wifi_reconnect() {
  if (networkTransport.isWifiDriverActive()) esp_wifi_scan_stop();
  if (networkManager.isMqttConnected()) networkManager.disconnectMqtt();
  // Alte Verbindung trennen; connectWifi() liest die frisch gespeicherten
  // Zugangsdaten aus der Config. Danach uebernimmt networkManager.update()
  // (WebAdmin/NTP/MQTT wie bei jedem normalen Verbindungsaufbau).
  if (networkTransport.isWifiConnected()) WiFi.disconnect();
  networkManager.deferMqttReconnect(6000);
  networkManager.connectWifi();
}

// WLAN-Popup "Trennen" und System-Popup "Pairing": gleiches Pending-Flag-
// Muster - der LVGL-Event-Callback setzt nur das Flag, die Netzwerkarbeit
// laeuft im Hauptloop.
static bool wifi_disconnect_pending = false;
static bool ha_pair_pending = false;

static void request_wifi_disconnect() {
  wifi_disconnect_pending = true;
}

static void request_ha_pair() {
  ha_pair_pending = true;
}

// Update-ueber-GitHub (System-Popup): Check und Install laufen blockierend
// auf dem Loop-Task - die Klick-Handler im Popup setzen nur diese Flags
// (gleiches Muster wie Hotspot-Toggle und WLAN-Reconnect).
static bool fw_check_pending = false;
static bool fw_install_pending = false;
static bool system_reboot_pending = false;
static char fw_install_tag[24] = {};
// Mehrfaches schnelles Tippen bzw. gleichzeitige UI-/Web-Anfragen darf nicht
// mehrere GitHub-TLS-Handshakes und damit Transport-Last direkt hintereinander
// erzeugen. Das letzte Ergebnis ist fuer dieses kurze Fenster ausreichend.
static constexpr uint32_t kFwCheckCacheMs = 15000;
static bool fw_check_running = false;
static bool fw_last_check_valid = false;
static uint32_t fw_last_check_at = 0;
static GithubUpdate::CheckResult fw_last_check_result;

static void request_fw_check() {
  fw_check_pending = true;
}

static void request_fw_install(const char* tag) {
  snprintf(fw_install_tag, sizeof(fw_install_tag), "%s", tag ? tag : "");
  fw_install_pending = true;
}

static void request_system_reboot() {
  system_reboot_pending = true;
}

// GitHub-OTA-Auto-Retry: Frisch gebootet laeuft der 6-MB-Download nachweislich
// durch (Feldtest 2026-07-16), waehrend der ESP-Hosted-Link im gealterten
// System an zufaelliger Stelle abreisst. Nach dem sicheren Neustart wegen
// eines fehlgeschlagenen Installs stoesst das Geraet den Install deshalb von
// selbst erneut an. Tag + Versuchszaehler ueberleben den Neustart im NVS.
static constexpr uint8_t kFwInstallMaxAutoRetries = 3;
static constexpr const char* kFwRetryNvsNamespace = "otaretry";
static bool fw_install_auto_retry_checked = false;

static void arm_fw_install_auto_retry(const char* tag) {
  Preferences prefs;
  if (!prefs.begin(kFwRetryNvsNamespace, false)) return;
  const uint8_t attempts = prefs.getUChar("att", 0);
  if (attempts >= kFwInstallMaxAutoRetries) {
    prefs.clear();
    prefs.end();
    Serial.printf("[Update] Auto-Retry aufgegeben (%u Versuche verbraucht)\n",
                  static_cast<unsigned>(attempts));
    return;
  }
  prefs.putString("tag", tag);
  prefs.putUChar("att", attempts + 1);
  prefs.end();
  Serial.printf("[Update] Auto-Retry %u/%u nach Neustart geplant\n",
                static_cast<unsigned>(attempts + 1),
                static_cast<unsigned>(kFwInstallMaxAutoRetries));
}

static void clear_fw_install_auto_retry() {
  Preferences prefs;
  if (prefs.begin(kFwRetryNvsNamespace, false)) {
    prefs.clear();
    prefs.end();
  }
}

static GithubUpdate::CheckResult perform_fw_check() {
  const uint32_t now_ms = millis();
  if (fw_last_check_valid &&
      (uint32_t)(now_ms - fw_last_check_at) < kFwCheckCacheMs) {
    Serial.println("[Update] Check: letztes Ergebnis wiederverwendet");
    return fw_last_check_result;
  }

  // Ein zweiter synchroner Aufrufer ist praktisch nicht moeglich (Loop-Task),
  // der Guard haelt den Vertrag fuer UI und Web-Admin trotzdem eindeutig.
  if (fw_check_running) {
    Serial.println("[Update] Check laeuft bereits");
    return fw_last_check_valid ? fw_last_check_result
                               : GithubUpdate::CheckResult{};
  }
  fw_check_running = true;

  // Der reine Versions-Check trennt MQTT NICHT mehr. Seine TLS-Allokationen
  // landen bevorzugt im PSRAM; ein MQTT-Disconnect hat daher kein schnelles
  // SRAM freigegeben, aber jedes Mal einen retained Subscribe-/State-Sturm
  // ausgeloest und den SDIO-DMA-Heap weiter fragmentiert.
  Serial.println("[Update] Check: MQTT bleibt verbunden (TLS in PSRAM)");
  GithubUpdate::CheckResult res = GithubUpdate::checkLatest();
  if (!res.ok && res.tls_alloc_failed) {
    Serial.println(
        "[Update] Check: TLS-Speicher trotz PSRAM-Fallback fehlgeschlagen; "
        "schneller SRAM-Draw-Puffer bleibt aktiv");
  }
  fw_last_check_result = res;
  fw_last_check_at = millis();
  fw_last_check_valid = true;
  fw_check_running = false;
  return res;
}

static void apply_fw_check() {
  // Die "Suche..."-Statuszeile noch auf den Schirm bringen, bevor der
  // TLS-Handshake den Loop fuer 1-3 Sekunden blockiert
  lv_refr_now(displayManager.getDisplay());

  const GithubUpdate::CheckResult res = perform_fw_check();
  settings_fw_check_result(res.ok, res.latest_tag, res.update_available);
}

static void fw_install_progress(size_t written, size_t total) {
  displayManager.resetActivityTimer();  // kein Display-Sleep mitten im Update

  // GitHub-OTA laeuft im selben "Display aus, Hintergrund still"-Modus wie
  // Web-OTA. Keine LVGL-Timer pumpen: Animationen/Flushes erzeugen sonst
  // zusaetzliche DMA- und CPU-Last genau waehrend des SDIO-RX-Stroms.
  static uint32_t last_ui_ms = 0;
  static size_t last_written = 0;
  const uint32_t now_ms = millis();
  if (written < last_written || written == 0 || written == total ||
      now_ms - last_ui_ms >= 500) {
    last_written = written;
    last_ui_ms = now_ms;
    settings_fw_install_progress(written, total);
  }
  delay(1);
}

static void apply_fw_install() {
  Serial.printf("[Update] Install angefordert: %s\n", fw_install_tag);
  // Internes RAM freimachen, exakt wie prepareDisplayForOtaInstall() beim
  // Web-OTA: Display aus, Draw-Puffer nach PSRAM, MQTT still. Das ist weniger
  // huebsch als ein live gerenderter Balken, aber der Web-OTA-Pfad ist damit
  // stabil und der ESP32-P4-SDIO-WLAN-Treiber braucht diesen Freiraum.
  displayManager.setInputEnabled(false);
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayPowerSaveOn();
  displayManager.setBufferLines(8);  // unter SRAM-Minimum -> PSRAM-Puffer
  networkManager.prepareMqttForOta();
  if (webAdminServer.isRunning()) webAdminServer.stop();
  log_memory_status("before-github-ota");
  delay(20);

  String err;
  const bool ok = GithubUpdate::install(fw_install_tag, fw_install_progress, err);
  if (ok) {
    clear_fw_install_auto_retry();
    settings_fw_install_done();
    Serial.println("[Update] Erfolgreich - Neustart");
    BoardHAL::prepareForRestart();
    delay(800);  // Erfolgsmeldung kurz stehen lassen
    BoardHAL::restart();
    return;
  }

  Serial.printf("[Update] Fehlgeschlagen: %s\n", err.c_str());
  webAdminServer.setGithubUpdateInstallFailed(err.c_str());
  settings_fw_install_failed(err.c_str());
  // Der sichere Neustart unten hinterlaesst keinen Core-Dump. Damit der
  // Fehlschlag trotzdem nachvollziehbar bleibt, wandern Fehlertext und die
  // Range-/Speicher-Forensik aus install() in den Absturzbericht
  // (/crashlog.txt, Web-Admin-Abschnitt "Absturzbericht").
  CrashLog::appendOtaFailureReport(fw_install_tag, err,
                                   GithubUpdate::lastInstallDiag());
  // Frisch gebootet klappt der Download zuverlaessig - also nach dem
  // Neustart automatisch weiterprobieren (nur bei Transportfehlern; ein
  // "device mismatch" o.ae. wuerde sonst eine Reboot-Schleife erzeugen).
  if (GithubUpdate::lastInstallRetryable()) {
    arm_fw_install_auto_retry(fw_install_tag);
  } else {
    clear_fw_install_auto_retry();
  }
  // Nach einem abgebrochenen grossen HTTPS-Transfer kann der ESP-Hosted-
  // Coprozessor bereits festhaengen. Ein Wiederanlauf von WLAN/MQTT fuehrt
  // dann nur zu wiederholten 5-Sekunden-RPC-Timeouts und blockiert die UI.
  // Da das Image vollstaendig vor Update.begin() geladen wird (bzw. ein
  // gestartetes Update bei Fehler abgebrochen wurde), ist ein Neustart sicher
  // und bootet die unveraenderte aktive Firmware.
  Serial.println("[Update] Sicherer Neustart nach fehlgeschlagenem Install");
  BoardHAL::prepareForRestart();
  delay(500);
  BoardHAL::restart();
}

static void apply_system_reboot() {
  Serial.println("[System] Neustart angefordert");
  displayManager.setInputEnabled(false);
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::prepareForRestart();
  delay(800);
  BoardHAL::restart();
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

// ---------------------------------------------------------------------------
// Single-Owner MQTT Worker (Etappe 2)
//
// Dieser Task ist der EINZIGE, der das PubSubClient-Objekt anfasst
// (connect/loop/publish/subscribe/setBufferSize) -- alle anderen Tasks reden
// nur ueber die Outbound-Queue bzw. volatile Flags mit ihm (siehe
// network_manager.h). Idle-Prioritaet, damit er den IDLE0-Task (Watchdog-
// Futter) nie verdraengen kann; Stack liegt im PSRAM, um das knappe interne
// RAM nicht zu belasten.
// ---------------------------------------------------------------------------
static TaskHandle_t g_mqtt_worker_handle = nullptr;

static void mqtt_worker_task(void* param) {
  (void)param;
  for (;;) {
    networkManager.serviceMqttWorker();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void setup() {
  g_lvgl_tick_last_ms = millis();
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== HOMETILES STARTUP ===");
  Serial.printf("[Setup] Firmware: hometiles-%s-%s\n", FW_VERSION, Device::profile().key);
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
  log_memory_status("after-boardhal");
  Serial.flush();

  // LittleFS (primary storage on flash)
  Serial.println("[Setup] LittleFS init...");
  Serial.flush();
  if (!Device::initLittleFS()) {
    Serial.println("[Setup] LittleFS FAILED!");
    while(1) delay(1000);
  }
  Serial.println("[Setup] LittleFS OK");
  log_memory_status("after-littlefs");
  Serial.flush();

  // Nach einem Absturz: Reset-Grund + Core-Dump-Zusammenfassung an
  // /crashlog.txt anhaengen (Web-Admin: Abschnitt "Absturzbericht").
  // Bewusst frueh, damit der Eintrag auch dann geschrieben ist, wenn ein
  // spaeterer Init-Schritt gleich wieder abstuerzen sollte.
  CrashLog::logBootDiagnostics();

  // SD Card (optional, for screenshots)
  Serial.println("[Setup] SD Card init...");
  Serial.flush();
  log_memory_status("before-sd");
  BoardHAL::initSDCard();
  log_memory_status("after-sd");
  Serial.flush();

  // Migrate tile data from SD to LittleFS on first boot
  Serial.println("[Setup] Storage migration check...");
  Serial.flush();
  Device::migrateStorageFromSD();
  log_memory_status("after-storage-migration");
  Serial.flush();

  Serial.println("[Setup] displayManager.init()...");
  Serial.flush();
  if (!displayManager.init()) {
    Serial.println("[Setup] Display FEHLER!");
    while(1) delay(1000);
  }
  Serial.println("[Setup] Display OK");
  log_memory_status("after-display");
  Serial.flush();
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_M5STACKS_TAB5) || \
    defined(DEVICE_GUITION_JC8012P4A1)
  boot_black_warmup("after-display");
#endif

  // NVS + Konfiguration (insbesondere die Display-Rotation) muessen VOR dem
  // ersten sichtbaren Splash-Frame geladen sein. displayManager.init() setzt
  // die Rotation nur auf Device::kRotationDefault; die tatsaechlich vom
  // Nutzer gespeicherte Rotation kommt erst aus configManager.load(). Frueher
  // liefen NVS-Init/Config-Load erst NACH dem Splash-Wake -- wessen Rotation
  // vom Default abweicht (z.B. 180°-geflippt), sah den Splash kurz falsch
  // gedreht ("verdreht"), weil dessen Frames schon mit der falschen Rotation
  // geflusht wurden und nie neu gezeichnet werden, bevor er wieder verschwindet.
  Serial.println("[Setup] NVS init...");
  Serial.flush();
  init_nvs();
  log_memory_status("after-nvs");
  Serial.flush();

  Serial.println("[Setup] Loading configs...");
  Serial.flush();
  bool has_config = configManager.load();
  haBridgeConfig.load();
  tileConfig.load();
  screensaverConfig.load();
  if (has_config) {
    displayManager.setRotation(configManager.getConfig().display_rotation_quarters);
  }
  Serial.println("[Setup] Configs OK");
  log_memory_status("after-configs");
  Serial.flush();

  // Ab hier gibt es einen aktiven LVGL-Screen -- kurz die Begruessung zeigen,
  // waehrend der Rest bootet. Die grossen DSI/DPI-Panels zeigen den aktiven
  // Framebuffer direkt; waere er waehrend der ersten Splash-Refreshes sichtbar,
  // koennen die LVGL-Flush-Streifen fuer wenige Millisekunden als Treppen-Flash
  // aufblitzen. Deshalb Panel dunkel, Splash fertig rendern, dann erst sichtbar.
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_M5STACKS_TAB5) || \
    defined(DEVICE_GUITION_JC8012P4A1)
  BoardHAL::displaySleep();
  delay(kBootBlackGapMs);
  BoardHAL::displayFillScreen(0x0000);
  BoardHAL::displayWaitDisplay();
#endif
  BootSplash::show();
  // Layout (Flex-Positionen, Bild-Skalierung/Pivot) VOR dem ersten Refresh
  // fertigrechnen -- sonst kann der allererste Frame einen halbfertigen
  // Zwischenzustand zeigen (verzerrt wirkendes Icon/Text), bevor sich beim
  // naechsten Refresh die endgueltige Position einstellt.
  lv_obj_update_layout(lv_screen_active());
#if !defined(DEVICE_WAVESHARE_TOUCH_LCD_X) && \
    !defined(DEVICE_M5STACKS_TAB5) && \
    !defined(DEVICE_GUITION_JC8012P4A1)
  BoardHAL::displayWake();
#endif
  lv_obj_invalidate(lv_screen_active());
#if defined(DEVICE_M5STACKS_TAB5)
  tab5_timed_refresh_now("splash-1");
  tab5_timed_display_wait("splash-1");
#else
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayWaitDisplay();
#endif
  delay(20);
  lv_obj_invalidate(lv_screen_active());
#if defined(DEVICE_M5STACKS_TAB5)
  tab5_timed_refresh_now("splash-2");
  tab5_timed_display_wait("splash-2");
#else
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayWaitDisplay();
#endif
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_X) || \
    defined(DEVICE_M5STACKS_TAB5) || \
    defined(DEVICE_GUITION_JC8012P4A1)
  BoardHAL::displayWake();
  BoardHAL::displayWaitDisplay();
#endif
  const uint32_t boot_splash_shown_at = millis();

  Serial.println("[Setup] powerManager.init()...");
  Serial.flush();
  powerManager.init();
  Serial.println("[Setup] Power OK");
  log_memory_status("after-power");
  Serial.flush();

  // Sleep bis zu 60s nach dem Boot sperren, bis der erste frische Bridge-Sync
  // (processMqttMessage() -> powerManager.allowSleep() bei erfolgreichem
  // applyJson()) angekommen ist. Ohne das kann ein kurzes Auto-Sleep-Timeout
  // das Geraet einschlafen lassen, bevor WLAN/MQTT/HA ueberhaupt die
  // aktuellen Sensordaten geliefert haben -- reines Zeitverschieben (wie
  // resetActivityTimer() unten) reicht dafuer nicht, weil die Sync-Dauer
  // schwankt. Kommt gar kein Sync (Broker/HA nicht erreichbar), greift nach
  // 60s automatisch wieder das normale Idle-Timeout als Fallback.
  powerManager.blockSleep(60000);

  // Waveshare 720×720: Square display, no rotation needed.
  // Skip auto-rotation detection (no IMU).
  Serial.println("[Setup] Display rotation: fixed (square display)");
  Serial.flush();

  Serial.println("[Setup] Setting brightness...");
  Serial.flush();
  {
    const DeviceConfig& dcfg = configManager.getConfig();
    uint8_t boot_brightness = dcfg.display_brightness;
#if defined(DEVICE_M5STACKS_TAB5)
    // Brownout-Bootschleife durchbrechen: nach einem BOD-Reset wuerde das
    // volle Backlight schon beim ersten WLAN-Verbinden den naechsten
    // Brownout ausloesen (Helligkeit wird VOR dem Funk-Start gesetzt).
    if (esp_reset_reason() == ESP_RST_BROWNOUT &&
        boot_brightness > kTab5SafeBrightness) {
      boot_brightness = kTab5SafeBrightness;
      tab5_brightness_capped = true;
      tab5_brightness_cap_wait_since = millis();
      Serial.println("[Setup] Brownout-Reset erkannt: Helligkeit gedrosselt bis WLAN steht");
    }
#endif
    BoardHAL::setBrightness(boot_brightness);
  }
  Serial.println("[Setup] Brightness OK");
  Serial.flush();

  // Splash lang genug stehen lassen, dann komplett weg -- BEVOR die
  // eigentliche UI gebaut wird, damit sich Splash und Kacheln nie denselben
  // Screen teilen. Frueher lief das nach dem UI-Build mit einem Overlay-
  // Trick (bringToFront), aber build_ui_task macht offenbar selbst
  // zwischendurch sichtbare Refreshes, wodurch die Kacheln kurz durchblitzten,
  // bevor der Splash sich wieder nach vorne draengte -- daher jetzt komplett
  // sequenziell statt ueberlappend.
  {
    const uint32_t elapsed = millis() - boot_splash_shown_at;
    if (elapsed < kBootSplashMinVisibleMs) {
      delay(kBootSplashMinVisibleMs - elapsed);
    }
  }
  BootSplash::hide();
  // Waehrend des kompletten UI-Aufbaus (inkl. Statusbar-Befuellung weiter
  // unten) die Display-Invalidierung abschalten -- switchToTab(0) in
  // buildUI() macht sonst selbst schon einen sichtbaren Zwischen-Refresh
  // (Kacheln mit noch leerer Uhrzeit), bevor updateStatusbar() unten die
  // echten Werte setzt. Zusammen mit dem Schwarzbild-Refresh, der hier vorher
  // stand, ergab das ein "treppenartiges" Umschalten (schwarz -> Kacheln ohne
  // Uhrzeit -> Kacheln mit Uhrzeit) statt eines einzigen sauberen Schnitts
  // von Splash auf die fertige Oberflaeche. Gleiche Technik wie in
  // tiles_reload_layout() (tab_tiles_unified.cpp) -- dort schon bewaehrt, um
  // Kachel-Aufbau unsichtbar zu halten, bis alles fertig ist.
  if (lv_display_t* disp = displayManager.getDisplay()) {
    lv_display_enable_invalidation(disp, false);
  }

  Serial.println("[Setup] Building UI...");
  Serial.flush();
  ui_scene_cb = mqttPublishScene;
  image_screensaver_set_scene_callback(mqttPublishScene);
  ui_hotspot_cb = set_hotspot_mode;
  settings_set_wifi_reconnect_callback(request_wifi_reconnect);
  settings_set_wifi_disconnect_callback(request_wifi_disconnect);
  settings_set_ha_pair_callback(request_ha_pair);
  settings_set_fw_check_callback(request_fw_check);
  settings_set_fw_install_callback(request_fw_install);
  settings_set_system_reboot_callback(request_system_reboot);
  webAdminServer.setGithubUpdateCallbacks(perform_fw_check, request_fw_install);
  ui_build_waiter = xTaskGetCurrentTaskHandle();
  xTaskCreatePinnedToCore(build_ui_task, "buildUI", 24576, nullptr, 2, nullptr, ARDUINO_RUNNING_CORE);
  if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(30000)) == 0) {
    Serial.println("[Setup] WARNUNG: UI Build Timeout!");
  }
  ui_build_waiter = nullptr;
  Serial.println("[Setup] UI built");
  Serial.flush();
  preload_image_screensaver();

  uiManager.updateStatusbar();
  Serial.println("[Setup] Statusbar updated");
  Serial.flush();

  // Ab hier ist die Oberflaeche komplett fertig (Kacheln + Statusbar) --
  // Invalidierung wieder einschalten, der folgende Wake-Refresh zeigt dann in
  // einem Schritt die fertige Oberflaeche statt Zwischenstufen.
  if (lv_display_t* disp = displayManager.getDisplay()) {
    lv_display_enable_invalidation(disp, true);
  }

  BoardHAL::displayWake();
  lv_obj_invalidate(lv_screen_active());
#if defined(DEVICE_M5STACKS_TAB5)
  tab5_timed_refresh_now("wake-1");
  tab5_timed_display_wait("wake-1");
#else
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayWaitDisplay();
#endif
  delay(20);
  lv_obj_invalidate(lv_screen_active());
#if defined(DEVICE_M5STACKS_TAB5)
  tab5_timed_refresh_now("wake-2");
  tab5_timed_display_wait("wake-2");
#else
  lv_refr_now(displayManager.getDisplay());
  BoardHAL::displayWaitDisplay();
#endif
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
    // Vor init(): Media-Tiles in der Config verlangen den 24-KB-Empfangs-
    // puffer ab der ersten Verbindung, sonst verwirft PubSubClient das
    // retained Cover-Payload direkt nach dem Subscribe.
    networkManager.setMqttMediaBufferNeeded(mqttAnyMediaTileConfigured());
    networkManager.init();
    if (networkTransport.isConnected()) uiManager.scheduleNtpSync(0);
    Serial.println("[Setup] Network OK");
    log_memory_status("after-network-init");
    Serial.flush();

    Serial.println("[Setup] MQTT-Worker...");
    Serial.flush();
    networkManager.beginMqttWorker();
    const BaseType_t mqtt_worker_core = (ARDUINO_RUNNING_CORE == 0) ? 1 : 0;
    if (xTaskCreatePinnedToCoreWithCaps(mqtt_worker_task, "mqttWorker", 12288,
                                        nullptr, tskIDLE_PRIORITY,
                                        &g_mqtt_worker_handle, mqtt_worker_core,
                                        MALLOC_CAP_SPIRAM) == pdPASS) {
      Serial.printf("[Setup] MQTT-Worker auf Core %d gestartet\n", (int)mqtt_worker_core);
    } else {
      // Bewusst KEIN stiller Fallback auf den Loop-Task: Single-Owner ohne
      // Worker ergibt keinen Sinn, und ein geteilter Client waere genau die
      // Race-Quelle, die dieser Umbau beseitigt. Laut scheitern.
      g_mqtt_worker_handle = nullptr;
      Serial.println("[Setup] FEHLER: MQTT-Worker konnte nicht gestartet werden -- MQTT bleibt offline!");
    }
    log_memory_status("after-mqtt-worker");
    Serial.flush();
  } else {
    Serial.println("[Setup] Ueberspringe Netzwerk (keine Config)");
  }

  // last_activity_time wurde schon ganz am Anfang in displayManager.init()
  // gesetzt -- alles seitdem (Config-Load, Splash, UI-Build, bis zu 30s
  // Timeout) zaehlte bisher schon gegen den Idle-/Sleep-Timer, obwohl der
  // Nutzer das Geraet noch gar nicht sehen/bedienen konnte. Bei kurzem
  // Auto-Sleep-Timeout schlief das Geraet dadurch faktisch sofort nach dem
  // Boot ein. Timer hier neu starten, sobald die UI wirklich bedienbar ist.
  displayManager.resetActivityTimer();

  Serial.println("\n=== SETUP COMPLETE ===\n");
  log_memory_status("setup-complete");
  Serial.flush();
}

void loop() {
  static bool first_run = true;
  static bool was_asleep = false;
  static bool logged_wifi_connected = false;
  static bool logged_mqtt_connected = false;
  static uint32_t last_mem_log_ms = 0;
  if (first_run) {
    Serial.println("[Loop] ERSTE ITERATION!");
    Serial.flush();
  }

  if (first_run) Serial.println("[Loop] millis()...");
  uint32_t now = millis();

  if (first_run) Serial.println("[Loop] lv_tick_inc()...");
  lv_tick_inc(now - g_lvgl_tick_last_ms);
  g_lvgl_tick_last_ms = now;

  if (hotspot_mode_change_pending) {
    hotspot_mode_change_pending = false;
    apply_hotspot_mode(hotspot_mode_requested);
  }

  if (wifi_reconnect_pending) {
    wifi_reconnect_pending = false;
    apply_wifi_reconnect();
  }

  if (wifi_disconnect_pending) {
    wifi_disconnect_pending = false;
    if (networkTransport.isWifiDriverActive()) esp_wifi_scan_stop();
    networkManager.disconnectWifiManual();
  }
  if (ha_pair_pending) {
    ha_pair_pending = false;
    // Erzwungener MQTT-Reconnect: die Post-Connect-Publishes (Status/
    // Settings/Snapshot) lassen die HA-Bridge das Geraet neu erkennen.
    networkManager.requestMqttReconfigure();
  }

  // Geplanten Auto-Retry aus dem NVS einmalig einloesen, sobald WLAN steht
  // und das frisch gebootete System kurz zur Ruhe gekommen ist.
  if (!fw_install_auto_retry_checked && millis() > 30000 &&
      networkManager.isNetworkConnected()) {
    fw_install_auto_retry_checked = true;
    Preferences prefs;
    if (prefs.begin(kFwRetryNvsNamespace, true)) {
      const String retry_tag = prefs.getString("tag", "");
      prefs.end();
      if (retry_tag.length()) {
        Serial.printf("[Update] Setze Update nach Neustart automatisch fort: %s\n",
                      retry_tag.c_str());
        request_fw_install(retry_tag.c_str());
      }
    }
  }

  if (fw_check_pending) {
    fw_check_pending = false;
    apply_fw_check();
  }
  if (fw_install_pending) {
    fw_install_pending = false;
    apply_fw_install();
  }
  if (system_reboot_pending) {
    system_reboot_pending = false;
    apply_system_reboot();
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
    // Ordner-Taps setzen nur ein Pending-Flag (tiles_switch_to_folder);
    // konsumiert wird es ausschliesslich hier bzw. im normalen Loop-Pfad.
    // Ohne diesen Aufruf war die Ordner-Navigation im AP-Betrieb tot und der
    // aufgestaute Wechsel feuerte nach AP-Ende verspaetet nach.
    tiles_process_reload_requests();
    settings_update_ap_mode(true);
    settings_update_wifi_status_ap(webConfigApSsid(), webConfigApPassword());
    settings_update_power_status();

    if (webConfigServer.hasNewConfig()) {
      webConfigServer.resetConfigFlag();
      // WLAN-Zugangsdaten sind schon gespeichert (siehe WebConfigServer::handleSave) -
      // wie beim WLAN-Popup im Settings-Tab reicht ein Live-Reconnect, der ohnehin
      // schon beim normalen "AP beenden" laeuft (apply_hotspot_mode(false) stoppt
      // den AP und verbindet neu, siehe dort). Kein Neustart mehr noetig.
      set_hotspot_mode(false);
    }

    if (ap_mode_started_at != 0 && (uint32_t)(now - ap_mode_started_at) > AP_MODE_TIMEOUT_MS) {
      set_hotspot_mode(false);
    }

#if defined(DEVICE_M5STACKS_TAB5)
    // Deckel im AP-Betrieb aktiv durchsetzen: der Helligkeits-Slider im
    // Display-Popup bleibt hier erreichbar und wuerde den Brownout-Schutz
    // sonst aushebeln. Der Config-Wert bleibt gespeichert und wird nach
    // AP-Ende wiederhergestellt.
    if (BoardHAL::getBrightness() > kTab5SafeBrightness) {
      BoardHAL::setBrightness(kTab5SafeBrightness);
      tab5_brightness_capped = true;
      tab5_brightness_cap_wait_since = millis();
    }
#endif

    delay(1);

    if (first_run) {
      Serial.println("[Loop] === ERSTE ITERATION KOMPLETT ===");
      Serial.flush();
      first_run = false;
    }
    return;
  }

#if defined(DEVICE_M5STACKS_TAB5)
  // Brownout-Deckel aufheben, sobald der Funk die kritische Phase hinter
  // sich hat (verbunden) oder nichts mehr kommt (Timeout, z.B. Router weg).
  if (tab5_brightness_capped) {
    if (WiFi.status() == WL_CONNECTED ||
        (uint32_t)(now - tab5_brightness_cap_wait_since) > kTab5BrightnessRestoreTimeoutMs) {
      tab5_brightness_capped = false;
      BoardHAL::setBrightness(configManager.getConfig().display_brightness);
      Serial.println("[Power] Brownout-Helligkeitsdrossel aufgehoben");
    } else if (BoardHAL::getBrightness() > kTab5SafeBrightness) {
      // Auch waehrend der Wartephase durchsetzen (Slider-Aenderung im
      // Reconnect-Fenster wuerde den Schutz sonst umgehen).
      BoardHAL::setBrightness(kTab5SafeBrightness);
    }
  }
#endif

  service_image_screensaver_auto(displayManager.getLastActivityTime());
  if (first_run) Serial.println("[Loop] powerManager.update()...");
  powerManager.update(displayManager.getLastActivityTime());

  if (first_run) Serial.println("[Loop] Nach powerManager.update()!");

  // --- SLEEP ---
  if (powerManager.isInSleep()) {
    if (!was_asleep) {
      Serial.println("[Loop] SLEEP MODE AKTIV!");
      was_asleep = true;
    }
    // first_run muss auch hier geloescht werden -- sonst druckt JEDE
    // Sleep-Iteration den ganzen "if (first_run)"-Block am Loop-Anfang neu
    // (Endlos-Spam), falls das Geraet schon in seiner allerersten
    // Loop-Iteration schlaeft (z.B. Boot/UI-Build dauerte laenger als das
    // konfigurierte Auto-Sleep-Timeout). Frueher unsichtbar, weil der alte
    // touch_cb-Bug jeden lv_timer_handler()-Aufruf als Wake gewertet und den
    // Sleep-Zweig so nie laenger als einen Tick am Stueck lief.
    if (first_run) {
      Serial.println("[Loop] Sleep direkt nach Boot erkannt");
      Serial.flush();
      first_run = false;
    }
    if (configManager.isConfigured()) {
      networkManager.update();
      if (webAdminServer.isRunning()) webAdminServer.handle();
      // Der MQTT-Worker laeuft auch im Sleep weiter und reiht Empfangenes in
      // die Inbound-Queue ein -- ohne Drain wuerde sie volllaufen (Drops).
      // Post-Connect ebenfalls hier, damit ein Reconnect im Sleep wie frueher
      // sofort wieder Subscribes/Discovery bekommt.
      mqttServicePostConnect();
      mqtt_process_inbound_queue();
      // tiles_update_sensor_by_entity() queued Live-Werte jetzt auch waehrend
      // echtem Sleep (frueher: nur gecacht, UI-Queues blieben leer -- das
      // brauchte einen Wake-Sonder-Catchup, der z.B. Media-Tiles vergessen
      // hat). Hier draenieren, damit die Kacheln laufend aktuell sind, nicht
      // erst ab dem Aufwachen. Kein Rendering-Kosten: Refresh-Timer ist
      // pausiert, ein aktualisiertes Label markiert nur seine eigene kleine
      // Flaeche dirty, ohne dass auf den abgeschalteten Screen gezeichnet wird.
      // process_tile_graph_queue() bewusst NICHT hier: Graph-Historie ist
      // Request/Response (network round-trip), keine laufende Werte-Quelle
      // wie die anderen vier -- bleibt beim bestehenden On-Demand-Pfad.
      process_sensor_update_queue();
      process_switch_update_queue();
      process_climate_update_queue();
      process_weather_update_queue();
      process_media_update_queue();
      // Uhrzeit/WLAN-/Power-Status haben denselben Bug wie die Sensor-Queues
      // oben: uiManager.updateStatusbar() & Co. liefen bisher nur alle 2s im
      // AKTIVEN Loop-Pfad, nie im Sleep -- die Uhr blieb also auf dem Stand
      // von vor dem Einschlafen stehen und sprang erst beim Aufwachen auf
      // die aktuelle Zeit. Guenstig genug (liest nur Systemzeit/WiFi-Status,
      // setzt Label-Text), um hier ebenfalls alle ~20ms mitzulaufen.
      settings_update_power_status();
      if (networkManager.isNetworkConnected()) {
        const DeviceConfig& sleep_cfg = configManager.getConfig();
        const char* network_name =
            networkTransport.activeKind() == NetworkTransportKind::Wifi
                ? sleep_cfg.wifi_ssid
                : networkTransport.activeName();
        settings_update_wifi_status(
            true, network_name,
            networkTransport.localIP().toString().c_str());
      } else {
        settings_update_wifi_status(false, nullptr, nullptr);
      }
      uiManager.updateStatusbar();
      // Gleiches Muster nochmal, zwei weitere Stellen gefunden, die nur im
      // aktiven Loop liefen: tiles_process_visible_cache_refresh() wendet
      // Struktur-/Icon-Aenderungen (aus dem 60s-Bridge-Sync oben) auf
      // sichtbare Kacheln an -- ohne das blieben Icon-Updates waehrend des
      // Sleeps haengen. mqttServiceLocalSensors() aktualisiert die interne
      // Batterie- und den externen OneWire-Temperatursensor (alle 500ms
      // eigen-throttled) -- dieselbe Luecke wie bei den Statusbar-Werten.
      tiles_process_visible_cache_refresh(true);
      mqttServiceLocalSensors();
      tiles_process_bridge_cache_refresh(true);
      service_background_state_refresh(true);
      process_energy_response_queue();
      energy_service_periodic();
    }
    // Touch-Wake: abfragen ob Touch aktiv. Aufwecken ist NUR NOCH eine
    // Display-Hardware-Aktion (Backlight/Panel an) -- keine Sonderbehandlung
    // fuer Daten mehr (kein Extra-Request, kein Extra-Drain). Die Hintergrund-
    // verarbeitung oben laeuft ohnehin durchgehend, gleiche Logik ob wach oder
    // schlafend; das kurze delay(20) unten (statt vorher 150ms) haelt das
    // Zeitfenster fuer "gerade eingetroffener Wert noch nicht verarbeitet"
    // strukturell klein, ohne dass das Aufwachen selbst etwas anstossen muss.
    BoardHAL::TouchPoint tp;
    if (BoardHAL::getTouch(&tp)) {
      Serial.printf("[Power] Sleep-Poll Touch erkannt: x=%d y=%d\n", tp.x, tp.y);
      powerManager.wakeFromDisplaySleep("sleep-poll");
      was_asleep = false;
      return;
    }
    delay(20);
    return;
  }
  // Zurück im aktiven Modus
  was_asleep = false;

  // Diagnostic: bracket every major step between here and the lv_timer_handler()
  // call below, so a slow segment shows up by name instead of having to guess
  // and instrument one function at a time (see project memory: repeated rounds
  // of "found one cost, animation still hitches" -- this covers the whole gap
  // in one pass). Only prints if the total exceeds 80ms.
  uint32_t t_loop0 = millis();

  // Kein Wake-getriggerter Extra-Request mehr hier (frueher: wake_bridge_
  // request_pending -> publishBridgeRequest()/energy_request_day_for_tiles()).
  // Beides laeuft schon unabhaengig vom Sleep/Wake-Zustand periodisch von
  // alleine (service_background_state_refresh() alle 60s, energy_service_
  // periodic() alle 60s, beide oben/unten unconditional aufgerufen) --
  // Aufwachen soll keine Datenverarbeitung anstossen, die nicht ohnehin
  // schon laeuft, nur das Display anschalten.
  uint32_t t_wake = millis();

  const bool camera_popup_busy = camera_popup_is_busy();
  const bool admin_busy = webAdminRecentlyActive(20000);
  const bool ui_idle_for_background_refresh =
      !camera_popup_busy &&
      !powerManager.isHighPerformance() &&
      !admin_busy;
  service_background_state_refresh(ui_idle_for_background_refresh);
  uint32_t t_bg_refresh = millis();
  tiles_process_bridge_cache_refresh(ui_idle_for_background_refresh);
  uint32_t t_bridge_cache = millis();
  tiles_process_visible_cache_refresh(ui_idle_for_background_refresh);
  uint32_t t_visible_cache = millis();

  // --- ACTIVE ---
  // Lokale Sensoren (z. B. externer OneWire-Temperatursensor)
  mqttServiceLocalSensors();
  uint32_t t_local_sensors = millis();

  if (first_run) Serial.println("[Loop] process_sensor_update_queue()...");
  // Die Kamera verdeckt alle anderen Popups. Deren Daten bleiben in ihren
  // zusammenfassenden Queues erhalten und werden direkt nach dem Schliessen
  // verarbeitet; so unterbrechen z.B. Wetter-JSON und Graph-Aufbau kein Bild.
  if (!camera_popup_busy) {
    process_sensor_popup_queue();
    process_weather_popup_queue();
    process_energy_response_queue();
    process_energy_popup_queue();
  }
  process_camera_popup();
  uint32_t t_popup_queues = millis();

  // Im Idle nur alle 2s tile/sensor Queues verarbeiten (spart CPU bei 10 FPS)
  if (!camera_popup_busy) {
    static uint32_t last_queue_ms = 0;
    bool idle = !powerManager.isHighPerformance();
    if (!idle || (millis() - last_queue_ms >= 2000)) {
      // Im Idle-Fall koennen sich bis zu 32 Sensor-/Switch- bzw. 16 Wetter-Updates
      // in den 2s angesammelt haben. Alle auf einmal synchron abzuarbeiten kann
      // den Frame direkt vor lv_timer_handler() spuerbar verzoegern (sichtbares
      // kurzes Hakeln z.B. bei laufenden Animation-Tiles). Wie beim Media-Limit
      // (2) wird hier pro Loop-Durchlauf nur ein Haeppchen verarbeitet; der Rest
      // folgt in den naechsten Iterationen, die im Idle-Fall ebenfalls durch
      // dieses 2s-Fenster laufen -- bei normalem MQTT-Aufkommen bleibt die Queue
      // dadurch praktisch nie lange gefuellt.
      process_sensor_update_queue(6);  // WICHTIG: VOR lv_timer_handler()!
      process_switch_update_queue(6);
      process_climate_update_queue(4);
      process_weather_update_queue(4);
      process_media_update_queue(2);
      process_tile_graph_queue();
      if (idle) energy_service_periodic();
      last_queue_ms = millis();
    }
  }
  uint32_t t_update_queues = millis();
  // Navigation/Layout/Style-Reloads bleiben als Pending-Flags erhalten. Hinter
  // dem vollflächigen Kamera-Popup wären sie unsichtbar, könnten aber einen
  // kompletten Frame kosten.
  if (!camera_popup_busy) {
    tiles_process_reload_requests();
  }
  uint32_t t_reload_requests = millis();

  if ((t_reload_requests - t_loop0) >= 80) {
    Serial.printf(
        "[LoopGap] total=%ums wake=%u bg_refresh=%u bridge_cache=%u visible_cache=%u "
        "local_sensors=%u popup_queues=%u update_queues=%u reload_requests=%u\n",
        (unsigned)(t_reload_requests - t_loop0), (unsigned)(t_wake - t_loop0),
        (unsigned)(t_bg_refresh - t_wake), (unsigned)(t_bridge_cache - t_bg_refresh),
        (unsigned)(t_visible_cache - t_bridge_cache), (unsigned)(t_local_sensors - t_visible_cache),
        (unsigned)(t_popup_queues - t_local_sensors), (unsigned)(t_update_queues - t_popup_queues),
        (unsigned)(t_reload_requests - t_update_queues));
  }

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

  if (first_run) Serial.println("[Loop] Network check...");
  if (configManager.isConfigured()) {
    // MQTT-Socket, Reconnects und Puffer-Housekeeping laufen komplett auf
    // dem Worker-Task (mqtt_worker_task oben, Single-Owner). Auf dem
    // Loop-Task bleiben nur die beiden Queue-Enden, weil beide Flash/LVGL
    // anfassen: das Post-Connect-Hochfahren (Subscribes/Discovery) und das
    // Verarbeiten eingegangener Nachrichten.
    mqttServicePostConnect();
    // Kamera: kleine MQTT-Bursts gleichmaessig ueber die Frames verteilen.
    // Vier Nachrichten pro Loop reichen bei 24 FPS fuer bis zu 96/s; der
    // Empfangs-Worker und die 64er PSRAM-Queue laufen dabei unveraendert weiter.
    mqtt_process_inbound_queue(camera_popup_is_busy() ? 4 : 0);
    if (!camera_popup_is_busy()) {
      mqttServiceDynamicSlotsReload();
    }
    static uint8_t net_tick = 0;
    if (++net_tick % 5 == 0) {
      if (first_run) Serial.println("[Loop] networkManager.update()...");
      networkManager.update();
    }
    if (!logged_wifi_connected && networkManager.isNetworkConnected()) {
      logged_wifi_connected = true;
      log_memory_status("network-connected");
    }
    if (!logged_mqtt_connected && networkManager.isMqttConnected()) {
      logged_mqtt_connected = true;
      log_memory_status("mqtt-connected");
    }
  }

  if (now - last_mem_log_ms >= 60000UL) {
    last_mem_log_ms = now;
    log_memory_status("runtime-60s");
  }

  // Neuen Heap-Tiefststand SOFORT melden (nicht erst im 60s-Raster): ein
  // "Min seit Boot: 4 KB" ohne Zeitpunkt laesst sich keinem Verursacher
  // zuordnen. Schwelle 8 KB haelt das Log frei von Normalbetriebs-Rauschen.
  {
    static uint32_t last_reported_min_heap = 0xFFFFFFFF;
    const uint32_t min_heap = ESP.getMinFreeHeap();
    if (min_heap + 8192 < last_reported_min_heap) {
      last_reported_min_heap = min_heap;
      Serial.printf("[Mem] NEUER TIEFSTSTAND: min=%u KB (frei=%u KB, largest=%u KB) @ %lu ms\n",
                    static_cast<unsigned>(min_heap / 1024),
                    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                    static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024),
                    static_cast<unsigned long>(now));
    }
  }


  if (now - last_status_update > 2000UL) {
    last_status_update = now;
    settings_update_power_status();
    if (configManager.isConfigured()) {
      uiManager.serviceNtpSync();
      const DeviceConfig& c = configManager.getConfig();
      if (networkManager.isNetworkConnected()) {
        const char* network_name =
            networkTransport.activeKind() == NetworkTransportKind::Wifi
                ? c.wifi_ssid
                : networkTransport.activeName();
        settings_update_wifi_status(
            true, network_name,
            networkTransport.localIP().toString().c_str());
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

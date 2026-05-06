#include "src/core/power_manager.h"
#include "src/core/board_hal.h"
#include "src/core/display_manager.h"
#include "src/core/config_manager.h"
#include "src/core/battery_state.h"
#include "src/network/network_manager.h"
#include "src/network/mqtt_handlers.h"
#include <cmath>
#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-cpu.h"
#include "esp_log.h"
#endif

PowerManager powerManager;

// Waveshare board has no battery – always report mains
static bool detect_powered_by_mains_hw() {
  return true;
}

void PowerManager::applyCpuFrequency(uint16_t mhz) {
#if defined(ARDUINO_ARCH_ESP32)
  if (mhz == 0) return;
  if (last_cpu_mhz == 0) {
    last_cpu_mhz = static_cast<uint16_t>(getCpuFrequencyMhz());
  }
  if (last_cpu_mhz == mhz) return;
  const bool ok = setCpuFrequencyMhz(static_cast<uint32_t>(mhz));
  last_cpu_mhz = static_cast<uint16_t>(getCpuFrequencyMhz());
  Serial.printf("CPU Freq req=%u MHz, set=%s, actual=%u MHz\n",
                static_cast<unsigned>(mhz),
                ok ? "ok" : "fail",
                static_cast<unsigned>(last_cpu_mhz));
#else
  (void)mhz;
#endif
}

void PowerManager::init() {
  Serial.println("[Power] Init Power Manager...");
  disp = displayManager.getDisplay();
  is_high_performance = true;
#if defined(ARDUINO_ARCH_ESP32)
  last_cpu_mhz = static_cast<uint16_t>(getCpuFrequencyMhz());
#endif
}

void PowerManager::setHighPerformance(bool enable) {
  if (enable == is_high_performance) return;
  is_high_performance = enable;

  if (enable) {
    applyCpuFrequency(CPU_FREQ_HIGH);
    networkManager.setWifiPowerSaving(false);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_set_period(rt, 1000 / FPS_HIGH);
    }
#endif
  } else {
    applyCpuFrequency(CPU_FREQ_LOW);
    networkManager.setWifiPowerSaving(true);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_set_period(rt, 1000 / FPS_LOW);
    }
#endif
  }
}

uint32_t PowerManager::getSleepTimeout() const {
  const DeviceConfig& cfg = configManager.getConfig();
  // Waveshare is always on mains – use mains sleep settings only
  if (!cfg.auto_sleep_enabled) return 0xFFFFFFFF;
  return cfg.auto_sleep_seconds * 1000UL;
}

bool PowerManager::isPoweredByMains() const {
  return true;  // Waveshare: always mains-powered
}

bool PowerManager::isTouchWakeEnabled() const {
  // No IMU on Waveshare – touch wake is always the mode
  return true;
}

bool PowerManager::isImuWakeEnabled() const {
  // No IMU on Waveshare
  return false;
}

// No IMU on Waveshare – stubs
bool PowerManager::ensureImuReady() { return false; }
bool PowerManager::detectAutoRotation(bool*) { return false; }
void PowerManager::serviceImuWake() {}

void PowerManager::update(uint32_t last_activity_time) {
  if (is_display_sleeping) {
    // Touch-wake is handled in the main loop
    return;
  }
  uint32_t now = millis();
  uint32_t sleep_timeout = getSleepTimeout();

  // Schlaf-Sperre aktiv?
  if (sleep_blocked) {
    if (sleep_block_until != 0 && (int32_t)(now - sleep_block_until) >= 0) {
      sleep_blocked = false;
    } else {
      setHighPerformance(true);
      return;
    }
  }

  if (is_high_performance && (now - last_activity_time > IDLE_TIMEOUT_MS)) {
    setHighPerformance(false);
  }

  if (!is_high_performance && sleep_timeout != 0xFFFFFFFF) {
    if (now - last_activity_time > sleep_timeout) {
      enterDisplaySleep();
    }
  }
}

void PowerManager::enterDisplaySleep() {
  if (is_display_sleeping) return;
  Serial.println("[Power] Sleep...");

  saved_brightness = BoardHAL::getBrightness();

  // 1) LVGL-Rendering sofort stoppen
#if LV_VERSION_MAJOR >= 9
  if (disp) {
    lv_timer_t* rt = lv_display_get_refr_timer(disp);
    if (rt) lv_timer_pause(rt);
  }
#endif
  // 2) DMA abwarten
  BoardHAL::displayWaitDisplay();

  // Touch-Wake ist auf Waveshare immer aktiv (kein IMU)
  display_hw_sleeping = false;
  BoardHAL::setBrightness(0);
  BoardHAL::displayWaitDisplay();
  delay(30);
  BoardHAL::displayPowerSaveOn();
  BoardHAL::displayWaitDisplay();
  delay(60);
  esp_log_level_set("lcd.dsi.dpi", ESP_LOG_NONE);
  applyCpuFrequency(CPU_FREQ_SLEEP);
  esp_log_level_set("lcd.dsi.dpi", ESP_LOG_ERROR);
  BoardHAL::displayWaitDisplay();
  delay(20);

  displayManager.setInputEnabled(true);  // Touch bleibt aktiv fuer Wake
  networkManager.setSleepWifiProfile(false);
  networkManager.setWifiPowerSaving(false);
  is_display_sleeping = true;
  is_high_performance = false;
  mqttPublishDeviceSettings();
}

void PowerManager::wakeFromDisplaySleep() {
  if (!is_display_sleeping) return;

  BoardHAL::setBrightness(0);

  esp_log_level_set("lcd.dsi.dpi", ESP_LOG_NONE);
  applyCpuFrequency(CPU_FREQ_HIGH);
  esp_log_level_set("lcd.dsi.dpi", ESP_LOG_ERROR);
  BoardHAL::displayWaitDisplay();
  delay(20);
  displayManager.setInputEnabled(true);
  if (display_hw_sleeping) {
    BoardHAL::displayWake();
    display_hw_sleeping = false;
  }
  BoardHAL::displayPowerSaveOff();
  BoardHAL::displayWaitDisplay();
  delay(30);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) {
            lv_timer_resume(rt);
            lv_timer_set_period(rt, 1000 / FPS_HIGH);
        }
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(disp);
    }
#endif
  BoardHAL::displayWaitDisplay();
  delay(10);
  BoardHAL::setBrightness(saved_brightness);

  is_display_sleeping = false;
  is_high_performance = true;

  networkManager.setSleepWifiProfile(false);
  networkManager.setWifiPowerSaving(false);
  displayManager.resetActivityTimer();
  displayManager.armWakeTouchGuard();
  mqttPublishDeviceSettings();
}

void PowerManager::updatePowerMode() {
}

void PowerManager::blockSleep(uint32_t duration_ms) {
  sleep_blocked = true;
  sleep_block_until = duration_ms ? millis() + duration_ms : 0;
  if (is_display_sleeping) {
    wakeFromDisplaySleep();
  }
  setHighPerformance(true);
}

void PowerManager::allowSleep() {
  sleep_blocked = false;
  sleep_block_until = 0;
}

bool PowerManager::isSleepBlocked() const {
  if (!sleep_blocked) return false;
  if (sleep_block_until == 0) return true;
  return (int32_t)(millis() - sleep_block_until) < 0;
}

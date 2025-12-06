#include "src/core/power_manager.h"
#include <M5Unified.h>
#include "src/core/display_manager.h"
#include "src/core/config_manager.h"
#include "src/network/network_manager.h"

PowerManager powerManager;

void PowerManager::init() {
  Serial.println("⚡ Init Power Manager...");
  disp = displayManager.getDisplay();
  is_high_performance = true;
}

void PowerManager::setHighPerformance(bool enable) {
  if (enable == is_high_performance) return;
  is_high_performance = enable;

  if (enable) {
    if (isPoweredByMains()) networkManager.setWifiPowerSaving(false);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_set_period(rt, 1000 / FPS_HIGH);
    }
#endif
  } else {
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
  if (!isPoweredByMains()) return SLEEP_TIMEOUT_MS_BATTERY;
  const DeviceConfig& cfg = configManager.getConfig();
  if (!cfg.auto_sleep_enabled) return 0xFFFFFFFF;
  return cfg.auto_sleep_minutes * 60 * 1000UL;
}

bool PowerManager::isPoweredByMains() const {
  return (M5.Power.getBatteryCurrent() <= 50);
}

void PowerManager::update(uint32_t last_activity_time) {
  if (is_display_sleeping) return;
  uint32_t now = millis();

  if (is_high_performance && (now - last_activity_time > IDLE_TIMEOUT_MS)) {
    setHighPerformance(false); 
  }

  uint32_t sleep_timeout = getSleepTimeout();
  if (!is_high_performance && (now - last_activity_time > sleep_timeout)) {
    enterDisplaySleep();
  }
}

void PowerManager::enterDisplaySleep() {
  if (is_display_sleeping) return;
  Serial.println("😴 Sleep...");
  
  saved_brightness = M5.Display.getBrightness();
  M5.update();
  M5.Display.setBrightness(0);

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) lv_timer_pause(rt);
    }
#endif
  
  networkManager.setWifiPowerSaving(true);
  is_display_sleeping = true;
  is_high_performance = false;
}

void PowerManager::wakeFromDisplaySleep() {
  if (!is_display_sleeping) return;

  M5.Display.setBrightness(saved_brightness);
  
#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) {
            lv_timer_resume(rt);
            lv_timer_set_period(rt, 1000 / FPS_HIGH);
        }
    }
#endif
  
  is_display_sleeping = false;
  is_high_performance = true;
  
  if (isPoweredByMains()) networkManager.setWifiPowerSaving(false);
  displayManager.resetActivityTimer();
}

void PowerManager::updatePowerMode() {
}
#include "src/core/power_manager.h"
#include <M5Unified.h>
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

static constexpr uint32_t kImuWakePollMsSleep = 120;
static constexpr uint32_t kImuWakePollMsAwake = 80;
static constexpr float kImuGravityAlpha = 0.95f;
static constexpr float kImuWakeLinMag = 0.024f;
static constexpr float kImuWakeLinJerk = 0.005f;
static constexpr float kImuWakeGravDelta = 0.025f;
static constexpr float kImuNoiseAlpha = 0.90f;
static constexpr float kImuNoiseClamp = 0.015f;
static constexpr float kImuTapNoiseMult = 1.3f;
static constexpr float kImuTapNoiseOffset = 0.004f;
static constexpr float kImuTapPeakMin = 0.012f;
static constexpr float kImuTapJerk = 0.005f;
static constexpr float kImuHoldNoiseMult = 1.2f;
static constexpr float kImuHoldNoiseOffset = 0.006f;
static constexpr float kImuHoldMin = 0.012f;
static constexpr float kImuHoldJerk = 0.006f;
static constexpr float kImuHoldGravDelta = 0.015f;
static constexpr uint8_t kImuHoldSamples = 2;
static constexpr uint32_t kImuHoldMs = 20000;
static constexpr float kImuAutoRotateAxisMin = 0.35f;
static constexpr uint8_t kImuAutoRotateStableSamples = 1;
static constexpr uint32_t kImuAutoRotateMinIntervalMs = 80;
static constexpr float kImuWakeStrong = 0.07f;
static constexpr float kImuWakeStrongJerk = 0.025f;
static constexpr uint32_t kImuWakeCooldownMs = 100;
static constexpr bool kImuWakeDebug = false;
static constexpr uint32_t kImuWakeLogMs = 200;
static constexpr uint32_t kIdleTimeoutBatteryMs = 800;
static constexpr uint32_t kImuI2cSleepHz = 100000;
static constexpr uint32_t kImuI2cWakeHz = 400000;
static constexpr uint8_t kBmi270RegPwrCtrl = 0x7D;
static constexpr uint8_t kBmi270PwrCtrlAccelOnly = 0x04;
static constexpr uint8_t kBmi270PwrCtrlAllOn = 0x0F;

static bool detect_powered_by_mains_hw() {
  batteryStateUpdate();
  return batteryStateIsOnMains();
}

static void imuSetAccelOnly(bool enable) {
  auto imu = M5.Imu.getImuInstancePtr(0);
  if (!imu) return;
  if (enable) {
    imu->writeRegister8(kBmi270RegPwrCtrl, kBmi270PwrCtrlAccelOnly);
  } else {
    imu->writeRegister8(kBmi270RegPwrCtrl, kBmi270PwrCtrlAllOn);
  }
}

bool PowerManager::ensureImuReady() {
  if (imu_checked) return imu_ready;
  imu_checked = true;
  if (M5.Imu.isEnabled()) {
    imu_ready = true;
    return true;
  }
  imu_ready = M5.Imu.begin();
  return imu_ready;
}

bool PowerManager::detectAutoRotation(bool* rotated_out) {
  if (!rotated_out) return false;
  if (!ensureImuReady()) return false;
  auto mask = M5.Imu.update();
  if (!(mask & m5::IMU_Class::sensor_mask_accel)) return false;
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return false;
  float abs_x = std::fabs(ax);
  float abs_y = std::fabs(ay);
  float abs_axis = (abs_x >= abs_y) ? abs_x : abs_y;
  if (abs_axis < kImuAutoRotateAxisMin) return false;
  float axis = (abs_x >= abs_y) ? ax : ay;
  *rotated_out = (axis > 0.0f);
  return true;
}

void PowerManager::serviceImuWake() {
  if (!ensureImuReady()) return;
  uint32_t now = millis();
  uint32_t poll_ms = is_display_sleeping ? kImuWakePollMsSleep : kImuWakePollMsAwake;
  if (now - imu_last_poll_ms < poll_ms) return;
  imu_last_poll_ms = now;

  auto mask = M5.Imu.update();
  if (!(mask & m5::IMU_Class::sensor_mask_accel)) return;

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;

  if (!imu_have_last) {
    imu_grav_x = ax;
    imu_grav_y = ay;
    imu_grav_z = az;
    imu_last_lin_mag = 0.0f;
    imu_noise_ema = 0.0f;
    imu_last_wake_ms = 0;
    imu_last_log_ms = 0;
    imu_have_last = true;
    return;
  }

  float prev_grav_x = imu_grav_x;
  float prev_grav_y = imu_grav_y;
  float prev_grav_z = imu_grav_z;

  imu_grav_x = kImuGravityAlpha * imu_grav_x + (1.0f - kImuGravityAlpha) * ax;
  imu_grav_y = kImuGravityAlpha * imu_grav_y + (1.0f - kImuGravityAlpha) * ay;
  imu_grav_z = kImuGravityAlpha * imu_grav_z + (1.0f - kImuGravityAlpha) * az;

  float lin_x = ax - imu_grav_x;
  float lin_y = ay - imu_grav_y;
  float lin_z = az - imu_grav_z;
  float lin_mag = std::sqrt(lin_x * lin_x + lin_y * lin_y + lin_z * lin_z);
  float lin_jerk = std::fabs(lin_mag - imu_last_lin_mag);
  imu_last_lin_mag = lin_mag;

  float grav_delta = std::fabs(imu_grav_x - prev_grav_x) +
                     std::fabs(imu_grav_y - prev_grav_y) +
                     std::fabs(imu_grav_z - prev_grav_z);

  if (lin_mag < kImuNoiseClamp) {
    imu_noise_ema = kImuNoiseAlpha * imu_noise_ema + (1.0f - kImuNoiseAlpha) * lin_mag;
  }

  float tap_threshold = imu_noise_ema * kImuTapNoiseMult + kImuTapNoiseOffset;
  if (tap_threshold < kImuTapPeakMin) tap_threshold = kImuTapPeakMin;

  float hold_threshold = imu_noise_ema * kImuHoldNoiseMult + kImuHoldNoiseOffset;
  if (hold_threshold < kImuHoldMin) hold_threshold = kImuHoldMin;

  uint32_t now_ms = millis();
  bool hold_motion = (lin_mag >= hold_threshold) ||
                     (lin_jerk >= kImuHoldJerk) ||
                     (grav_delta >= kImuHoldGravDelta);
  if (hold_motion) {
    if (imu_hold_hits < kImuHoldSamples) imu_hold_hits++;
  } else if (imu_hold_hits > 0) {
    imu_hold_hits--;
  }
  if (imu_hold_hits >= kImuHoldSamples) {
    imu_last_motion_ms = now_ms;
    if (!is_display_sleeping) {
      displayManager.resetActivityTimer();
      if (isPoweredByMains()) {
        setHighPerformance(true);
      }
    }
  }

  if (!is_display_sleeping) {
    const DeviceConfig& cfg = configManager.getConfig();
    if (cfg.display_rotation_mode == kDisplayRotationAuto) {
      if (!imu_auto_rotate_valid) {
        imu_auto_rotate_state = displayManager.isRotationFlipped();
        imu_auto_rotate_valid = true;
      }
      if (now_ms - imu_last_auto_rotate_ms >= kImuAutoRotateMinIntervalMs) {
        float abs_x = std::fabs(ax);
        float abs_y = std::fabs(ay);
        float axis = (abs_x >= abs_y) ? ax : ay;
        float abs_axis = (abs_x >= abs_y) ? abs_x : abs_y;
        bool candidate = imu_auto_rotate_state;
        if (abs_axis >= kImuAutoRotateAxisMin) {
          candidate = (axis > 0.0f);
          if (candidate != imu_auto_rotate_state) {
            imu_auto_rotate_hits++;
            if (imu_auto_rotate_hits >= kImuAutoRotateStableSamples) {
              imu_auto_rotate_state = candidate;
              imu_auto_rotate_hits = 0;
              imu_last_auto_rotate_ms = now_ms;
              displayManager.setRotationFlipped(candidate);
              configManager.setRuntimeDisplayRotation(candidate);
              mqttPublishDeviceSettings();
            }
          } else {
            imu_auto_rotate_hits = 0;
          }
        } else {
          imu_auto_rotate_hits = 0;
        }
      }
    } else {
      imu_auto_rotate_valid = false;
      imu_auto_rotate_hits = 0;
    }
  }

  if (now_ms - imu_last_wake_ms < kImuWakeCooldownMs) {
    return;
  }

  if (lin_mag >= kImuWakeStrong || lin_jerk >= kImuWakeStrongJerk) {
    imu_last_wake_ms = now_ms;
    if (kImuWakeDebug) {
      Serial.printf("[IMU] WAKE strong lin=%.3f jerk=%.3f thr=%.3f\n",
                    (double)lin_mag, (double)lin_jerk, (double)kImuWakeStrong);
    }
    wakeFromDisplaySleep();
    return;
  }

  bool peak = (lin_mag >= tap_threshold) || (lin_jerk >= kImuTapJerk);
  if (peak) {
    imu_last_wake_ms = now_ms;
    if (kImuWakeDebug) {
      Serial.printf("[IMU] WAKE peak lin=%.3f jerk=%.3f thr=%.3f noise=%.3f\n",
                    (double)lin_mag, (double)lin_jerk, (double)tap_threshold, (double)imu_noise_ema);
    }
    wakeFromDisplaySleep();
    return;
  }

  if (grav_delta >= kImuWakeGravDelta) {
    imu_last_wake_ms = now_ms;
    if (kImuWakeDebug) {
      Serial.printf("[IMU] WAKE tilt dG=%.3f\n", (double)grav_delta);
    }
    wakeFromDisplaySleep();
    return;
  }

  if (kImuWakeDebug && (now_ms - imu_last_log_ms) >= kImuWakeLogMs) {
    imu_last_log_ms = now_ms;
    Serial.printf("[IMU] ax=%.3f ay=%.3f az=%.3f lin=%.3f jerk=%.3f dG=%.3f thr=%.3f noise=%.3f\n",
                  (double)ax, (double)ay, (double)az,
                  (double)lin_mag, (double)lin_jerk, (double)grav_delta,
                  (double)tap_threshold, (double)imu_noise_ema);
  }
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
  Serial.println("⚡ Init Power Manager...");
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
    if (isPoweredByMains()) networkManager.setWifiPowerSaving(false);

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
  if (isPoweredByMains()) {
    if (!cfg.auto_sleep_enabled) return 0xFFFFFFFF;
    return cfg.auto_sleep_seconds * 1000UL;
  }
  if (!cfg.auto_sleep_battery_enabled) return 0xFFFFFFFF;
  return cfg.auto_sleep_battery_seconds * 1000UL;
}

bool PowerManager::isPoweredByMains() const {
  return detect_powered_by_mains_hw();
}

bool PowerManager::isTouchWakeEnabled() const {
  const DeviceConfig& cfg = configManager.getConfig();
  uint8_t mode = isPoweredByMains() ? cfg.wake_mode_mains : cfg.wake_mode_battery;
  return mode == kWakeModeTouch;
}

bool PowerManager::isImuWakeEnabled() const {
  const DeviceConfig& cfg = configManager.getConfig();
  uint8_t mode = isPoweredByMains() ? cfg.wake_mode_mains : cfg.wake_mode_battery;
  return mode == kWakeModeImu;
}

void PowerManager::update(uint32_t last_activity_time) {
  if (is_display_sleeping) {
    if (isImuWakeEnabled()) {
      serviceImuWake();
    }
    return;
  }
  uint32_t now = millis();
  bool on_mains = isPoweredByMains();
  uint32_t sleep_timeout = getSleepTimeout();
  bool need_imu = (sleep_timeout != 0xFFFFFFFF);
  if (!need_imu) {
    const DeviceConfig& cfg = configManager.getConfig();
    need_imu = (cfg.display_rotation_mode == kDisplayRotationAuto);
  }
  if (need_imu) {
    serviceImuWake();
  }

  uint32_t last_activity = last_activity_time;

  // Schlaf-Sperre aktiv? -> Keine Sleep-Checks, optional Timeout pruefen
  if (sleep_blocked) {
    if (sleep_block_until != 0 && (int32_t)(now - sleep_block_until) >= 0) {
      sleep_blocked = false;  // Sperre abgelaufen
    } else {
      // Sicherstellen, dass wir nicht in den Low-Power-Sleep fallen
      setHighPerformance(true);
      return;
    }
  }

  if (!on_mains && is_high_performance) {
    networkManager.setWifiPowerSaving(true);
  }

  uint32_t idle_timeout = on_mains ? IDLE_TIMEOUT_MS : kIdleTimeoutBatteryMs;
  if (is_high_performance && (now - last_activity > idle_timeout)) {
    setHighPerformance(false); 
  }

  if (!is_high_performance && sleep_timeout != 0xFFFFFFFF) {
    uint32_t quiet_ms = (sleep_timeout <= 10000) ? sleep_timeout : 10000;
    if (now - last_activity > sleep_timeout) {
      if (imu_last_motion_ms == 0 || (now - imu_last_motion_ms) >= quiet_ms) {
        enterDisplaySleep();
      }
    }
  }
}

void PowerManager::enterDisplaySleep() {
  if (is_display_sleeping) return;
  Serial.println("😴 Sleep...");
  
  saved_brightness = M5.Display.getBrightness();
  M5.update();
  imu_have_last = false;
  imu_last_poll_ms = 0;
  imu_noise_ema = 0.0f;
  imu_last_wake_ms = 0;
  imu_last_log_ms = 0;
  imu_last_motion_ms = 0;
  imu_hold_hits = 0;
  imu_auto_rotate_valid = false;
  imu_auto_rotate_hits = 0;
  if (ensureImuReady()) {
    M5.Imu.setClock(kImuI2cSleepHz);
    imuSetAccelOnly(true);
  }
  // 1) LVGL-Rendering sofort stoppen (kein neuer Frame-Flush)
#if LV_VERSION_MAJOR >= 9
  if (disp) {
    lv_timer_t* rt = lv_display_get_refr_timer(disp);
    if (rt) lv_timer_pause(rt);
  }
#endif
  // 2) Letzten laufenden DMA-Transfer abwarten
  M5.Display.waitDisplay();

  bool touch_wake = isTouchWakeEnabled();
  if (!touch_wake) {
    // 3) Erst Brightness 0, dann ein Frame abwarten, dann Sleep
    M5.Display.setBrightness(0);
    M5.Display.waitDisplay();
    delay(30);  // Backlight sicher aus
    M5.Display.powerSaveOn();
    M5.Display.waitDisplay();
    delay(30);  // Panel-Idle stabilisieren
    M5.Display.sleep();
    delay(60);  // DSI-Controller beendet letzten PSRAM-Read
    M5.Display.waitDisplay();
    display_hw_sleeping = true;
    // 4) Erst jetzt CPU runter — DSI ist aus, aber PLL-Wechsel kann
    //    kurzzeitig einen harmlosen Underrun-Log ausloesen -> unterdruecken
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_NONE);
    applyCpuFrequency(CPU_FREQ_SLEEP);
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_ERROR);
    M5.Display.waitDisplay();
    delay(20);  // Nach PLL/Freq-Wechsel kurz stabilisieren
  } else {
    display_hw_sleeping = false;
    M5.Display.setBrightness(0);
    M5.Display.waitDisplay();
    delay(30);  // Erst dunkel schalten, dann Panel-Mode wechseln
    M5.Display.powerSaveOn();
    // Touch-Wake: Display bleibt an, CPU trotzdem auf Sleep-Frequenz senken.
    // Der Refr-Timer bleibt pausiert; Touch-Wake wird im Loop direkt abgefragt.
    M5.Display.waitDisplay();
    delay(60);  // Panel-Idle stabilisieren, bevor CPU/PLL gewechselt wird
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_NONE);
    applyCpuFrequency(CPU_FREQ_SLEEP);
    esp_log_level_set("lcd.dsi.dpi", ESP_LOG_ERROR);
    M5.Display.waitDisplay();
    delay(20);  // Nach PLL/Freq-Wechsel kurz stabilisieren
  }
  displayManager.setInputEnabled(touch_wake);

  networkManager.setWifiPowerSaving(true);
  is_display_sleeping = true;
  is_high_performance = false;
  mqttPublishDeviceSettings();
}

void PowerManager::wakeFromDisplaySleep() {
  if (!is_display_sleeping) return;

  // Backlight bis zum stabilen ersten Frame sicher aus lassen.
  M5.Display.setBrightness(0);

  // CPU zuerst hoch -> genug PSRAM-Bandbreite fuer DSI
  // PLL-Wechsel kann kurzzeitig harmlosen Underrun-Log ausloesen -> unterdruecken
  esp_log_level_set("lcd.dsi.dpi", ESP_LOG_NONE);
  applyCpuFrequency(CPU_FREQ_HIGH);
  esp_log_level_set("lcd.dsi.dpi", ESP_LOG_ERROR);
  M5.Display.waitDisplay();
  delay(20);  // Nach PLL/Freq-Wechsel kurz stabilisieren
  displayManager.setInputEnabled(true);
  if (display_hw_sleeping) {
    M5.Display.wakeup();
    display_hw_sleeping = false;
  }
  M5.Display.powerSaveOff();
  M5.Display.waitDisplay();
  delay(30);  // Panel stabilisieren, dann Backlight wieder an
  if (imu_ready) {
    M5.Imu.setClock(kImuI2cWakeHz);
    imuSetAccelOnly(false);
  }

#if LV_VERSION_MAJOR >= 9
    if (disp) {
        lv_timer_t * rt = lv_display_get_refr_timer(disp);
        if(rt) {
            lv_timer_resume(rt);
            lv_timer_set_period(rt, 1000 / FPS_HIGH);
        }
        // Erst einen Frame rendern, dann Backlight einschalten.
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(disp);
    }
#endif
  M5.Display.waitDisplay();
  delay(10);
  M5.Display.setBrightness(saved_brightness);
  
  is_display_sleeping = false;
  is_high_performance = true;
  
  if (isPoweredByMains()) networkManager.setWifiPowerSaving(false);
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

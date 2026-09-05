#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>

// ========== Power management settings ==========
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define CPU_FREQ_HIGH       240
#define CPU_FREQ_LOW        240   // RGB stream and WiFi remain stable at the S3 maximum
#define CPU_FREQ_SLEEP      240
#else
#define CPU_FREQ_HIGH       360   // Maximum responsiveness on touch.
#define CPU_FREQ_LOW        360   // DSI continuously reads PSRAM; 90 MHz is not viable with the display on.
#define CPU_FREQ_SLEEP      360   // Lowest stable frequency with an active DSI panel.
#endif
#define FPS_HIGH            60    // 60 FPS for smooth interaction.
#define FPS_LOW             60    // 10 FPS to save power.
#define FPS_SLEEP           1     // 1 FPS during display sleep, for touch detection only.
#define IDLE_TIMEOUT_MS     3000  // High performance for 3 seconds after the last touch.

// Power manager: owns the energy modes and the performance level.
class PowerManager {
public:
  // Initialize power management.
  void init();

  // Set the performance mode.
  void setHighPerformance(bool enable);

  // Status
  bool isHighPerformance() const { return is_high_performance; }

  // Update loop; checks the idle timeout.
  void update(uint32_t last_activity_time);

  // Display sleep mode: panel off, CPU at minimum, touch stays active.
  void enterDisplaySleep();
  void wakeFromDisplaySleep(const char* reason = "unknown");
  // Updates the requested backlight level. During display sleep this only
  // changes the value to restore on wake-up; the backlight stays safely off.
  void setDisplayBrightness(uint8_t brightness);
  bool isInSleep() const { return is_display_sleeping; }
  bool isTouchWakeEnabled() const;
  bool isImuWakeEnabled() const;
  bool detectAutoRotation(bool* rotated_out);

  // Block or allow sleep, for example while Web Admin is active.
  void blockSleep(uint32_t duration_ms = 0);  // Zero means unlimited; otherwise use the timeout.
  void allowSleep();
  bool isSleepBlocked() const;

  // Current sleep timeout, depending on battery or mains power.
  uint32_t getSleepTimeout() const;

  // True on mains power, false on battery.
  bool isPoweredByMains() const;

  // Power Mode Update (WiFi Power Saving, etc.)
  void updatePowerMode();

private:
  void applyCpuFrequency(uint16_t mhz);
  void serviceImuWake();
  bool ensureImuReady();
  bool last_power_mode = true;  // true = mains, false = battery
  bool is_high_performance = true;
  bool is_display_sleeping = false;
  uint8_t saved_brightness = 150;  // Brightness saved before sleep.
  lv_display_t* disp = nullptr;  // Display reference used to adjust FPS.
  uint16_t last_cpu_mhz = 0;
  bool sleep_blocked = false;
  uint32_t sleep_block_until = 0; // millis() deadline; zero means no deadline.
  bool imu_checked = false;
  bool imu_ready = false;
  bool imu_have_last = false;
  uint32_t imu_last_poll_ms = 0;
  float imu_grav_x = 0.0f;
  float imu_grav_y = 0.0f;
  float imu_grav_z = 0.0f;
  float imu_last_lin_mag = 0.0f;
  float imu_noise_ema = 0.0f;
  uint32_t imu_last_wake_ms = 0;
  uint32_t imu_last_log_ms = 0;
  uint32_t imu_last_motion_ms = 0;
  uint8_t imu_hold_hits = 0;
  bool imu_auto_rotate_state = false;
  bool imu_auto_rotate_valid = false;
  uint8_t imu_auto_rotate_hits = 0;
  uint32_t imu_last_auto_rotate_ms = 0;
  bool display_hw_sleeping = false;
};

// Shared instance.
extern PowerManager powerManager;

#endif // POWER_MANAGER_H

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>

// ========== Power Management Einstellungen ==========
#define CPU_FREQ_HIGH       360   // Ultra-schnell bei Touch
#define CPU_FREQ_LOW        360   // DSI liest PSRAM kontinuierlich -> 90 MHz unmoeglich bei Display an
#define CPU_FREQ_SLEEP      90    // Niedrigster stabiler Wert auf Tab5 (40 kann Display underrun/crash auslösen)
#define FPS_HIGH            60    // 60 FPS für flüssige Bedienung
#define FPS_LOW             10    // 10 FPS zum Stromsparen
#define FPS_SLEEP           1     // 1 FPS im Display-Sleep (nur für Touch-Erkennung)
#define IDLE_TIMEOUT_MS     3000  // 3 Sekunden High-Performance nach letztem Touch

// Power Manager - Verwaltet Energiemodi und Performance
class PowerManager {
public:
  // Initialisierung
  void init();

  // Performance-Modus setzen
  void setHighPerformance(bool enable);

  // Status
  bool isHighPerformance() const { return is_high_performance; }

  // Update-Schleife (prüft Idle-Timeout)
  void update(uint32_t last_activity_time);

  // Display-Sleep-Modus verwalten (Display aus, CPU minimal, Touch bleibt aktiv)
  void enterDisplaySleep();
  void wakeFromDisplaySleep();
  bool isInSleep() const { return is_display_sleeping; }
  bool isTouchWakeEnabled() const;
  bool isImuWakeEnabled() const;
  bool detectAutoRotation(bool* rotated_out);

  // Schlaf sperren/freigeben (z.B. Web-Admin aktiv)
  void blockSleep(uint32_t duration_ms = 0);  // 0 = unbegrenzt, sonst Timeout
  void allowSleep();
  bool isSleepBlocked() const;

  // Ermittelt aktuelles Sleep-Timeout basierend auf Batterie/Netzteil
  uint32_t getSleepTimeout() const;

  // Prüft ob am Netzteil (true) oder Batterie (false)
  bool isPoweredByMains() const;

  // Power Mode Update (WiFi Power Saving, etc.)
  void updatePowerMode();

private:
  void applyCpuFrequency(uint16_t mhz);
  void serviceImuWake();
  bool ensureImuReady();
  bool last_power_mode = true;  // true = Netzteil, false = Batterie
  bool is_high_performance = true;
  bool is_display_sleeping = false;
  uint8_t saved_brightness = 150;  // Gespeicherte Helligkeit vor Sleep
  lv_display_t* disp = nullptr;  // Referenz zum Display (für FPS-Anpassung)
  uint16_t last_cpu_mhz = 0;
  bool sleep_blocked = false;
  uint32_t sleep_block_until = 0; // millis-Deadline, 0 = keine Deadline
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

// Globale Instanz
extern PowerManager powerManager;

#endif // POWER_MANAGER_H

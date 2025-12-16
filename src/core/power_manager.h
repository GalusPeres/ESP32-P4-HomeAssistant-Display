#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>

// ========== Power Management Einstellungen ==========
#define CPU_FREQ_HIGH       360   // Ultra-schnell bei Touch
#define CPU_FREQ_LOW        80    // Stromsparen bei Inaktivität
#define CPU_FREQ_SLEEP      80    // MUSS mindestens 80 MHz sein wegen MIPI-DSI DMA!
#define FPS_HIGH            60    // 60 FPS für flüssige Bedienung
#define FPS_LOW             10    // 10 FPS zum Stromsparen
#define FPS_SLEEP           1     // 1 FPS im Display-Sleep (nur für Touch-Erkennung)
#define IDLE_TIMEOUT_MS     3000  // 3 Sekunden High-Performance nach letztem Touch
#define SLEEP_TIMEOUT_MS_BATTERY 3000 // 30 Sekunden im Batteriebetrieb (fest)

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
  bool last_power_mode = true;  // true = Netzteil, false = Batterie
  bool is_high_performance = true;
  bool is_display_sleeping = false;
  uint8_t saved_brightness = 150;  // Gespeicherte Helligkeit vor Sleep
  lv_display_t* disp = nullptr;  // Referenz zum Display (für FPS-Anpassung)
  bool sleep_blocked = false;
  uint32_t sleep_block_until = 0; // millis-Deadline, 0 = keine Deadline
};

// Globale Instanz
extern PowerManager powerManager;

#endif // POWER_MANAGER_H

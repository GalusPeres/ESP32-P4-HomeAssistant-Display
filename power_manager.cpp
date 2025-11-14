#include "power_manager.h"
#include <M5Unified.h>
#include "display_manager.h"
#include "config_manager.h"
#include "network_manager.h"

// Globale Instanz
PowerManager powerManager;

// ========== Initialisierung ==========
void PowerManager::init() {
  Serial.println("⚡ Initialisiere Power Manager...");

  // CPU-Frequenz auf Maximum für schnelle Initialisierung
  setCpuFrequencyMhz(CPU_FREQ_HIGH);
  Serial.printf("✓ CPU Freq: %d MHz\n", getCpuFrequencyMhz());

  // Display-Referenz holen
  disp = displayManager.getDisplay();

  is_high_performance = true;

  Serial.printf("⚡ High-Performance: %d MHz, %d FPS\n",
                CPU_FREQ_HIGH, FPS_HIGH);
  Serial.printf("💤 Power-Saving: %d MHz, %d FPS\n",
                CPU_FREQ_LOW, FPS_LOW);
  Serial.printf("😴 Display-Sleep: %d MHz, %d FPS, Display AUS\n",
                CPU_FREQ_SLEEP, FPS_SLEEP);
}

// ========== Performance-Modus setzen ==========
void PowerManager::setHighPerformance(bool enable) {
  if (enable == is_high_performance) return;

  is_high_performance = enable;

  if (enable) {
    // ULTRA-SCHNELL bei Touch!
    setCpuFrequencyMhz(CPU_FREQ_HIGH);

#if LVGL_VERSION_MAJOR >= 9
    if (disp && lv_display_get_refr_timer) {
      lv_timer_t *rt = lv_display_get_refr_timer(disp);
      if (rt) lv_timer_set_period(rt, 1000 / FPS_HIGH);  // 60 FPS
    }
#endif
    Serial.println("⚡ HIGH PERFORMANCE MODE (CPU + FPS)");

  } else {
    // Stromsparen bei Inaktivität (nur CPU + FPS, Helligkeit bleibt unverändert)
    setCpuFrequencyMhz(CPU_FREQ_LOW);

#if LVGL_VERSION_MAJOR >= 9
    if (disp && lv_display_get_refr_timer) {
      lv_timer_t *rt = lv_display_get_refr_timer(disp);
      if (rt) lv_timer_set_period(rt, 1000 / FPS_LOW);  // 10 FPS
    }
#endif
    Serial.println("💤 POWER SAVING MODE (CPU + FPS)");
  }
}

// ========== Ermittelt Sleep-Timeout basierend auf Batterie/Netzteil ==========
uint32_t PowerManager::getSleepTimeout() const {
  // Nutze Stromrichtung für zuverlässige Detection
  int32_t batCurrent = M5.Power.getBatteryCurrent();
  int batVoltage = M5.Power.getBatteryVoltage();
  int batLevel = M5.Power.getBatteryLevel();

  // Debug: Einmal beim Start ausgeben
  static bool first_call = true;
  if (first_call) {
    Serial.printf("🔌 Power Status:\n");
    Serial.printf("   Current: %d mA\n", batCurrent);
    Serial.printf("   Voltage: %d mV\n", batVoltage);
    Serial.printf("   Level: %d%%\n", batLevel);
    first_call = false;
  }

  // Stromrichtung: NEGATIV = lädt (Netzteil), ~0 mA = kein Akku/Netzteil, POSITIV = entlädt (Batterie)
  bool isPowered = (batCurrent <= 50);

  if (!isPowered) {
    // Batteriebetrieb: Fest 30 Sekunden für maximale Akkulaufzeit
    return SLEEP_TIMEOUT_MS_BATTERY;
  } else {
    // Am Netzteil: User-Einstellung aus Config verwenden
    const DeviceConfig& cfg = configManager.getConfig();

    // Wenn Auto-Sleep deaktiviert, nie schlafen (sehr hoher Wert)
    if (!cfg.auto_sleep_enabled) {
      return 0xFFFFFFFF;  // Praktisch unendlich
    }

    // Minuten in Millisekunden umrechnen
    uint32_t timeout_ms = cfg.auto_sleep_minutes * 60 * 1000UL;
    return timeout_ms;
  }
}

// ========== Prüft ob am Netzteil ==========
bool PowerManager::isPoweredByMains() const {
  int32_t batCurrent = M5.Power.getBatteryCurrent();
  // Stromrichtung: NEGATIV = lädt (Netzteil), ~0 mA = kein Akku/Netzteil, POSITIV = entlädt (Batterie)
  return (batCurrent <= 50);
}

// ========== Update (Idle-Detection) ==========
void PowerManager::update(uint32_t last_activity_time) {
  uint32_t now = millis();

  // Wenn im Display-Sleep, nichts tun
  if (is_display_sleeping) return;

  // Prüfen ob Idle-Timeout erreicht
  if (is_high_performance && (now - last_activity_time > IDLE_TIMEOUT_MS)) {
    setHighPerformance(false);  // Stromsparen aktivieren
  }

  // Prüfen ob Sleep-Timeout erreicht (dynamisch je nach Batterie/Netzteil)
  uint32_t sleep_timeout = getSleepTimeout();
  if (!is_high_performance && (now - last_activity_time > sleep_timeout)) {
    enterDisplaySleep();
  }
}

// ========== Display Sleep Modus ==========
// Display aus, CPU minimal (40 MHz), LVGL gestoppt
void PowerManager::enterDisplaySleep() {
  if (is_display_sleeping) return;

  Serial.println("\n😴 Gehe in Display-Sleep Modus...");

  // Aktuelle Helligkeit speichern
  saved_brightness = M5.Display.getBrightness();

  // Touch-Buffer leeren um False-Wake zu verhindern
  M5.update();  // Touch Events verarbeiten
  while(M5.Touch.getCount() > 0) {
    M5.update();
    delay(10);
  }

  // LVGL Rendering komplett stoppen
#if LVGL_VERSION_MAJOR >= 9
  if (disp && lv_display_get_refr_timer) {
    lv_timer_t *rt = lv_display_get_refr_timer(disp);
    if (rt) {
      lv_timer_pause(rt);  // Timer pausieren statt langsam laufen lassen
    }
  }
#endif

  // Warten bis LVGL fertig ist mit Rendern
  delay(100);

  // Display komplett ausschalten
  M5.Display.setBrightness(0);
  // NICHT M5.Display.sleep() - dann funktioniert Touch nicht mehr!
  // M5.Display.sleep();  // Display in Sleep-Modus

  // CPU auf Minimum
  setCpuFrequencyMhz(CPU_FREQ_SLEEP);

  is_display_sleeping = true;
  is_high_performance = false;

  Serial.println("😴 Display-Sleep aktiv - Touch zum Aufwachen");
}

void PowerManager::wakeFromDisplaySleep() {
  if (!is_display_sleeping) return;

  Serial.println("\n⚡ Aufgewacht aus Display-Sleep!");

  is_display_sleeping = false;

  // Schritt 1: CPU auf Maximum hochfahren - LÄNGER WARTEN!
  Serial.println("  1. CPU auf 360 MHz...");
  setCpuFrequencyMhz(CPU_FREQ_HIGH);
  delay(200);  // Verdoppelt auf 200ms für stabile Frequenz
  Serial.printf("     CPU: %d MHz\n", getCpuFrequencyMhz());

  // Schritt 2: Display einschalten (BEVOR LVGL läuft!)
  Serial.println("  2. Display einschalten...");
  M5.Display.setBrightness(saved_brightness);
  delay(150);  // Display braucht Zeit um stabil zu werden

  // Schritt 3: High-Performance aktivieren
  is_high_performance = true;

  // Schritt 4: LVGL mit niedriger FPS starten
  Serial.println("  3. LVGL mit 10 FPS starten...");
#if LVGL_VERSION_MAJOR >= 9
  if (disp && lv_display_get_refr_timer) {
    lv_timer_t *rt = lv_display_get_refr_timer(disp);
    if (rt) {
      lv_timer_set_period(rt, 1000 / FPS_LOW);  // 10 FPS setzen
      lv_timer_resume(rt);  // DANN erst aktivieren
    }
  }
#endif
  delay(100);  // Ein paar Frames bei 10 FPS rendern lassen

  // Schritt 5: Auf 60 FPS hochschalten
  Serial.println("  4. Auf 60 FPS erhöhen...");
#if LVGL_VERSION_MAJOR >= 9
  if (disp && lv_display_get_refr_timer) {
    lv_timer_t *rt = lv_display_get_refr_timer(disp);
    if (rt) {
      lv_timer_set_period(rt, 1000 / FPS_HIGH);  // 60 FPS
    }
  }
#endif

  // Activity-Timer zurücksetzen
  displayManager.resetActivityTimer();

  Serial.println("✓ Display wieder aktiv - Aufwachen abgeschlossen\n");
}

// ========== Power Mode Update ==========
void PowerManager::updatePowerMode() {
  bool is_powered = isPoweredByMains();

  // Nur bei Wechsel zwischen Modi reagieren
  if (is_powered != last_power_mode) {
    last_power_mode = is_powered;

    if (is_powered) {
      // NETZTEIL-MODUS: Volle Performance
      networkManager.setWifiPowerSaving(false);
      Serial.println("⚡ Netzteil-Modus: WiFi Full Power");
    } else {
      // BATTERIE-MODUS: Stromsparen
      networkManager.setWifiPowerSaving(true);
      Serial.println("🔋 Batterie-Modus: WiFi Power Saving");
    }
  }
}

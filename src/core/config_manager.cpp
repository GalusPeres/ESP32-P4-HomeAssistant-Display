#include "src/core/config_manager.h"
#include <Preferences.h>

// Globale Instanz
ConfigManager configManager;

// Preferences namespace
static const char* PREF_NAMESPACE = "tab5_config";

static uint16_t normalize_sleep_seconds(uint16_t seconds) {
  uint16_t closest = kSleepOptionsSec[0];
  uint16_t best_diff = (seconds > closest) ? (seconds - closest) : (closest - seconds);
  for (size_t i = 1; i < kSleepOptionsSecCount; ++i) {
    uint16_t option = kSleepOptionsSec[i];
    uint16_t diff = (seconds > option) ? (seconds - option) : (option - seconds);
    if (diff < best_diff) {
      best_diff = diff;
      closest = option;
    }
  }
  return closest;
}

ConfigManager::ConfigManager() {
  memset(&config, 0, sizeof(config));
  config.configured = false;
  config.mqtt_port = 1883;  // Default MQTT Port
  strncpy(config.mqtt_base_topic, "tab5", CONFIG_MQTT_BASE_MAX - 1);
  strncpy(config.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);

  // Display & Power Defaults
  config.display_brightness = 200;
  config.auto_sleep_enabled = true;
  config.auto_sleep_seconds = 60;
  config.auto_sleep_battery_enabled = true;
  config.auto_sleep_battery_seconds = config.auto_sleep_seconds;
}

bool ConfigManager::load() {
  Preferences prefs;

  if (!prefs.begin(PREF_NAMESPACE, true)) {  // readonly
    Serial.println("⚠️ ConfigManager: Preferences öffnen fehlgeschlagen");
    return false;
  }

  // Prüfe ob Konfiguration vorhanden ist
  config.configured = prefs.getBool("configured", false);

  if (!config.configured) {
    Serial.println("ℹ️ ConfigManager: Keine Konfiguration vorhanden");
    prefs.end();
    return false;
  }

  // Lade Konfigurationsdaten
  prefs.getString("wifi_ssid", config.wifi_ssid, CONFIG_WIFI_SSID_MAX);
  prefs.getString("wifi_pass", config.wifi_pass, CONFIG_WIFI_PASS_MAX);
  prefs.getString("mqtt_host", config.mqtt_host, CONFIG_MQTT_HOST_MAX);
  config.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  prefs.getString("mqtt_user", config.mqtt_user, CONFIG_MQTT_USER_MAX);
  prefs.getString("mqtt_pass", config.mqtt_pass, CONFIG_MQTT_PASS_MAX);
  prefs.getString("mqtt_base", config.mqtt_base_topic, CONFIG_MQTT_BASE_MAX);
  prefs.getString("ha_prefix", config.ha_prefix, CONFIG_HA_PREFIX_MAX);

  // Display & Power Settings laden
  config.display_brightness = prefs.getUChar("disp_bright", 200);
  config.auto_sleep_enabled = prefs.getBool("sleep_en", true);
  uint16_t sleep_seconds = 60;
  if (prefs.isKey("sleep_sec")) {
    sleep_seconds = prefs.getUShort("sleep_sec", 60);
  } else {
    uint16_t sleep_minutes = prefs.getUShort("sleep_min", 1);
    if (sleep_minutes == 0) {
      sleep_minutes = 1;
    }
    sleep_seconds = sleep_minutes * 60;
  }
  config.auto_sleep_seconds = normalize_sleep_seconds(sleep_seconds);
  config.auto_sleep_battery_enabled = prefs.isKey("sleep_bat_en")
                                      ? prefs.getBool("sleep_bat_en", config.auto_sleep_enabled)
                                      : config.auto_sleep_enabled;
  uint16_t sleep_bat_seconds = config.auto_sleep_seconds;
  if (prefs.isKey("sleep_bat_sec")) {
    sleep_bat_seconds = prefs.getUShort("sleep_bat_sec", config.auto_sleep_seconds);
  } else if (prefs.isKey("sleep_bat_min")) {
    uint16_t sleep_bat_minutes = prefs.getUShort("sleep_bat_min", (sleep_seconds / 60));
    if (sleep_bat_minutes == 0) {
      sleep_bat_minutes = 1;
    }
    sleep_bat_seconds = sleep_bat_minutes * 60;
  }
  config.auto_sleep_battery_seconds = normalize_sleep_seconds(sleep_bat_seconds);

  if (config.display_brightness < 75 || config.display_brightness > 255) {
    config.display_brightness = 200;
  }

  // Fallback: 0 Minuten korrigieren (kann durch ungültige Speicherung entstehen)
  if (config.mqtt_base_topic[0] == '\0') {
    strncpy(config.mqtt_base_topic, "tab5", CONFIG_MQTT_BASE_MAX - 1);
  }
  if (config.ha_prefix[0] == '\0') {
    strncpy(config.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);
  }

  prefs.end();

  Serial.println("✓ ConfigManager: Konfiguration geladen");
  Serial.printf("  WiFi SSID: %s\n", config.wifi_ssid);
  Serial.printf("  MQTT Host: %s:%u\n", config.mqtt_host, config.mqtt_port);
  Serial.printf("  MQTT User: %s\n", config.mqtt_user);

  return true;
}

bool ConfigManager::save(const DeviceConfig& cfg) {
  Preferences prefs;

  if (!prefs.begin(PREF_NAMESPACE, false)) {  // read/write
    Serial.println("⚠️ ConfigManager: Preferences öffnen fehlgeschlagen");
    return false;
  }

  // Speichere alle Felder
  prefs.putString("wifi_ssid", cfg.wifi_ssid);
  prefs.putString("wifi_pass", cfg.wifi_pass);
  prefs.putString("mqtt_host", cfg.mqtt_host);
  prefs.putUShort("mqtt_port", cfg.mqtt_port);
  prefs.putString("mqtt_user", cfg.mqtt_user);
  prefs.putString("mqtt_pass", cfg.mqtt_pass);
  prefs.putString("mqtt_base", cfg.mqtt_base_topic);
  prefs.putString("ha_prefix", cfg.ha_prefix);

  // Display & Power Settings speichern
  prefs.putUChar("disp_bright", cfg.display_brightness);
  prefs.putBool("sleep_en", cfg.auto_sleep_enabled);
  prefs.putUShort("sleep_sec", cfg.auto_sleep_seconds);
  prefs.putBool("sleep_bat_en", cfg.auto_sleep_battery_enabled);
  prefs.putUShort("sleep_bat_sec", cfg.auto_sleep_battery_seconds);

  uint16_t sleep_minutes = (cfg.auto_sleep_seconds + 59) / 60;
  if (sleep_minutes == 0) {
    sleep_minutes = 1;
  }
  prefs.putUShort("sleep_min", sleep_minutes);

  uint16_t sleep_bat_minutes = (cfg.auto_sleep_battery_seconds + 59) / 60;
  if (sleep_bat_minutes == 0) {
    sleep_bat_minutes = 1;
  }
  prefs.putUShort("sleep_bat_min", sleep_bat_minutes);

  prefs.putBool("configured", true);

  prefs.end();

  // Update lokale Kopie
  config = cfg;
  config.configured = true;

  Serial.println("✓ ConfigManager: Konfiguration gespeichert");
  Serial.printf("  WiFi SSID: %s\n", config.wifi_ssid);
  Serial.printf("  MQTT Host: %s:%u\n", config.mqtt_host, config.mqtt_port);

  return true;
}

bool ConfigManager::saveDisplaySettings(uint8_t brightness,
                                        bool sleep_enabled,
                                        uint16_t sleep_seconds,
                                        bool sleep_battery_enabled,
                                        uint16_t sleep_battery_seconds) {
  Preferences prefs;

  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("⚠️ ConfigManager: Preferences öffnen fehlgeschlagen");
    return false;
  }

  // Speichere nur Display-Settings
  uint16_t normalized_sleep_seconds = normalize_sleep_seconds(sleep_seconds);
  uint16_t normalized_bat_seconds = normalize_sleep_seconds(sleep_battery_seconds);

  prefs.putUChar("disp_bright", brightness);
  prefs.putBool("sleep_en", sleep_enabled);
  prefs.putUShort("sleep_sec", normalized_sleep_seconds);
  prefs.putBool("sleep_bat_en", sleep_battery_enabled);
  prefs.putUShort("sleep_bat_sec", normalized_bat_seconds);

  uint16_t sleep_minutes = (normalized_sleep_seconds + 59) / 60;
  if (sleep_minutes == 0) {
    sleep_minutes = 1;
  }
  prefs.putUShort("sleep_min", sleep_minutes);

  uint16_t sleep_bat_minutes = (normalized_bat_seconds + 59) / 60;
  if (sleep_bat_minutes == 0) {
    sleep_bat_minutes = 1;
  }
  prefs.putUShort("sleep_bat_min", sleep_bat_minutes);

  prefs.end();

  // Update lokale Kopie
  config.display_brightness = brightness;
  config.auto_sleep_enabled = sleep_enabled;
  config.auto_sleep_seconds = normalized_sleep_seconds;
  config.auto_sleep_battery_enabled = sleep_battery_enabled;
  config.auto_sleep_battery_seconds = normalized_bat_seconds;

  Serial.println("✓ ConfigManager: Display-Einstellungen gespeichert");
  return true;
}

void ConfigManager::clear() {
  Preferences prefs;

  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("⚠️ ConfigManager: Preferences öffnen fehlgeschlagen");
    return;
  }

  prefs.clear();
  prefs.end();

  memset(&config, 0, sizeof(config));
  config.configured = false;
  config.mqtt_port = 1883;
  strncpy(config.mqtt_base_topic, "tab5", CONFIG_MQTT_BASE_MAX - 1);
  strncpy(config.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);

  Serial.println("✓ ConfigManager: Konfiguration gelöscht");
}

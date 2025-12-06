#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

// WiFi/MQTT Configuration Manager
// Speichert und lädt Verbindungsdaten aus dem Flash-Speicher (Preferences)

#define CONFIG_WIFI_SSID_MAX     32
#define CONFIG_WIFI_PASS_MAX     64
#define CONFIG_MQTT_HOST_MAX     64
#define CONFIG_MQTT_USER_MAX     32
#define CONFIG_MQTT_PASS_MAX     64
#define CONFIG_MQTT_BASE_MAX     32
#define CONFIG_HA_PREFIX_MAX     48

struct DeviceConfig {
  char wifi_ssid[CONFIG_WIFI_SSID_MAX];
  char wifi_pass[CONFIG_WIFI_PASS_MAX];
  char mqtt_host[CONFIG_MQTT_HOST_MAX];
  uint16_t mqtt_port;
  char mqtt_user[CONFIG_MQTT_USER_MAX];
  char mqtt_pass[CONFIG_MQTT_PASS_MAX];
  char mqtt_base_topic[CONFIG_MQTT_BASE_MAX];
  char ha_prefix[CONFIG_HA_PREFIX_MAX];
  bool configured;  // Flag ob Konfiguration vorhanden ist

  // Display & Power Settings
  uint8_t display_brightness;  // 75-255
  bool auto_sleep_enabled;     // Auto-Sleep aktiv?
  uint16_t auto_sleep_minutes; // Minuten bis Auto-Sleep (1-60)
};

class ConfigManager {
public:
  ConfigManager();

  // Lädt Konfiguration aus Flash-Speicher
  bool load();

  // Speichert Konfiguration in Flash-Speicher
  bool save(const DeviceConfig& cfg);

  // Speichert nur Display-Einstellungen
  bool saveDisplaySettings(uint8_t brightness, bool sleep_enabled, uint16_t sleep_minutes);

  // Löscht gespeicherte Konfiguration
  void clear();

  // Prüft ob eine gültige Konfiguration vorhanden ist
  bool isConfigured() const { return config.configured; }
  bool hasWifiCredentials() const { return config.wifi_ssid[0] != '\0'; }
  bool hasMqttConfig() const { return config.mqtt_host[0] != '\0'; }

  // Getter für Config-Daten
  const DeviceConfig& getConfig() const { return config; }

private:
  DeviceConfig config;
};

// Globale Instanz
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H

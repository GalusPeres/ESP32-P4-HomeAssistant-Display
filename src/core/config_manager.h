#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

#include "src/devices/device.h"

// WiFi/MQTT Configuration Manager
// Speichert und lädt Verbindungsdaten aus dem Flash-Speicher (Preferences)

#define CONFIG_WIFI_SSID_MAX     32
#define CONFIG_WIFI_PASS_MAX     64
#define CONFIG_IP_ADDR_MAX       16
#define CONFIG_MQTT_HOST_MAX     64
#define CONFIG_MQTT_USER_MAX     32
#define CONFIG_MQTT_PASS_MAX     64
#define CONFIG_MQTT_CLIENT_ID_MAX 64
#define CONFIG_MQTT_BASE_MAX     32
#define CONFIG_HA_PREFIX_MAX     48
#define CONFIG_LANG_MAX          8
#define CONFIG_TIMEZONE_MAX      24

static constexpr uint16_t kSleepOptionsSec[] = {5, 15, 30, 60, 300, 900, 1800, 3600};
static constexpr size_t kSleepOptionsSecCount = sizeof(kSleepOptionsSec) / sizeof(kSleepOptionsSec[0]);

static constexpr uint8_t kDisplayRotationNormal = 0;
static constexpr uint8_t kDisplayRotationFlipped = 1;
static constexpr uint8_t kDisplayRotationAuto = 2;
static constexpr uint8_t kWakeModeTouch = 0;
static constexpr uint8_t kWakeModeImu = 1;
static constexpr uint8_t kScreensaverBrightnessPctMin = 1;
static constexpr uint8_t kScreensaverBrightnessPctMax = 100;
static constexpr uint8_t kScreensaverBrightnessPctDefault = 25;

struct DeviceConfig {
  char wifi_ssid[CONFIG_WIFI_SSID_MAX];
  char wifi_pass[CONFIG_WIFI_PASS_MAX];
  char wifi_static_ip[CONFIG_IP_ADDR_MAX];
  char wifi_gateway[CONFIG_IP_ADDR_MAX];
  char wifi_subnet[CONFIG_IP_ADDR_MAX];
  char wifi_dns[CONFIG_IP_ADDR_MAX];
  // Gemeinsame IP-Konfiguration fuer WLAN und Ethernet. Die wifi_-
  // Feldnamen bleiben aus Kompatibilitaetsgruenden zu bestehenden Daten.
  bool wifi_static_enabled;
  char mqtt_host[CONFIG_MQTT_HOST_MAX];
  uint16_t mqtt_port;
  char mqtt_user[CONFIG_MQTT_USER_MAX];
  char mqtt_pass[CONFIG_MQTT_PASS_MAX];
  char mqtt_client_id[CONFIG_MQTT_CLIENT_ID_MAX];
  char mqtt_base_topic[CONFIG_MQTT_BASE_MAX];
  char ha_prefix[CONFIG_HA_PREFIX_MAX];
  char language[CONFIG_LANG_MAX];
  char timezone[CONFIG_TIMEZONE_MAX];
  uint8_t global_time_format;
  uint8_t global_date_format;
  uint8_t keyboard_layout;  // 0=Auto (folgt Sprache), 1=Deutsch QWERTZ, 2=English QWERTY
  bool configured;  // Flag ob Konfiguration vorhanden ist

  // Display & Power Settings
  uint8_t display_brightness;  // Geraetespezifischer Rohwert: backlight_input_min..255
  // Einheitlicher sichtbarer Prozentwert. Die Umrechnung auf den jeweiligen
  // Treiberbereich erfolgt ueber Device::backlightRawFromPercent().
  uint8_t screensaver_brightness_pct;  // 1-100
  bool tile_borders;           // Feine Rahmen um normale Dashboard-Kacheln
  bool display_rotated_180;    // Display 180 deg gedreht?
  uint8_t display_rotation_quarters; // 0=0°, 1=90°, 2=180°, 3=270°
  uint8_t display_rotation_mode; // 0=Normal, 1=180, 2=Auto
  uint8_t wake_mode_mains;       // 0=Touch, 1=IMU
  uint8_t wake_mode_battery;     // 0=Touch, 1=IMU
  bool auto_sleep_enabled;     // Auto-Sleep aktiv?
  uint16_t auto_sleep_seconds; // Seconds until auto-sleep (5-3600)
  bool auto_screensaver_enabled;     // Screensaver nach Inaktivitaet?
  uint16_t auto_screensaver_seconds; // Seconds until screensaver (5-3600)
  bool auto_sleep_battery_enabled;     // Auto-Sleep aktiv im Batteriebetrieb?
  uint16_t auto_sleep_battery_seconds; // Seconds until auto-sleep (5-3600)
  uint8_t status_time_font_size;      // 24 or 48
  uint8_t status_date_font_size;      // 20 or 24

  // Fester Netzwerkmodus: false = WLAN (Default), true = Ethernet. Wird nur
  // beim Boot ausgewertet - im Ethernet-Modus startet WLAN/ESP-Hosted nie,
  // damit sich beide Stacks nicht das interne DMA-RAM streitig machen.
  bool ethernet_enabled;
};

class ConfigManager {
public:
  ConfigManager();

  // Lädt Konfiguration aus Flash-Speicher
  bool load();

  // Speichert Konfiguration in Flash-Speicher
  bool save(const DeviceConfig& cfg);

  // Speichert nur Display-Einstellungen
  bool saveDisplaySettings(uint8_t brightness,
                           bool sleep_enabled,
                           uint16_t sleep_seconds,
                           bool sleep_battery_enabled,
                           uint16_t sleep_battery_seconds,
                           uint8_t rotation_mode,
                           bool rotate_180,
                           uint8_t rotation_quarters,
                           uint8_t wake_mode_mains,
                           uint8_t wake_mode_battery);

  bool saveScreensaverTimeout(bool enabled, uint16_t seconds);
  bool saveScreensaverBrightness(uint8_t brightness_pct);
  bool saveTileBorders(bool enabled);
  bool saveEthernetEnabled(bool enabled);
  bool saveStaticAddressingEnabled(bool enabled);

  // Entfernt die gemeinsame statische Adressierung. Der Netzwerkmodus bleibt.
  bool clearStaticAddressing();

  void setRuntimeDisplayRotation(bool rotate_180);
  void setRuntimeDisplayRotationQuarters(uint8_t rotation_quarters);

  // Löscht gespeicherte Konfiguration
  void clear();

  // Prüft ob eine gültige Konfiguration vorhanden ist
  bool isConfigured() const { return config.configured; }
  bool hasWifiCredentials() const { return config.wifi_ssid[0] != '\0'; }
  bool hasMqttConfig() const { return config.mqtt_host[0] != '\0'; }
  bool bootStaticAddressingEnabled() const { return boot_static_enabled; }

  // Getter für Config-Daten
  const DeviceConfig& getConfig() const { return config; }

private:
  DeviceConfig config;
  bool boot_static_enabled = false;
  // Rotation can be applied to the live display before the settings save is
  // issued. Keep that runtime mutation from being mistaken for an NVS no-op.
  bool runtime_rotation_dirty = false;
};

// Globale Instanz
extern ConfigManager configManager;

#endif // CONFIG_MANAGER_H

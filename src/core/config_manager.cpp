#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/types/clock/clock_format.h"
#include <Preferences.h>
#include <string.h>

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

static uint8_t rotation_quarters_from_legacy(bool rotate_180) {
  return rotate_180 ? Device::kRotationFlipped : Device::kRotationDefault;
}

static uint8_t normalize_rotation_quarters(uint8_t rotation_quarters) {
  return Device::normalizeRotationQuarterTurns(rotation_quarters);
}

static uint8_t rotation_mode_from_quarters(uint8_t rotation_quarters, uint8_t fallback_mode) {
  if (fallback_mode == kDisplayRotationAuto) {
    return kDisplayRotationAuto;
  }
  return (rotation_quarters == Device::kRotationFlipped)
             ? kDisplayRotationFlipped
             : kDisplayRotationNormal;
}

static void apply_device_capability_limits(DeviceConfig& cfg) {
  if (!Device::kCapabilities.supports_auto_rotation &&
      cfg.display_rotation_mode == kDisplayRotationAuto) {
    cfg.display_rotation_mode =
        (cfg.display_rotation_quarters == Device::kRotationFlipped)
            ? kDisplayRotationFlipped
            : kDisplayRotationNormal;
  }

  if (!Device::kCapabilities.has_imu) {
    cfg.wake_mode_mains = kWakeModeTouch;
    cfg.wake_mode_battery = kWakeModeTouch;
  }

  if (!Device::kCapabilities.supports_battery_sleep_profile) {
    cfg.auto_sleep_battery_enabled = cfg.auto_sleep_enabled;
    cfg.auto_sleep_battery_seconds = cfg.auto_sleep_seconds;
  }
}

static void set_language_code(char* dst, size_t dst_size, const char* language_code) {
  if (!dst || dst_size == 0) return;
  const char* normalized = i18n::normalize_language_code(language_code);
  strncpy(dst, normalized, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

static const char* normalize_timezone_code(const char* timezone_code) {
  if (!timezone_code || !timezone_code[0]) return "berlin";
  struct KnownTimezone {
    const char* code;
  };
  static const KnownTimezone kKnownTimezones[] = {
      {"berlin"},
      {"london"},
      {"utc"},
      {"athens"},
      {"istanbul"},
      {"moscow"},
      {"johannesburg"},
      {"nairobi"},
      {"dubai"},
      {"karachi"},
      {"kolkata"},
      {"dhaka"},
      {"bangkok"},
      {"singapore"},
      {"perth"},
      {"new_york"},
      {"chicago"},
      {"denver"},
      {"phoenix"},
      {"los_angeles"},
      {"honolulu"},
      {"buenos_aires"},
      {"sao_paulo"},
      {"tokyo"},
      {"darwin"},
      {"sydney"},
      {"auckland"},
  };
  for (const auto& tz : kKnownTimezones) {
    if (strcasecmp(timezone_code, tz.code) == 0) {
      return tz.code;
    }
  }
  return "berlin";
}

static void set_timezone_code(char* dst, size_t dst_size, const char* timezone_code) {
  if (!dst || dst_size == 0) return;
  const char* normalized = normalize_timezone_code(timezone_code);
  strncpy(dst, normalized, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

static uint8_t normalize_global_time_format(int raw) {
  return clock_tile::normalize_time_format(raw);
}

static uint8_t normalize_global_date_format(int raw) {
  return clock_tile::normalize_date_format(raw);
}

ConfigManager::ConfigManager() {
  memset(&config, 0, sizeof(config));
  config.configured = false;
  config.mqtt_port = 1883;  // Default MQTT Port
  strncpy(config.mqtt_base_topic, "hometiles", CONFIG_MQTT_BASE_MAX - 1);
  strncpy(config.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);
  set_language_code(config.language, sizeof(config.language), "en");
  set_timezone_code(config.timezone, sizeof(config.timezone), "berlin");
  config.global_time_format = clock_tile::TIME_FORMAT_AUTO;
  config.global_date_format = clock_tile::DATE_FORMAT_AUTO;
  config.keyboard_layout = 0;  // Auto (folgt Sprache)

  // Display & Power Defaults
  config.display_brightness = 200;
  config.screensaver_brightness_pct = kScreensaverBrightnessPctDefault;
  config.tile_borders = true;
  config.display_rotated_180 = false;
  config.display_rotation_quarters = Device::kRotationDefault;
  config.display_rotation_mode = kDisplayRotationNormal;
  config.wake_mode_mains = kWakeModeTouch;
  config.wake_mode_battery = kWakeModeTouch;
  config.auto_sleep_enabled = true;
  config.auto_sleep_seconds = 60;
  config.auto_screensaver_enabled = false;
  config.auto_screensaver_seconds = 60;
  config.auto_sleep_battery_enabled = config.auto_sleep_enabled;
  config.auto_sleep_battery_seconds = config.auto_sleep_seconds;
  config.status_time_font_size = 48;
  config.status_date_font_size = 24;
  config.ethernet_enabled = false;
  config.wifi_static_enabled = false;
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
  config.ethernet_enabled = prefs.getBool("eth_mode", false);
  prefs.getString("wifi_ssid", config.wifi_ssid, CONFIG_WIFI_SSID_MAX);
  prefs.getString("wifi_pass", config.wifi_pass, CONFIG_WIFI_PASS_MAX);
  prefs.getString("wifi_ip", config.wifi_static_ip, CONFIG_IP_ADDR_MAX);
  prefs.getString("wifi_gw", config.wifi_gateway, CONFIG_IP_ADDR_MAX);
  prefs.getString("wifi_subnet", config.wifi_subnet, CONFIG_IP_ADDR_MAX);
  prefs.getString("wifi_dns", config.wifi_dns, CONFIG_IP_ADDR_MAX);
  const bool legacy_wifi_static =
      config.wifi_static_ip[0] || config.wifi_gateway[0] ||
      config.wifi_subnet[0] || config.wifi_dns[0];
  char legacy_eth_ip[CONFIG_IP_ADDR_MAX] = {};
  char legacy_eth_gateway[CONFIG_IP_ADDR_MAX] = {};
  char legacy_eth_subnet[CONFIG_IP_ADDR_MAX] = {};
  char legacy_eth_dns[CONFIG_IP_ADDR_MAX] = {};
  prefs.getString("eth_ip", legacy_eth_ip, sizeof(legacy_eth_ip));
  prefs.getString("eth_gw", legacy_eth_gateway, sizeof(legacy_eth_gateway));
  prefs.getString("eth_subnet", legacy_eth_subnet, sizeof(legacy_eth_subnet));
  prefs.getString("eth_dns", legacy_eth_dns, sizeof(legacy_eth_dns));
  const bool legacy_ethernet_static =
      legacy_eth_ip[0] || legacy_eth_gateway[0] ||
      legacy_eth_subnet[0] || legacy_eth_dns[0];
  const bool migrate_ethernet_values =
      legacy_ethernet_static &&
      (config.ethernet_enabled || !legacy_wifi_static);
  if (migrate_ethernet_values) {
    strncpy(config.wifi_static_ip, legacy_eth_ip, CONFIG_IP_ADDR_MAX - 1);
    strncpy(config.wifi_gateway, legacy_eth_gateway, CONFIG_IP_ADDR_MAX - 1);
    strncpy(config.wifi_subnet, legacy_eth_subnet, CONFIG_IP_ADDR_MAX - 1);
    strncpy(config.wifi_dns, legacy_eth_dns, CONFIG_IP_ADDR_MAX - 1);
    config.wifi_static_ip[CONFIG_IP_ADDR_MAX - 1] = '\0';
    config.wifi_gateway[CONFIG_IP_ADDR_MAX - 1] = '\0';
    config.wifi_subnet[CONFIG_IP_ADDR_MAX - 1] = '\0';
    config.wifi_dns[CONFIG_IP_ADDR_MAX - 1] = '\0';
  }
  const bool legacy_static_enabled =
      migrate_ethernet_values
          ? prefs.getBool("eth_static", legacy_ethernet_static)
          : prefs.getBool("wifi_static", legacy_wifi_static);
  config.wifi_static_enabled =
      prefs.isKey("net_static")
          ? prefs.getBool("net_static", legacy_static_enabled)
          : legacy_static_enabled;
  prefs.getString("mqtt_host", config.mqtt_host, CONFIG_MQTT_HOST_MAX);
  config.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  prefs.getString("mqtt_user", config.mqtt_user, CONFIG_MQTT_USER_MAX);
  prefs.getString("mqtt_pass", config.mqtt_pass, CONFIG_MQTT_PASS_MAX);
  prefs.getString("mqtt_client_id", config.mqtt_client_id, CONFIG_MQTT_CLIENT_ID_MAX);
  prefs.getString("mqtt_base", config.mqtt_base_topic, CONFIG_MQTT_BASE_MAX);
  prefs.getString("ha_prefix", config.ha_prefix, CONFIG_HA_PREFIX_MAX);
  char stored_language[CONFIG_LANG_MAX] = {0};
  prefs.getString("lang", stored_language, CONFIG_LANG_MAX);
  set_language_code(config.language, sizeof(config.language), stored_language);
  char stored_timezone[CONFIG_TIMEZONE_MAX] = {0};
  prefs.getString("tz", stored_timezone, CONFIG_TIMEZONE_MAX);
  set_timezone_code(config.timezone, sizeof(config.timezone), stored_timezone);
  config.global_time_format =
      normalize_global_time_format(prefs.getUChar("time_fmt", clock_tile::TIME_FORMAT_AUTO));
  config.global_date_format =
      normalize_global_date_format(prefs.getUChar("date_fmt", clock_tile::DATE_FORMAT_AUTO));
  config.keyboard_layout = prefs.getUChar("kb_layout", 0);
  if (config.keyboard_layout > 2) config.keyboard_layout = 0;

  // Display & Power Settings laden
  config.display_brightness = prefs.getUChar("disp_bright", 200);
  // Der Screensaver soll auf Bestandsgeraeten ebenfalls wirklich dimmen.
  // Den fehlenden Wert daher nicht aus der normalen Display-Helligkeit
  // ableiten (bei 100 % war die neue Funktion sonst optisch wirkungslos),
  // sondern mit der eigenen sinnvollen Voreinstellung starten.
  config.screensaver_brightness_pct =
      prefs.getUChar("ss_bright", kScreensaverBrightnessPctDefault);
  config.tile_borders = prefs.getBool("tile_border", true);
  bool rot_180 = prefs.getBool("disp_rot180", false);
  uint8_t rot_mode = rot_180 ? kDisplayRotationFlipped : kDisplayRotationNormal;
  if (prefs.isKey("disp_rot_mode")) {
    rot_mode = prefs.getUChar("disp_rot_mode", rot_mode);
  }
  if (rot_mode > kDisplayRotationAuto) {
    rot_mode = rot_180 ? kDisplayRotationFlipped : kDisplayRotationNormal;
  }
  uint8_t rotation_quarters = rotation_quarters_from_legacy(rot_180);
  if (prefs.isKey("disp_rot_q")) {
    rotation_quarters = prefs.getUChar("disp_rot_q", rotation_quarters);
  }
  rotation_quarters = normalize_rotation_quarters(rotation_quarters);
  config.display_rotation_quarters = rotation_quarters;
  config.display_rotated_180 = (rotation_quarters == Device::kRotationFlipped);
  config.display_rotation_mode = rotation_mode_from_quarters(rotation_quarters, rot_mode);
  uint8_t wake_mains = prefs.getUChar("wake_mains", config.wake_mode_mains);
  if (wake_mains > kWakeModeImu) wake_mains = config.wake_mode_mains;
  config.wake_mode_mains = wake_mains;
  uint8_t wake_battery = prefs.getUChar("wake_bat", config.wake_mode_battery);
  if (wake_battery > kWakeModeImu) wake_battery = config.wake_mode_battery;
  config.wake_mode_battery = wake_battery;
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
  config.auto_screensaver_enabled = prefs.getBool("ss_auto_en", false);
  config.auto_screensaver_seconds = normalize_sleep_seconds(
      prefs.getUShort("ss_auto_sec", 60));
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
  config.status_time_font_size = prefs.getUChar("status_time_font", 48);
  if (config.status_time_font_size != 24 && config.status_time_font_size != 48) config.status_time_font_size = 48;
  config.status_date_font_size = prefs.getUChar("status_date_font", 24);
  if (config.status_date_font_size != 20 && config.status_date_font_size != 24) config.status_date_font_size = 24;

  apply_device_capability_limits(config);
  boot_static_enabled = config.wifi_static_enabled;

  if (config.display_brightness < 121 || config.display_brightness > 255) {
    config.display_brightness = 200;
  }
  if (config.screensaver_brightness_pct < kScreensaverBrightnessPctMin ||
      config.screensaver_brightness_pct > kScreensaverBrightnessPctMax) {
    config.screensaver_brightness_pct =
        Device::backlightPercentFromRaw(config.display_brightness);
  }

  // Fallback: 0 Minuten korrigieren (kann durch ungültige Speicherung entstehen)
  if (config.mqtt_base_topic[0] == '\0') {
    strncpy(config.mqtt_base_topic, "hometiles", CONFIG_MQTT_BASE_MAX - 1);
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
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;

  if (!prefs.begin(PREF_NAMESPACE, false)) {  // read/write
    Serial.println("⚠️ ConfigManager: Preferences öffnen fehlgeschlagen");
    return false;
  }

  DeviceConfig normalized = cfg;
  normalized.display_rotation_quarters =
      normalize_rotation_quarters(normalized.display_rotation_quarters);
  normalized.display_rotated_180 =
      (normalized.display_rotation_quarters == Device::kRotationFlipped);
  normalized.display_rotation_mode = rotation_mode_from_quarters(
      normalized.display_rotation_quarters, normalized.display_rotation_mode);
  if (normalized.display_brightness < 121) normalized.display_brightness = 200;
  if (normalized.screensaver_brightness_pct < kScreensaverBrightnessPctMin ||
      normalized.screensaver_brightness_pct > kScreensaverBrightnessPctMax) {
    normalized.screensaver_brightness_pct =
        Device::backlightPercentFromRaw(normalized.display_brightness);
  }
  apply_device_capability_limits(normalized);

  // Speichere alle Felder
  prefs.putString("wifi_ssid", normalized.wifi_ssid);
  prefs.putString("wifi_pass", normalized.wifi_pass);
  prefs.putString("wifi_ip", normalized.wifi_static_ip);
  prefs.putString("wifi_gw", normalized.wifi_gateway);
  prefs.putString("wifi_subnet", normalized.wifi_subnet);
  prefs.putString("wifi_dns", normalized.wifi_dns);
  prefs.putBool("wifi_static", normalized.wifi_static_enabled);
  prefs.putBool("net_static", normalized.wifi_static_enabled);
  // Alte Test-Builds mit getrennten Profilen lesen weiterhin dieselben Werte.
  prefs.putString("eth_ip", normalized.wifi_static_ip);
  prefs.putString("eth_gw", normalized.wifi_gateway);
  prefs.putString("eth_subnet", normalized.wifi_subnet);
  prefs.putString("eth_dns", normalized.wifi_dns);
  prefs.putBool("eth_static", normalized.wifi_static_enabled);
  prefs.putString("mqtt_host", normalized.mqtt_host);
  prefs.putUShort("mqtt_port", normalized.mqtt_port);
  prefs.putString("mqtt_user", normalized.mqtt_user);
  prefs.putString("mqtt_pass", normalized.mqtt_pass);
  prefs.putString("mqtt_client_id", normalized.mqtt_client_id);
  prefs.putString("mqtt_base", normalized.mqtt_base_topic);
  prefs.putString("ha_prefix", normalized.ha_prefix);
  set_language_code(normalized.language, sizeof(normalized.language), normalized.language);
  prefs.putString("lang", normalized.language);
  set_timezone_code(normalized.timezone, sizeof(normalized.timezone), normalized.timezone);
  prefs.putString("tz", normalized.timezone);
  normalized.global_time_format = normalize_global_time_format(normalized.global_time_format);
  normalized.global_date_format = normalize_global_date_format(normalized.global_date_format);
  prefs.putUChar("time_fmt", normalized.global_time_format);
  prefs.putUChar("date_fmt", normalized.global_date_format);
  if (normalized.keyboard_layout > 2) normalized.keyboard_layout = 0;
  prefs.putUChar("kb_layout", normalized.keyboard_layout);

  // Display & Power Settings speichern
  prefs.putUChar("disp_bright", normalized.display_brightness);
  prefs.putUChar("ss_bright", normalized.screensaver_brightness_pct);
  prefs.putBool("tile_border", normalized.tile_borders);
  prefs.putBool("eth_mode", normalized.ethernet_enabled);
  prefs.putBool("disp_rot180", normalized.display_rotated_180);
  prefs.putUChar("disp_rot_q", normalized.display_rotation_quarters);
  prefs.putUChar("disp_rot_mode", normalized.display_rotation_mode);
  prefs.putUChar("wake_mains", normalized.wake_mode_mains);
  prefs.putUChar("wake_bat", normalized.wake_mode_battery);
  prefs.putBool("sleep_en", normalized.auto_sleep_enabled);
  prefs.putUShort("sleep_sec", normalized.auto_sleep_seconds);
  prefs.putBool("ss_auto_en", normalized.auto_screensaver_enabled);
  prefs.putUShort("ss_auto_sec",
                  normalize_sleep_seconds(normalized.auto_screensaver_seconds));
  prefs.putBool("sleep_bat_en", normalized.auto_sleep_battery_enabled);
  prefs.putUShort("sleep_bat_sec", normalized.auto_sleep_battery_seconds);
  prefs.putUChar("status_time_font", (normalized.status_time_font_size == 24) ? 24 : 48);
  prefs.putUChar("status_date_font", (normalized.status_date_font_size == 20) ? 20 : 24);

  uint16_t sleep_minutes = (normalized.auto_sleep_seconds + 59) / 60;
  if (sleep_minutes == 0) {
    sleep_minutes = 1;
  }
  prefs.putUShort("sleep_min", sleep_minutes);

  uint16_t sleep_bat_minutes = (normalized.auto_sleep_battery_seconds + 59) / 60;
  if (sleep_bat_minutes == 0) {
    sleep_bat_minutes = 1;
  }
  prefs.putUShort("sleep_bat_min", sleep_bat_minutes);

  prefs.putBool("configured", true);

  prefs.end();

  // Update lokale Kopie
  config = normalized;
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
                                        uint16_t sleep_battery_seconds,
                                        uint8_t rotation_mode,
                                        bool rotate_180,
                                        uint8_t rotation_quarters,
                                        uint8_t wake_mode_mains,
                                        uint8_t wake_mode_battery) {
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;

  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("⚠️ ConfigManager: Preferences öffnen fehlgeschlagen");
    return false;
  }

  // Speichere nur Display-Settings
  uint16_t normalized_sleep_seconds = normalize_sleep_seconds(sleep_seconds);
  uint16_t normalized_bat_seconds = normalize_sleep_seconds(sleep_battery_seconds);
  rotation_quarters = normalize_rotation_quarters(rotation_quarters);
  rotate_180 = (rotation_quarters == Device::kRotationFlipped);
  rotation_mode = rotation_mode_from_quarters(rotation_quarters, rotation_mode);
  DeviceConfig normalized_cfg = config;
  normalized_cfg.display_brightness = brightness;
  normalized_cfg.display_rotated_180 = rotate_180;
  normalized_cfg.display_rotation_quarters = rotation_quarters;
  normalized_cfg.display_rotation_mode = rotation_mode;
  normalized_cfg.wake_mode_mains = wake_mode_mains;
  normalized_cfg.wake_mode_battery = wake_mode_battery;
  normalized_cfg.auto_sleep_enabled = sleep_enabled;
  normalized_cfg.auto_sleep_seconds = normalized_sleep_seconds;
  normalized_cfg.auto_sleep_battery_enabled = sleep_battery_enabled;
  normalized_cfg.auto_sleep_battery_seconds = normalized_bat_seconds;
  apply_device_capability_limits(normalized_cfg);
  wake_mode_mains = normalized_cfg.wake_mode_mains;
  wake_mode_battery = normalized_cfg.wake_mode_battery;
  sleep_battery_enabled = normalized_cfg.auto_sleep_battery_enabled;
  sleep_battery_seconds = normalized_cfg.auto_sleep_battery_seconds;
  normalized_bat_seconds = sleep_battery_seconds;

  prefs.putUChar("disp_bright", brightness);
  prefs.putBool("disp_rot180", rotate_180);
  prefs.putUChar("disp_rot_q", rotation_quarters);
  prefs.putUChar("disp_rot_mode", rotation_mode);
  prefs.putUChar("wake_mains", wake_mode_mains);
  prefs.putUChar("wake_bat", wake_mode_battery);
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
  config.display_rotated_180 = rotate_180;
  config.display_rotation_quarters = rotation_quarters;
  config.display_rotation_mode = rotation_mode;
  config.wake_mode_mains = wake_mode_mains;
  config.wake_mode_battery = wake_mode_battery;
  config.auto_sleep_enabled = sleep_enabled;
  config.auto_sleep_seconds = normalized_sleep_seconds;
  config.auto_sleep_battery_enabled = sleep_battery_enabled;
  config.auto_sleep_battery_seconds = normalized_bat_seconds;
  apply_device_capability_limits(config);

  Serial.println("✓ ConfigManager: Display-Einstellungen gespeichert");
  return true;
}

bool ConfigManager::saveScreensaverTimeout(bool enabled, uint16_t seconds) {
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("ConfigManager: Screensaver-Preferences oeffnen fehlgeschlagen");
    return false;
  }
  const uint16_t normalized_seconds = normalize_sleep_seconds(seconds);
  prefs.putBool("ss_auto_en", enabled);
  prefs.putUShort("ss_auto_sec", normalized_seconds);
  prefs.end();

  config.auto_screensaver_enabled = enabled;
  config.auto_screensaver_seconds = normalized_seconds;
  return true;
}

bool ConfigManager::saveTileBorders(bool enabled) {
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("ConfigManager: Tile-Border-Preferences oeffnen fehlgeschlagen");
    return false;
  }
  prefs.putBool("tile_border", enabled);
  prefs.end();

  config.tile_borders = enabled;
  return true;
}

bool ConfigManager::saveEthernetEnabled(bool enabled) {
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("ConfigManager: Netzwerkmodus-Preferences oeffnen fehlgeschlagen");
    return false;
  }
  prefs.putBool("eth_mode", enabled);
  prefs.end();

  config.ethernet_enabled = enabled;
  return true;
}

bool ConfigManager::saveScreensaverBrightness(uint8_t brightness_pct) {
  if (brightness_pct < kScreensaverBrightnessPctMin) {
    brightness_pct = kScreensaverBrightnessPctMin;
  } else if (brightness_pct > kScreensaverBrightnessPctMax) {
    brightness_pct = kScreensaverBrightnessPctMax;
  }

  Device::ScopedStorageWrite storage_write;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("ConfigManager: Screensaver-Helligkeit konnte nicht gespeichert werden");
    return false;
  }
  prefs.putUChar("ss_bright", brightness_pct);
  prefs.end();

  config.screensaver_brightness_pct = brightness_pct;
  return true;
}

bool ConfigManager::saveStaticAddressingEnabled(bool enabled) {
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("ConfigManager: IP-Modus-Preferences oeffnen fehlgeschlagen");
    return false;
  }
  prefs.putBool("net_static", enabled);
  prefs.putBool("wifi_static", enabled);
  prefs.putBool("eth_static", enabled);
  prefs.end();

  config.wifi_static_enabled = enabled;
  return true;
}

bool ConfigManager::clearStaticAddressing() {
  Device::ScopedStorageWrite storage_write;
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    Serial.println("ConfigManager: DHCP-Preferences oeffnen fehlgeschlagen");
    return false;
  }
  prefs.putBool("net_static", false);
  prefs.putBool("wifi_static", false);
  prefs.putString("wifi_ip", "");
  prefs.putString("wifi_gw", "");
  prefs.putString("wifi_subnet", "");
  prefs.putString("wifi_dns", "");
  prefs.putBool("eth_static", false);
  prefs.putString("eth_ip", "");
  prefs.putString("eth_gw", "");
  prefs.putString("eth_subnet", "");
  prefs.putString("eth_dns", "");
  prefs.end();

  config.wifi_static_enabled = false;
  config.wifi_static_ip[0] = '\0';
  config.wifi_gateway[0] = '\0';
  config.wifi_subnet[0] = '\0';
  config.wifi_dns[0] = '\0';
  return true;
}

void ConfigManager::clear() {
  Device::ScopedStorageWrite storage_write;
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
  strncpy(config.mqtt_base_topic, "hometiles", CONFIG_MQTT_BASE_MAX - 1);
  strncpy(config.ha_prefix, "ha/statestream", CONFIG_HA_PREFIX_MAX - 1);
  set_language_code(config.language, sizeof(config.language), "en");
  set_timezone_code(config.timezone, sizeof(config.timezone), "berlin");
  config.global_time_format = clock_tile::TIME_FORMAT_AUTO;
  config.global_date_format = clock_tile::DATE_FORMAT_AUTO;
  config.display_brightness = 200;
  config.screensaver_brightness_pct = kScreensaverBrightnessPctDefault;
  config.tile_borders = true;
  config.display_rotated_180 = false;
  config.display_rotation_quarters = Device::kRotationDefault;
  config.display_rotation_mode = kDisplayRotationNormal;
  config.status_time_font_size = 48;
  config.wifi_static_enabled = false;
  boot_static_enabled = false;
  config.status_date_font_size = 24;

  Serial.println("✓ ConfigManager: Konfiguration gelöscht");
}

void ConfigManager::setRuntimeDisplayRotation(bool rotate_180) {
  config.display_rotated_180 = rotate_180;
  config.display_rotation_quarters = rotation_quarters_from_legacy(rotate_180);
  config.display_rotation_mode = rotate_180 ? kDisplayRotationFlipped : kDisplayRotationNormal;
  apply_device_capability_limits(config);
}

void ConfigManager::setRuntimeDisplayRotationQuarters(uint8_t rotation_quarters) {
  config.display_rotation_quarters = normalize_rotation_quarters(rotation_quarters);
  config.display_rotated_180 = (config.display_rotation_quarters == Device::kRotationFlipped);
  if (config.display_rotation_mode != kDisplayRotationAuto) {
    config.display_rotation_mode =
        config.display_rotated_180 ? kDisplayRotationFlipped : kDisplayRotationNormal;
  }
  apply_device_capability_limits(config);
}

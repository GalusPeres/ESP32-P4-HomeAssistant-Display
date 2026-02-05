#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/network/network_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/sensor_popup.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/core/config_manager.h"
#include <M5Unified.h>
#include <PubSubClient.h>
#include <algorithm>
#include <vector>

// Cached values for outgoing snapshots
static float g_outside_c = 21.7f;
static float g_inside_c = 22.4f;
static int g_soc_pct = 73;

static const uint8_t kDisplayRotationDefault = 1;
static const uint8_t kDisplayRotationFlipped = 3;

static const char* kSleepOptionLabels[] = {
  "5 s",
  "15 s",
  "30 s",
  "60 s",
  "5 min",
  "15 min",
  "30 min",
  "60 min",
  "Nie"
};

static constexpr size_t kSleepOptionLabelCount =
    sizeof(kSleepOptionLabels) / sizeof(kSleepOptionLabels[0]);

static const char* kSleepOptionsJson =
    "[\"5 s\",\"15 s\",\"30 s\",\"60 s\",\"5 min\",\"15 min\",\"30 min\",\"60 min\",\"Nie\"]";

using RouteHandler = void (*)(const char* payload, size_t len);

struct TopicRoute {
  TopicKey key;
  RouteHandler handler;
  bool use_large_buffer;
};

struct DynamicSensorRoute {
  String topic;
  String entity_id;
  std::vector<uint8_t> slots;
};

static std::vector<DynamicSensorRoute> g_dynamic_routes;

struct DynamicWeatherRoute {
  String topic;
  String entity_id;
};

static std::vector<DynamicWeatherRoute> g_dynamic_weather_routes;

static void handleOutside(const char* payload, size_t) {
  g_outside_c = atof(payload);
}

static void handleInside(const char* payload, size_t) {
  g_inside_c = atof(payload);
}

static void handleSoc(const char* payload, size_t) {
  g_soc_pct = atoi(payload);
}

static void handleSceneCommand(const char* payload, size_t) {
  Serial.printf("Scene command received: %s\n", payload);
}

static void handleHaWohnTemp(const char* payload, size_t) {
  float v = atof(payload);
  Serial.printf("HA Wohnbereich Temperatur: %s -> %.2f C\n", payload, v);
}

static bool parseBoolPayload(const char* payload, bool* out) {
  if (!payload || !out) return false;
  String s(payload);
  s.trim();
  s.toLowerCase();
  if (s == "1" || s == "on" || s == "true" || s == "yes") {
    *out = true;
    return true;
  }
  if (s == "0" || s == "off" || s == "false" || s == "no") {
    *out = false;
    return true;
  }
  return false;
}

static const char* sleepLabelFromConfig(bool enabled, uint16_t seconds) {
  if (!enabled) return "Nie";
  uint16_t closest = kSleepOptionsSec[0];
  size_t closest_index = 0;
  uint16_t best_diff = (seconds > closest) ? (seconds - closest) : (closest - seconds);
  for (size_t i = 1; i < kSleepOptionsSecCount; ++i) {
    uint16_t option = kSleepOptionsSec[i];
    uint16_t diff = (seconds > option) ? (seconds - option) : (option - seconds);
    if (diff < best_diff) {
      best_diff = diff;
      closest = option;
      closest_index = i;
    }
  }
  if (closest_index >= kSleepOptionLabelCount - 1) {
    closest_index = kSleepOptionLabelCount - 2;
  }
  return kSleepOptionLabels[closest_index];
}

static bool parseSleepPayload(const char* payload, bool* enabled, uint16_t* seconds) {
  if (!payload || !enabled || !seconds) return false;
  String s(payload);
  s.trim();
  s.toLowerCase();
  if (!s.length()) return false;

  if (s == "nie" || s == "never" || s == "off" || s == "0") {
    *enabled = false;
    return true;
  }

  for (size_t i = 0; i + 1 < kSleepOptionLabelCount; ++i) {
    String label = kSleepOptionLabels[i];
    label.toLowerCase();
    if (s == label) {
      *enabled = true;
      *seconds = kSleepOptionsSec[i];
      return true;
    }
  }

  String compact = s;
  compact.replace(" ", "");
  bool is_min = compact.endsWith("min") || compact.endsWith("m");
  bool is_sec = compact.endsWith("s");
  int value = 0;
  bool found_digit = false;
  for (size_t i = 0; i < compact.length(); ++i) {
    char c = compact.charAt(i);
    if (c >= '0' && c <= '9') {
      value = value * 10 + (c - '0');
      found_digit = true;
    } else if (found_digit) {
      break;
    }
  }
  if (!found_digit) return false;

  uint32_t secs = is_min ? (uint32_t)value * 60u : (uint32_t)value;
  if (!is_min && !is_sec && value > 0 && value <= 3600) {
    secs = (uint32_t)value;
  }
  if (secs == 0) {
    *enabled = false;
    return true;
  }

  *enabled = true;
  *seconds = static_cast<uint16_t>(secs);
  return true;
}

static void handleDisplayBrightnessCommand(const char* payload, size_t) {
  if (!payload || !*payload) return;
  int value = atoi(payload);
  if (value < 75) value = 75;
  if (value > 255) value = 255;

  M5.Display.setBrightness(value);

  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      static_cast<uint8_t>(value),
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      cfg.display_rotated_180);
  mqttPublishDeviceSettings();
}

static void handleDisplayRotateCommand(const char* payload, size_t) {
  bool rotate = false;
  if (!parseBoolPayload(payload, &rotate)) return;
  M5.Display.setRotation(rotate ? kDisplayRotationFlipped : kDisplayRotationDefault);

  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      rotate);
  mqttPublishDeviceSettings();
}

static void handleSleepMainsCommand(const char* payload, size_t) {
  bool enabled = false;
  uint16_t seconds = 0;
  if (!parseSleepPayload(payload, &enabled, &seconds)) return;

  const DeviceConfig& cfg = configManager.getConfig();
  uint16_t new_seconds = enabled ? seconds : cfg.auto_sleep_seconds;
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      enabled,
      new_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      cfg.display_rotated_180);
  mqttPublishDeviceSettings();
}

static void handleSleepBatteryCommand(const char* payload, size_t) {
  bool enabled = false;
  uint16_t seconds = 0;
  if (!parseSleepPayload(payload, &enabled, &seconds)) return;

  const DeviceConfig& cfg = configManager.getConfig();
  uint16_t new_seconds = enabled ? seconds : cfg.auto_sleep_battery_seconds;
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      enabled,
      new_seconds,
      cfg.display_rotated_180);
  mqttPublishDeviceSettings();
}

static const TopicRoute kRoutes[] = {
  {TopicKey::SENSOR_OUT, handleOutside, false},
  {TopicKey::SENSOR_IN, handleInside, false},
  {TopicKey::SENSOR_SOC, handleSoc, false},
  {TopicKey::SCENE_CMND, handleSceneCommand, false},
  {TopicKey::HA_WOHN_TEMP, handleHaWohnTemp, false},
  {TopicKey::DISPLAY_BRIGHTNESS_CMND, handleDisplayBrightnessCommand, false},
  {TopicKey::DISPLAY_ROTATE_CMND, handleDisplayRotateCommand, false},
  {TopicKey::SLEEP_MAINS_CMND, handleSleepMainsCommand, false},
  {TopicKey::SLEEP_BAT_CMND, handleSleepBatteryCommand, false},
};

static String buildHaStatestreamTopic(const String& entity_id, const char* suffix) {
  String topic = mqttTopics.haPrefix();
  if (!topic.length()) {
    topic = "ha/statestream";
  }
  topic += "/";
  for (size_t i = 0; i < entity_id.length(); ++i) {
    char c = entity_id.charAt(i);
    topic += (c == '.') ? '/' : c;
  }
  topic += "/";
  topic += (suffix && *suffix) ? suffix : "state";
  return topic;
}

static void rebuildDynamicRoutes(std::vector<DynamicSensorRoute>& routes) {
  routes.clear();

  auto add_route = [&](const String& entity, int slot_index) {
    String ent = entity;
    ent.trim();
    if (!ent.length()) return;

    String topic = buildHaStatestreamTopic(ent, "state");
    auto it = std::find_if(
        routes.begin(),
        routes.end(),
        [&](const DynamicSensorRoute& r) { return r.topic == topic; });

    if (it == routes.end()) {
      DynamicSensorRoute route;
      route.topic = topic;
      route.entity_id = ent;
      if (slot_index >= 0) {
        route.slots.push_back(static_cast<uint8_t>(slot_index));
      }
      routes.push_back(route);
    } else {
      if (slot_index >= 0) {
        it->slots.push_back(static_cast<uint8_t>(slot_index));
      }
      if (!it->entity_id.equalsIgnoreCase(ent)) {
        it->entity_id = ent;  // prefer latest casing
      }
    }
  };

  const HaBridgeConfigData& cfg = haBridgeConfig.get();
  // Legacy HA sensor slots
  for (uint8_t slot = 0; slot < HA_SENSOR_SLOT_COUNT; ++slot) {
    add_route(cfg.sensor_slots[slot], slot);
  }

  // Sensor tiles from ALL folders (no slot index, entity-based update)
  auto add_grid_entities = [&](const TileGridConfig& grid) {
    for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
      const Tile& tile = grid.tiles[i];
      if ((tile.type == TILE_SENSOR || tile.type == TILE_SWITCH) && tile.sensor_entity.length()) {
        add_route(tile.sensor_entity, -1);
      }
    }
  };

  // Scan all folders, not just the active one
  const std::vector<FolderEntry>& folders = tileConfig.getFolders();
  for (const auto& folder : folders) {
    TileGridConfig grid;
    if (tileConfig.loadFolderGrid(folder.id, grid)) {
      add_grid_entities(grid);
    }
  }
}

static void rebuildDynamicWeatherRoutes(std::vector<DynamicWeatherRoute>& routes) {
  routes.clear();

  auto add_route = [&](const String& entity) {
    String ent = entity;
    ent.trim();
    if (!ent.length()) return;

    String topic = buildHaStatestreamTopic(ent, "weather");
    auto it = std::find_if(
        routes.begin(),
        routes.end(),
        [&](const DynamicWeatherRoute& r) { return r.topic == topic; });

    if (it == routes.end()) {
      DynamicWeatherRoute route;
      route.topic = topic;
      route.entity_id = ent;
      routes.push_back(route);
    } else if (!it->entity_id.equalsIgnoreCase(ent)) {
      it->entity_id = ent;
    }
  };

  const std::vector<FolderEntry>& folders = tileConfig.getFolders();
  for (const auto& folder : folders) {
    TileGridConfig grid;
    if (tileConfig.loadFolderGrid(folder.id, grid)) {
      for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
        const Tile& tile = grid.tiles[i];
        if (tile.type == TILE_WEATHER && tile.sensor_entity.length()) {
          add_route(tile.sensor_entity);
        }
      }
    }
  }
}

static bool tryHandleDynamicSensor(const char* topic, const char* payload) {
  for (const auto& route : g_dynamic_routes) {
    if (route.topic == topic) {
      // Update tile-based system (display) - active folder only
      tiles_update_sensor_by_entity(GridType::TAB0, route.entity_id.c_str(), payload);
      // Update sensor values map (for web interface)
      haBridgeConfig.updateSensorValue(route.entity_id, payload);
      return true;
    }
  }
  return false;
}

static bool tryHandleDynamicWeather(const char* topic, const char* payload) {
  for (const auto& route : g_dynamic_weather_routes) {
    if (route.topic == topic) {
      tiles_update_weather_by_entity(GridType::TAB0, route.entity_id.c_str(), payload);
      return true;
    }
  }
  return false;
}

static constexpr size_t SMALL_BUF = 96;
static constexpr size_t LARGE_BUF = 16384;
static char small_buf[SMALL_BUF];
static char large_buf[LARGE_BUF];

// ========== MQTT Callback (Topic-Routing) ==========
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  yield();  // Webserver atmen lassen!

  const char* apply_topic = networkManager.getBridgeApplyTopic();
  if (apply_topic && strcmp(topic, apply_topic) == 0) {
    static char cfg_buf[16384];  // Groesser fuer lange Config-Payloads
    if (length >= sizeof(cfg_buf)) {
      Serial.printf("[Bridge] WARNUNG: Payload zu gross (%u bytes), wird abgeschnitten!\n", length);
    }
    size_t copy_len = length < sizeof(cfg_buf) - 1 ? length : sizeof(cfg_buf) - 1;
    memcpy(cfg_buf, payload, copy_len);
    cfg_buf[copy_len] = '\0';
    yield();  // Nach großem Copy
    Serial.printf("[Bridge] apply-topic hit (%u bytes)\n", (unsigned)copy_len);
    bool reload = false;
    bool icons_changed = false;
    if (haBridgeConfig.applyJson(cfg_buf, &reload, &icons_changed)) {
      Serial.println("[Bridge] Konfiguration von HA empfangen");
      if (reload) {
        yield();  // Nach JSON Parse
        networkManager.publishBridgeConfig();
        yield();  // Nach Publish
        mqttReloadDynamicSlots();
      }
      if (icons_changed) {
        tiles_request_icon_refresh();
      }
    } else {
      Serial.println("[Bridge] Ungueltige Bridge-Konfiguration empfangen");
    }
    return;
  }

  bool processed_static = false;
  for (const auto& route : kRoutes) {
    const char* expected = mqttTopics.topic(route.key);
    if (!expected || strcmp(topic, expected) != 0) {
      continue;
    }

    char* buf = route.use_large_buffer ? large_buf : small_buf;
    size_t buf_len = route.use_large_buffer ? sizeof(large_buf) : sizeof(small_buf);
    size_t copy_len = length < (buf_len - 1) ? length : (buf_len - 1);
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    route.handler(buf, copy_len);
    processed_static = true;
    break;
  }

  // Dynamische Sensor-Slots pruefen (JSON kann groesser sein -> ggf. Large Buffer)
  char* dyn_buf = (length < SMALL_BUF) ? small_buf : large_buf;
  size_t dyn_len = (length < SMALL_BUF) ? sizeof(small_buf) : sizeof(large_buf);
  size_t copy_len = length < (dyn_len - 1) ? length : (dyn_len - 1);
  memcpy(dyn_buf, payload, copy_len);
  dyn_buf[copy_len] = '\0';
  if (tryHandleDynamicSensor(topic, dyn_buf)) {
    yield();  // Nach Sensor-Update
    return;
  }
  if (tryHandleDynamicWeather(topic, dyn_buf)) {
    yield();
    return;
  }

  const char* history_topic = networkManager.getHistoryResponseTopic();
  if (history_topic && strcmp(topic, history_topic) == 0) {
    size_t copy_len = length < (LARGE_BUF - 1) ? length : (LARGE_BUF - 1);
    memcpy(large_buf, payload, copy_len);
    large_buf[copy_len] = '\0';
    queue_sensor_popup_history(nullptr, large_buf, copy_len);
    queue_tile_graph_history(nullptr, large_buf, copy_len);
    return;
  }

  if (processed_static) {
    return;
  }

  Serial.printf("MQTT: Unhandled topic %s\n", topic);
}

// ========== Subscribe zu Topics ==========
void mqttSubscribeTopics() {
  PubSubClient& mqtt = networkManager.getMqttClient();

  for (const auto& route : kRoutes) {
    const char* tpc = mqttTopics.topic(route.key);
    if (!tpc || !*tpc) continue;
    mqtt.subscribe(tpc);
    Serial.printf("MQTT: subscribed %s\n", tpc);
  }

  mqttReloadDynamicSlots();
}

// ========== Home Snapshot publizieren ==========
void mqttPublishHomeSnapshot() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  char buf[24];
  dtostrf(g_outside_c, 0, 1, buf);
  mqtt.publish(mqttTopics.topic(TopicKey::SENSOR_OUT), buf, true);

  dtostrf(g_inside_c, 0, 1, buf);
  mqtt.publish(mqttTopics.topic(TopicKey::SENSOR_IN), buf, true);

  snprintf(buf, sizeof(buf), "%d", g_soc_pct);
  mqtt.publish(mqttTopics.topic(TopicKey::SENSOR_SOC), buf, true);
}

// ========== Device Settings publizieren ==========
void mqttPublishDeviceSettings() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  const DeviceConfig& cfg = configManager.getConfig();

  auto publish_state = [&](TopicKey key, const char* payload) {
    const char* tpc = mqttTopics.topic(key);
    if (!tpc || !*tpc || !payload) return;
    mqtt.publish(tpc, payload, true);
  };

  char buf[16];
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(cfg.display_brightness));
  publish_state(TopicKey::DISPLAY_BRIGHTNESS_STAT, buf);
  publish_state(TopicKey::DISPLAY_ROTATE_STAT, cfg.display_rotated_180 ? "ON" : "OFF");

  publish_state(TopicKey::SLEEP_MAINS_STAT,
                sleepLabelFromConfig(cfg.auto_sleep_enabled, cfg.auto_sleep_seconds));
  publish_state(TopicKey::SLEEP_BAT_STAT,
                sleepLabelFromConfig(cfg.auto_sleep_battery_enabled, cfg.auto_sleep_battery_seconds));
}

// ========== Scene Command publizieren ==========
void mqttPublishScene(const char* scene_name) {
  if (!scene_name || !*scene_name) return;

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) {
    Serial.printf("Scene command skipped (MQTT offline): %s\n", scene_name);
    return;
  }

  bool ok = mqtt.publish(mqttTopics.topic(TopicKey::SCENE_CMND), scene_name, false);
  Serial.printf("Scene command -> MQTT '%s' (%s)\n", scene_name, ok ? "ok" : "fail");
}

// ========== Light/Switch Command publizieren ==========
void mqttPublishSwitchCommand(const char* entity_id, const char* state) {
  if (!entity_id || !*entity_id) return;

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) {
    Serial.printf("Switch command skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* action = (state && *state) ? state : "toggle";
  const char* topic = nullptr;
  if (strncmp(entity_id, "light.", 6) == 0) {
    topic = mqttTopics.topic(TopicKey::LIGHT_CMND);
  } else {
    topic = mqttTopics.topic(TopicKey::SWITCH_CMND);
  }

  if (!topic || !*topic) {
    Serial.printf("Switch command skipped (no topic): %s\n", entity_id);
    return;
  }

  char payload[256];
  snprintf(payload, sizeof(payload), "{\"entity_id\":\"%s\",\"state\":\"%s\"}", entity_id, action);
  bool ok = mqtt.publish(topic, payload, false);
  Serial.printf("Switch command -> MQTT '%s' (%s)\n", topic, ok ? "ok" : "fail");
}

void mqttPublishLightCommand(const char* entity_id, const char* state, int brightness_pct, bool has_color, uint32_t color) {
  if (!entity_id || !*entity_id) return;

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) {
    Serial.printf("Light command skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* topic = mqttTopics.topic(TopicKey::LIGHT_CMND);
  if (!topic || !*topic) {
    Serial.printf("Light command skipped (no topic): %s\n", entity_id);
    return;
  }

  String payload = "{\"entity_id\":\"";
  payload += entity_id;
  payload += "\"";

  if (state && *state) {
    payload += ",\"state\":\"";
    payload += state;
    payload += "\"";
  }

  if (brightness_pct >= 0) {
    if (brightness_pct > 100) brightness_pct = 100;
    payload += ",\"brightness_pct\":";
    payload += brightness_pct;
  }

  if (has_color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    payload += ",\"rgb_color\":[";
    payload += r;
    payload += ",";
    payload += g;
    payload += ",";
    payload += b;
    payload += "]";
  }

  payload += "}";

  bool ok = mqtt.publish(topic, payload.c_str(), false);
  Serial.printf("Light command -> MQTT '%s' (%s)\n", topic, ok ? "ok" : "fail");
}

void mqttPublishHistoryRequest(const char* entity_id) {
  if (!entity_id || !*entity_id) return;

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) {
    Serial.printf("History request skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* topic = networkManager.getHistoryRequestTopic();
  if (!topic || !*topic) {
    Serial.printf("History request skipped (no topic): %s\n", entity_id);
    return;
  }

  String payload = "{\"entity_id\":\"";
  payload += entity_id;
  payload += "\",\"hours\":24,\"period_minutes\":5,\"points\":288,\"stat\":\"mean\"}";
  bool ok = mqtt.publish(topic, payload.c_str(), false);
  Serial.printf("History request -> MQTT '%s' (%s)\n", topic, ok ? "ok" : "fail");
}

// ========== Home Assistant MQTT Discovery ==========
void mqttPublishDiscovery() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  Serial.println("Publishing Home Assistant discovery payloads...");

  char did[24];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(did, sizeof(did), "tab5_lvgl_%04X", (uint16_t)(mac & 0xFFFF));

  char tpc[128];
  char js[1024];

  const char* stat_topic = mqttTopics.topic(TopicKey::STAT_CONN);

  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_outside_c/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Outside\",\"stat_t\":\"%s\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_out\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Tab5 LVGL\",\"mf\":\"M5Stack\",\"mdl\":\"Tab5\"}}",
    mqttTopics.topic(TopicKey::SENSOR_OUT), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_inside_c/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Inside\",\"stat_t\":\"%s\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_in\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Tab5 LVGL\",\"mf\":\"M5Stack\",\"mdl\":\"Tab5\"}}",
    mqttTopics.topic(TopicKey::SENSOR_IN), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_soc_pct/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Battery SoC\",\"stat_t\":\"%s\",\"unit_of_meas\":\"%%\",\"dev_cla\":\"battery\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_soc\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Tab5 LVGL\",\"mf\":\"M5Stack\",\"mdl\":\"Tab5\"}}",
    mqttTopics.topic(TopicKey::SENSOR_SOC), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_uptime/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Uptime\",\"stat_t\":\"%s\",\"unit_of_meas\":\"s\",\"uniq_id\":\"%s_up\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::TELE_UP), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_abend/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Scene Abend\",\"cmd_t\":\"%s\",\"pl_prs\":\"Abend\",\"uniq_id\":\"%s_btn_abend\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::SCENE_CMND), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_lesen/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Scene Lesen\",\"cmd_t\":\"%s\",\"pl_prs\":\"Lesen\",\"uniq_id\":\"%s_btn_lesen\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::SCENE_CMND), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_allesaus/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Tab5 Scene Alles Aus\",\"cmd_t\":\"%s\",\"pl_prs\":\"AllesAus\",\"uniq_id\":\"%s_btn_allesaus\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::SCENE_CMND), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  Serial.println("Home Assistant discovery published");
}

void mqttReloadDynamicSlots() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (mqtt.connected()) {
    for (const auto& route : g_dynamic_routes) {
      mqtt.unsubscribe(route.topic.c_str());
    }
    for (const auto& route : g_dynamic_weather_routes) {
      mqtt.unsubscribe(route.topic.c_str());
    }
  }
  rebuildDynamicRoutes(g_dynamic_routes);
  rebuildDynamicWeatherRoutes(g_dynamic_weather_routes);
  if (mqtt.connected()) {
    for (const auto& route : g_dynamic_routes) {
      mqtt.subscribe(route.topic.c_str());
      Serial.printf("MQTT: subscribed %s\n", route.topic.c_str());
    }
    for (const auto& route : g_dynamic_weather_routes) {
      mqtt.subscribe(route.topic.c_str());
      Serial.printf("MQTT: subscribed %s\n", route.topic.c_str());
    }
  }
}

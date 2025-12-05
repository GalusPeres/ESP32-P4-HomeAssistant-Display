#include "mqtt_handlers.h"
#include "mqtt_topics.h"
#include "network_manager.h"
#include "ha_bridge_config.h"
#include "tab_home.h"
#include "tab_solar.h"
#include <PubSubClient.h>
#include <algorithm>
#include <vector>

// Cached values for outgoing snapshots
static float g_outside_c = 21.7f;
static float g_inside_c = 22.4f;
static int g_soc_pct = 73;

using RouteHandler = void (*)(const char* payload, size_t len);

struct TopicRoute {
  TopicKey key;
  RouteHandler handler;
  bool use_large_buffer;
};

struct DynamicSensorRoute {
  String topic;
  std::vector<uint8_t> slots;
};

static std::vector<DynamicSensorRoute> g_dynamic_routes;

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

static void handleHaPvGarage(const char* payload, size_t) {
  float v = atof(payload);
  Serial.printf("HA PV Garage: %s -> %.0f W\n", payload, v);
  solar_update_power(v);
}

static void handlePvHistory(const char* payload, size_t len) {
  Serial.printf("HA PV Garage History: %u bytes\n", static_cast<unsigned>(len));
  solar_set_history_csv(payload);
}

static const TopicRoute kRoutes[] = {
  {TopicKey::SENSOR_OUT, handleOutside, false},
  {TopicKey::SENSOR_IN, handleInside, false},
  {TopicKey::SENSOR_SOC, handleSoc, false},
  {TopicKey::SCENE_CMND, handleSceneCommand, false},
  {TopicKey::HA_WOHN_TEMP, handleHaWohnTemp, false},
  {TopicKey::HA_PV_GARAGE, handleHaPvGarage, false},
  {TopicKey::HA_PV_GARAGE_HISTORY, handlePvHistory, true},
};

static String buildHaStatestreamTopic(const String& entity_id) {
  String topic = mqttTopics.haPrefix();
  if (!topic.length()) {
    topic = "ha/statestream";
  }
  topic += "/";
  for (size_t i = 0; i < entity_id.length(); ++i) {
    char c = entity_id.charAt(i);
    topic += (c == '.') ? '/' : c;
  }
  topic += "/state";
  return topic;
}

static void rebuildDynamicRoutes(std::vector<DynamicSensorRoute>& routes) {
  routes.clear();
  const HaBridgeConfigData& cfg = haBridgeConfig.get();
  for (uint8_t slot = 0; slot < HA_SENSOR_SLOT_COUNT; ++slot) {
    const String& entity = cfg.sensor_slots[slot];
    if (!entity.length()) {
      continue;
    }
    String topic = buildHaStatestreamTopic(entity);
    auto it = std::find_if(
        routes.begin(),
        routes.end(),
        [&](const DynamicSensorRoute& r) { return r.topic == topic; });
    if (it == routes.end()) {
      DynamicSensorRoute route;
      route.topic = topic;
      route.slots.push_back(slot);
      routes.push_back(route);
    } else {
      it->slots.push_back(slot);
    }
  }
}

static bool tryHandleDynamicSensor(const char* topic, const char* payload) {
  for (const auto& route : g_dynamic_routes) {
    if (route.topic == topic) {
      for (uint8_t slot : route.slots) {
        home_set_sensor_slot_value(slot, payload);
      }
      return true;
    }
  }
  return false;
}

static constexpr size_t SMALL_BUF = 96;
static constexpr size_t LARGE_BUF = 4096;
static char small_buf[SMALL_BUF];
static char large_buf[LARGE_BUF];

// ========== MQTT Callback (Topic-Routing) ==========
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  const char* apply_topic = networkManager.getBridgeApplyTopic();
  if (apply_topic && strcmp(topic, apply_topic) == 0) {
    static char cfg_buf[8192];  // Reduziert fuer mehr verfuegbaren Heap
    if (length >= sizeof(cfg_buf)) {
      Serial.printf("[Bridge] WARNUNG: Payload zu gross (%u bytes), wird abgeschnitten!\n", length);
    }
    size_t copy_len = length < sizeof(cfg_buf) - 1 ? length : sizeof(cfg_buf) - 1;
    memcpy(cfg_buf, payload, copy_len);
    cfg_buf[copy_len] = '\0';
    Serial.printf("[Bridge] apply-topic hit (%u bytes)\n", (unsigned)copy_len);
    if (haBridgeConfig.applyJson(cfg_buf)) {
      Serial.println("[Bridge] Konfiguration von HA empfangen");
      networkManager.publishBridgeConfig();
      home_reload_layout();
      mqttReloadDynamicSlots();
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

  // Dynamische Sensor-Slots pruefen
  size_t copy_len = length < (SMALL_BUF - 1) ? length : (SMALL_BUF - 1);
  memcpy(small_buf, payload, copy_len);
  small_buf[copy_len] = '\0';
  if (tryHandleDynamicSensor(topic, small_buf)) {
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

// ========== Home Assistant MQTT Discovery ==========
void mqttPublishDiscovery() {
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  Serial.println("Publishing Home Assistant discovery payloads...");

  char did[24];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(did, sizeof(did), "tab5_lvgl_%04X", (uint16_t)(mac & 0xFFFF));

  char tpc[96];
  char js[256];

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
  }
  rebuildDynamicRoutes(g_dynamic_routes);
  if (mqtt.connected()) {
    for (const auto& route : g_dynamic_routes) {
      mqtt.subscribe(route.topic.c_str());
      Serial.printf("MQTT: subscribed %s\n", route.topic.c_str());
    }
  }
}

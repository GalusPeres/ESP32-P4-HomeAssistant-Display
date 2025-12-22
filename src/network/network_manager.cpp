#include "src/network/network_manager.h"
#include "src/core/config_manager.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin.h"
#include "src/ui/ui_manager.h"
#include "src/ui/tab_settings.h"

// Globale Instanz
Tab5NetworkManager networkManager;

static void buildDeviceId(char* buffer, size_t len) {
  if (!buffer || !len) return;
  uint64_t mac = ESP.getEfuseMac();
  snprintf(buffer, len, "tab5_lvgl_%04X", (uint16_t)(mac & 0xFFFF));
}

// ========== Initialisierung ==========
void Tab5NetworkManager::init() {
  Serial.println("🌐 Initialisiere Network Manager...");

  if (!configManager.isConfigured()) {
    Serial.println("⚠️ Keine Netzwerk-Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  // WiFi-Setup
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  wifi_retry_at = 0;  // Sofortiger Verbindungsversuch

  mqtt_enabled = configManager.hasMqttConfig();
  if (mqtt_enabled) {
    // MQTT-Setup
    mqtt_client.setClient(net_client);
    mqtt_client.setServer(cfg.mqtt_host, cfg.mqtt_port);
    mqtt_client.setBufferSize(4096);  // Für große History-CSV-Daten
    mqtt_client.setCallback(mqttCallback);
  } else {
    Serial.println("MQTT: keine Konfiguration vorhanden - ueberspringe Verbindung");
  }

  Serial.println("✓ Network Manager initialisiert");
}

// ========== WiFi verbinden ==========
void Tab5NetworkManager::connectWifi() {
  wifi_retry_at = millis() + 5000UL;  // Retry in 5s

  if (!configManager.isConfigured()) {
    Serial.println("WiFi: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();
  if (cfg.wifi_ssid && cfg.wifi_ssid[0]) {
    Serial.printf("WiFi: Verbinde mit %s\n", cfg.wifi_ssid);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
  }
}

// ========== MQTT verbinden ==========
void Tab5NetworkManager::connectMqtt() {
  if (!mqtt_enabled) return;
  mqtt_retry_at = millis() + 3000UL;  // Retry in 3s

  if (WiFi.status() != WL_CONNECTED) return;

  if (!configManager.isConfigured()) {
    Serial.println("MQTT: Keine Konfiguration vorhanden");
    return;
  }

  const DeviceConfig& cfg = configManager.getConfig();

  char client_id[48];
  uint64_t mac = ESP.getEfuseMac();
  uint16_t short_id = (uint16_t)(mac & 0xFFFF);
  snprintf(client_id, sizeof(client_id), "Tab5_LVGL-%04X", short_id);

  char did[24];
  buildDeviceId(did, sizeof(did));
  bridge_apply_topic_ = "tab5_lvgl/config/";
  bridge_apply_topic_ += did;
  bridge_apply_topic_ += "/bridge/apply";

  bridge_request_topic_ = "tab5_lvgl/config/";
  bridge_request_topic_ += did;
  bridge_request_topic_ += "/bridge/request";

  history_request_topic_ = "tab5_lvgl/config/";
  history_request_topic_ += did;
  history_request_topic_ += "/history/request";

  history_response_topic_ = "tab5_lvgl/config/";
  history_response_topic_ += did;
  history_response_topic_ += "/history/response";

  Serial.printf("MQTT: Verbinde mit %s:%u als %s\n", cfg.mqtt_host, cfg.mqtt_port, client_id);

  const char* stat_topic = mqttTopics.topic(TopicKey::STAT_CONN);
  if (!stat_topic || !*stat_topic) {
    stat_topic = "tab5/stat/connected";
  }

  bool ok = false;
  if (cfg.mqtt_user && cfg.mqtt_user[0]) {
    ok = mqtt_client.connect(client_id, cfg.mqtt_user, cfg.mqtt_pass,
                             stat_topic, 0, true, "0");
  } else {
    ok = mqtt_client.connect(client_id, nullptr, nullptr,
                             stat_topic, 0, true, "0");
  }

  if (!ok) {
    Serial.printf("MQTT: Verbindung fehlgeschlagen, State=%d\n", mqtt_client.state());
    return;
  }

  Serial.println("✓ MQTT verbunden");

  // Connected - Status publizieren und Topics subscriben
  mqtt_client.publish(stat_topic, "1", true);
  mqttSubscribeTopics();
  if (!bridge_apply_topic_.isEmpty()) {
    mqtt_client.subscribe(bridge_apply_topic_.c_str());
    Serial.printf("[MQTT] Listening for bridge config on %s\n", bridge_apply_topic_.c_str());
  }
  if (!history_response_topic_.isEmpty()) {
    mqtt_client.subscribe(history_response_topic_.c_str());
    Serial.printf("[MQTT] Listening for history responses on %s\n", history_response_topic_.c_str());
  }
  mqttPublishDiscovery();
  publishBridgeConfig();
  publishBridgeRequest();
}

// ========== WiFi-Status ==========
bool Tab5NetworkManager::isWifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

// ========== MQTT-Status ==========
bool Tab5NetworkManager::isMqttConnected() {
  return mqtt_client.connected();
}

PubSubClient& Tab5NetworkManager::getMqttClient() {
  return mqtt_client;
}

// ========== Telemetrie senden ==========
void Tab5NetworkManager::publishTelemetry() {
  if (!mqtt_client.connected()) return;

  uint32_t now = millis();
  if (now - last_telemetry > 30000UL) {  // 30 Sekunden
    last_telemetry = now;
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)(now / 1000UL));
    const char* tele_topic = mqttTopics.topic(TopicKey::TELE_UP);
    if (tele_topic && *tele_topic) {
      mqtt_client.publish(tele_topic, buf, true);
    }
  }
}

void Tab5NetworkManager::publishBridgeConfig() {
  if (!mqtt_client.connected()) return;
  if (!configManager.isConfigured()) return;

  const DeviceConfig& cfg = configManager.getConfig();
  char did[24];
  buildDeviceId(did, sizeof(did));
  String payload = haBridgeConfig.buildJsonPayload(did, cfg.mqtt_base_topic, cfg.ha_prefix);
  if (payload.isEmpty()) return;

  String topic = "tab5_lvgl/config/";
  topic += did;
  topic += "/bridge";
  mqtt_client.publish(topic.c_str(), payload.c_str(), true);
  Serial.println("[Network] Home Assistant Bridge-Konfiguration publiziert");
}

const char* Tab5NetworkManager::getBridgeApplyTopic() const {
  return bridge_apply_topic_.length() ? bridge_apply_topic_.c_str() : nullptr;
}

void Tab5NetworkManager::publishBridgeRequest() {
  if (!mqtt_client.connected()) return;
  if (bridge_request_topic_.isEmpty()) return;
  mqtt_client.publish(bridge_request_topic_.c_str(), "", false);
  Serial.println("[Network] Home Assistant Bridge-Aktualisierung angefordert");
}

const char* Tab5NetworkManager::getBridgeRequestTopic() const {
  return bridge_request_topic_.length() ? bridge_request_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getHistoryRequestTopic() const {
  return history_request_topic_.length() ? history_request_topic_.c_str() : nullptr;
}

const char* Tab5NetworkManager::getHistoryResponseTopic() const {
  return history_response_topic_.length() ? history_response_topic_.c_str() : nullptr;
}

// ========== Update-Schleife ==========
void Tab5NetworkManager::update() {
  if (!configManager.isConfigured()) {
    return;
  }

  uint32_t now_ms = millis();
  bool is_connected = (WiFi.status() == WL_CONNECTED);

  // WiFi-Verbindung verwalten
  if (!is_connected) {
    // Nicht verbunden - Retry
    if ((int32_t)(now_ms - wifi_retry_at) >= 0) {
      connectWifi();
    }

    // WebAdmin stoppen wenn Verbindung verloren
    if (was_connected && webAdminServer.isRunning()) {
      webAdminServer.stop();
    }
  } else {
    // Verbunden

    // WebAdmin starten wenn gerade verbunden
    if (!was_connected && !webAdminServer.isRunning()) {
      webAdminServer.start();
    }

    // NTP-Sync triggern bei neuer Verbindung
    if (!was_connected) {
      uiManager.scheduleNtpSync(0);
    }

    // MQTT verwalten
    if (mqtt_enabled) {
      if (!mqtt_client.connected()) {
        if ((int32_t)(now_ms - mqtt_retry_at) >= 0) {
          connectMqtt();
        }
      } else {
        mqtt_client.loop();
        publishTelemetry();
      }
    }
  }

  // WiFi-Status für nächste Runde merken
  was_connected = is_connected;
}

// ========== WiFi Power Management ==========
void Tab5NetworkManager::setWifiPowerSaving(bool enable) {
  if (!isWifiConnected()) return;

  if (enable) {
    // Batteriemodus: Stromsparen aktivieren
    WiFi.setSleep(WIFI_PS_MIN_MODEM);  // Light Sleep (spart ~40mA)
    WiFi.setTxPower(WIFI_POWER_11dBm); // TX Power reduzieren (spart ~20mA)
    Serial.println("🔋 WiFi Power Saving: Light Sleep + 11dBm");
  } else {
    // Netzteilmodus: Volle Performance
    WiFi.setSleep(WIFI_PS_NONE);       // Kein Sleep
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // Maximale Reichweite
    Serial.println("🔌 WiFi Full Power: No Sleep + 19.5dBm");
  }
}


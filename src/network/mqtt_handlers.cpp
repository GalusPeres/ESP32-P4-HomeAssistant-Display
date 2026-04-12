#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/network/network_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/tab_settings.h"
#include "src/types/energy/energy_data.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/core/device_entities.h"
#include "src/core/power_manager.h"
#include "src/core/battery_state.h"
#include "src/core/board_hal.h"
#include <PubSubClient.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <time.h>
#include <vector>

#ifndef TAB5_HAS_ONEWIRE_DS18X20
#define TAB5_HAS_ONEWIRE_DS18X20 0
#endif

#if defined(__has_include)
#if __has_include(<OneWire.h>) && __has_include(<DallasTemperature.h>)
#undef TAB5_HAS_ONEWIRE_DS18X20
#define TAB5_HAS_ONEWIRE_DS18X20 1
#include <OneWire.h>
#include <DallasTemperature.h>
#endif
#endif

// Cached values for outgoing snapshots
static float g_outside_c = 21.7f;
static float g_inside_c = 22.4f;
static int g_soc_pct = -1;
static float g_external_temp_c = NAN;
static bool g_external_temp_valid = false;
static constexpr uint32_t kExternalTempGridRefreshMs = 5000;
static String g_external_last_grid_payload;
static uint32_t g_external_last_grid_ms = 0;
static constexpr uint16_t kExternalTempHistoryHoursMax = 168;
static constexpr uint16_t kExternalTempHistorySampleMinutes = 5;
static constexpr uint16_t kExternalTempHistoryPoints =
    static_cast<uint16_t>((kExternalTempHistoryHoursMax * 60U) / kExternalTempHistorySampleMinutes);
static constexpr uint32_t kExternalTempHistorySampleMs =
    static_cast<uint32_t>(kExternalTempHistorySampleMinutes) * 60UL * 1000UL;
static float g_external_history_values[kExternalTempHistoryPoints] = {};
static bool g_external_history_valid[kExternalTempHistoryPoints] = {};
static uint16_t g_external_history_head = 0;
static uint16_t g_external_history_count = 0;
static uint32_t g_external_history_last_store_ms = 0;
static constexpr uint32_t kHistoryHaResponseTimeoutMs = 2000;
static constexpr uint8_t kHistoryPendingSlots = 8;

struct PendingHistoryRequest {
  String entity_id;
  uint32_t requested_at_ms = 0;
  uint16_t hours = 24;
  uint16_t period_minutes = 5;
  uint16_t points = 288;
  bool active = false;
};

static PendingHistoryRequest g_pending_history[kHistoryPendingSlots];

static String buildHaStatestreamTopic(const String& entity_id, const char* suffix);

static void update_all_grids(const char* entity_id, const char* payload) {
  if (!entity_id || !payload) return;
  tiles_update_sensor_by_entity(GridType::TAB0, entity_id, payload);
  tiles_update_sensor_by_entity(GridType::TAB1, entity_id, payload);
  tiles_update_sensor_by_entity(GridType::TAB2, entity_id, payload);
}

static bool is_external_temp_entity(const char* entity_id) {
  if (!entity_id || !*entity_id) return false;
  String normalized(entity_id);
  normalized.trim();
  return normalized.equalsIgnoreCase(kEntityExternalTemperature);
}

static bool is_internal_tab5_entity(const char* entity_id) {
  if (!entity_id || !*entity_id) return false;
  String normalized(entity_id);
  normalized.trim();
  normalized.toLowerCase();

  if (normalized.equalsIgnoreCase(kEntityDisplayBrightness)) return true;
  if (normalized.equalsIgnoreCase(kEntityDisplayRotate)) return true;
  if (normalized.equalsIgnoreCase(kEntityDisplaySleep)) return true;
  if (normalized.equalsIgnoreCase(kEntityExternalTemperature)) return true;

  return normalized.startsWith("sensor.tab5_") ||
         normalized.startsWith("switch.tab5_") ||
         normalized.startsWith("light.tab5_");
}

static bool has_valid_local_time_for_history() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;

  const int year = timeinfo.tm_year + 1900;
  const int month = timeinfo.tm_mon + 1;
  const int day = timeinfo.tm_mday;
  const int hour = timeinfo.tm_hour;
  const int minute = timeinfo.tm_min;

  return (year >= 2024 && year <= 2100) &&
         (month >= 1 && month <= 12) &&
         (day >= 1 && day <= 31) &&
         (hour >= 0 && hour < 24) &&
         (minute >= 0 && minute < 60);
}

static void push_external_temp_history(float value, bool valid) {
  g_external_history_values[g_external_history_head] = value;
  g_external_history_valid[g_external_history_head] = valid;
  g_external_history_head = static_cast<uint16_t>((g_external_history_head + 1) % kExternalTempHistoryPoints);
  if (g_external_history_count < kExternalTempHistoryPoints) {
    ++g_external_history_count;
  }
}

static void store_external_temp_history(uint32_t now_ms) {
  if (!has_valid_local_time_for_history()) {
    // Erst mit valider Uhrzeit starten, damit Historien nicht mit
    // pre-NTP Bootdaten gefuellt werden.
    g_external_history_last_store_ms = 0;
    return;
  }

  if (g_external_history_last_store_ms == 0) {
    if (!g_external_temp_valid || std::isnan(g_external_temp_c) || std::isinf(g_external_temp_c)) {
      return;
    }
    g_external_history_last_store_ms = now_ms - kExternalTempHistorySampleMs;
  }

  if ((int32_t)(now_ms - g_external_history_last_store_ms) < static_cast<int32_t>(kExternalTempHistorySampleMs)) {
    return;
  }
  g_external_history_last_store_ms = now_ms;

  if (g_external_temp_valid && !std::isnan(g_external_temp_c) && !std::isinf(g_external_temp_c)) {
    push_external_temp_history(g_external_temp_c, true);
  } else if (g_external_history_count > 0) {
    push_external_temp_history(0.0f, false);
  }
}

static String build_external_temp_history_payload(const char* entity_id,
                                                  uint16_t hours,
                                                  uint16_t period_minutes,
                                                  uint16_t points) {
  if (hours == 0) hours = 24;
  if (hours > kExternalTempHistoryHoursMax) hours = kExternalTempHistoryHoursMax;
  if (period_minutes == 0) period_minutes = kExternalTempHistorySampleMinutes;
  if (points == 0) {
    points = static_cast<uint16_t>((static_cast<uint32_t>(hours) * 60U) / period_minutes);
  }
  if (points == 0) points = 1;

  const uint16_t samples_per_bucket = static_cast<uint16_t>(
      (period_minutes + kExternalTempHistorySampleMinutes - 1U) / kExternalTempHistorySampleMinutes);
  const uint32_t requested_base_points =
      static_cast<uint32_t>(points) * static_cast<uint32_t>(samples_per_bucket);
  const uint16_t clamped_base_points = static_cast<uint16_t>(
      requested_base_points > kExternalTempHistoryPoints ? kExternalTempHistoryPoints : requested_base_points);
  const uint16_t available_base_points =
      (g_external_history_count > clamped_base_points) ? clamped_base_points : g_external_history_count;
  const uint16_t missing_base_points = static_cast<uint16_t>(clamped_base_points - available_base_points);
  const uint16_t history_start = static_cast<uint16_t>(
      (g_external_history_head + kExternalTempHistoryPoints - available_base_points) % kExternalTempHistoryPoints);

  String payload;
  payload.reserve(12288);
  payload = "{\"entity_id\":\"";
  payload += entity_id ? entity_id : kEntityExternalTemperature;
  payload += "\",\"unit\":\"C\",\"current\":\"";

  char current_buf[24];
  const bool has_current_value = g_external_temp_valid && !std::isnan(g_external_temp_c) && !std::isinf(g_external_temp_c);
  if (has_current_value) {
    dtostrf(g_external_temp_c, 0, 1, current_buf);
    payload += current_buf;
  } else {
    payload += "unavailable";
  }
  payload += "\",\"hours\":";
  payload += String(hours);
  payload += ",\"period_minutes\":";
  payload += String(period_minutes);
  payload += ",\"stat\":\"mean\",\"values\":[";

  bool first = true;

  for (uint16_t bucket = 0; bucket < points; ++bucket) {
    if (!first) payload += ",";

    float sum = 0.0f;
    uint16_t valid_count = 0;
    for (uint16_t sample = 0; sample < samples_per_bucket; ++sample) {
      const uint32_t virtual_index =
          static_cast<uint32_t>(bucket) * static_cast<uint32_t>(samples_per_bucket) + sample;
      if (virtual_index < missing_base_points) continue;

      const uint32_t local_offset = virtual_index - missing_base_points;
      if (local_offset >= available_base_points) continue;

      const uint16_t idx = static_cast<uint16_t>((history_start + local_offset) % kExternalTempHistoryPoints);
      if (!g_external_history_valid[idx] || std::isnan(g_external_history_values[idx]) ||
          std::isinf(g_external_history_values[idx])) {
        continue;
      }
      sum += g_external_history_values[idx];
      ++valid_count;
    }

    if (valid_count == 0) {
      payload += "null";
    } else {
      dtostrf(sum / valid_count, 0, 1, current_buf);
      payload += current_buf;
    }
    first = false;
  }

  payload += "]}";
  return payload;
}

static String build_empty_history_payload(const char* entity_id,
                                          uint16_t hours = 0,
                                          uint16_t period_minutes = 0,
                                          uint16_t points = 0) {
  String payload;
  payload.reserve(160);
  payload = "{\"entity_id\":\"";
  payload += entity_id ? entity_id : "";
  payload += "\"";
  if (hours) {
    payload += ",\"hours\":";
    payload += String(hours);
  }
  if (period_minutes) {
    payload += ",\"period_minutes\":";
    payload += String(period_minutes);
  }
  if (points) {
    payload += ",\"points\":";
    payload += String(points);
  }
  payload += ",\"values\":[]}";
  return payload;
}

static bool extract_json_string_field(const char* json, const char* key, String& out) {
  out = "";
  if (!json || !*json || !key || !*key) return false;

  String pattern = "\"";
  pattern += key;
  pattern += "\"";

  const char* found = strstr(json, pattern.c_str());
  if (!found) return false;

  const char* colon = strchr(found + pattern.length(), ':');
  if (!colon) return false;

  const char* q1 = strchr(colon + 1, '\"');
  if (!q1) return false;

  const char* q2 = strchr(q1 + 1, '\"');
  if (!q2 || q2 <= q1 + 1) return false;

  out = String(q1 + 1);
  out.remove(static_cast<unsigned int>(q2 - (q1 + 1)));
  out.trim();
  return out.length() > 0;
}

static void clear_pending_history_request(const char* entity_id) {
  if (!entity_id || !*entity_id) return;
  for (uint8_t i = 0; i < kHistoryPendingSlots; ++i) {
    if (!g_pending_history[i].active) continue;
    if (g_pending_history[i].entity_id.equalsIgnoreCase(entity_id)) {
      g_pending_history[i].active = false;
      g_pending_history[i].entity_id = "";
      g_pending_history[i].requested_at_ms = 0;
      g_pending_history[i].hours = 24;
      g_pending_history[i].period_minutes = 5;
      g_pending_history[i].points = 288;
    }
  }
}

static void mark_pending_history_request(const char* entity_id,
                                         uint32_t now_ms,
                                         uint16_t hours,
                                         uint16_t period_minutes,
                                         uint16_t points) {
  if (!entity_id || !*entity_id) return;

  int free_idx = -1;
  int oldest_idx = -1;
  uint32_t oldest_ms = 0xFFFFFFFFu;

  for (uint8_t i = 0; i < kHistoryPendingSlots; ++i) {
    if (g_pending_history[i].active) {
      if (g_pending_history[i].entity_id.equalsIgnoreCase(entity_id)) {
        g_pending_history[i].requested_at_ms = now_ms;
        g_pending_history[i].hours = hours;
        g_pending_history[i].period_minutes = period_minutes;
        g_pending_history[i].points = points;
        return;
      }
      if (g_pending_history[i].requested_at_ms < oldest_ms) {
        oldest_ms = g_pending_history[i].requested_at_ms;
        oldest_idx = i;
      }
    } else if (free_idx < 0) {
      free_idx = i;
    }
  }

  int idx = (free_idx >= 0) ? free_idx : oldest_idx;
  if (idx < 0) return;

  g_pending_history[idx].active = true;
  g_pending_history[idx].entity_id = entity_id;
  g_pending_history[idx].requested_at_ms = now_ms;
  g_pending_history[idx].hours = hours;
  g_pending_history[idx].period_minutes = period_minutes;
  g_pending_history[idx].points = points;
}

static bool queue_history_fallback_for_entity(const char* entity_id,
                                              bool time_valid,
                                              const char* reason,
                                              uint16_t hours,
                                              uint16_t period_minutes,
                                              uint16_t points) {
  if (!entity_id || !*entity_id) return false;

  if (is_external_temp_entity(entity_id)) {
    if (time_valid) {
      store_external_temp_history(millis());
      String local_payload = build_external_temp_history_payload(entity_id, hours, period_minutes, points);
      if (local_payload.length()) {
        queue_sensor_popup_history(entity_id, local_payload.c_str(), local_payload.length());
        queue_tile_graph_history(entity_id, local_payload.c_str(), local_payload.length());
        Serial.printf("[History] %s -> lokale Historie fuer %s (%u Punkte, %uh/%umin)\n",
                      reason ? reason : "Fallback", entity_id, points, hours, period_minutes);
      }
    } else {
      String empty_payload = build_empty_history_payload(entity_id, hours, period_minutes, points);
      queue_sensor_popup_history(entity_id, empty_payload.c_str(), empty_payload.length());
      queue_tile_graph_history(entity_id, empty_payload.c_str(), empty_payload.length());
      Serial.printf("[History] %s -> Zeit ungueltig, leere Historie fuer %s\n",
                    reason ? reason : "Fallback", entity_id);
    }
    return true;
  }

  if (is_internal_tab5_entity(entity_id)) {
    String empty_payload = build_empty_history_payload(entity_id, hours, period_minutes, points);
    queue_sensor_popup_history(entity_id, empty_payload.c_str(), empty_payload.length());
    queue_tile_graph_history(entity_id, empty_payload.c_str(), empty_payload.length());
    Serial.printf("[History] %s -> interne Historie fuer %s leer\n",
                  reason ? reason : "Fallback", entity_id);
    return true;
  }

  return false;
}

static void service_pending_history_fallback() {
  const uint32_t now_ms = millis();
  const bool time_valid = has_valid_local_time_for_history();

  for (uint8_t i = 0; i < kHistoryPendingSlots; ++i) {
    if (!g_pending_history[i].active) continue;
    if ((int32_t)(now_ms - g_pending_history[i].requested_at_ms) <
        static_cast<int32_t>(kHistoryHaResponseTimeoutMs)) {
      continue;
    }

    String entity = g_pending_history[i].entity_id;
    const uint16_t hours = g_pending_history[i].hours;
    const uint16_t period_minutes = g_pending_history[i].period_minutes;
    const uint16_t points = g_pending_history[i].points;
    g_pending_history[i].active = false;
    g_pending_history[i].entity_id = "";
    g_pending_history[i].requested_at_ms = 0;
    g_pending_history[i].hours = 24;
    g_pending_history[i].period_minutes = 5;
    g_pending_history[i].points = 288;

    if (!entity.length()) continue;
    queue_history_fallback_for_entity(entity.c_str(), time_valid, "HA Timeout", hours, period_minutes, points);
  }
}

#if TAB5_HAS_ONEWIRE_DS18X20
static constexpr uint32_t kExternalTempSampleMs = 3000;
static constexpr uint32_t kExternalTempConvertMs = 800;
static constexpr uint32_t kExternalTempDiscoveryRetryMs = 2000;
static constexpr uint8_t kExternalTempMaxFailures = 1;
static constexpr uint32_t kExternalTempMqttRepublishMs = 60000;
static constexpr uint32_t kExternalTempProbeLogMs = 10000;

static OneWire g_onewire_1(1);
static OneWire g_onewire_50(50);
static DallasTemperature g_dallas_1(&g_onewire_1);
static DallasTemperature g_dallas_50(&g_onewire_50);

struct ExternalTempCandidate {
  uint8_t pin;
  OneWire* bus;
  DallasTemperature* sensor;
};

static const ExternalTempCandidate kExternalTempCandidates[] = {
  {50, &g_onewire_50, &g_dallas_50},
  {1, &g_onewire_1, &g_dallas_1},
};

static DallasTemperature* g_external_dallas = nullptr;
static uint8_t g_external_pin = 0xFF;
static DeviceAddress g_external_addr = {0};
static uint8_t g_external_failures = 0;
static bool g_external_pending = false;
static uint32_t g_external_request_ms = 0;
static uint32_t g_external_last_sample_ms = 0;
static uint32_t g_external_last_discovery_ms = 0;
static uint32_t g_external_last_mqtt_ms = 0;
static uint32_t g_external_last_probe_log_ms = 0;
static String g_external_last_payload;

static bool is_supported_ds18x20_family(uint8_t family) {
  return family == 0x28 || family == 0x10 || family == 0x22;
}

static bool init_external_temp_on_bus(const ExternalTempCandidate& candidate,
                                      DeviceAddress out_addr,
                                      bool verbose_log) {
  if (!candidate.bus || !candidate.sensor) return false;

  pinMode(candidate.pin, INPUT_PULLUP);
  delay(2);

  uint8_t addr[8] = {0};
  candidate.bus->reset_search();
  if (!candidate.bus->search(addr)) {
    if (verbose_log) {
      Serial.printf("[OneWire] GPIO %u: kein 1-Wire Geraet gefunden\n",
                    static_cast<unsigned>(candidate.pin));
    }
    return false;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
    if (verbose_log) {
      Serial.printf("[OneWire] GPIO %u: CRC Fehler auf dem Bus\n",
                    static_cast<unsigned>(candidate.pin));
    }
    return false;
  }

  if (!is_supported_ds18x20_family(addr[0])) {
    if (verbose_log) {
      Serial.printf("[OneWire] GPIO %u: unbekannte Family 0x%02X\n",
                    static_cast<unsigned>(candidate.pin),
                    static_cast<unsigned>(addr[0]));
    }
    return false;
  }

  candidate.sensor->begin();
  candidate.sensor->setWaitForConversion(false);
  memcpy(out_addr, addr, sizeof(DeviceAddress));
  candidate.sensor->setResolution(out_addr, 12);
  Serial.printf("[OneWire] DS18x20 gefunden auf GPIO %u (Family 0x%02X)\n",
                static_cast<unsigned>(candidate.pin),
                static_cast<unsigned>(addr[0]));
  return true;
}

static void discover_external_temp_sensor(uint32_t now_ms) {
  if (g_external_dallas) return;
  if (g_external_last_discovery_ms != 0 &&
      (int32_t)(now_ms - g_external_last_discovery_ms) < static_cast<int32_t>(kExternalTempDiscoveryRetryMs)) {
    return;
  }
  g_external_last_discovery_ms = now_ms;
  const bool verbose_log =
    (g_external_last_probe_log_ms == 0) ||
    ((int32_t)(now_ms - g_external_last_probe_log_ms) >= static_cast<int32_t>(kExternalTempProbeLogMs));
  if (verbose_log) {
    g_external_last_probe_log_ms = now_ms;
  }

  DeviceAddress addr = {0};
  for (const auto& candidate : kExternalTempCandidates) {
    if (!candidate.sensor || !candidate.bus) continue;
    if (init_external_temp_on_bus(candidate, addr, verbose_log)) {
      g_external_dallas = candidate.sensor;
      g_external_pin = candidate.pin;
      memcpy(g_external_addr, addr, sizeof(DeviceAddress));
      g_external_failures = 0;
      g_external_pending = false;
      return;
    }
  }

  if (verbose_log) {
    Serial.println("[OneWire] Kein DS18x20 auf GPIO 1/50 gefunden (DATA + 4.7k Pull-up zu 3V3 pruefen).");
  }
}

static void service_external_temp_sensor() {
  const uint32_t now_ms = millis();
  discover_external_temp_sensor(now_ms);
  if (!g_external_dallas) {
    g_external_temp_valid = false;
    return;
  }

  if (!g_external_pending) {
    if (g_external_last_sample_ms != 0 &&
        (int32_t)(now_ms - g_external_last_sample_ms) < static_cast<int32_t>(kExternalTempSampleMs)) {
      return;
    }
    g_external_dallas->requestTemperaturesByAddress(g_external_addr);
    g_external_request_ms = now_ms;
    g_external_pending = true;
    return;
  }

  if ((int32_t)(now_ms - g_external_request_ms) < static_cast<int32_t>(kExternalTempConvertMs)) {
    return;
  }

  g_external_pending = false;
  g_external_last_sample_ms = now_ms;

  const float value_c = g_external_dallas->getTempC(g_external_addr);
  if (value_c == DEVICE_DISCONNECTED_C || value_c < -55.0f || value_c > 125.0f) {
    g_external_temp_valid = false;
    if (g_external_failures < 255) ++g_external_failures;
    if (g_external_failures >= kExternalTempMaxFailures) {
      Serial.println("[OneWire] Sensor getrennt, starte Neusuche...");
      g_external_dallas = nullptr;
      g_external_pin = 0xFF;
      g_external_failures = 0;
      g_external_last_discovery_ms = now_ms - kExternalTempDiscoveryRetryMs;
    }
    return;
  }

  g_external_failures = 0;
  g_external_temp_valid = true;
  g_external_temp_c = value_c;
}
#else
static constexpr uint32_t kExternalTempMqttRepublishMs = 60000;
static uint32_t g_external_last_mqtt_ms = 0;
static String g_external_last_payload;

static void service_external_temp_sensor() {
  static uint32_t last_warn_ms = 0;
  const uint32_t now_ms = millis();
  if (last_warn_ms == 0 ||
      (int32_t)(now_ms - last_warn_ms) >= 10000) {
    Serial.println("[OneWire] OneWire/DallasTemperature nicht gefunden, externer DS18x20 Sensor deaktiviert.");
    last_warn_ms = now_ms;
  }
  g_external_temp_valid = false;
}
#endif

static void sync_external_temp_entity(bool publish_mqtt) {
  service_external_temp_sensor();
  const uint32_t now_ms = millis();
  store_external_temp_history(now_ms);

  String sensor_name = "Waveshare Intern DS18x20 Temperatur (GPIO 1/50)";
#if TAB5_HAS_ONEWIRE_DS18X20
  if (g_external_pin != 0xFF) {
    sensor_name = "Waveshare Intern DS18x20 Temperatur (GPIO ";
    sensor_name += String(static_cast<unsigned>(g_external_pin));
    sensor_name += ")";
  }
#endif
  haBridgeConfig.registerSensorMeta(kEntityExternalTemperature, sensor_name, "C");
  haBridgeConfig.updateEntityMeta(kEntityExternalTemperature, sensor_name, "C", "thermometer");

  char temp_payload[24];
  const char* payload = "unavailable";
  if (g_external_temp_valid && !std::isnan(g_external_temp_c) && !std::isinf(g_external_temp_c)) {
    dtostrf(g_external_temp_c, 0, 1, temp_payload);
    payload = temp_payload;
  }

  haBridgeConfig.updateSensorValue(kEntityExternalTemperature, payload);
  if (g_external_last_grid_payload != payload ||
      g_external_last_grid_ms == 0 ||
      (int32_t)(now_ms - g_external_last_grid_ms) >= static_cast<int32_t>(kExternalTempGridRefreshMs)) {
    update_all_grids(kEntityExternalTemperature, payload);
    g_external_last_grid_payload = payload;
    g_external_last_grid_ms = now_ms;
  }

  if (!publish_mqtt) return;
  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  if (g_external_last_payload == payload &&
      (int32_t)(now_ms - g_external_last_mqtt_ms) < static_cast<int32_t>(kExternalTempMqttRepublishMs)) {
    return;
  }

  String topic = buildHaStatestreamTopic(kEntityExternalTemperature, "state");
  mqtt.publish(topic.c_str(), payload, true);
  g_external_last_payload = payload;
  g_external_last_mqtt_ms = now_ms;

#if TAB5_HAS_ONEWIRE_DS18X20
  static bool reported_pin = false;
  if (!reported_pin && g_external_pin != 0xFF) {
    Serial.printf("[OneWire] MQTT Publish auf %s (GPIO %u)\n",
                  topic.c_str(),
                  static_cast<unsigned>(g_external_pin));
    reported_pin = true;
  }
#endif
}

static int readBatterySocPercent() {
  batteryStateUpdate();
  const BatteryTelemetry& batt = batteryStateGet();

  if (batt.level_valid && batt.level_pct >= 0 && batt.level_pct <= 100) {
    g_soc_pct = batt.level_pct;
  } else if (g_soc_pct < 0 &&
             !batt.battery_missing &&
             batt.raw_level_pct >= 0 &&
             batt.raw_level_pct <= 100) {
    // One-time seed on startup only. Do not continuously fall back to raw,
    // otherwise short raw spikes can create vertical jumps in history graphs.
    g_soc_pct = batt.raw_level_pct;
  }

  if (g_soc_pct < 0) return 0;
  if (g_soc_pct > 100) return 100;
  return g_soc_pct;
}

static void sync_internal_battery_entity() {
  if (batteryStateIsBatteryMissing()) {
    return;
  }
  const int soc = readBatterySocPercent();
  char soc_payload[8];
  snprintf(soc_payload, sizeof(soc_payload), "%d", soc);

  const char* sensor_name = "WS_P4 Intern Batterie SoC";
  haBridgeConfig.registerSensorMeta(kEntityInternalBatterySoc, sensor_name, "%");
  haBridgeConfig.updateEntityMeta(kEntityInternalBatterySoc, sensor_name, "%", "battery");
  haBridgeConfig.updateSensorValue(kEntityInternalBatterySoc, soc_payload);
  update_all_grids(kEntityInternalBatterySoc, soc_payload);
}

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

static constexpr uint8_t kDisplayBrightnessMin = 121;
static constexpr uint8_t kDisplayBrightnessMax = 255;

static bool entityEquals(const char* entity_id, const char* expected) {
  if (!entity_id || !expected) return false;
  String a(entity_id);
  String b(expected);
  a.trim();
  b.trim();
  return a.equalsIgnoreCase(b);
}

static int brightnessPctFromRaw(int raw) {
  if (raw < kDisplayBrightnessMin) raw = kDisplayBrightnessMin;
  if (raw > kDisplayBrightnessMax) raw = kDisplayBrightnessMax;
  const int span = kDisplayBrightnessMax - kDisplayBrightnessMin;
  if (span <= 0) return 100;
  return 1 + static_cast<int>((static_cast<long>(raw - kDisplayBrightnessMin) * 99L + (span / 2)) / span);
}

static uint8_t brightnessRawFromPct(int pct) {
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  const int span = kDisplayBrightnessMax - kDisplayBrightnessMin;
  int raw = kDisplayBrightnessMin + static_cast<int>((static_cast<long>(pct - 1) * span + 49L) / 99L);
  if (raw < kDisplayBrightnessMin) raw = kDisplayBrightnessMin;
  if (raw > kDisplayBrightnessMax) raw = kDisplayBrightnessMax;
  return static_cast<uint8_t>(raw);
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

  BoardHAL::setBrightness(value);

  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      static_cast<uint8_t>(value),
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      cfg.display_rotation_mode,
      cfg.display_rotated_180,
      cfg.display_rotation_quarters,
      cfg.wake_mode_mains,
      cfg.wake_mode_battery);
  mqttPublishDeviceSettings();
}

static void handleDisplayRotateCommand(const char* payload, size_t) {
  bool rotate = false;
  if (!parseBoolPayload(payload, &rotate)) return;
  displayManager.setRotationFlipped(rotate);
  settings_sync_display_rotation(rotate);

  const DeviceConfig& cfg = configManager.getConfig();
  uint8_t rotation_quarters = rotate ? Device::kRotationFlipped : Device::kRotationDefault;
  uint8_t rotation_mode = rotate ? kDisplayRotationFlipped : kDisplayRotationNormal;
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      rotation_mode,
      rotate,
      rotation_quarters,
      cfg.wake_mode_mains,
      cfg.wake_mode_battery);
  mqttPublishDeviceSettings();
}

static void handleDisplaySleepCommand(const char* payload, size_t) {
  bool sleep = false;
  if (!parseBoolPayload(payload, &sleep)) return;
  if (sleep) {
    powerManager.enterDisplaySleep();
  } else {
    powerManager.wakeFromDisplaySleep();
  }
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
      cfg.display_rotation_mode,
      cfg.display_rotated_180,
      cfg.display_rotation_quarters,
      cfg.wake_mode_mains,
      cfg.wake_mode_battery);
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
      cfg.display_rotation_mode,
      cfg.display_rotated_180,
      cfg.display_rotation_quarters,
      cfg.wake_mode_mains,
      cfg.wake_mode_battery);
  mqttPublishDeviceSettings();
}

static void sync_local_device_entities(bool publish_mqtt) {
  const DeviceConfig& cfg = configManager.getConfig();

  const int bright_pct = brightnessPctFromRaw(static_cast<int>(cfg.display_brightness));
  char bright_payload[96];
  snprintf(bright_payload, sizeof(bright_payload), "{\"state\":\"on\",\"brightness_pct\":%d}", bright_pct);

  haBridgeConfig.updateSensorValue(kEntityDisplayBrightness, bright_payload);
  haBridgeConfig.updateEntityMeta(kEntityDisplayBrightness, "Display Helligkeit", "%", "brightness-6");

  const char* rotate_state = cfg.display_rotated_180 ? "ON" : "OFF";
  haBridgeConfig.updateSensorValue(kEntityDisplayRotate, rotate_state);
  haBridgeConfig.updateEntityMeta(kEntityDisplayRotate, "Display Rotation", "", "screen-rotation");

  const char* sleep_state = powerManager.isInSleep() ? "ON" : "OFF";
  haBridgeConfig.updateSensorValue(kEntityDisplaySleep, sleep_state);
  haBridgeConfig.updateEntityMeta(kEntityDisplaySleep, "Display Sleep", "", "sleep");

  update_all_grids(kEntityDisplayBrightness, bright_payload);
  update_all_grids(kEntityDisplayRotate, rotate_state);
  update_all_grids(kEntityDisplaySleep, sleep_state);
  sync_internal_battery_entity();
  sync_external_temp_entity(publish_mqtt);
}

static bool resolve_toggle_action(const char* action, bool current, bool* desired) {
  if (!desired) return false;
  if (!action || !*action) {
    *desired = !current;
    return true;
  }
  String s(action);
  s.trim();
  s.toLowerCase();
  if (s == "toggle") {
    *desired = !current;
    return true;
  }
  return parseBoolPayload(s.c_str(), desired);
}

static bool handle_local_switch_command(const char* entity_id, const char* action) {
  if (entityEquals(entity_id, kEntityDisplayBrightness)) {
    sync_local_device_entities(false);
    return true;
  }
  if (entityEquals(entity_id, kEntityDisplayRotate)) {
    bool desired = false;
    const DeviceConfig& cfg = configManager.getConfig();
    if (!resolve_toggle_action(action, cfg.display_rotated_180, &desired)) return true;
    handleDisplayRotateCommand(desired ? "ON" : "OFF", 0);
    return true;
  }
  if (entityEquals(entity_id, kEntityDisplaySleep)) {
    bool desired = false;
    if (!resolve_toggle_action(action, powerManager.isInSleep(), &desired)) return true;
    handleDisplaySleepCommand(desired ? "ON" : "OFF", 0);
    return true;
  }
  return false;
}

static bool handle_local_light_command(const char* entity_id, const char* state, int brightness_pct) {
  if (!entityEquals(entity_id, kEntityDisplayBrightness)) return false;
  if (brightness_pct >= 0) {
    uint8_t raw = brightnessRawFromPct(brightness_pct);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(raw));
    handleDisplayBrightnessCommand(buf, 0);
  } else {
    (void)state;
    sync_local_device_entities(false);
  }
  return true;
}

static const TopicRoute kRoutes[] = {
  {TopicKey::SENSOR_OUT, handleOutside, false},
  {TopicKey::SENSOR_IN, handleInside, false},
  {TopicKey::SENSOR_SOC, handleSoc, false},
  {TopicKey::SCENE_CMND, handleSceneCommand, false},
  {TopicKey::HA_WOHN_TEMP, handleHaWohnTemp, false},
  {TopicKey::DISPLAY_BRIGHTNESS_CMND, handleDisplayBrightnessCommand, false},
  {TopicKey::DISPLAY_ROTATE_CMND, handleDisplayRotateCommand, false},
  {TopicKey::DISPLAY_SLEEP_CMND, handleDisplaySleepCommand, false},
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
      sync_local_device_entities(false);
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
    String response_entity;
    if (extract_json_string_field(large_buf, "entity_id", response_entity) && response_entity.length()) {
      clear_pending_history_request(response_entity.c_str());
    }
    queue_sensor_popup_history(nullptr, large_buf, copy_len);
    queue_tile_graph_history(nullptr, large_buf, copy_len);
    return;
  }

  const char* energy_topic = networkManager.getEnergyResponseTopic();
  if (energy_topic && strcmp(topic, energy_topic) == 0) {
    size_t copy_len = length < (LARGE_BUF - 1) ? length : (LARGE_BUF - 1);
    memcpy(large_buf, payload, copy_len);
    large_buf[copy_len] = '\0';
    queue_energy_response(large_buf, copy_len);
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

  snprintf(buf, sizeof(buf), "%d", readBatterySocPercent());
  mqtt.publish(mqttTopics.topic(TopicKey::SENSOR_SOC), buf, true);
}

// ========== Device Settings publizieren ==========
void mqttPublishDeviceSettings() {
  sync_local_device_entities(false);
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
  publish_state(TopicKey::DISPLAY_SLEEP_STAT, powerManager.isInSleep() ? "ON" : "OFF");

  publish_state(TopicKey::SLEEP_MAINS_STAT,
                sleepLabelFromConfig(cfg.auto_sleep_enabled, cfg.auto_sleep_seconds));
  publish_state(TopicKey::SLEEP_BAT_STAT,
                sleepLabelFromConfig(cfg.auto_sleep_battery_enabled, cfg.auto_sleep_battery_seconds));
}

void mqttServiceLocalSensors() {
  service_pending_history_fallback();

  static uint32_t last_run_ms = 0;
  const uint32_t now_ms = millis();
  if (last_run_ms != 0 && (int32_t)(now_ms - last_run_ms) < 500) {
    return;
  }
  last_run_ms = now_ms;
  sync_internal_battery_entity();
  sync_external_temp_entity(true);
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
  if (handle_local_switch_command(entity_id, state)) return;

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

void mqttPublishLightCommand(const char* entity_id,
                             const char* state,
                             int brightness_pct,
                             bool has_color,
                             uint32_t color,
                             int color_temp_kelvin) {
  if (!entity_id || !*entity_id) return;
  if (handle_local_light_command(entity_id, state, brightness_pct)) return;

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

  if (color_temp_kelvin > 0) {
    payload += ",\"color_temp_kelvin\":";
    payload += color_temp_kelvin;
  }

  payload += "}";

  bool ok = mqtt.publish(topic, payload.c_str(), false);
  Serial.printf("Light command -> MQTT '%s' (%s)\n", topic, ok ? "ok" : "fail");
}

void mqttPublishHistoryRequest(const char* entity_id,
                               uint16_t hours,
                               uint16_t period_minutes,
                               uint16_t points) {
  if (!entity_id || !*entity_id) return;

  if (hours == 0) hours = 24;
  if (period_minutes == 0) period_minutes = 5;
  if (points == 0) {
    points = static_cast<uint16_t>((static_cast<uint32_t>(hours) * 60U) / period_minutes);
  }
  if (points == 0) points = 1;

  PubSubClient& mqtt = networkManager.getMqttClient();
  const bool mqtt_online = mqtt.connected();
  const char* history_topic = networkManager.getHistoryRequestTopic();
  const bool can_request_ha = mqtt_online && history_topic && *history_topic;
  const bool time_valid = has_valid_local_time_for_history();

  // Vorherige Pending-Eintraege fuer dieselbe Entity verwerfen.
  clear_pending_history_request(entity_id);

  // HA hat Prioritaet: wenn erreichbar, zuerst dort Historie holen.
  if (can_request_ha) {
    if (!time_valid && is_internal_tab5_entity(entity_id)) {
      Serial.printf("[History] Zeit lokal ungueltig, fordere HA-Historie fuer %s an\n", entity_id);
    }

    String payload = "{\"entity_id\":\"";
    payload += entity_id;
    payload += "\",\"hours\":";
    payload += String(hours);
    payload += ",\"period_minutes\":";
    payload += String(period_minutes);
    payload += ",\"points\":";
    payload += String(points);
    payload += ",\"stat\":\"mean\"}";
    bool ok = mqtt.publish(history_topic, payload.c_str(), false);
    Serial.printf("History request -> MQTT '%s' (%s)\n", history_topic, ok ? "ok" : "fail");
    if (ok) {
      mark_pending_history_request(entity_id, millis(), hours, period_minutes, points);
      return;
    }
    queue_history_fallback_for_entity(entity_id, time_valid, "HA Publish fehlgeschlagen",
                                      hours, period_minutes, points);
    return;
  }

  if (queue_history_fallback_for_entity(entity_id, time_valid, "HA nicht verfuegbar",
                                        hours, period_minutes, points)) {
    return;
  }

  if (!mqtt_online) {
    Serial.printf("History request skipped (MQTT offline): %s\n", entity_id);
  } else if (!history_topic || !*history_topic) {
    Serial.printf("History request skipped (no topic): %s\n", entity_id);
  }
}

void mqttPublishWeatherRequest(const char* entity_id) {
  if (!entity_id || !*entity_id) return;

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) return;

  const char* weather_topic = networkManager.getWeatherRequestTopic();
  if (!weather_topic || !*weather_topic) return;

  String payload = "{\"entity_id\":\"";
  payload += entity_id;
  payload += "\"}";

  bool ok = mqtt.publish(weather_topic, payload.c_str(), false);
  Serial.printf("Weather request -> MQTT '%s' (%s)\n", weather_topic, ok ? "ok" : "fail");
}

void mqttPublishEnergyRequest(const char* period) {
  const char* p = (period && *period) ? period : "day";
  if (strcmp(p, "week") != 0 && strcmp(p, "month") != 0) {
    p = "day";
  }

  PubSubClient& mqtt = networkManager.getMqttClient();
  if (!mqtt.connected()) {
    Serial.printf("Energy request skipped (MQTT offline): %s\n", p);
    return;
  }

  const char* energy_topic = networkManager.getEnergyRequestTopic();
  if (!energy_topic || !*energy_topic) {
    Serial.printf("Energy request skipped (no topic): %s\n", p);
    return;
  }

  String payload = "{\"period\":\"";
  payload += p;
  payload += "\"}";

  bool ok = mqtt.publish(energy_topic, payload.c_str(), false);
  Serial.printf("Energy request -> MQTT '%s' period=%s (%s)\n",
                energy_topic,
                p,
                ok ? "ok" : "fail");
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
    "{\"name\":\"Waveshare Outside\",\"stat_t\":\"%s\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_out\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Waveshare P4 Panel\",\"mf\":\"Waveshare\",\"mdl\":\"ESP32-P4-WIFI6-Touch-LCD-4B\"}}",
    mqttTopics.topic(TopicKey::SENSOR_OUT), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_inside_c/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Waveshare Inside\",\"stat_t\":\"%s\",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\",\"stat_cla\":\"measurement\",\"uniq_id\":\"%s_in\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"Waveshare P4 Panel\",\"mf\":\"Waveshare\",\"mdl\":\"ESP32-P4-WIFI6-Touch-LCD-4B\"}}",
    mqttTopics.topic(TopicKey::SENSOR_IN), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  // Legacy Discovery entfernen: diese Sensoren kommen jetzt konsistent ueber die
  // HA-Integration (tab5_lvgl) und sonst entstuenden Dubletten.
  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_external_c/config", did);
  mqtt.publish(tpc, "", true);
  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_soc_pct/config", did);
  mqtt.publish(tpc, "", true);

  snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_uptime/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Waveshare Uptime\",\"stat_t\":\"%s\",\"unit_of_meas\":\"s\",\"uniq_id\":\"%s_up\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::TELE_UP), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_abend/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Waveshare Scene Abend\",\"cmd_t\":\"%s\",\"pl_prs\":\"Abend\",\"uniq_id\":\"%s_btn_abend\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::SCENE_CMND), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_lesen/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Waveshare Scene Lesen\",\"cmd_t\":\"%s\",\"pl_prs\":\"Lesen\",\"uniq_id\":\"%s_btn_lesen\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
    mqttTopics.topic(TopicKey::SCENE_CMND), did, stat_topic, did);
  mqtt.publish(tpc, js, true);

  snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_scene_allesaus/config", did);
  snprintf(js, sizeof(js),
    "{\"name\":\"Waveshare Scene Alles Aus\",\"cmd_t\":\"%s\",\"pl_prs\":\"AllesAus\",\"uniq_id\":\"%s_btn_allesaus\",\"avty_t\":\"%s\",\"pl_avail\":\"1\",\"pl_not_avail\":\"0\",\"dev\":{\"ids\":[\"%s\"]}}",
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

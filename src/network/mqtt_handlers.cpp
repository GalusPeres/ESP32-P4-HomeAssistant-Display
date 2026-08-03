#include "src/network/mqtt_handlers.h"
#include "src/network/mqtt_topics.h"
#include "src/network/network_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/screensaver_config.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/camera_popup.h"
#include "src/ui/image_screensaver.h"
#include "src/video/camera_geometry.h"
#include "src/ui/tab_settings.h"
#include "src/types/energy/energy_data.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/core/i18n.h"
#include "src/core/device_entities.h"
#include "src/core/power_manager.h"
#include "src/core/battery_state.h"
#include "src/core/board_hal.h"
#include "src/core/lvgl_tick_service.h"
#include "src/io/hardware_io.h"
#include "src/web/web_admin.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <time.h>
#include <vector>

#ifndef TAB5_HAS_ONEWIRE_DS18X20
#define TAB5_HAS_ONEWIRE_DS18X20 0
#endif

#if defined(__has_include) && !defined(CONFIG_IDF_TARGET_ESP32P4)
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
static uint8_t* g_external_history_storage = nullptr;
static float* g_external_history_values = nullptr;
static bool* g_external_history_valid = nullptr;
static uint16_t g_external_history_head = 0;
static uint16_t g_external_history_count = 0;
static uint32_t g_external_history_last_store_ms = 0;
static constexpr uint32_t kHistoryHaResponseTimeoutMs = 2000;
static constexpr uint8_t kHistoryPendingSlots = 8;
static constexpr uint32_t kDynamicSlotReloadQuietMs = 3000;
static constexpr uint32_t kDynamicSlotReloadAdminQuietMs = 5000;

static volatile bool g_dynamic_slots_reload_requested = false;
static uint32_t g_dynamic_slots_reload_due_ms = 0;
static bool g_bridge_initial_sync_complete = false;

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
  if (normalized.equalsIgnoreCase(kEntityScreensaverBrightness)) return true;
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

static bool ensure_external_temp_history_storage() {
  if (g_external_history_storage) return true;

  constexpr size_t kValuesBytes =
      sizeof(float) * static_cast<size_t>(kExternalTempHistoryPoints);
  constexpr size_t kValidBytes =
      sizeof(bool) * static_cast<size_t>(kExternalTempHistoryPoints);
  g_external_history_storage = static_cast<uint8_t*>(heap_caps_calloc(
      1, kValuesBytes + kValidBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!g_external_history_storage) {
    static bool logged = false;
    if (!logged) {
      logged = true;
      Serial.printf("[History] Kein PSRAM fuer lokalen Temperaturverlauf (%u Bytes)\n",
                    static_cast<unsigned>(kValuesBytes + kValidBytes));
    }
    return false;
  }

  g_external_history_values =
      reinterpret_cast<float*>(g_external_history_storage);
  g_external_history_valid =
      reinterpret_cast<bool*>(g_external_history_storage + kValuesBytes);
  return true;
}

static void push_external_temp_history(float value, bool valid) {
  if (!ensure_external_temp_history_storage()) return;
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
    const bool fallback_queued = queue_history_fallback_for_entity(
        entity.c_str(), time_valid, "HA Timeout", hours, period_minutes, points);
    if (!fallback_queued) {
      Serial.printf("[History] HA Timeout fuer %s (kein lokaler Fallback)\n",
                    entity.c_str());
    }
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
  static bool warned = false;
  if (!warned) {
    warned = true;
    Serial.println("[OneWire] OneWire/DallasTemperature nicht gefunden, externer DS18x20 Sensor deaktiviert.");
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
  if (!networkManager.isMqttConnected()) return;

  if (g_external_last_payload == payload &&
      (int32_t)(now_ms - g_external_last_mqtt_ms) < static_cast<int32_t>(kExternalTempMqttRepublishMs)) {
    return;
  }

  String topic = buildHaStatestreamTopic(kEntityExternalTemperature, "state");
  networkManager.mqttEnqueuePublish(topic.c_str(), payload, true);
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
  if (value < kDisplayBrightnessMin) value = kDisplayBrightnessMin;
  if (value > 255) value = 255;

  // Eine HA-Aenderung der Normalhelligkeit darf einen sichtbaren
  // Screensaver nicht aufhellen. Im Sleep wird nur der sichere Wake-Wert
  // aktualisiert, das Backlight bleibt aus.
  if (!is_image_screensaver_visible()) {
    powerManager.setDisplayBrightness(static_cast<uint8_t>(value));
  }

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

static void handleScreensaverBrightnessCommand(const char* payload, size_t) {
  if (!payload || !*payload) return;
  int percent = atoi(payload);
  if (percent < kScreensaverBrightnessPctMin) {
    percent = kScreensaverBrightnessPctMin;
  }
  if (percent > kScreensaverBrightnessPctMax) {
    percent = kScreensaverBrightnessPctMax;
  }

  configManager.saveScreensaverBrightness(static_cast<uint8_t>(percent));
  image_screensaver_brightness_changed();
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
    powerManager.wakeFromDisplaySleep("mqtt-cmd");
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

  char screensaver_bright_payload[96];
  snprintf(screensaver_bright_payload, sizeof(screensaver_bright_payload),
           "{\"state\":\"on\",\"brightness_pct\":%u}",
           static_cast<unsigned>(cfg.screensaver_brightness_pct));
  haBridgeConfig.updateSensorValue(kEntityScreensaverBrightness,
                                   screensaver_bright_payload);
  haBridgeConfig.updateEntityMeta(kEntityScreensaverBrightness,
                                  "Screensaver Helligkeit", "%",
                                  "brightness-4");

  const char* rotate_state = cfg.display_rotated_180 ? "ON" : "OFF";
  haBridgeConfig.updateSensorValue(kEntityDisplayRotate, rotate_state);
  haBridgeConfig.updateEntityMeta(kEntityDisplayRotate, "Display Rotation", "", "screen-rotation");

  const char* sleep_state = powerManager.isInSleep() ? "ON" : "OFF";
  haBridgeConfig.updateSensorValue(kEntityDisplaySleep, sleep_state);
  haBridgeConfig.updateEntityMeta(kEntityDisplaySleep, "Display Sleep", "", "sleep");

  update_all_grids(kEntityDisplayBrightness, bright_payload);
  update_all_grids(kEntityScreensaverBrightness,
                   screensaver_bright_payload);
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
  if (hardwareIo.handleLocalEntityCommand(entity_id, action)) return true;
  if (entityEquals(entity_id, kEntityDisplayBrightness)) {
    sync_local_device_entities(false);
    return true;
  }
  if (entityEquals(entity_id, kEntityScreensaverBrightness)) {
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
  if (entityEquals(entity_id, kEntityDisplayBrightness)) {
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
  if (entityEquals(entity_id, kEntityScreensaverBrightness)) {
    if (brightness_pct >= 0) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%d", brightness_pct);
      handleScreensaverBrightnessCommand(buf, 0);
    } else {
      (void)state;
      sync_local_device_entities(false);
    }
    return true;
  }
  return false;
}

static void handleCameraStatus(const char* payload, size_t) {
  camera_popup_handle_mqtt_status(payload);
}

static const TopicRoute kRoutes[] = {
  {TopicKey::SENSOR_OUT, handleOutside, false},
  {TopicKey::SENSOR_IN, handleInside, false},
  {TopicKey::SENSOR_SOC, handleSoc, false},
  {TopicKey::SCENE_CMND, handleSceneCommand, false},
  {TopicKey::HA_WOHN_TEMP, handleHaWohnTemp, false},
  {TopicKey::DISPLAY_BRIGHTNESS_CMND, handleDisplayBrightnessCommand, false},
  {TopicKey::SCREENSAVER_BRIGHTNESS_CMND, handleScreensaverBrightnessCommand, false},
  {TopicKey::DISPLAY_ROTATE_CMND, handleDisplayRotateCommand, false},
  {TopicKey::DISPLAY_SLEEP_CMND, handleDisplaySleepCommand, false},
  {TopicKey::SLEEP_MAINS_CMND, handleSleepMainsCommand, false},
  {TopicKey::SLEEP_BAT_CMND, handleSleepBatteryCommand, false},
  {TopicKey::CAMERA_STAT, handleCameraStatus, true},
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

  auto add_route = [&](const String& entity, int slot_index, const char* suffix = "state") {
    String ent = entity;
    ent.trim();
    if (!ent.length()) return;
    // Physische Panel-I/O-Entities verwenden dieselbe ID wie in HA, werden
    // auf diesem Panel aber direkt aktualisiert und gesteuert. Eine parallele
    // HA-Statestream-Subscription waere redundant und koennte alten State
    // ueber den echten GPIO-State schreiben.
    if (hardwareIo.isLocalEntityId(ent.c_str())) return;

    String topic = buildHaStatestreamTopic(ent, suffix);
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

  // Sensor tiles from ALL folders (no slot index, entity-based update). Geht
  // ueber den PSRAM-Ordner-Entity-Cache von TileConfig -- der Flash-Read pro
  // Ordner (~20ms) faellt nur beim ersten Scan bzw. nach einer Grid-Aenderung
  // an, danach ist der komplette Ordner-Durchlauf praktisch kostenlos.
  bool has_media_tiles = false;
  auto add_grid_entities = [&](const FolderEntitySlotView* slots, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      const FolderEntitySlotView& slot = slots[i];
      if ((slot.type == TILE_SENSOR || slot.type == TILE_ENERGY ||
           slot.type == TILE_SWITCH || slot.type == TILE_MEDIA ||
           slot.type == TILE_CLIMATE) &&
          slot.entity[0]) {
        add_route(String(slot.entity), -1);
        if (slot.type == TILE_MEDIA) {
          has_media_tiles = true;
          // New bridges publish cover-free state changes here first. Keep the
          // normal state subscription as well for retained full payloads and
          // compatibility with older bridge versions.
          add_route(String(slot.entity), -1, "state_fast");
        }
      }
    }
  };

  // Scan all folders, not just the active one
  const std::vector<FolderEntry>& folders = tileConfig.getFolders();
  FolderEntitySlotView slots[TILES_PER_GRID];
  for (const auto& folder : folders) {
    uint32_t t_load0 = millis();
    bool loaded = tileConfig.getFolderEntitiesCached(folder.id, slots, TILES_PER_GRID);
    uint32_t load_ms = millis() - t_load0;
    if (load_ms >= 5) {
      Serial.printf("[Bridge]   getFolderEntitiesCached(%u): %u ms\n", folder.id, (unsigned)load_ms);
    }
    if (loaded) {
      add_grid_entities(slots, TILES_PER_GRID);
    }
    // This runs synchronously on the loop task (mqtt_process_inbound_queue ->
    // processMqttMessage -> mqttReloadDynamicSlots), before lv_timer_handler()
    // gets its turn at the end of loop(). On a cache miss (first scan / after
    // a grid change) each folder still costs a flash read -- servicing LVGL
    // between folders keeps the UI breathing during the reload.
    lvglServiceDuringBlockingWork();
  }

  // Das Screensaver-Grid liegt absichtlich ausserhalb der Ordnerstruktur.
  // Seine Entitys muessen deshalb separat in dieselben MQTT-Routen eingehen.
  const TileGridConfig& screensaver_grid = screensaverConfig.tileGrid();
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = screensaver_grid.tiles[i];
    if ((tile.type != TILE_SENSOR && tile.type != TILE_ENERGY &&
         tile.type != TILE_SWITCH &&
         tile.type != TILE_MEDIA) ||
        !tile.sensor_entity.length()) {
      continue;
    }
    add_route(tile.sensor_entity, -1);
    if (tile.type == TILE_MEDIA) {
      has_media_tiles = true;
      add_route(tile.sensor_entity, -1, "state_fast");
    }
  }

  // Media-States mit eingebettetem Cover (~19 KB) brauchen einen groesseren
  // Empfangspuffer als die 16-KB-Basis; der Worker gleicht die Groesse an.
  networkManager.setMqttMediaBufferNeeded(has_media_tiles);
}

// Boot-Scan fuer setup(): steht schon VOR networkManager.init() ein Media-Tile
// in der gespeicherten Konfiguration, wird der MQTT-Puffer gleich in der
// passenden Groesse angelegt -- das retained Cover direkt nach dem ersten
// Subscribe ginge sonst verloren (Grow waere erst nach dem Sturmfenster dran).
bool mqttAnyMediaTileConfigured() {
  const TileGridConfig& screensaver_grid = screensaverConfig.tileGrid();
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = screensaver_grid.tiles[i];
    if (tile.type == TILE_MEDIA && tile.sensor_entity.length()) return true;
  }
  const std::vector<FolderEntry>& folders = tileConfig.getFolders();
  FolderEntitySlotView slots[TILES_PER_GRID];
  for (const auto& folder : folders) {
    if (!tileConfig.getFolderEntitiesCached(folder.id, slots, TILES_PER_GRID)) continue;
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      if (slots[i].type == TILE_MEDIA && slots[i].entity[0]) return true;
    }
  }
  return false;
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
  FolderEntitySlotView slots[TILES_PER_GRID];
  for (const auto& folder : folders) {
    uint32_t t_load0 = millis();
    bool loaded = tileConfig.getFolderEntitiesCached(folder.id, slots, TILES_PER_GRID);
    uint32_t load_ms = millis() - t_load0;
    if (load_ms >= 5) {
      Serial.printf("[Bridge]   getFolderEntitiesCached(%u): %u ms\n", folder.id, (unsigned)load_ms);
    }
    if (loaded) {
      for (size_t i = 0; i < TILES_PER_GRID; ++i) {
        const FolderEntitySlotView& slot = slots[i];
        if (slot.type == TILE_WEATHER && slot.entity[0]) {
          add_route(String(slot.entity));
        }
      }
    }
    // Same reasoning as rebuildDynamicRoutes() above: give LVGL a turn between
    // folders so this synchronous MQTT-callback reload doesn't stall the screen.
    lvglServiceDuringBlockingWork();
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
static constexpr size_t LARGE_BUF = 32768;
static constexpr size_t CFG_BUF = 32768;
static char small_buf[SMALL_BUF];
static char* g_large_buf = nullptr;
static char* g_cfg_buf = nullptr;

static char* mqttScratchBuffer(char*& slot, size_t bytes, const char* name) {
  if (slot) return slot;
  slot = static_cast<char*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  const char* where = "PSRAM";
  if (!slot) {
    slot = static_cast<char*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT));
    where = "internal";
  }
  if (!slot) {
    Serial.printf("[MQTT] Scratch buffer alloc failed: %s %u bytes\n",
                  name ? name : "?",
                  static_cast<unsigned>(bytes));
    return nullptr;
  }
  Serial.printf("[MQTT] Scratch buffer %s: %u bytes in %s\n",
                name ? name : "?",
                static_cast<unsigned>(bytes),
                where);
  return slot;
}

static char* mqttLargeBuffer() {
  return mqttScratchBuffer(g_large_buf, LARGE_BUF, "large");
}

static char* mqttConfigBuffer() {
  return mqttScratchBuffer(g_cfg_buf, CFG_BUF, "config");
}

// ---------------------------------------------------------------------------
// Inbound MQTT decoupling (Etappe 1 von 3)
//
// PubSubClient::loop() reassembles a whole incoming message (an ~11.6KB bridge
// payload can take ~170ms over several TCP segments) and then calls mqttCallback
// synchronously on whatever task ran loop(). Doing the heavy per-topic work
// right there is what stalls the render loop. Split it: mqttCallback() only
// copies the raw (topic,payload) into a PSRAM-backed message and pushes a
// pointer onto a FreeRTOS queue; mqtt_process_inbound_queue() (called from the
// main loop) drains it and runs the real processing, processMqttMessage(),
// entirely on the loop task. That keeps ALL existing flash / LVGL / shared-state
// access single-threaded on the loop -- so Etappe 2 (moving loop() onto a
// second-core worker) needs no locks on any of it: the worker only ever runs
// the thin enqueue path (socket read + one PSRAM alloc + queue send).
// ---------------------------------------------------------------------------
struct MqttInboundMsg {
  char* topic;       // -> into the same backing allocation
  uint8_t* payload;  // -> into the same backing allocation
  unsigned int length;
};

// Ein kompletter retained Reconnect umfasst aktuell 46 dynamische Topics plus
// Bridge-/Settings-Nachrichten. 16 Eintraege liefen waehrend des 1,3-s-
// Route-Reloads nachweislich voll und warfen u.a. Media-State weg. Die
// eigentlichen Nachrichten liegen im PSRAM; hier wachsen nur die Queue-
// Pointer, daher kann ein kompletter Burst ohne relevanten SRAM-Verbrauch
// aufgenommen werden.
static constexpr size_t kMqttInboundQueueDepth = 64;
static QueueHandle_t g_mqtt_inbound_queue = nullptr;
static MqttInboundMsg* g_deferred_bridge_apply = nullptr;

static void processMqttMessage(char* topic, uint8_t* payload, unsigned int length);

static QueueHandle_t mqttInboundQueue() {
  if (!g_mqtt_inbound_queue) {
    g_mqtt_inbound_queue = xQueueCreate(kMqttInboundQueueDepth, sizeof(MqttInboundMsg*));
    if (!g_mqtt_inbound_queue) {
      Serial.println("[MQTT] Inbound-Queue konnte nicht erstellt werden");
    }
  }
  return g_mqtt_inbound_queue;
}

// One allocation per message, laid out as [MqttInboundMsg][topic\0][payload].
// PSRAM preferred (freed by the drainer after processMqttMessage()).
static MqttInboundMsg* mqttAllocInbound(const char* topic, const uint8_t* payload, unsigned int length) {
  const size_t topic_len = topic ? strlen(topic) : 0;
  const size_t total = sizeof(MqttInboundMsg) + topic_len + 1 + length;
  uint8_t* block = static_cast<uint8_t*>(heap_caps_malloc(total, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!block) block = static_cast<uint8_t*>(heap_caps_malloc(total, MALLOC_CAP_8BIT));
  if (!block) return nullptr;
  MqttInboundMsg* msg = reinterpret_cast<MqttInboundMsg*>(block);
  msg->topic = reinterpret_cast<char*>(block + sizeof(MqttInboundMsg));
  msg->payload = reinterpret_cast<uint8_t*>(msg->topic + topic_len + 1);
  msg->length = length;
  if (topic_len) memcpy(msg->topic, topic, topic_len);
  msg->topic[topic_len] = '\0';
  if (length) memcpy(msg->payload, payload, length);
  return msg;
}

void mqtt_process_inbound_queue(uint8_t max_msgs) {
  QueueHandle_t q = g_mqtt_inbound_queue;  // do NOT lazily create here -- nothing to drain if never used
  uint8_t processed = 0;

  // A bridge/apply payload rebuilds several indexes and schedules hidden tile
  // cache work. Keep MQTT receiving while the camera popup is visible, but
  // retain only the newest full bridge snapshot in PSRAM and apply it after
  // the popup closes. Normal sensor/switch/media messages continue below.
  const bool camera_busy = camera_popup_is_busy();
  if (!camera_busy && g_deferred_bridge_apply &&
      (max_msgs == 0 || processed < max_msgs)) {
    MqttInboundMsg* deferred = g_deferred_bridge_apply;
    g_deferred_bridge_apply = nullptr;
    Serial.printf(
        "[Bridge] Aufgeschobenen Vollabgleich nach Kamera anwenden (%u Bytes)\n",
        deferred->length);
    processMqttMessage(
        deferred->topic, deferred->payload, deferred->length);
    heap_caps_free(deferred);
    ++processed;
  }

  if (!q) return;
  MqttInboundMsg* msg = nullptr;
  while ((max_msgs == 0 || processed < max_msgs) && xQueueReceive(q, &msg, 0) == pdTRUE) {
    if (msg) {
      const char* apply_topic = networkManager.getBridgeApplyTopic();
      if (camera_busy && apply_topic &&
          strcmp(msg->topic, apply_topic) == 0) {
        if (g_deferred_bridge_apply) {
          heap_caps_free(g_deferred_bridge_apply);
        }
        g_deferred_bridge_apply = msg;
        Serial.printf(
            "[Bridge] Vollabgleich waehrend Kamera aufgeschoben (%u Bytes)\n",
            msg->length);
        msg = nullptr;
        ++processed;
        continue;
      }
      processMqttMessage(msg->topic, msg->payload, msg->length);
      heap_caps_free(msg);
      ++processed;
    }
  }
}

// ========== MQTT Callback (thin: enqueue only) ==========
void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  // How much real time passed since the LVGL tick was last serviced, before we
  // even get here. This is the pure socket-read / loop-cadence latency (the
  // reassembly of a large multi-segment payload), independent of our own
  // processing -- which now runs later, on the loop drainer.
  uint32_t entry_since_tick = millis() - g_lvgl_tick_last_ms;
  if (entry_since_tick >= 50) {
    Serial.printf("[Bridge] mqttCallback entry: %ums since last LVGL tick service\n",
                  (unsigned)entry_since_tick);
  }

  QueueHandle_t q = mqttInboundQueue();
  MqttInboundMsg* msg = q ? mqttAllocInbound(topic, payload, length) : nullptr;
  if (!msg) {
    Serial.printf("[MQTT] Inbound-Alloc/Queue fehlt -> '%s' verworfen\n", topic ? topic : "?");
    return;
  }
  // mqttCallback laeuft jetzt auf dem MQTT-Worker (Single-Owner) -- der alte
  // Inline-Fallback (processMqttMessage direkt hier) ist damit tabu, weil
  // processMqttMessage() Flash und LVGL anfasst (loop-task-only). Bei voller
  // Queue kurz warten (der Loop-Task drained laufend), danach verwerfen.
  // Drop-Log gedrosselt: im Display-Sleep drained der Loop nur alle ~150ms,
  // das soll das Log nicht fluten.
  if (xQueueSend(q, &msg, pdMS_TO_TICKS(50)) != pdTRUE) {
    heap_caps_free(msg);
    static uint32_t last_drop_log_ms = 0;
    const uint32_t drop_now_ms = millis();
    if (last_drop_log_ms == 0 || (uint32_t)(drop_now_ms - last_drop_log_ms) >= 5000) {
      last_drop_log_ms = drop_now_ms;
      Serial.printf("[MQTT] Inbound-Queue voll -> Nachricht verworfen (zuletzt: %s)\n",
                    topic ? topic : "?");
    }
  }
}

// ========== MQTT message processing (Topic-Routing, runs on the loop) ==========
static void processMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  yield();  // Webserver atmen lassen!

  // Lokale Relais haben bewusst einen kleinen, direkten Topic-Pfad. Das
  // vermeidet JSON-Parsing und laesst normale Bridge-/Kamera-Nachrichten
  // weiterhin durch den bestehenden Router laufen.
  if (hardwareIo.handleMqttMessage(topic, payload, length)) return;

  const char* apply_topic = networkManager.getBridgeApplyTopic();
  if (apply_topic && strcmp(topic, apply_topic) == 0) {
    char* cfg_buf = mqttConfigBuffer();
    if (!cfg_buf) return;
    if (length >= CFG_BUF) {
      Serial.printf("[Bridge] WARNUNG: Payload zu gross (%u bytes), wird abgeschnitten!\n", length);
    }
    size_t copy_len = length < CFG_BUF - 1 ? length : CFG_BUF - 1;
    memcpy(cfg_buf, payload, copy_len);
    cfg_buf[copy_len] = '\0';
    yield();  // Nach großem Copy
    Serial.printf("[Bridge] apply-topic hit (%u bytes)\n", (unsigned)copy_len);
    bool reload = false;
    bool icons_changed = false;
    uint32_t t_parse0 = millis();
    bool applied = haBridgeConfig.applyJson(cfg_buf, &reload, &icons_changed);
    Serial.printf("[Bridge] applyJson: %u ms\n", (unsigned)(millis() - t_parse0));
    if (applied) {
      Serial.println("[Bridge] Konfiguration von HA empfangen");
      // applyJson ersetzt die Bridge-Metadaten-/Werte-Maps. Panel-interne
      // Local-I/O-Entities sofort wieder eintragen, damit sie auch ohne
      // Bridge-Abhaengigkeit im Tile-Editor und Runtime-Cache erhalten bleiben.
      hardwareIo.refreshLocalEntityCache();
      // Nur der erste erfolgreiche Bridge-Sync darf die Boot-Sleep-Sperre
      // freigeben und den Idle-Timer starten. Die Bridge sendet auch spaeter
      // regelmaessige apply-Refreshes; jeder davon hatte zuvor den Timer
      // zurueckgesetzt und dadurch 60-min-Screensaver dauerhaft verhindert.
      if (!g_bridge_initial_sync_complete) {
        g_bridge_initial_sync_complete = true;
        powerManager.allowSleep();
        displayManager.resetActivityTimer();
        Serial.println("[Bridge] Initial-Sync: Idle-Timer einmalig gestartet");
      }
      tiles_request_bridge_cache_refresh();
      if (reload) {
        yield();  // Nach JSON Parse
        // Nicht erneut die Geräteankündigung senden: HA beantwortet jede
        // Ankündigung mit einem neuen bridge/apply. Eine Antwort an dieser
        // Stelle erzeugt daher eine Apply/Announce-Rückkopplung.
        // Die dynamischen Topics stattdessen nur nach einem ruhigen Fenster
        // aktualisieren und mehrere Reloads zusammenfassen.
        mqttRequestDynamicSlotsReload();
        Serial.println("[Bridge] mqttReloadDynamicSlots nach Ruhefenster eingeplant");
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

  const char* icons_topic = networkManager.getBridgeIconsTopic();
  if (icons_topic && strcmp(topic, icons_topic) == 0) {
    char* large_buf = mqttLargeBuffer();
    if (!large_buf) return;
    size_t copy_len = length < LARGE_BUF - 1 ? length : LARGE_BUF - 1;
    memcpy(large_buf, payload, copy_len);
    large_buf[copy_len] = '\0';
    if (haBridgeConfig.applyIconUpdate(large_buf)) {
      tiles_request_icon_refresh();
    }
    return;
  }

  bool processed_static = false;
  for (const auto& route : kRoutes) {
    const char* expected = mqttTopics.topic(route.key);
    if (!expected || strcmp(topic, expected) != 0) {
      continue;
    }

    char* buf = route.use_large_buffer ? mqttLargeBuffer() : small_buf;
    if (!buf) return;
    size_t buf_len = route.use_large_buffer ? LARGE_BUF : sizeof(small_buf);
    size_t copy_len = length < (buf_len - 1) ? length : (buf_len - 1);
    memcpy(buf, payload, copy_len);
    buf[copy_len] = '\0';

    route.handler(buf, copy_len);
    processed_static = true;
    break;
  }

  if (processed_static) {
    return;
  }

  // Dynamische Sensor-Slots pruefen (JSON kann groesser sein -> ggf. Large Buffer)
  char* dyn_buf = (length < SMALL_BUF) ? small_buf : mqttLargeBuffer();
  if (!dyn_buf) return;
  size_t dyn_len = (length < SMALL_BUF) ? sizeof(small_buf) : LARGE_BUF;
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
    char* large_buf = mqttLargeBuffer();
    if (!large_buf) return;
    size_t copy_len = length < (LARGE_BUF - 1) ? length : (LARGE_BUF - 1);
    memcpy(large_buf, payload, copy_len);
    large_buf[copy_len] = '\0';
    String response_entity;
    if (extract_json_string_field(large_buf, "entity_id", response_entity) && response_entity.length()) {
      clear_pending_history_request(response_entity.c_str());
    }
    Serial.printf("[History] Antwort empfangen: %s, %u Bytes\n",
                  response_entity.length() ? response_entity.c_str()
                                           : "entity unbekannt",
                  static_cast<unsigned>(copy_len));
    queue_sensor_popup_history(nullptr, large_buf, copy_len);
    queue_tile_graph_history(nullptr, large_buf, copy_len);
    return;
  }

  const char* energy_topic = networkManager.getEnergyResponseTopic();
  if (energy_topic && strcmp(topic, energy_topic) == 0) {
    char* large_buf = mqttLargeBuffer();
    if (!large_buf) return;
    size_t copy_len = length < (LARGE_BUF - 1) ? length : (LARGE_BUF - 1);
    memcpy(large_buf, payload, copy_len);
    large_buf[copy_len] = '\0';
    queue_energy_response(large_buf, copy_len);
    return;
  }

  Serial.printf("MQTT: Unhandled topic %s\n", topic);
}

// ========== Subscribe zu Topics ==========
void mqttSubscribeTopics() {
  for (const auto& route : kRoutes) {
    const char* tpc = mqttTopics.topic(route.key);
    if (!tpc || !*tpc) continue;
    if (networkManager.mqttEnqueueSubscribe(tpc)) {
      Serial.printf("MQTT: subscribe queued %s\n", tpc);
    }
  }

  // Nach einem MQTT-(Re)connect existieren brokerseitig noch keine
  // Subscriptions. Die bereits aufgebauten Routen deshalb genau einmal alle
  // anmelden, aber nicht vorher sinnlos als Unsubscribe einreihen.
  mqttReloadDynamicSlots(true);
  hardwareIo.subscribeMqttTopics();
}

// ========== Home Snapshot publizieren ==========
void mqttPublishHomeSnapshot() {
  if (!networkManager.isMqttConnected()) return;

  char buf[24];
  dtostrf(g_outside_c, 0, 1, buf);
  networkManager.mqttEnqueuePublish(mqttTopics.topic(TopicKey::SENSOR_OUT), buf, true);

  dtostrf(g_inside_c, 0, 1, buf);
  networkManager.mqttEnqueuePublish(mqttTopics.topic(TopicKey::SENSOR_IN), buf, true);

  snprintf(buf, sizeof(buf), "%d", readBatterySocPercent());
  networkManager.mqttEnqueuePublish(mqttTopics.topic(TopicKey::SENSOR_SOC), buf, true);
}

// ========== Device Settings publizieren ==========
void mqttPublishDeviceSettings() {
  sync_local_device_entities(false);
  if (!networkManager.isMqttConnected()) return;

  const DeviceConfig& cfg = configManager.getConfig();

  auto publish_state = [&](TopicKey key, const char* payload) {
    const char* tpc = mqttTopics.topic(key);
    if (!tpc || !*tpc || !payload) return;
    networkManager.mqttEnqueuePublish(tpc, payload, true);
  };

  char buf[16];
  snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(cfg.display_brightness));
  publish_state(TopicKey::DISPLAY_BRIGHTNESS_STAT, buf);
  snprintf(buf, sizeof(buf), "%u",
           static_cast<unsigned>(cfg.screensaver_brightness_pct));
  publish_state(TopicKey::SCREENSAVER_BRIGHTNESS_STAT, buf);
  publish_state(TopicKey::DISPLAY_ROTATE_STAT, cfg.display_rotated_180 ? "ON" : "OFF");
  publish_state(TopicKey::DISPLAY_SLEEP_STAT, powerManager.isInSleep() ? "ON" : "OFF");

  publish_state(TopicKey::SLEEP_MAINS_STAT,
                sleepLabelFromConfig(cfg.auto_sleep_enabled, cfg.auto_sleep_seconds));
  publish_state(TopicKey::SLEEP_BAT_STAT,
                sleepLabelFromConfig(cfg.auto_sleep_battery_enabled, cfg.auto_sleep_battery_seconds));
}

void mqttServiceLocalSensors() {
  service_pending_history_fallback();
  // Eine einzelne, faellige 1-Wire-Phase pro Aufruf; bei Relais ist dieser
  // Pfad praktisch kostenlos. Die Zustandsmaschine wartet nie synchron auf
  // die DS18x20-Konvertierung.
  hardwareIo.service();

  static uint32_t last_run_ms = 0;
  const uint32_t now_ms = millis();
  if (last_run_ms != 0 && (int32_t)(now_ms - last_run_ms) < 500) {
    return;
  }
  last_run_ms = now_ms;
  sync_internal_battery_entity();
#if !defined(CONFIG_IDF_TARGET_ESP32P4)
  // Das alte GPIO-1/50-Autoprobing bleibt nur fuer bereits unterstuetzte
  // Nicht-P4-Installationen erhalten. Auf P4 war OneWire hier ohnehin
  // compile-time deaktiviert; ein permanentes "unavailable" wuerde neben den
  // neuen, explizit zugeordneten Local-I/O-Entities nur eine tote Alt-Entity
  // erzeugen.
  if (!hardwareIo.hasTemperatureChannels()) {
    sync_external_temp_entity(true);
  }
#endif
}

// ========== Scene Command publizieren ==========
void mqttPublishScene(const char* scene_name) {
  if (!scene_name || !*scene_name) return;

  if (!networkManager.isMqttConnected()) {
    Serial.printf("Scene command skipped (MQTT offline): %s\n", scene_name);
    return;
  }

  bool queued = networkManager.mqttEnqueuePublish(mqttTopics.topic(TopicKey::SCENE_CMND), scene_name, false);
  Serial.printf("Scene command -> MQTT '%s' (%s)\n", scene_name, queued ? "queued" : "queue-full");
}

// ========== Light/Switch Command publizieren ==========
void mqttPublishSwitchCommand(const char* entity_id, const char* state) {
  if (!entity_id || !*entity_id) return;
  if (handle_local_switch_command(entity_id, state)) return;

  if (!networkManager.isMqttConnected()) {
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
  bool queued = networkManager.mqttEnqueuePublish(topic, payload, false);
  Serial.printf("Switch command -> MQTT '%s' (%s)\n", topic, queued ? "queued" : "queue-full");
}

void mqttPublishMediaCommand(const char* entity_id, const char* command) {
  if (!entity_id || !*entity_id) return;

  if (!networkManager.isMqttConnected()) {
    Serial.printf("Media command skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* topic = mqttTopics.topic(TopicKey::MEDIA_CMND);
  if (!topic || !*topic) {
    Serial.printf("Media command skipped (no topic): %s\n", entity_id);
    return;
  }

  const char* action = (command && *command) ? command : "play_pause";
  char payload[256];
  snprintf(payload, sizeof(payload), "{\"entity_id\":\"%s\",\"command\":\"%s\"}", entity_id, action);
  bool queued = networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Media command -> MQTT '%s' (%s, priority)\n",
                topic, queued ? "queued" : "queue-full");
}

void mqttPublishMediaSeek(const char* entity_id, float position_seconds) {
  if (!entity_id || !*entity_id) return;
  if (position_seconds < 0.0f) position_seconds = 0.0f;

  if (!networkManager.isMqttConnected()) {
    Serial.printf("Media seek skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* topic = mqttTopics.topic(TopicKey::MEDIA_CMND);
  if (!topic || !*topic) {
    Serial.printf("Media seek skipped (no topic): %s\n", entity_id);
    return;
  }

  char payload[288];
  snprintf(payload,
           sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"media_seek\",\"seek_position\":%.1f}",
           entity_id,
           position_seconds);
  bool queued = networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Media seek -> MQTT '%s' %.1fs (%s, priority)\n",
                topic,
                position_seconds,
                queued ? "queued" : "queue-full");
}

void mqttPublishMediaVolume(const char* entity_id, float volume_level) {
  if (!entity_id || !*entity_id) return;
  if (volume_level < 0.0f) volume_level = 0.0f;
  if (volume_level > 1.0f) volume_level = 1.0f;

  if (!networkManager.isMqttConnected()) {
    Serial.printf("Media volume skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* topic = mqttTopics.topic(TopicKey::MEDIA_CMND);
  if (!topic || !*topic) {
    Serial.printf("Media volume skipped (no topic): %s\n", entity_id);
    return;
  }

  char payload[288];
  snprintf(payload,
           sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"volume_set\",\"volume_level\":%.3f}",
           entity_id,
           volume_level);
  bool queued = networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Media volume -> MQTT '%s' %.0f%% (%s, priority)\n",
                topic, volume_level * 100.0f,
                queued ? "queued" : "queue-full");
}

void mqttPublishMediaMute(const char* entity_id, bool muted) {
  if (!entity_id || !*entity_id) return;

  if (!networkManager.isMqttConnected()) {
    Serial.printf("Media mute skipped (MQTT offline): %s\n", entity_id);
    return;
  }

  const char* topic = mqttTopics.topic(TopicKey::MEDIA_CMND);
  if (!topic || !*topic) {
    Serial.printf("Media mute skipped (no topic): %s\n", entity_id);
    return;
  }

  char payload[288];
  snprintf(payload,
           sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"volume_mute\",\"is_volume_muted\":%s}",
           entity_id,
           muted ? "true" : "false");
  bool queued = networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Media mute -> MQTT '%s' %s (%s, priority)\n",
                topic, muted ? "on" : "off",
                queued ? "queued" : "queue-full");
}

void mqttPublishClimateTemperature(const char* entity_id,
                                   float temperature,
                                   bool use_range,
                                   float target_low,
                                   float target_high) {
  if (!entity_id || !*entity_id) return;
  if (!networkManager.isMqttConnected()) {
    Serial.printf("Climate temperature skipped (MQTT offline): %s\n", entity_id);
    return;
  }
  const char* topic = mqttTopics.topic(TopicKey::CLIMATE_CMND);
  if (!topic || !*topic) return;

  char payload[320];
  if (use_range) {
    snprintf(payload, sizeof(payload),
             "{\"entity_id\":\"%s\",\"command\":\"set_temperature\","
             "\"target_temp_low\":%.2f,\"target_temp_high\":%.2f}",
             entity_id, target_low, target_high);
  } else {
    snprintf(payload, sizeof(payload),
             "{\"entity_id\":\"%s\",\"command\":\"set_temperature\","
             "\"temperature\":%.2f}",
             entity_id, temperature);
  }
  const bool queued =
      networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Climate temperature -> MQTT '%s' (%s, priority)\n",
                topic, queued ? "queued" : "queue-full");
}

void mqttPublishClimateHumidity(const char* entity_id, float humidity) {
  if (!entity_id || !*entity_id) return;
  if (!networkManager.isMqttConnected()) {
    Serial.printf("Climate humidity skipped (MQTT offline): %s\n", entity_id);
    return;
  }
  const char* topic = mqttTopics.topic(TopicKey::CLIMATE_CMND);
  if (!topic || !*topic) return;

  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"set_humidity\","
           "\"humidity\":%.1f}",
           entity_id, humidity);
  const bool queued =
      networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Climate humidity -> MQTT '%s' (%s, priority)\n",
                topic, queued ? "queued" : "queue-full");
}

void mqttPublishClimateHvacMode(const char* entity_id, const char* hvac_mode) {
  if (!entity_id || !*entity_id || !hvac_mode || !*hvac_mode) return;
  if (!networkManager.isMqttConnected()) {
    Serial.printf("Climate mode skipped (MQTT offline): %s\n", entity_id);
    return;
  }
  const char* topic = mqttTopics.topic(TopicKey::CLIMATE_CMND);
  if (!topic || !*topic) return;

  char payload[256];
  snprintf(payload, sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"set_hvac_mode\","
           "\"hvac_mode\":\"%s\"}",
           entity_id, hvac_mode);
  const bool queued =
      networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Climate mode -> MQTT '%s' (%s, priority)\n",
                topic, queued ? "queued" : "queue-full");
}

void mqttPublishClimatePresetMode(
    const char* entity_id, const char* preset_mode) {
  if (!entity_id || !*entity_id || !preset_mode || !*preset_mode) return;
  if (!networkManager.isMqttConnected()) {
    Serial.printf("Climate preset skipped (MQTT offline): %s\n", entity_id);
    return;
  }
  const char* topic = mqttTopics.topic(TopicKey::CLIMATE_CMND);
  if (!topic || !*topic) return;

  char payload[272];
  snprintf(payload, sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"set_preset_mode\","
           "\"preset_mode\":\"%s\"}",
           entity_id, preset_mode);
  const bool queued =
      networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Climate preset -> MQTT '%s' (%s, priority)\n",
                topic, queued ? "queued" : "queue-full");
}

static void mqtt_publish_climate_option(
    const char* entity_id,
    const char* command,
    const char* field,
    const char* value,
    const char* log_name) {
  if (!entity_id || !*entity_id || !command || !*command ||
      !field || !*field || !value || !*value) {
    return;
  }
  if (!networkManager.isMqttConnected()) {
    Serial.printf("Climate %s skipped (MQTT offline): %s\n",
                  log_name, entity_id);
    return;
  }
  const char* topic = mqttTopics.topic(TopicKey::CLIMATE_CMND);
  if (!topic || !*topic) return;

  char payload[304];
  snprintf(payload, sizeof(payload),
           "{\"entity_id\":\"%s\",\"command\":\"%s\",\"%s\":\"%s\"}",
           entity_id, command, field, value);
  const bool queued =
      networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("Climate %s -> MQTT '%s' (%s, priority)\n",
                log_name, topic, queued ? "queued" : "queue-full");
}

void mqttPublishClimateFanMode(
    const char* entity_id, const char* fan_mode) {
  mqtt_publish_climate_option(
      entity_id, "set_fan_mode", "fan_mode", fan_mode, "fan");
}

void mqttPublishClimateSwingMode(
    const char* entity_id, const char* swing_mode) {
  mqtt_publish_climate_option(
      entity_id, "set_swing_mode", "swing_mode", swing_mode, "swing");
}

void mqttPublishClimateHorizontalSwingMode(
    const char* entity_id, const char* swing_mode) {
  mqtt_publish_climate_option(
      entity_id, "set_swing_horizontal_mode",
      "swing_horizontal_mode", swing_mode, "horizontal swing");
}

void mqttPublishLightCommand(const char* entity_id,
                             const char* state,
                             int brightness_pct,
                             bool has_color,
                             uint32_t color,
                             int color_temp_kelvin) {
  if (!entity_id || !*entity_id) return;
  if (handle_local_light_command(entity_id, state, brightness_pct)) return;

  if (!networkManager.isMqttConnected()) {
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

  bool queued = networkManager.mqttEnqueuePublish(topic, payload.c_str(), false);
  Serial.printf("Light command -> MQTT '%s' (%s)\n", topic, queued ? "queued" : "queue-full");
}

void mqttPublishHistoryRequest(const char* entity_id,
                               uint16_t hours,
                               uint16_t period_minutes,
                               uint16_t points) {
  if (!entity_id || !*entity_id) return;
  // Der lokale Live-Wert ist ohne Bridge verfuegbar, eine Historie dagegen
  // nicht. Deshalb niemals unnoetig einen HA-Request fuer Panel-I/O starten.
  if (hardwareIo.isLocalEntityId(entity_id)) return;

  if (hours == 0) hours = 24;
  if (period_minutes == 0) period_minutes = 5;
  if (points == 0) {
    points = static_cast<uint16_t>((static_cast<uint32_t>(hours) * 60U) / period_minutes);
  }
  if (points == 0) points = 1;

  const bool mqtt_online = networkManager.isMqttConnected();
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
    // Der Nutzer wartet sichtbar auf diesen Graphen. Vor dem History-Request
    // koennen nach einem Route-Reload viele sicher gedrosselte Subscribe-
    // Kommandos stehen; hinten einsortiert wuerde der 2-s-Fallback den Graphen
    // leeren, bevor der Request ueberhaupt gesendet wurde.
    bool ok = networkManager.mqttEnqueuePublishWithLargeBuffer(
        history_topic, payload.c_str(), false, 20000, true);
    Serial.printf("History request -> MQTT '%s' (%s, priority)\n",
                  history_topic, ok ? "queued" : "queue-full");
    if (ok) {
      mark_pending_history_request(entity_id, millis(), hours, period_minutes, points);
      return;
    }
    queue_history_fallback_for_entity(entity_id, time_valid, "HA Publish fehlgeschlagen",
                                      hours, period_minutes, points);
    return;
  }

  if (queue_history_fallback_for_entity(entity_id, time_valid, "HA nicht verfügbar",
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

  if (!networkManager.isMqttConnected()) return;

  const char* weather_topic = networkManager.getWeatherRequestTopic();
  if (!weather_topic || !*weather_topic) return;

  String payload = "{\"entity_id\":\"";
  payload += entity_id;
  payload += "\"}";

  bool queued = networkManager.mqttEnqueuePublish(weather_topic, payload.c_str(), false);
  Serial.printf("Weather request -> MQTT '%s' (%s)\n", weather_topic, queued ? "queued" : "queue-full");
}

bool mqttPublishEnergyRequest(const char* period) {
  const char* p = (period && *period) ? period : "day";
  if (strcmp(p, "week") != 0 && strcmp(p, "month") != 0) {
    p = "day";
  }

  if (!networkManager.isMqttConnected()) {
    Serial.printf("Energy request skipped (MQTT offline): %s\n", p);
    return false;
  }

  const char* energy_topic = networkManager.getEnergyRequestTopic();
  if (!energy_topic || !*energy_topic) {
    Serial.printf("Energy request skipped (no topic): %s\n", p);
    return false;
  }

  String payload = "{\"period\":\"";
  payload += p;
  payload += "\"}";

  // Wie History-Requests ist auch der gewaehlte Zeitraum interaktiv. Vor
  // einem langen Subscribe-Sturm einsortieren, ohne den MQTT-Client aus dem
  // Loop-Task anzufassen oder die P4-SDIO-Schutzabstaende zu umgehen.
  bool queued = networkManager.mqttEnqueuePublishWithLargeBuffer(
      energy_topic, payload.c_str(), false, 12000, true);
  Serial.printf("Energy request -> MQTT '%s' period=%s (%s, priority)\n",
                energy_topic,
                p,
                queued ? "queued" : "queue-full");
  return queued;
}

// ========== Home Assistant MQTT Discovery ==========
void mqttPublishDiscovery() {
  if (!networkManager.isMqttConnected()) return;

  Serial.println("Publishing Home Assistant discovery payloads...");

  char did[24];
  buildDeviceId(did, sizeof(did));

  char tpc[128];

  // Legacy Discovery komplett entfernen: alle diese Sensoren/Buttons kommen
  // laengst konsistent ueber die HA-Integration (tab5_lvgl/HomeTiles Bridge).
  // Jede dieser Nachrichten war mit retain=true gesetzt -- ein leerer Payload
  // (weiterhin retained) ist bei HA's nativer MQTT-Discovery die dokumentierte
  // Loesch-Anweisung fuer den Discovery-Eintrag. Ohne das haette jede
  // device_id-Aenderung (z.B. der 48-Bit-MAC-Fix) ein weiteres verwaistes
  // "Waveshare P4 Panel"-Phantomgeraet unter der nativen MQTT-Integration
  // hinterlassen -- bei diesem Nutzer schon viermal passiert.
  const char* legacy_configs[] = {
    "outside_c", "inside_c", "external_c", "soc_pct", "uptime",
  };
  for (const char* leaf : legacy_configs) {
    snprintf(tpc, sizeof(tpc), "homeassistant/sensor/%s_%s/config", did, leaf);
    networkManager.mqttEnqueuePublish(tpc, "", true);
  }
  const char* legacy_buttons[] = {
    "scene_abend", "scene_lesen", "scene_allesaus",
  };
  for (const char* leaf : legacy_buttons) {
    snprintf(tpc, sizeof(tpc), "homeassistant/button/%s_%s/config", did, leaf);
    networkManager.mqttEnqueuePublish(tpc, "", true);
  }

  Serial.println("Legacy Home Assistant discovery cleared");
}

static bool topicListContains(const std::vector<String>& topics,
                              const String& needle) {
  return std::find(topics.begin(), topics.end(), needle) != topics.end();
}

void mqttReloadDynamicSlots(bool subscribe_all) {
  const bool mqtt_online = networkManager.isMqttConnected();
  std::vector<String> old_topics;
  old_topics.reserve(g_dynamic_routes.size() + g_dynamic_weather_routes.size());
  for (const auto& route : g_dynamic_routes) {
    if (!topicListContains(old_topics, route.topic)) old_topics.push_back(route.topic);
  }
  for (const auto& route : g_dynamic_weather_routes) {
    if (!topicListContains(old_topics, route.topic)) old_topics.push_back(route.topic);
  }

  uint32_t t_sensor0 = millis();
  rebuildDynamicRoutes(g_dynamic_routes);
  Serial.printf("[Bridge] rebuildDynamicRoutes: %u ms\n", (unsigned)(millis() - t_sensor0));
  uint32_t t_weather0 = millis();
  rebuildDynamicWeatherRoutes(g_dynamic_weather_routes);
  Serial.printf("[Bridge] rebuildDynamicWeatherRoutes: %u ms\n", (unsigned)(millis() - t_weather0));

  std::vector<String> new_topics;
  new_topics.reserve(g_dynamic_routes.size() + g_dynamic_weather_routes.size());
  for (const auto& route : g_dynamic_routes) {
    if (!topicListContains(new_topics, route.topic)) new_topics.push_back(route.topic);
  }
  for (const auto& route : g_dynamic_weather_routes) {
    if (!topicListContains(new_topics, route.topic)) new_topics.push_back(route.topic);
  }

  uint32_t t_control0 = millis();
  size_t unsubscribe_count = 0;
  size_t subscribe_count = 0;
  if (mqtt_online) {
    // Bei einem normalen Bridge-Refresh nur die echte Differenz anwenden.
    // Zuvor wurden jedes Mal alle alten Topics ab- und alle neuen wieder
    // angemeldet. Zwei schnelle Refreshes konnten so mehr als 128 Control-
    // Kommandos erzeugen und die Queue dauerhaft ueberfuellen.
    if (!subscribe_all) {
      for (const auto& topic : old_topics) {
        if (topicListContains(new_topics, topic)) continue;
        if (networkManager.mqttEnqueueUnsubscribe(topic.c_str())) {
          ++unsubscribe_count;
        }
        lvglServiceDuringBlockingWork();
      }
    }
    for (const auto& topic : new_topics) {
      if (!subscribe_all && topicListContains(old_topics, topic)) continue;
      if (networkManager.mqttEnqueueSubscribe(topic.c_str())) {
        ++subscribe_count;
        Serial.printf("MQTT: subscribe queued %s\n", topic.c_str());
      }
      lvglServiceDuringBlockingWork();
    }
  }
  Serial.printf("[Bridge] mqttReloadDynamicSlots: -%u +%u Topics in %u ms%s\n",
                static_cast<unsigned>(unsubscribe_count),
                static_cast<unsigned>(subscribe_count),
                static_cast<unsigned>(millis() - t_control0),
                subscribe_all ? " (reconnect)" : "");
}

void mqttRequestDynamicSlotsReload(uint32_t quiet_ms) {
  if (quiet_ms == 0) quiet_ms = kDynamicSlotReloadQuietMs;
  const uint32_t due = millis() + quiet_ms;
  if (!g_dynamic_slots_reload_requested ||
      (int32_t)(due - g_dynamic_slots_reload_due_ms) > 0) {
    g_dynamic_slots_reload_due_ms = due;
  }
  g_dynamic_slots_reload_requested = true;
}

void mqttServiceDynamicSlotsReload() {
  if (!g_dynamic_slots_reload_requested) return;

  const uint32_t now = millis();
  if (webAdminRecentlyActive(kDynamicSlotReloadAdminQuietMs)) {
    g_dynamic_slots_reload_due_ms = now + kDynamicSlotReloadQuietMs;
    return;
  }
  if ((int32_t)(now - g_dynamic_slots_reload_due_ms) < 0) {
    return;
  }

  g_dynamic_slots_reload_requested = false;
  uint32_t t_slots0 = millis();
  mqttReloadDynamicSlots();
  Serial.printf("[Bridge] deferred mqttReloadDynamicSlots: %u ms\n",
                (unsigned)(millis() - t_slots0));
}

// ========== Post-Connect (Loop-Task) ==========
// Der MQTT-Worker setzt nach jedem erfolgreichen (Re-)Connect ein Pending-
// Flag; diese Funktion (jede Loop-Iteration aufgerufen) konsumiert es und
// faehrt die App-Ebene hoch. Sie MUSS auf dem Loop-Task laufen:
// mqttSubscribeTopics() -> mqttReloadDynamicSlots() scannt Flash-Ordner und
// pumpt LVGL, mqttPublishDeviceSettings()/mqttPublishHomeSnapshot() lesen
// Batterie-I2C und aktualisieren die Grids. Die dabei anfallenden
// publishes/subscribes gehen per Outbound-Queue zurueck an den Worker.
void mqttServicePostConnect() {
  if (!networkManager.consumeMqttPostConnectPending()) return;
  Serial.println("[MQTT] Post-Connect: Subscribes/Discovery/Settings (Loop-Task)");
  mqttSubscribeTopics();
  mqttPublishDiscovery();
  mqttPublishDeviceSettings();
  mqttPublishHomeSnapshot();
  // Announce the exact MAC-based Bridge topic after every connection. Without
  // this retained message the HA integration cannot discover a new device or
  // repair an entry that still points at an older device ID. The publish uses
  // the large-buffer queue, which is held back until the startup storm ends.
  networkManager.publishBridgeConfig();
}

void mqttPublishCameraCommand(const char* entity_id, const char* command) {
  if (!entity_id || !*entity_id) return;
  const auto& text = i18n::strings(configManager.getConfig().language);
  if (!networkManager.isMqttConnected()) {
    camera_popup_set_status(text.camera_mqtt_disconnected, true);
    return;
  }

  const char* topic = mqttTopics.topic(TopicKey::CAMERA_CMND);
  if (!topic || !*topic) {
    camera_popup_set_status(text.camera_mqtt_topic_missing, true);
    return;
  }

  const char* action = (command && *command) ? command : "open";
  char payload[384];
  if (strcmp(action, "open") == 0) {
    snprintf(payload, sizeof(payload),
             "{\"entity_id\":\"%s\",\"command\":\"%s\","
             "\"width\":%u,\"height\":%u,\"fps\":%u,"
             "\"transport\":\"tcp-ack-v1\",\"protocol_version\":1}",
             entity_id, action,
             camera_geometry::kWidth,
             camera_geometry::kHeight,
             camera_geometry::kFps);
  } else {
    snprintf(payload, sizeof(payload),
             "{\"entity_id\":\"%s\",\"command\":\"%s\"}",
             entity_id, action);
  }
  const bool queued =
      networkManager.mqttEnqueuePublishPriority(topic, payload, false);
  Serial.printf("[Camera] command %s -> %s (%s)\n", action, topic,
                queued ? "queued" : "queue-full");
  if (!queued) camera_popup_set_status(text.camera_mqtt_queue_full, true);
}

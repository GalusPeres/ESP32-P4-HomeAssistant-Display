#include "src/types/energy/energy_data.h"

#include <ArduinoJson.h>
#include <cstring>
#include <math.h>
#include <vector>

#include "src/core/power_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/network_manager.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/ui/energy_popup.h"
#include "src/ui/tab_tiles_unified.h"

namespace {

struct PendingEnergyResponse {
  String payload;
  bool valid = false;
};

PendingEnergyResponse g_pending_energy;
std::vector<EnergyEntryData> g_day_entries;
std::vector<EnergyEntryData> g_week_entries;
std::vector<EnergyEntryData> g_month_entries;
uint32_t g_last_day_request_ms = 0;
uint32_t g_last_week_request_ms = 0;
uint32_t g_last_month_request_ms = 0;
uint32_t g_last_periodic_ms = 0;

const char* normalize_period(const char* period) {
  if (!period || !*period) return "day";
  if (strcmp(period, "week") == 0) return "week";
  if (strcmp(period, "month") == 0) return "month";
  return "day";
}

uint32_t& last_request_slot(const char* period) {
  const char* p = normalize_period(period);
  if (strcmp(p, "week") == 0) return g_last_week_request_ms;
  if (strcmp(p, "month") == 0) return g_last_month_request_ms;
  return g_last_day_request_ms;
}

std::vector<EnergyEntryData>& cache_for_period(const char* period) {
  const char* p = normalize_period(period);
  if (strcmp(p, "week") == 0) return g_week_entries;
  if (strcmp(p, "month") == 0) return g_month_entries;
  return g_day_entries;
}

bool is_disabled_token(const String& value) {
  if (!value.length()) return false;
  String t = value;
  t.trim();
  if (!t.length()) return true;
  t.toLowerCase();
  return t == "-" || t == "none" || t == "null" || t == "no" || t == "off";
}

String format_energy_total(float value, bool is_cost) {
  if (!isfinite(value)) return String("--");
  return String(value, is_cost ? 2 : 3);
}

float apply_energy_sign(float value, int8_t sign) {
  if (sign < 0 && value > 0.0f) return -value;
  return value;
}

const char* icon_for_energy(const EnergyEntryData& entry) {
  if (entry.is_cost) return "currency-eur";
  String cat = entry.category;
  cat.toLowerCase();
  if (cat == "solar") return "solar-power";
  if (cat == "grid") return "transmission-tower";
  if (cat == "battery") return "battery-charging";
  if (cat == "gas") return "fire";
  if (cat == "water" || cat == "device_water") return "water";
  return "lightning-bolt";
}

void queue_energy_tile_update_for_entry(const EnergyEntryData& entry) {
  if (!entry.id.length()) return;

  String raw_value = format_energy_total(entry.total, entry.is_cost);
  tiles_cache_entity_payload(entry.id.c_str(), raw_value.c_str());

  if (powerManager.isInSleep()) return;
  if (!tiles_is_loaded(GridType::TAB0)) return;

  const TileGridConfig& grid = tileConfig.getActiveGrid();
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type != TILE_ENERGY) continue;
    if (!tile.sensor_entity.equalsIgnoreCase(entry.id)) continue;

    String unit = tile.sensor_unit;
    if (is_disabled_token(unit)) {
      unit = "";
    } else if (!unit.length()) {
      unit = entry.unit.length() ? entry.unit : haBridgeConfig.findSensorUnit(entry.id);
    }
    queue_sensor_tile_update(GridType::TAB0,
                             i,
                             raw_value.c_str(),
                             unit.length() ? unit.c_str() : nullptr);
  }
}

bool active_grid_has_energy_tile() {
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type == TILE_ENERGY && tile.sensor_entity.length()) return true;
  }
  return false;
}

void parse_energy_response(const char* payload) {
  DynamicJsonDocument doc(32768);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[Energy] Response JSON invalid: %s\n", err.c_str());
    return;
  }

  const char* period_c = doc["period"] | "day";
  const char* period = normalize_period(period_c);
  String start = doc["start"] | "";
  JsonArray entries = doc["entries"].as<JsonArray>();
  if (entries.isNull()) {
    Serial.println("[Energy] Response without entries");
    return;
  }

  std::vector<EnergyEntryData> parsed;
  parsed.reserve(entries.size());

  for (JsonObject obj : entries) {
    const char* id_c = obj["id"] | "";
    if (!id_c || !*id_c) continue;

    EnergyEntryData entry;
    entry.id = id_c;
    entry.period = period;
    entry.start = start;
    entry.category = obj["category"] | "";
    entry.name = obj["name"] | "";
    entry.unit = obj["unit"] | "";
    entry.is_cost = obj["is_cost"] | false;
    entry.is_total = obj["is_total"] | false;
    int raw_sign = obj["sign"] | 1;
    entry.sign = raw_sign < 0 ? -1 : 1;

    if (!entry.name.length()) {
      entry.name = haBridgeConfig.findSensorName(entry.id);
    }
    if (!entry.unit.length()) {
      entry.unit = haBridgeConfig.findSensorUnit(entry.id);
    }

    JsonVariant total = obj["total"];
    entry.total = total.isNull() ? 0.0f : apply_energy_sign(total.as<float>(), entry.sign);
    JsonVariant cost = obj["cost"];
    if (!cost.isNull()) {
      entry.cost = cost.as<float>();
      entry.has_cost = true;
    }

    JsonArray values = obj["values"].as<JsonArray>();
    if (!values.isNull()) {
      for (JsonVariant v : values) {
        if (entry.value_count >= ENERGY_VALUES_MAX) break;
        const uint8_t idx = entry.value_count++;
        if (v.isNull()) {
          entry.value_valid[idx] = false;
          entry.values[idx] = 0.0f;
        } else {
          float f = apply_energy_sign(v.as<float>(), entry.sign);
          entry.value_valid[idx] = isfinite(f);
          entry.values[idx] = entry.value_valid[idx] ? f : 0.0f;
        }
      }
    }

    if (strcmp(period, "day") == 0) {
      queue_energy_tile_update_for_entry(entry);
    }
    parsed.push_back(entry);
  }

  cache_for_period(period) = parsed;
  queue_energy_popup_refresh(period);
  Serial.printf("[Energy] Response parsed: period=%s entries=%u\n",
                period,
                static_cast<unsigned>(parsed.size()));
}

}  // namespace

void queue_energy_response(const char* payload, size_t len) {
  if (!payload || len == 0) return;
  g_pending_energy.payload = String(payload).substring(0, len);
  g_pending_energy.valid = true;
}

void process_energy_response_queue() {
  if (!g_pending_energy.valid) return;
  String payload = g_pending_energy.payload;
  g_pending_energy.valid = false;
  g_pending_energy.payload = "";
  parse_energy_response(payload.c_str());
}

bool energy_request_period(const char* period, bool force) {
  const char* p = normalize_period(period);
  uint32_t& last = last_request_slot(p);
  const uint32_t now = millis();
  if (!force && last != 0 && (uint32_t)(now - last) < 10000UL) {
    return true;
  }
  if (!mqttPublishEnergyRequest(p)) {
    return false;
  }
  last = now;
  return true;
}

bool energy_request_day_for_tiles(bool force) {
  if (!active_grid_has_energy_tile()) return true;
  return energy_request_period("day", force);
}

void energy_service_periodic() {
  if (!networkManager.isMqttConnected()) return;
  const uint32_t now = millis();
  if (g_last_periodic_ms != 0 && (uint32_t)(now - g_last_periodic_ms) < 5UL * 60UL * 1000UL) {
    return;
  }
  if (energy_request_day_for_tiles(false)) {
    g_last_periodic_ms = now;
  }
}

bool energy_find_entry(const String& id, const char* period, EnergyEntryData& out) {
  if (!id.length()) return false;
  const std::vector<EnergyEntryData>& entries = cache_for_period(period);
  for (const auto& entry : entries) {
    if (entry.id.equalsIgnoreCase(id)) {
      out = entry;
      return true;
    }
  }
  return false;
}

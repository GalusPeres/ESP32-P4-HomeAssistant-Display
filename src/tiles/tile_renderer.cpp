#include "src/tiles/tile_renderer.h"
#include "src/network/ha_bridge_config.h"
#include "src/network/mqtt_handlers.h"
#include "src/game/game_ws_server.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/ui_manager.h"
#include "src/ui/light_popup.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/media_popup.h"
#include "src/types/types_registry.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/web/web_admin.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <ctype.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <libb64/cdecode.h>
#include <libs/tjpgd/tjpgd.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

/* === Layout-Konstanten === */

/* === Globale State f++r Updates === */
SensorTileWidgets g_tab0_sensors[TILES_PER_GRID];
SensorTileWidgets g_tab1_sensors[TILES_PER_GRID];
SensorTileWidgets g_tab2_sensors[TILES_PER_GRID];

SwitchTileWidgets g_tab0_switches[TILES_PER_GRID];
SwitchTileWidgets g_tab1_switches[TILES_PER_GRID];
SwitchTileWidgets g_tab2_switches[TILES_PER_GRID];

WeatherTileWidgets g_tab0_weather[TILES_PER_GRID];
WeatherTileWidgets g_tab1_weather[TILES_PER_GRID];
WeatherTileWidgets g_tab2_weather[TILES_PER_GRID];

MediaTileWidgets g_tab0_media[TILES_PER_GRID];
MediaTileWidgets g_tab1_media[TILES_PER_GRID];
MediaTileWidgets g_tab2_media[TILES_PER_GRID];

SwitchState g_tab0_switch_states[TILES_PER_GRID];
SwitchState g_tab1_switch_states[TILES_PER_GRID];
SwitchState g_tab2_switch_states[TILES_PER_GRID];

SensorTileWidgets* tile_renderer_get_sensor_widgets(GridType grid_type) {
  if (grid_type == GridType::TAB1) return g_tab1_sensors;
  if (grid_type == GridType::TAB2) return g_tab2_sensors;
  return g_tab0_sensors;
}

SwitchTileWidgets* tile_renderer_get_switch_widgets(GridType grid_type) {
  if (grid_type == GridType::TAB1) return g_tab1_switches;
  if (grid_type == GridType::TAB2) return g_tab2_switches;
  return g_tab0_switches;
}

WeatherTileWidgets* tile_renderer_get_weather_widgets(GridType grid_type) {
  if (grid_type == GridType::TAB1) return g_tab1_weather;
  if (grid_type == GridType::TAB2) return g_tab2_weather;
  return g_tab0_weather;
}

MediaTileWidgets* tile_renderer_get_media_widgets(GridType grid_type) {
  if (grid_type == GridType::TAB1) return g_tab1_media;
  if (grid_type == GridType::TAB2) return g_tab2_media;
  return g_tab0_media;
}

SwitchState* tile_renderer_get_switch_states(GridType grid_type) {
  if (grid_type == GridType::TAB1) return g_tab1_switch_states;
  if (grid_type == GridType::TAB2) return g_tab2_switch_states;
  return g_tab0_switch_states;
}


bool is_light_entity_id(const String& entity_id);

static void clear_sensor_widgets(GridType grid_type) {
  SensorTileWidgets* target = g_tab0_sensors;
  if (grid_type == GridType::TAB1) target = g_tab1_sensors;
  else if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    target[i].value_label = nullptr;
    target[i].unit_label = nullptr;
    target[i].gauge = nullptr;
    target[i].gauge_min = 0;
    target[i].gauge_max = 100;
    target[i].chart = nullptr;
    target[i].series = nullptr;
  }
}

void reset_sensor_widget(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return;
  SensorTileWidgets* target = g_tab0_sensors;
  if (grid_type == GridType::TAB1) target = g_tab1_sensors;
  else if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  target[grid_index] = {};
}

void reset_sensor_widgets(GridType grid_type) {
  clear_sensor_widgets(grid_type);
}

static void clear_switch_widgets(GridType grid_type) {
  SwitchTileWidgets* target = g_tab0_switches;
  SwitchState* state_target = g_tab0_switch_states;
  if (grid_type == GridType::TAB1) {
    target = g_tab1_switches;
    state_target = g_tab1_switch_states;
  } else if (grid_type == GridType::TAB2) {
    target = g_tab2_switches;
    state_target = g_tab2_switch_states;
  }
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    target[i].icon_label = nullptr;
    target[i].title_label = nullptr;
    target[i].switch_obj = nullptr;
    state_target[i] = {};
  }
}

void reset_switch_widget(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return;
  SwitchTileWidgets* target = g_tab0_switches;
  SwitchState* state_target = g_tab0_switch_states;
  if (grid_type == GridType::TAB1) {
    target = g_tab1_switches;
    state_target = g_tab1_switch_states;
  } else if (grid_type == GridType::TAB2) {
    target = g_tab2_switches;
    state_target = g_tab2_switch_states;
  }
  target[grid_index] = {};
  state_target[grid_index] = {};
}

void reset_switch_widgets(GridType grid_type) {
  clear_switch_widgets(grid_type);
}

static void clear_weather_widgets(GridType grid_type) {
  WeatherTileWidgets* target = g_tab0_weather;
  if (grid_type == GridType::TAB1) target = g_tab1_weather;
  else if (grid_type == GridType::TAB2) target = g_tab2_weather;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    target[i] = {};
  }
}

void reset_weather_widget(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return;
  WeatherTileWidgets* target = g_tab0_weather;
  if (grid_type == GridType::TAB1) target = g_tab1_weather;
  else if (grid_type == GridType::TAB2) target = g_tab2_weather;
  target[grid_index] = {};
}

void reset_weather_widgets(GridType grid_type) {
  clear_weather_widgets(grid_type);
}

static void clear_media_widgets(GridType grid_type) {
  MediaTileWidgets* target = g_tab0_media;
  if (grid_type == GridType::TAB1) target = g_tab1_media;
  else if (grid_type == GridType::TAB2) target = g_tab2_media;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    target[i] = {};
  }
}

void reset_media_widget(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return;
  MediaTileWidgets* target = g_tab0_media;
  if (grid_type == GridType::TAB1) target = g_tab1_media;
  else if (grid_type == GridType::TAB2) target = g_tab2_media;
  target[grid_index] = {};
}

void reset_media_widgets(GridType grid_type) {
  clear_media_widgets(grid_type);
}

void tile_renderer_snapshot_tab0(TileWidgetCache* out) {
  if (!out) return;
  memcpy(out->sensors, g_tab0_sensors, sizeof(g_tab0_sensors));
  memcpy(out->switches, g_tab0_switches, sizeof(g_tab0_switches));
  memcpy(out->switch_states, g_tab0_switch_states, sizeof(g_tab0_switch_states));
  memcpy(out->weather, g_tab0_weather, sizeof(g_tab0_weather));
  memcpy(out->media, g_tab0_media, sizeof(g_tab0_media));
}

void tile_renderer_restore_tab0(const TileWidgetCache* in) {
  if (!in) return;
  memcpy(g_tab0_sensors, in->sensors, sizeof(g_tab0_sensors));
  memcpy(g_tab0_switches, in->switches, sizeof(g_tab0_switches));
  memcpy(g_tab0_switch_states, in->switch_states, sizeof(g_tab0_switch_states));
  memcpy(g_tab0_weather, in->weather, sizeof(g_tab0_weather));
  memcpy(g_tab0_media, in->media, sizeof(g_tab0_media));
}

/* === Thread-Safe Update Queue (MQTT ��� Main Loop) === */
struct SensorUpdate {
  GridType grid_type;
  uint8_t grid_index;
  String value;
  String unit;
  bool valid;
};

static const uint8_t QUEUE_SIZE = 32;
static SensorUpdate g_update_queue[QUEUE_SIZE];
static volatile uint8_t g_queue_head = 0;
static volatile uint8_t g_queue_tail = 0;
static uint32_t g_queue_overflow_count = 0;

static uint8_t get_sensor_decimals(GridType grid_type, uint8_t grid_index) {
  if (grid_index >= TILES_PER_GRID) return 0xFF;
  (void)grid_type;
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  return grid.tiles[grid_index].sensor_decimals;
}

static uint32_t fnv1a_hash(const char* data) {
  if (!data) return 0;
  uint32_t hash = 2166136261u;
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);
  while (*ptr) {
    hash ^= static_cast<uint32_t>(*ptr++);
    hash *= 16777619u;
  }
  return hash;
}

static const lv_font_t* get_sensor_value_font(const Tile& tile) {
  switch (tile.sensor_value_font) {
    case 1: return &ui_font_20;
    case 2: return &ui_font_24;
    case 3: return &ui_font_32;
    case 4: return &ui_font_40;
    default: return FONT_VALUE;
  }
}

static bool apply_decimals(String& value, uint8_t decimals) {
  if (decimals == 0xFF) return false;  // Keine Rundung gewuenscht
  String normalized = value;
  normalized.replace(",", ".");
  char* end = nullptr;
  float f = strtof(normalized.c_str(), &end);
  if (!end || end == normalized.c_str()) return false;  // Keine Zahl
  if (isnan(f) || isinf(f)) return false;
  uint8_t d = decimals > 6 ? 6 : decimals;
  value = String(f, static_cast<unsigned int>(d));
  return true;
}

static bool parse_sensor_number(const String& value, float& out) {
  String normalized = value;
  normalized.trim();
  if (!normalized.length()) return false;
  normalized.replace(",", ".");
  char* end = nullptr;
  float f = strtof(normalized.c_str(), &end);
  if (!end || end == normalized.c_str()) return false;
  if (isnan(f) || isinf(f)) return false;
  out = f;
  return true;
}

bool is_light_entity_id(const String& entity_id) {
  return entity_id.length() >= 6 && entity_id.startsWith("light.");
}

static int32_t map_gauge_arc_value(float numeric, int32_t min_value, int32_t max_value) {
  if (max_value <= min_value) {
    min_value = 0;
    max_value = 100;
  }
  const float span = static_cast<float>(max_value - min_value);
  float ratio = (numeric - static_cast<float>(min_value)) / span;
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;
  return static_cast<int32_t>(roundf(ratio * static_cast<float>(GAUGE_ARC_STEPS)));
}

// MQTT Callback ruft das auf (thread-safe!)
void queue_sensor_tile_update(GridType grid_type, uint8_t grid_index, const char* value, const char* unit) {
  if (grid_index >= TILES_PER_GRID || !value) {
    return;
  }

  // Bestehendes, noch nicht verarbeitetes Update fuer dieselbe Tile ersetzen
  uint8_t idx = g_queue_tail;
  while (idx != g_queue_head) {
    SensorUpdate& pending = g_update_queue[idx];
    if (pending.valid &&
        pending.grid_type == grid_type &&
        pending.grid_index == grid_index) {
      pending.value = String(value);
      pending.unit = unit ? String(unit) : "";
      return;
    }
    idx = (idx + 1) % QUEUE_SIZE;
  }

  uint8_t next_head = (g_queue_head + 1) % QUEUE_SIZE;

  if (next_head == g_queue_tail) {
    // Queue voll - aeltestes Element verwerfen und ueberschreiben
    if ((g_queue_overflow_count++ % 10) == 0) {
      Serial.println("[Queue] VOLL! Aeltestes Sensor-Update wird ueberschrieben");
    }
    g_queue_tail = (g_queue_tail + 1) % QUEUE_SIZE;
  }

  g_update_queue[g_queue_head].grid_type = grid_type;
  g_update_queue[g_queue_head].grid_index = grid_index;
  g_update_queue[g_queue_head].value = String(value);
  g_update_queue[g_queue_head].unit = unit ? String(unit) : "";
  g_update_queue[g_queue_head].valid = true;

  g_queue_head = next_head;
}

// Main Loop ruft das VOR lv_timer_handler() auf!
void process_sensor_update_queue(uint8_t max_updates) {
  uint8_t processed = 0;
  while (g_queue_tail != g_queue_head && (max_updates == 0 || processed < max_updates)) {
    SensorUpdate& upd = g_update_queue[g_queue_tail];

    if (upd.valid) {
      update_sensor_tile_value(upd.grid_type, upd.grid_index, upd.value.c_str(),
                              upd.unit.length() > 0 ? upd.unit.c_str() : nullptr);
      upd.valid = false;
      ++processed;
    }

    g_queue_tail = (g_queue_tail + 1) % QUEUE_SIZE;
  }
}

/* === Thread-Safe Update Queue (MQTT -> Main Loop) fuer Switches === */
struct SwitchUpdate {
  GridType grid_type;
  uint8_t grid_index;
  String payload;
  bool valid;
};

static const uint8_t SWITCH_QUEUE_SIZE = 32;
static SwitchUpdate g_switch_queue[SWITCH_QUEUE_SIZE];
static volatile uint8_t g_switch_head = 0;
static volatile uint8_t g_switch_tail = 0;
static uint32_t g_switch_overflow_count = 0;

static uint32_t clamp_rgb(int r, int g, int b) {
  auto clamp = [](int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); };
  return (static_cast<uint32_t>(clamp(r)) << 16) |
         (static_cast<uint32_t>(clamp(g)) << 8) |
         static_cast<uint32_t>(clamp(b));
}

static bool parse_on_off(const String& text, bool& is_on) {
  String lower = text;
  lower.trim();
  lower.toLowerCase();
  if (lower == "on" || lower == "true" || lower == "1" || lower == "yes") {
    is_on = true;
    return true;
  }
  if (lower == "off" || lower == "false" || lower == "0" || lower == "no") {
    is_on = false;
    return true;
  }
  return false;
}

static bool parse_hex_color(const String& text, uint32_t& color) {
  String t = text;
  t.trim();
  if (t.startsWith("#")) t.remove(0, 1);
  if (t.startsWith("0x") || t.startsWith("0X")) t.remove(0, 2);
  if (t.length() != 6) return false;
  char* end = nullptr;
  long val = strtol(t.c_str(), &end, 16);
  if (!end || end == t.c_str() || *end != '\0') return false;
  color = static_cast<uint32_t>(val) & 0xFFFFFF;
  return true;
}

static bool parse_rgb_list(const String& list, int& r, int& g, int& b) {
  const char* ptr = list.c_str();
  char* end = nullptr;
  long vals[3];
  for (int i = 0; i < 3; ++i) {
    while (*ptr && (*ptr == ' ' || *ptr == ',')) ++ptr;
    if (!*ptr) return false;
    vals[i] = strtol(ptr, &end, 10);
    if (!end || end == ptr) return false;
    ptr = end;
  }
  r = static_cast<int>(vals[0]);
  g = static_cast<int>(vals[1]);
  b = static_cast<int>(vals[2]);
  return true;
}

static bool parse_hs_list(const String& list, float& h, float& s) {
  const char* ptr = list.c_str();
  char* end = nullptr;
  float vals[2];
  for (int i = 0; i < 2; ++i) {
    while (*ptr && (*ptr == ' ' || *ptr == ',')) ++ptr;
    if (!*ptr) return false;
    vals[i] = strtof(ptr, &end);
    if (!end || end == ptr) return false;
    ptr = end;
  }
  h = vals[0];
  s = vals[1];
  return true;
}

static uint32_t hs_to_rgb(float h, float s) {
  float hh = fmodf(h, 360.0f);
  if (hh < 0) hh += 360.0f;
  float sat = s / 100.0f;
  float c = sat;
  float x = c * (1.0f - fabsf(fmodf(hh / 60.0f, 2.0f) - 1.0f));
  float m = 1.0f - c;
  float r1 = 0, g1 = 0, b1 = 0;
  if (hh < 60.0f) { r1 = c; g1 = x; b1 = 0; }
  else if (hh < 120.0f) { r1 = x; g1 = c; b1 = 0; }
  else if (hh < 180.0f) { r1 = 0; g1 = c; b1 = x; }
  else if (hh < 240.0f) { r1 = 0; g1 = x; b1 = c; }
  else if (hh < 300.0f) { r1 = x; g1 = 0; b1 = c; }
  else { r1 = c; g1 = 0; b1 = x; }
  int r = static_cast<int>((r1 + m) * 255.0f);
  int g = static_cast<int>((g1 + m) * 255.0f);
  int b = static_cast<int>((b1 + m) * 255.0f);
  return clamp_rgb(r, g, b);
}

static bool extract_json_string_field(const String& src, const char* key, String& out) {
  if (!key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  int idx = src.indexOf(pattern);
  if (idx < 0) return false;
  int colon = src.indexOf(':', idx);
  if (colon < 0) return false;
  int q1 = src.indexOf('"', colon);
  if (q1 < 0) return false;
  int q2 = src.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  out = src.substring(q1 + 1, q2);
  out.trim();
  return out.length() > 0;
}

static bool extract_json_string_field_cstr(const char* src, const char* key, String& out) {
  if (!src || !key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  const char* idx = strstr(src, pattern.c_str());
  if (!idx) return false;
  const char* colon = strchr(idx + pattern.length(), ':');
  if (!colon) return false;
  const char* q1 = strchr(colon, '"');
  if (!q1) return false;
  const char* q2 = q1 + 1;
  bool escaped = false;
  while (*q2) {
    if (*q2 == '"' && !escaped) break;
    escaped = (*q2 == '\\' && !escaped);
    if (*q2 != '\\') escaped = false;
    ++q2;
  }
  if (*q2 != '"') return false;

  out = "";
  out.reserve(q2 - q1 - 1);
  for (const char* p = q1 + 1; p < q2; ++p) {
    out += *p;
  }
  out.trim();
  return out.length() > 0;
}

static bool extract_json_array_field(const String& src, const char* key, String& out) {
  if (!key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  int idx = src.indexOf(pattern);
  if (idx < 0) return false;
  int start = src.indexOf('[', idx);
  int end = src.indexOf(']', start);
  if (start < 0 || end < start) return false;
  out = src.substring(start + 1, end);
  out.trim();
  return out.length() > 0;
}

static bool extract_json_number_field(const String& src, const char* key, float& out) {
  if (!key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  int idx = src.indexOf(pattern);
  if (idx < 0) return false;
  int colon = src.indexOf(':', idx);
  if (colon < 0) return false;
  int pos = colon + 1;
  while (pos < src.length() && (src.charAt(pos) == ' ' || src.charAt(pos) == '\t')) {
    ++pos;
  }
  if (pos >= src.length()) return false;
  const char* start = src.c_str() + pos;
  char* end = nullptr;
  float val = strtof(start, &end);
  if (!end || end == start) return false;
  out = val;
  return true;
}

static bool extract_json_number_field_cstr(const char* src, const char* key, float& out) {
  if (!src || !key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  const char* idx = strstr(src, pattern.c_str());
  if (!idx) return false;
  const char* colon = strchr(idx + pattern.length(), ':');
  if (!colon) return false;
  const char* start = colon + 1;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
    ++start;
  }
  bool quoted = *start == '"';
  if (quoted) ++start;
  char* end = nullptr;
  float val = strtof(start, &end);
  if (!end || end == start) return false;
  if (quoted && *end != '"') return false;
  out = val;
  return true;
}

static bool extract_json_bool_field_cstr(const char* src, const char* key, bool& out) {
  if (!src || !key || !*key) return false;
  String pattern = String("\"") + key + "\"";
  const char* idx = strstr(src, pattern.c_str());
  if (!idx) return false;
  const char* colon = strchr(idx + pattern.length(), ':');
  if (!colon) return false;
  const char* start = colon + 1;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
    ++start;
  }
  if (strncmp(start, "true", 4) == 0) {
    out = true;
    return true;
  }
  if (strncmp(start, "false", 5) == 0) {
    out = false;
    return true;
  }
  if (*start == '"' && strncmp(start + 1, "true", 4) == 0) {
    out = true;
    return true;
  }
  if (*start == '"' && strncmp(start + 1, "false", 5) == 0) {
    out = false;
    return true;
  }
  return false;
}

static bool extract_json_object_field(const String& src, const char* key, String& out) {
  if (!key || !*key) return false;
  String pattern = "\"";
  pattern += key;
  pattern += "\"";
  int idx = src.indexOf(pattern);
  if (idx < 0) return false;
  int colon = src.indexOf(':', idx);
  if (colon < 0) return false;
  int pos = colon + 1;
  while (pos < src.length() && (src.charAt(pos) == ' ' || src.charAt(pos) == '\t')) {
    ++pos;
  }
  if (pos >= src.length() || src.charAt(pos) != '{') return false;
  int depth = 0;
  bool in_string = false;
  for (int i = pos; i < src.length(); ++i) {
    char c = src.charAt(i);
    if (c == '"' && (i == 0 || src.charAt(i - 1) != '\\')) {
      in_string = !in_string;
    }
    if (in_string) continue;
    if (c == '{') {
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0) {
        out = src.substring(pos, i + 1);
        return true;
      }
    }
  }
  return false;
}

static bool extract_json_number_or_string_field(const String& src, const char* key, float& out) {
  if (extract_json_number_field(src, key, out)) return true;
  String text;
  if (!extract_json_string_field(src, key, text)) return false;
  text.trim();
  if (!text.length()) return false;
  text.replace(",", ".");
  char* end = nullptr;
  float val = strtof(text.c_str(), &end);
  if (!end || end == text.c_str()) return false;
  out = val;
  return true;
}

static String format_weather_temp(float temp, const String& unit) {
  String text = String(temp, 1);
  if (text.endsWith(".0")) {
    text.remove(text.length() - 2);
  }
  if (unit.length()) {
    text += " ";
    text += unit;
  }
  return text;
}

static String format_weather_temp_value(float temp) {
  String text = String(temp, 1);
  if (text.endsWith(".0")) {
    text.remove(text.length() - 2);
  }
  return text;
}

static String format_weather_temp_unit(const String& unit) {
  return String("\xE2\x80\x89") + (unit.length() ? unit : String("\xC2\xB0\x43"));
}

static void position_tile_value_unit_centered(
    lv_obj_t* val_label, lv_obj_t* unit_label,
    lv_coord_t center_x, lv_coord_t y, lv_coord_t wrap_w) {
  lv_obj_update_layout(val_label);
  lv_obj_update_layout(unit_label);
  lv_coord_t val_w = lv_obj_get_width(val_label);
  lv_coord_t unit_w = lv_obj_get_width(unit_label);
  lv_coord_t total_w = val_w + unit_w;
  lv_coord_t x = center_x - (total_w / 2);
  if (x < 0) x = 0;
  if (x + total_w > wrap_w) x = wrap_w - total_w;
  lv_obj_set_pos(val_label, x, y);
  const lv_coord_t unit_y_offset = 5;
  lv_obj_set_pos(unit_label, x + val_w, y + unit_y_offset);
}

static void decode_basic_json_escapes(String& text) {
  if (!text.length()) return;
  auto is_hex_digit = [](char c) -> bool {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
  };
  auto hex_value = [](char c) -> uint8_t {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
    return 0;
  };
  auto append_utf8 = [](String& out, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
      out += static_cast<char>(codepoint);
    } else if (codepoint <= 0x7FF) {
      out += static_cast<char>(0xC0 | (codepoint >> 6));
      out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
      out += static_cast<char>(0xE0 | (codepoint >> 12));
      out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (codepoint >> 18));
      out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
  };

  String out;
  out.reserve(text.length());
  for (size_t i = 0; i < text.length(); ++i) {
    char c = text.charAt(i);
    if (c != '\\' || i + 1 >= text.length()) {
      out += c;
      continue;
    }

    char esc = text.charAt(i + 1);
    if (esc == 'u' && i + 5 < text.length() &&
        is_hex_digit(text.charAt(i + 2)) &&
        is_hex_digit(text.charAt(i + 3)) &&
        is_hex_digit(text.charAt(i + 4)) &&
        is_hex_digit(text.charAt(i + 5))) {
      uint16_t code = (static_cast<uint16_t>(hex_value(text.charAt(i + 2))) << 12) |
                      (static_cast<uint16_t>(hex_value(text.charAt(i + 3))) << 8) |
                      (static_cast<uint16_t>(hex_value(text.charAt(i + 4))) << 4) |
                      static_cast<uint16_t>(hex_value(text.charAt(i + 5)));
      i += 5;

      if (code >= 0xD800 && code <= 0xDBFF &&
          i + 6 < text.length() &&
          text.charAt(i + 1) == '\\' &&
          text.charAt(i + 2) == 'u' &&
          is_hex_digit(text.charAt(i + 3)) &&
          is_hex_digit(text.charAt(i + 4)) &&
          is_hex_digit(text.charAt(i + 5)) &&
          is_hex_digit(text.charAt(i + 6))) {
        uint16_t low = (static_cast<uint16_t>(hex_value(text.charAt(i + 3))) << 12) |
                       (static_cast<uint16_t>(hex_value(text.charAt(i + 4))) << 8) |
                       (static_cast<uint16_t>(hex_value(text.charAt(i + 5))) << 4) |
                       static_cast<uint16_t>(hex_value(text.charAt(i + 6)));
        if (low >= 0xDC00 && low <= 0xDFFF) {
          uint32_t codepoint = 0x10000 + (((code - 0xD800) << 10) | (low - 0xDC00));
          append_utf8(out, codepoint);
          i += 6;
          continue;
        }
      }

      append_utf8(out, code);
      continue;
    }

    switch (esc) {
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      case '/': out += '/'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      default: out += esc; break;
    }
    ++i;
  }
  text = out;
}

static String weather_icon_from_condition(const String& condition) {
  String key = condition;
  key.trim();
  key.toLowerCase();
  if (!key.length()) return "";
  if (key == "clear-night") return "mdi:weather-night";
  if (key == "cloudy") return "mdi:weather-cloudy";
  if (key == "exceptional") return "mdi:alert-circle-outline";
  if (key == "fog") return "mdi:weather-fog";
  if (key == "hail") return "mdi:weather-hail";
  if (key == "lightning") return "mdi:weather-lightning";
  if (key == "lightning-rainy") return "mdi:weather-lightning-rainy";
  if (key == "partlycloudy") return "mdi:weather-partly-cloudy";
  if (key == "pouring") return "mdi:weather-pouring";
  if (key == "rainy") return "mdi:weather-rainy";
  if (key == "snowy") return "mdi:weather-snowy";
  if (key == "snowy-rainy") return "mdi:weather-snowy-rainy";
  if (key == "sunny") return "mdi:weather-sunny";
  if (key == "windy") return "mdi:weather-windy";
  if (key == "windy-variant") return "mdi:weather-windy-variant";
  return "";
}

static String weather_condition_to_german(const String& condition) {
  return i18n::weather_condition_label(configManager.getConfig().language, condition);
  String key = condition;
  key.trim();
  key.toLowerCase();
  if (!key.length()) return "--";
  if (key == "clear-night") return "Klare Nacht";
  if (key == "cloudy") return "Bewölkt";
  if (key == "exceptional") return "Ausnahme";
  if (key == "fog") return "Nebel";
  if (key == "hail") return "Hagel";
  if (key == "lightning") return "Gewitter";
  if (key == "lightning-rainy") return "Gewitterregen";
  if (key == "partlycloudy") return "Teilw. bewölkt";
  if (key == "pouring") return "Starkregen";
  if (key == "rainy") return "Regen";
  if (key == "snowy") return "Schnee";
  if (key == "snowy-rainy") return "Schneeregen";
  if (key == "sunny") return "Sonnig";
  if (key == "windy") return "Windig";
  if (key == "windy-variant") return "Böig";
  String text = condition;
  text.replace("-", " ");
  text.replace("_", " ");
  text.trim();
  return text.length() ? text : "--";
}

static String short_date_from_iso(const String& iso) {
  int dash1 = iso.indexOf('-');
  if (dash1 < 0) return "";
  int dash2 = iso.indexOf('-', dash1 + 1);
  if (dash2 < 0) return "";
  String mm = iso.substring(dash1 + 1, dash2);
  if (dash2 + 2 > iso.length()) return "";
  String dd = iso.substring(dash2 + 1, dash2 + 3);
  if (!dd.length() || !mm.length()) return "";
  return dd + "." + mm;
}

static bool parse_iso_date(const String& iso, int& y, int& m, int& d) {
  if (iso.length() < 10) return false;
  if (iso.charAt(4) != '-' || iso.charAt(7) != '-') return false;
  y = iso.substring(0, 4).toInt();
  m = iso.substring(5, 7).toInt();
  d = iso.substring(8, 10).toInt();
  return (y > 0 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}

static String weekday_from_iso(const String& iso) {
  return i18n::weather_weekday_short(configManager.getConfig().language, iso);
  int y = 0, m = 0, d = 0;
  if (!parse_iso_date(iso, y, m, d)) return "";
  int mm = m;
  int yy = y;
  if (mm < 3) {
    mm += 12;
    yy -= 1;
  }
  int K = yy % 100;
  int J = yy / 100;
  int h = (d + (13 * (mm + 1)) / 5 + K + (K / 4) + (J / 4) + (5 * J)) % 7;
  // h: 0=Saturday, 1=Sunday, 2=Monday, ...
  int dow = (h + 6) % 7;  // 0=Sunday, 1=Monday, ...
  static const char* kDaysDe[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  if (dow < 0 || dow > 6) return "";
  return String(kDaysDe[dow]);
}

static const char* weather_today_tile_label() {
  return (configManager.getConfig().language[0] == 'd') ? "Heute" : "Today";
}

static bool get_local_today_date(String& date_out) {
  time_t now = time(nullptr);
  if (now < 1704067200) return false;

  struct tm local_tm;
  if (!localtime_r(&now, &local_tm)) return false;

  char date_buf[11];
  if (strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &local_tm) == 0) return false;
  date_out = date_buf;
  return true;
}

static String iso_date_add_days(const String& iso, int day_offset) {
  int y = 0, m = 0, d = 0;
  if (!parse_iso_date(iso, y, m, d)) return "";

  struct tm date_tm = {};
  date_tm.tm_year = y - 1900;
  date_tm.tm_mon = m - 1;
  date_tm.tm_mday = d + day_offset;
  date_tm.tm_hour = 12;

  time_t date_value = mktime(&date_tm);
  if (date_value < 0) return "";

  struct tm normalized_tm;
  if (!localtime_r(&date_value, &normalized_tm)) return "";

  char date_buf[11];
  if (strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &normalized_tm) == 0) return "";
  return date_buf;
}

static int iso_date_day_offset(const String& base_iso, const String& target_iso) {
  int base_y = 0, base_m = 0, base_d = 0;
  int target_y = 0, target_m = 0, target_d = 0;
  if (!parse_iso_date(base_iso, base_y, base_m, base_d) ||
      !parse_iso_date(target_iso, target_y, target_m, target_d)) {
    return -32768;
  }

  struct tm base_tm = {};
  base_tm.tm_year = base_y - 1900;
  base_tm.tm_mon = base_m - 1;
  base_tm.tm_mday = base_d;
  base_tm.tm_hour = 12;

  struct tm target_tm = {};
  target_tm.tm_year = target_y - 1900;
  target_tm.tm_mon = target_m - 1;
  target_tm.tm_mday = target_d;
  target_tm.tm_hour = 12;

  time_t base_value = mktime(&base_tm);
  time_t target_value = mktime(&target_tm);
  if (base_value < 0 || target_value < 0) return -32768;

  return static_cast<int>((target_value - base_value) / 86400);
}

static bool list_contains_mode(String list, const char* mode) {
  if (!mode || !*mode) return false;
  list.toLowerCase();
  list.replace("\"", "");
  int start = 0;
  while (start < list.length()) {
    int comma = list.indexOf(',', start);
    if (comma < 0) comma = list.length();
    String token = list.substring(start, comma);
    token.trim();
    if (token == mode) return true;
    start = comma + 1;
  }
  return false;
}

static bool is_color_mode(const String& mode) {
  return mode == "hs" ||
         mode == "rgb" ||
         mode == "xy" ||
         mode == "rgbw" ||
         mode == "rgbww" ||
         mode == "color_temp";
}

static bool is_switch_widget_style(const Tile& tile) {
  return tile.sensor_decimals == 1;
}

static LightPopupInit build_popup_init_from_state(
    GridType grid_type,
    uint8_t tile_index,
    const Tile& tile,
    const SwitchState& state) {
  LightPopupInit init;
  init.entity_id = tile.sensor_entity;
  init.title = tile.title;
  String icon_name = tile.icon_name;
  bool icon_disabled = isMdiIconDisabled(icon_name);
  icon_name = normalizeMdiIconName(icon_name);
  if (!icon_disabled && !icon_name.length() && tile.sensor_entity.length()) {
    icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
  }
  init.icon_name = icon_name;
  init.is_light = is_light_entity_id(tile.sensor_entity);
  init.keep_icon_white = is_switch_widget_style(tile);
  init.has_tile_ref = true;
  init.tile_grid = static_cast<uint8_t>(grid_type);
  init.tile_index = tile_index;
  init.has_state = state.has_state;
  init.has_color = state.has_color;
  init.has_brightness = state.has_brightness;
  init.has_color_temp = state.has_color_temp;
  init.has_hs = state.has_hs;
  init.hs_h = state.hs_h;
  init.hs_s = state.hs_s;
  init.color_temp_kelvin = state.color_temp_kelvin;
  init.min_color_temp_kelvin = state.min_color_temp_kelvin;
  init.max_color_temp_kelvin = state.max_color_temp_kelvin;

  if (state.has_state) {
    init.is_on = state.is_on;
  } else if (state.has_brightness) {
    init.is_on = state.brightness_pct > 0;
  } else {
    init.is_on = true;
  }

  if (init.is_light) {
    init.supports_color = state.supports_color;
    init.supports_brightness = state.supports_brightness || state.supports_color;
    init.supports_temperature = state.supports_temperature;
  } else {
    init.supports_color = false;
    init.supports_brightness = false;
    init.supports_temperature = false;
  }

  if (state.has_color) {
    init.color = state.color;
  }
  if (state.has_brightness) {
    init.brightness_pct = state.brightness_pct;
  } else if (state.has_state && !state.is_on) {
    init.brightness_pct = 0;
  } else {
    init.brightness_pct = 100;
  }

  return init;
}

static SwitchState parse_switch_payload(const char* payload) {
  SwitchState out;
  if (!payload) return out;
  String text = payload;
  text.trim();
  if (!text.length()) return out;

  auto set_color_temp_kelvin = [&out](float raw_kelvin) {
    int kelvin = static_cast<int>(roundf(raw_kelvin));
    if (kelvin < 1500) kelvin = 1500;
    if (kelvin > 9000) kelvin = 9000;
    out.has_color_temp = true;
    out.color_temp_kelvin = static_cast<uint16_t>(kelvin);
    out.supports_temperature = true;
  };

  auto set_color_temp_range = [&out](float raw_min_kelvin, float raw_max_kelvin) {
    int min_kelvin = static_cast<int>(roundf(raw_min_kelvin));
    int max_kelvin = static_cast<int>(roundf(raw_max_kelvin));
    if (min_kelvin < 1500) min_kelvin = 1500;
    if (max_kelvin > 9000) max_kelvin = 9000;
    if (max_kelvin < 1500) max_kelvin = 1500;
    if (min_kelvin > 9000) min_kelvin = 9000;
    if (min_kelvin > max_kelvin) {
      const int tmp = min_kelvin;
      min_kelvin = max_kelvin;
      max_kelvin = tmp;
    }
    out.min_color_temp_kelvin = static_cast<uint16_t>(min_kelvin);
    out.max_color_temp_kelvin = static_cast<uint16_t>(max_kelvin);
    out.supports_temperature = true;
  };

  auto try_extract_color_temp = [&set_color_temp_kelvin](const String& source) {
    float color_temp_kelvin = 0.0f;
    if (extract_json_number_field(source, "color_temp_kelvin", color_temp_kelvin) ||
        extract_json_number_or_string_field(source, "color_temp_kelvin", color_temp_kelvin) ||
        extract_json_number_field(source, "kelvin", color_temp_kelvin) ||
        extract_json_number_or_string_field(source, "kelvin", color_temp_kelvin)) {
      set_color_temp_kelvin(color_temp_kelvin);
      return true;
    }

    float color_temp_mired = 0.0f;
    if (extract_json_number_field(source, "color_temp", color_temp_mired) ||
        extract_json_number_or_string_field(source, "color_temp", color_temp_mired)) {
      if (color_temp_mired > 0.0f) {
        set_color_temp_kelvin(1000000.0f / color_temp_mired);
        return true;
      }
    }

    return false;
  };

  auto try_extract_color_temp_range = [&set_color_temp_range](const String& source) {
    float min_kelvin = 0.0f;
    float max_kelvin = 0.0f;
    const bool has_min_kelvin =
        extract_json_number_field(source, "min_color_temp_kelvin", min_kelvin) ||
        extract_json_number_or_string_field(source, "min_color_temp_kelvin", min_kelvin);
    const bool has_max_kelvin =
        extract_json_number_field(source, "max_color_temp_kelvin", max_kelvin) ||
        extract_json_number_or_string_field(source, "max_color_temp_kelvin", max_kelvin);
    if (has_min_kelvin && has_max_kelvin) {
      set_color_temp_range(min_kelvin, max_kelvin);
      return true;
    }

    float min_mired = 0.0f;
    float max_mired = 0.0f;
    const bool has_min_mired =
        extract_json_number_field(source, "min_mireds", min_mired) ||
        extract_json_number_or_string_field(source, "min_mireds", min_mired);
    const bool has_max_mired =
        extract_json_number_field(source, "max_mireds", max_mired) ||
        extract_json_number_or_string_field(source, "max_mireds", max_mired);
    if (has_min_mired && has_max_mired && min_mired > 0.0f && max_mired > 0.0f) {
      set_color_temp_range(1000000.0f / max_mired, 1000000.0f / min_mired);
      return true;
    }

    return false;
  };

  if (text.startsWith("{")) {
    String state;
    if (extract_json_string_field(text, "state", state)) {
      out.has_state = parse_on_off(state, out.is_on);
    }

    String supported_modes;
    if (extract_json_array_field(text, "supported_color_modes", supported_modes)) {
      out.supported_modes_known = true;
      bool has_brightness = list_contains_mode(supported_modes, "brightness");
      bool has_color = list_contains_mode(supported_modes, "hs") ||
                       list_contains_mode(supported_modes, "rgb") ||
                       list_contains_mode(supported_modes, "xy") ||
                       list_contains_mode(supported_modes, "rgbw") ||
                       list_contains_mode(supported_modes, "rgbww") ||
                       list_contains_mode(supported_modes, "color_temp");
      bool has_temperature = list_contains_mode(supported_modes, "color_temp");
      bool has_onoff = list_contains_mode(supported_modes, "onoff");
      out.supports_brightness = has_brightness;
      out.supports_color = has_color;
      out.supports_temperature = has_temperature;
      out.supported_onoff_only = has_onoff && !has_brightness && !has_color;
    }

    String color_mode;
    if (extract_json_string_field(text, "color_mode", color_mode)) {
      String mode = color_mode;
      mode.toLowerCase();
      if (mode == "brightness") {
        out.supports_brightness = true;
        out.supported_onoff_only = false;
      }
      if (mode == "color_temp") {
        out.supports_temperature = true;
      }
      if (is_color_mode(mode)) {
        out.supports_color = true;
        out.supported_onoff_only = false;
      }
      if (mode == "onoff" && !out.supported_modes_known) {
        out.supported_modes_known = true;
        out.supported_onoff_only = true;
      }
    }

    float bright_pct = -1.0f;
    float bright_raw = -1.0f;
    if (extract_json_number_field(text, "brightness_pct", bright_pct)) {
      int pct = static_cast<int>(roundf(bright_pct));
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      out.has_brightness = true;
      out.brightness_pct = static_cast<uint8_t>(pct);
    } else if (extract_json_number_field(text, "brightness", bright_raw)) {
      int pct = static_cast<int>(roundf((bright_raw / 255.0f) * 100.0f));
      if (pct < 0) pct = 0;
      if (pct > 100) pct = 100;
      out.has_brightness = true;
      out.brightness_pct = static_cast<uint8_t>(pct);
    }

    String color_text;
    if (extract_json_string_field(text, "color", color_text)) {
      uint32_t color = 0;
      if (parse_hex_color(color_text, color)) {
        out.has_color = true;
        out.color = color;
      }
    }

    if (!out.has_color) {
      String rgb_list;
      if (extract_json_array_field(text, "rgb_color", rgb_list)) {
        int r = 0, g = 0, b = 0;
        if (parse_rgb_list(rgb_list, r, g, b)) {
          out.has_color = true;
          out.color = clamp_rgb(r, g, b);
        }
      }
    }

    {
      String hs_list;
      if (extract_json_array_field(text, "hs_color", hs_list)) {
        float h = 0.0f, s = 0.0f;
        if (parse_hs_list(hs_list, h, s)) {
          out.has_hs = true;
          out.hs_h = h;
          out.hs_s = s;
          out.has_color = true;
          out.color = hs_to_rgb(h, s);
        }
      }
    }

    try_extract_color_temp(text);
    try_extract_color_temp_range(text);

    String attributes;
    if (extract_json_object_field(text, "attributes", attributes)) {
      if (extract_json_array_field(attributes, "supported_color_modes", supported_modes)) {
        out.supported_modes_known = true;
        bool has_brightness = list_contains_mode(supported_modes, "brightness");
        bool has_color = list_contains_mode(supported_modes, "hs") ||
                         list_contains_mode(supported_modes, "rgb") ||
                         list_contains_mode(supported_modes, "xy") ||
                         list_contains_mode(supported_modes, "rgbw") ||
                         list_contains_mode(supported_modes, "rgbww") ||
                         list_contains_mode(supported_modes, "color_temp");
        bool has_temperature = list_contains_mode(supported_modes, "color_temp");
        bool has_onoff = list_contains_mode(supported_modes, "onoff");
        out.supports_brightness = has_brightness;
        out.supports_color = has_color;
        out.supports_temperature = has_temperature;
        out.supported_onoff_only = has_onoff && !has_brightness && !has_color;
      }

      if (extract_json_string_field(attributes, "color_mode", color_mode)) {
        String mode = color_mode;
        mode.toLowerCase();
        if (mode == "brightness") {
          out.supports_brightness = true;
          out.supported_onoff_only = false;
        }
        if (mode == "color_temp") {
          out.supports_temperature = true;
        }
        if (is_color_mode(mode)) {
          out.supports_color = true;
          out.supported_onoff_only = false;
        }
        if (mode == "onoff" && !out.supported_modes_known) {
          out.supported_modes_known = true;
          out.supported_onoff_only = true;
        }
      }

      if (!out.has_brightness && extract_json_number_field(attributes, "brightness_pct", bright_pct)) {
        int pct = static_cast<int>(roundf(bright_pct));
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        out.has_brightness = true;
        out.brightness_pct = static_cast<uint8_t>(pct);
      } else if (!out.has_brightness && extract_json_number_field(attributes, "brightness", bright_raw)) {
        int pct = static_cast<int>(roundf((bright_raw / 255.0f) * 100.0f));
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        out.has_brightness = true;
        out.brightness_pct = static_cast<uint8_t>(pct);
      }

      if (!out.has_color) {
        if (extract_json_string_field(attributes, "color", color_text)) {
          uint32_t color = 0;
          if (parse_hex_color(color_text, color)) {
            out.has_color = true;
            out.color = color;
          }
        }
      }

      if (!out.has_color) {
        String rgb_list;
        if (extract_json_array_field(attributes, "rgb_color", rgb_list)) {
          int r = 0, g = 0, b = 0;
          if (parse_rgb_list(rgb_list, r, g, b)) {
            out.has_color = true;
            out.color = clamp_rgb(r, g, b);
          }
        }
      }

      {
        String hs_list;
        if (extract_json_array_field(attributes, "hs_color", hs_list)) {
          float h = 0.0f, s = 0.0f;
          if (parse_hs_list(hs_list, h, s)) {
            out.has_hs = true;
            out.hs_h = h;
            out.hs_s = s;
            out.has_color = true;
            out.color = hs_to_rgb(h, s);
          }
        }
      }

      try_extract_color_temp(attributes);
      try_extract_color_temp_range(attributes);
    }
  }

  if (!out.has_state) {
    out.has_state = parse_on_off(text, out.is_on);
  }

  if (!out.has_color) {
    uint32_t color = 0;
    if (parse_hex_color(text, color)) {
      out.has_color = true;
      out.color = color;
    } else if (text.startsWith("rgb(") && text.endsWith(")")) {
      String list = text.substring(4, text.length() - 1);
      int r = 0, g = 0, b = 0;
      if (parse_rgb_list(list, r, g, b)) {
        out.has_color = true;
        out.color = clamp_rgb(r, g, b);
      }
    }
  }

  if (!out.has_state && out.has_color) {
    out.has_state = true;
    out.is_on = true;
  }
  if (!out.has_state && out.has_brightness) {
    out.has_state = true;
    out.is_on = out.brightness_pct > 0;
  }
  if (!out.has_state && out.has_color_temp) {
    out.has_state = true;
    out.is_on = true;
  }

  if (out.has_color) {
    out.supports_color = true;
  }
  if (out.has_hs) {
    out.supports_color = true;
  }
  if (out.has_color_temp) {
    out.supports_temperature = true;
  }
  if (out.has_brightness) {
    out.supports_brightness = true;
    out.supported_onoff_only = false;
  }
  if (out.supports_color) {
    out.supports_brightness = true;
    out.supported_onoff_only = false;
  }

  return out;
}

void update_switch_tile_state(GridType grid_type, uint8_t grid_index, const char* payload) {
  if (grid_index >= TILES_PER_GRID || !payload) return;
  SwitchTileWidgets* target = g_tab0_switches;
  SwitchState* state_target = g_tab0_switch_states;
  if (grid_type == GridType::TAB1) {
    target = g_tab1_switches;
    state_target = g_tab1_switch_states;
  } else if (grid_type == GridType::TAB2) {
    target = g_tab2_switches;
    state_target = g_tab2_switch_states;
  }

  SwitchState state = parse_switch_payload(payload);
  if (!state.has_state &&
      !state.has_color &&
      !state.has_brightness &&
      !state.supports_color &&
      !state.supports_brightness) {
    return;
  }

  SwitchState prev = state_target[grid_index];
  if (!state.supported_modes_known && prev.supported_modes_known) {
    state.supported_modes_known = true;
    state.supported_onoff_only = prev.supported_onoff_only;
    state.supports_color = prev.supports_color;
    state.supports_brightness = prev.supports_brightness;
  }
  if (!state.supported_modes_known) {
    if (!state.supports_color && prev.supports_color) {
      state.supports_color = true;
    }
    if (!state.supports_brightness && prev.supports_brightness) {
      state.supports_brightness = true;
    }
  }
  if (state.supports_color) {
    state.supports_brightness = true;
  }

  if (!state.has_color && prev.has_color && !state.supported_onoff_only) {
    state.has_color = true;
    state.color = prev.color;
  }

  (void)grid_type;
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  const Tile& tile = grid.tiles[grid_index];
  const String& entity_id = tile.sensor_entity;
  const bool is_light_entity = is_light_entity_id(entity_id);
  const bool use_switch_widget = is_switch_widget_style(tile);
  if (is_light_entity) {
    if (state.supported_modes_known && state.supported_onoff_only) {
      state.supports_color = false;
      state.supports_brightness = false;
    }
  }

  state_target[grid_index] = state;

  if (tile.sensor_entity.length()) {
    LightPopupInit init = build_popup_init_from_state(grid_type, grid_index, tile, state);
    update_light_popup(init);
  }

  SwitchTileWidgets& widgets = target[grid_index];
  if (!widgets.icon_label && !widgets.title_label && !widgets.switch_obj) return;

  static const uint32_t kIconOn = 0xFFD54F;
  static const uint32_t kIconOff = 0xB0B0B0;
  static const uint32_t kIconNeutral = 0xFFFFFF;
  static const uint32_t kSwitchOff = 0xFFFFFF;
  static const uint32_t kSwitchOn = 0x3B82F6;

  uint32_t icon_color = kIconOff;
  if (!state.has_state || state.is_on) {
    icon_color = state.has_color ? state.color : kIconOn;
  }

  uint32_t label_color = use_switch_widget ? kIconNeutral : icon_color;
  lv_color_t lv_color = lv_color_hex(label_color);
  if (widgets.icon_label) {
    lv_obj_set_style_text_color(widgets.icon_label, lv_color, 0);
  } else if (widgets.title_label) {
    lv_obj_set_style_text_color(widgets.title_label, lv_color, 0);
  }

  if (widgets.switch_obj) {
    if (!state.has_state || state.is_on) {
      lv_obj_add_state(widgets.switch_obj, LV_STATE_CHECKED);
    } else {
      lv_obj_remove_state(widgets.switch_obj, LV_STATE_CHECKED);
    }
    uint32_t tile_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
    lv_obj_set_style_bg_color(widgets.switch_obj, lv_color_hex(tile_color), LV_PART_KNOB);
    lv_obj_set_style_bg_color(
        widgets.switch_obj,
        lv_color_hex(use_switch_widget ? kSwitchOff : kIconOff),
        LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(
        widgets.switch_obj,
        lv_color_hex(use_switch_widget ? kSwitchOn : icon_color),
        LV_PART_INDICATOR | LV_STATE_CHECKED);
  }
}

void queue_switch_tile_update(GridType grid_type, uint8_t grid_index, const char* payload) {
  if (grid_index >= TILES_PER_GRID || !payload) {
    return;
  }

  uint8_t idx = g_switch_tail;
  while (idx != g_switch_head) {
    SwitchUpdate& pending = g_switch_queue[idx];
    if (pending.valid &&
        pending.grid_type == grid_type &&
        pending.grid_index == grid_index) {
      pending.payload = String(payload);
      return;
    }
    idx = (idx + 1) % SWITCH_QUEUE_SIZE;
  }

  uint8_t next_head = (g_switch_head + 1) % SWITCH_QUEUE_SIZE;
  if (next_head == g_switch_tail) {
    if ((g_switch_overflow_count++ % 10) == 0) {
      Serial.println("[Queue] VOLL! Aeltestes Switch-Update wird ueberschrieben");
    }
    g_switch_tail = (g_switch_tail + 1) % SWITCH_QUEUE_SIZE;
  }

  g_switch_queue[g_switch_head].grid_type = grid_type;
  g_switch_queue[g_switch_head].grid_index = grid_index;
  g_switch_queue[g_switch_head].payload = String(payload);
  g_switch_queue[g_switch_head].valid = true;
  g_switch_head = next_head;
}

void process_switch_update_queue(uint8_t max_updates) {
  uint8_t processed = 0;
  while (g_switch_tail != g_switch_head && (max_updates == 0 || processed < max_updates)) {
    SwitchUpdate& upd = g_switch_queue[g_switch_tail];
    if (upd.valid) {
      update_switch_tile_state(upd.grid_type, upd.grid_index, upd.payload.c_str());
      upd.valid = false;
      ++processed;
    }
    g_switch_tail = (g_switch_tail + 1) % SWITCH_QUEUE_SIZE;
  }
}

/* === Thread-Safe Queue fuer Weather Updates (MQTT -> Main Loop) === */
struct WeatherUpdate {
  GridType grid_type;
  uint8_t grid_index;
  String payload;
  bool valid = false;
};

static const uint8_t WEATHER_QUEUE_SIZE = 16;
static WeatherUpdate g_weather_queue[WEATHER_QUEUE_SIZE];
static volatile uint8_t g_weather_head = 0;
static volatile uint8_t g_weather_tail = 0;
static uint32_t g_weather_overflow_count = 0;

static void update_weather_tile_state(GridType grid_type, uint8_t grid_index, const char* payload) {
  if (grid_index >= TILES_PER_GRID || !payload) return;
  WeatherTileWidgets* target = g_tab0_weather;
  if (grid_type == GridType::TAB1) target = g_tab1_weather;
  else if (grid_type == GridType::TAB2) target = g_tab2_weather;

  WeatherTileWidgets& widgets = target[grid_index];
  if (!widgets.icon_label && !widgets.temp_label && !widgets.condition_label && !widgets.location_label) {
    return;
  }

  uint32_t payload_hash = fnv1a_hash(payload);
  if (widgets.last_payload_hash == payload_hash) {
    return;
  }
  widgets.last_payload_hash = payload_hash;

  String json = payload;
  json.trim();
  if (!json.length()) return;

  String condition;
  if (!extract_json_string_field(json, "state", condition)) {
    extract_json_string_field(json, "condition", condition);
  }

  String icon_name;
  extract_json_string_field(json, "icon", icon_name);
  if (!icon_name.length() && condition.length()) {
    icon_name = weather_icon_from_condition(condition);
  }

  float temperature = 0.0f;
  bool has_temp = extract_json_number_or_string_field(json, "temperature", temperature);

  String unit;
  String units_obj;
  if (extract_json_object_field(json, "units", units_obj)) {
    extract_json_string_field(units_obj, "temperature", unit);
  } else {
    extract_json_string_field(json, "temperature_unit", unit);
  }
  decode_basic_json_escapes(unit);

  if (widgets.icon_label) {
    if (icon_name.length()) {
      String iconChar = getMdiChar(icon_name);
      if (iconChar.length()) {
        lv_label_set_text(widgets.icon_label, iconChar.c_str());
        lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  String condition_text = weather_condition_to_german(condition);

  const TileGridConfig& grid = tileConfig.getActiveGrid();
  const Tile& tile = grid.tiles[grid_index];
  const bool has_condition_text = condition_text.length() && condition_text != "--";
  const bool show_condition = (tile.span_w > 1) && has_condition_text;
  const uint8_t forecast_limit =
      (tile.span_h >= 2) ? weather_forecast_count(tile.span_w) : 0;
  String today_date;
  const bool has_today = get_local_today_date(today_date);

  if (widgets.temp_label) {
    String temp_text = has_temp ? format_weather_temp(temperature, unit) : String("--");
    if (!show_condition) {
      lv_label_set_text(widgets.temp_label, temp_text.c_str());
    } else if (!widgets.condition_label) {
      String combined = temp_text;
      if (condition_text.length() && condition_text != "--") {
        combined += " ";
        combined += condition_text;
      }
      lv_label_set_text(widgets.temp_label, combined.c_str());
    } else {
      lv_label_set_text(widgets.temp_label, temp_text.c_str());
    }
    lv_obj_clear_flag(widgets.temp_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (widgets.condition_label) {
    if (show_condition) {
      lv_label_set_text(widgets.condition_label, condition_text.c_str());
      lv_obj_clear_flag(widgets.condition_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(widgets.condition_label, "");
      lv_obj_add_flag(widgets.condition_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (widgets.condition_sep_label) {
    if (show_condition && has_temp) {
      lv_label_set_text(widgets.condition_sep_label, "|");
      lv_obj_clear_flag(widgets.condition_sep_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(widgets.condition_sep_label, "");
      lv_obj_add_flag(widgets.condition_sep_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (widgets.location_label) {
    String name = tile.title;
    name.trim();
    if (!name.length()) {
      String payload_name;
      if (extract_json_string_field(json, "name", payload_name)) {
        decode_basic_json_escapes(payload_name);
        name = payload_name;
      }
    }
    if (!name.length()) name = "--";
    lv_label_set_text(widgets.location_label, name.c_str());
    lv_obj_clear_flag(widgets.location_label, LV_OBJ_FLAG_HIDDEN);
  }

  // Forecast handling (up to WEATHER_FORECAST_MAX entries)
  struct TileForecastSlot {
    bool has_data = false;
    String date_local;
    String day_text;
    String icon_name;
    bool has_temp = false;
    float temp = 0.0f;
    bool has_low = false;
    float low = 0.0f;
  };

  TileForecastSlot forecast_slots[WEATHER_FORECAST_MAX];
  String forecast_raw;
  bool has_forecast = false;
  String base_forecast_date = has_today ? today_date : "";
  if (extract_json_array_field(json, "forecast", forecast_raw) && forecast_raw.indexOf('{') >= 0) {
    has_forecast = true;
  }

  if (has_forecast && forecast_limit > 0) {
    bool in_string = false;
    int depth = 0;
    int start = -1;
    uint8_t fallback_slot = 0;
    for (int i = 0; i < forecast_raw.length(); ++i) {
      char c = forecast_raw.charAt(i);
      if (c == '"' && (i == 0 || forecast_raw.charAt(i - 1) != '\\')) {
        in_string = !in_string;
      }
      if (in_string) continue;
      if (c == '{') {
        if (depth == 0) start = i;
        depth++;
      } else if (c == '}') {
        depth--;
        if (depth == 0 && start >= 0) {
          String obj = forecast_raw.substring(start, i + 1);
          String f_condition;
          String f_icon;
          String f_day;
          String f_date_local;
          float f_temp = 0.0f;
          float f_low = 0.0f;
          const bool f_has_temp = extract_json_number_or_string_field(obj, "temperature", f_temp);
          bool f_has_low = false;
          if (extract_json_number_or_string_field(obj, "templow", f_low)) f_has_low = true;
          if (!f_has_low && extract_json_number_or_string_field(obj, "temperature_low", f_low)) f_has_low = true;
          if (!f_has_low && extract_json_number_or_string_field(obj, "temp_low", f_low)) f_has_low = true;
          if (!f_has_low && extract_json_number_or_string_field(obj, "low", f_low)) f_has_low = true;
          extract_json_string_field(obj, "condition", f_condition);
          extract_json_string_field(obj, "icon", f_icon);
          String datetime;
          extract_json_string_field(obj, "date_local", f_date_local);
          if (extract_json_string_field(obj, "datetime", datetime)) {
            f_day = weekday_from_iso(datetime);
            if (!f_date_local.length() && datetime.length() >= 10) {
              f_date_local = datetime.substring(0, 10);
            }
          }
          if (!f_icon.length() && f_condition.length()) {
            f_icon = weather_icon_from_condition(f_condition);
          }
          if (!base_forecast_date.length() && f_date_local.length()) {
            base_forecast_date = f_date_local;
          }

          int slot_index = -1;
          if (base_forecast_date.length() && f_date_local.length()) {
            const int offset = iso_date_day_offset(base_forecast_date, f_date_local);
            if (offset >= 0 && offset < forecast_limit) {
              slot_index = offset;
            }
          }
          if (slot_index < 0) {
            while (fallback_slot < forecast_limit && forecast_slots[fallback_slot].has_data) ++fallback_slot;
            if (fallback_slot < forecast_limit) {
              slot_index = fallback_slot++;
            }
          }
          if (slot_index >= 0 && slot_index < forecast_limit) {
            TileForecastSlot& slot = forecast_slots[slot_index];
            slot.has_data = true;
            slot.date_local = f_date_local;
            slot.day_text = f_day;
            slot.icon_name = f_icon;
            slot.has_temp = f_has_temp;
            slot.temp = f_temp;
            slot.has_low = f_has_low;
            slot.low = f_low;
          }

          start = -1;
        }
      }
    }
  }

  const lv_color_t forecast_active_color = lv_color_white();
  const lv_color_t forecast_inactive_color = lv_color_hex(0x7F8BAA);
  for (uint8_t i = 0; i < forecast_limit; ++i) {
    WeatherForecastWidgets& fw = widgets.forecast[i];
    TileForecastSlot& slot = forecast_slots[i];
    String display_date = slot.date_local;
    if (!display_date.length() && base_forecast_date.length()) {
      display_date = iso_date_add_days(base_forecast_date, i);
    }

    String day_text = slot.day_text;
    if (i == 0 && (slot.has_data || display_date.length())) {
      day_text = weather_today_tile_label();
    } else if (has_today && display_date.length() && display_date == today_date) {
      day_text = weather_today_tile_label();
    } else if (!day_text.length() && display_date.length()) {
      day_text = weekday_from_iso(display_date);
    }
    if (!day_text.length()) day_text = "--";

    if (fw.day_label) {
      lv_label_set_text(fw.day_label, day_text.c_str());
      lv_obj_set_style_text_color(fw.day_label,
                                  slot.has_data ? forecast_active_color : forecast_inactive_color,
                                  0);
      lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.sep_label) {
      lv_obj_add_flag(fw.sep_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.icon_label) {
      if (slot.has_data && slot.icon_name.length()) {
        String icon_char = getMdiChar(slot.icon_name);
        if (icon_char.length()) {
          lv_label_set_text(fw.icon_label, icon_char.c_str());
          lv_obj_set_style_text_color(fw.icon_label, forecast_active_color, 0);
          lv_obj_clear_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_label_set_text(fw.icon_label, "");
          lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
        }
      } else {
        lv_label_set_text(fw.icon_label, "");
        lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
      }
    }
    if (fw.temp_high_label) {
      constexpr lv_coord_t kTileForecastTempTop = 52 + 54;
      constexpr lv_coord_t kTileForecastLowTop = kTileForecastTempTop + 30;
      constexpr lv_coord_t kTileColContentW = WEATHER_FORECAST_COL_W - 40;
      constexpr lv_coord_t kTileColCenterX = kTileColContentW / 2;
      String unit_text = format_weather_temp_unit(unit);
      if (slot.has_data && slot.has_temp) {
        lv_label_set_text(fw.temp_high_label, format_weather_temp_value(slot.temp).c_str());
        lv_label_set_text(fw.temp_high_unit_label, unit_text.c_str());
        lv_obj_set_style_text_color(fw.temp_high_label, forecast_active_color, 0);
        lv_obj_set_style_text_color(fw.temp_high_unit_label, forecast_active_color, 0);
        position_tile_value_unit_centered(fw.temp_high_label, fw.temp_high_unit_label,
                                          kTileColCenterX, kTileForecastTempTop, kTileColContentW);
        lv_obj_clear_flag(fw.temp_high_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(fw.temp_high_unit_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(fw.temp_high_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(fw.temp_high_unit_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (slot.has_data && slot.has_low) {
        lv_label_set_text(fw.temp_low_label, format_weather_temp_value(slot.low).c_str());
        lv_label_set_text(fw.temp_low_unit_label, unit_text.c_str());
        lv_obj_set_style_text_color(fw.temp_low_label, forecast_active_color, 0);
        lv_obj_set_style_text_color(fw.temp_low_unit_label, forecast_active_color, 0);
        position_tile_value_unit_centered(fw.temp_low_label, fw.temp_low_unit_label,
                                          kTileColCenterX, kTileForecastLowTop, kTileColContentW);
        lv_obj_clear_flag(fw.temp_low_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(fw.temp_low_unit_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(fw.temp_low_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(fw.temp_low_unit_label, LV_OBJ_FLAG_HIDDEN);
      }
    } else if (fw.temp_label) {
      if (slot.has_data) {
        String hi_text = slot.has_temp ? format_weather_temp(slot.temp, unit) : String("--");
        String lo_text = slot.has_low ? format_weather_temp(slot.low, unit) : String("--");
        lv_label_set_text(fw.temp_label, (hi_text + "\n" + lo_text).c_str());
        lv_obj_set_style_text_color(fw.temp_label, forecast_active_color, 0);
        lv_obj_clear_flag(fw.temp_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(fw.temp_label, "");
        lv_obj_add_flag(fw.temp_label, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

void queue_weather_tile_update(GridType grid_type, uint8_t grid_index, const char* payload) {
  if (grid_index >= TILES_PER_GRID || !payload) return;

  uint8_t idx = g_weather_tail;
  while (idx != g_weather_head) {
    WeatherUpdate& pending = g_weather_queue[idx];
    if (pending.valid &&
        pending.grid_type == grid_type &&
        pending.grid_index == grid_index) {
      pending.payload = String(payload);
      return;
    }
    idx = (idx + 1) % WEATHER_QUEUE_SIZE;
  }

  uint8_t next_head = (g_weather_head + 1) % WEATHER_QUEUE_SIZE;
  if (next_head == g_weather_tail) {
    if ((g_weather_overflow_count++ % 10) == 0) {
      Serial.println("[Queue] VOLL! Aeltestes Weather-Update wird ueberschrieben");
    }
    g_weather_tail = (g_weather_tail + 1) % WEATHER_QUEUE_SIZE;
  }

  g_weather_queue[g_weather_head].grid_type = grid_type;
  g_weather_queue[g_weather_head].grid_index = grid_index;
  g_weather_queue[g_weather_head].payload = String(payload);
  g_weather_queue[g_weather_head].valid = true;
  g_weather_head = next_head;
}

void process_weather_update_queue(uint8_t max_updates) {
  uint8_t processed = 0;
  while (g_weather_tail != g_weather_head && (max_updates == 0 || processed < max_updates)) {
    WeatherUpdate& upd = g_weather_queue[g_weather_tail];
    if (upd.valid) {
      update_weather_tile_state(upd.grid_type, upd.grid_index, upd.payload.c_str());
      upd.valid = false;
      ++processed;
    }
    g_weather_tail = (g_weather_tail + 1) % WEATHER_QUEUE_SIZE;
  }
}

/* === Thread-Safe Queue fuer Media Updates (MQTT -> Main Loop) === */
struct MediaUpdate {
  GridType grid_type;
  uint8_t grid_index;
  String payload;
  bool valid = false;
};

static const uint8_t MEDIA_QUEUE_SIZE = 24;
static MediaUpdate g_media_queue[MEDIA_QUEUE_SIZE];
static volatile uint8_t g_media_head = 0;
static volatile uint8_t g_media_tail = 0;
static uint32_t g_media_overflow_count = 0;

static String media_first_non_empty(const String& a,
                                    const String& b,
                                    const String& c = String(),
                                    const String& d = String()) {
  if (a.length()) return a;
  if (b.length()) return b;
  if (c.length()) return c;
  return d;
}

static bool media_text_same(String a, String b) {
  a.trim();
  b.trim();
  return a.length() && b.length() && a.equalsIgnoreCase(b);
}

static String media_empty_title_label(String state) {
  state.trim();
  state.toLowerCase();
  if (state == "playing") return "Wiedergabe";
  if (state == "paused") return "Pausiert";
  if (state == "idle") return "Bereit";
  if (state == "standby") return "Standby";
  if (state == "off") return "Aus";
  return "Keine Wiedergabe";
}

static void sanitize_media_display_text(String& text) {
  text.replace("`", "'");
  text.replace("\r", " ");
  text.replace("\n", " ");
  text.trim();
}

static bool set_label_text_if_changed(lv_obj_t* label, const char* text) {
  if (!label) return false;
  if (!text) text = "";
  const char* current = lv_label_get_text(label);
  if (!current || strcmp(current, text) != 0) {
    lv_label_set_text(label, text);
    return true;
  }
  return false;
}

static bool set_label_text_if_changed(lv_obj_t* label, const String& text) {
  return set_label_text_if_changed(label, text.c_str());
}

static bool set_label_long_mode_if_changed(lv_obj_t* label, lv_label_long_mode_t mode) {
  if (!label) return false;
  if (lv_label_get_long_mode(label) != mode) {
    lv_label_set_long_mode(label, mode);
    return true;
  }
  return false;
}

static void restart_visible_media_text_scroll(lv_obj_t* label) {
  if (!label || lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN)) return;
  if (lv_label_get_long_mode(label) != LV_LABEL_LONG_SCROLL) return;
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL);
}

static String media_icon_for_state(const String& state) {
  String lower = state;
  lower.trim();
  lower.toLowerCase();
  if (lower == "playing") return "pause";
  return "play";
}

static bool media_is_playing_state(const String& state) {
  String lower = state;
  lower.trim();
  lower.toLowerCase();
  return lower == "playing";
}

static String media_label_text(lv_obj_t* label) {
  if (!label || lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN)) return "";
  const char* text = lv_label_get_text(label);
  return text ? String(text) : String();
}

static bool media_widget_is_playing(const MediaTileWidgets& widgets) {
  if (!widgets.play_pause_label) return false;
  const char* current = lv_label_get_text(widgets.play_pause_label);
  String pause_icon = getMdiChar("pause");
  return current && pause_icon.length() && strcmp(current, pause_icon.c_str()) == 0;
}

static String media_friendly_name_from_entity_id(const String& entity_id) {
  int dot = entity_id.indexOf('.');
  String name = dot >= 0 ? entity_id.substring(dot + 1) : entity_id;
  name.replace('_', ' ');
  name.trim();
  bool capitalize_next = true;
  for (size_t i = 0; i < name.length(); ++i) {
    char c = name.charAt(i);
    if (capitalize_next && c >= 'a' && c <= 'z') {
      name.setCharAt(i, static_cast<char>(c - 32));
    }
    capitalize_next = (c == ' ');
  }
  return name;
}

static String media_popup_title_for_tile(const Tile& tile) {
  String title = tile.title;
  title.trim();
  if (!title.length() && tile.sensor_entity.length()) {
    title = haBridgeConfig.findSensorName(tile.sensor_entity);
  }
  if (!title.length() && tile.sensor_entity.length()) {
    title = media_friendly_name_from_entity_id(tile.sensor_entity);
  }
  return title;
}

static String media_popup_icon_for_tile(const Tile& tile) {
  String icon_name = normalizeMdiIconName(tile.icon_name);
  if (!icon_name.length() && tile.sensor_entity.length()) {
    icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
  }
  if (!icon_name.length()) icon_name = "television";
  return icon_name;
}

static String media_entity_for_grid_index(GridType grid_type, uint8_t grid_index) {
  (void)grid_type;
  if (grid_index >= TILES_PER_GRID) return "";
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  const Tile& tile = grid.tiles[grid_index];
  if (tile.type != TILE_MEDIA) return "";
  return tile.sensor_entity;
}

static void update_media_popup_from_widgets(GridType grid_type,
                                            uint8_t grid_index,
                                            MediaTileWidgets& widgets,
                                            const String& state_override = String()) {
  (void)grid_type;
  if (grid_index >= TILES_PER_GRID) return;
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  const Tile& tile = grid.tiles[grid_index];
  if (tile.type != TILE_MEDIA || !tile.sensor_entity.length()) return;

  MediaPopupInit init;
  init.entity_id = tile.sensor_entity;
  init.title = media_popup_title_for_tile(tile);
  init.icon_name = media_popup_icon_for_tile(tile);
  init.icon_char = media_label_text(widgets.icon_label);
  init.bg_color = tile.bg_color != 0 ? tile.bg_color : 0x2A2A2A;
  init.media_title = media_label_text(widgets.media_title_label);
  init.media_subtitle = media_label_text(widgets.media_subtitle_label);
  init.is_playing = state_override.length() ? media_is_playing_state(state_override)
                                            : media_widget_is_playing(widgets);
  init.has_volume = widgets.has_media_volume;
  init.volume_level = widgets.media_volume_level;
  init.is_muted = widgets.media_is_muted;
  const bool cover_visible = widgets.cover_ref &&
                             widgets.cover_ref->dsc &&
                             widgets.cover_clip &&
                             !lv_obj_has_flag(widgets.cover_clip, LV_OBJ_FLAG_HIDDEN);
  if (cover_visible) {
    init.cover_dsc = widgets.cover_ref->dsc;
    init.cover_hash = widgets.cover_ref->url_hash;
  }
  update_media_popup(init);
}

struct MediaCoverDecodeCtx {
  const uint8_t* data = nullptr;
  size_t len = 0;
  size_t pos = 0;
};

static size_t media_cover_jpeg_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  MediaCoverDecodeCtx* ctx = static_cast<MediaCoverDecodeCtx*>(jd->device);
  if (!ctx || !ctx->data || ctx->pos >= ctx->len) return 0;
  size_t remain = ctx->len - ctx->pos;
  size_t take = (ndata < remain) ? ndata : remain;
  if (buff && take) {
    memcpy(buff, ctx->data + ctx->pos, take);
  }
  ctx->pos += take;
  return take;
}

static void* alloc_media_cover_memory(size_t bytes, bool prefer_psram = false) {
  if (!bytes) return nullptr;
  void* data = nullptr;
  if (prefer_psram) {
    data = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!data) {
    data = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  return data;
}

static void free_media_cover_dsc(lv_image_dsc_t*& dsc) {
  if (!dsc) return;
  if (dsc->data) {
    free(const_cast<uint8_t*>(dsc->data));
  }
  free(dsc);
  dsc = nullptr;
}

static bool is_media_cover_jpeg(const uint8_t* data, size_t len) {
  return data && len >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

static bool is_media_cover_png(const uint8_t* data, size_t len) {
  return data && len >= 8 &&
         data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
         data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A;
}

static bool read_media_cover_png_size(const uint8_t* data, size_t len, uint16_t& w, uint16_t& h) {
  if (!is_media_cover_png(data, len) || len < 24) return false;
  uint32_t png_w = (static_cast<uint32_t>(data[16]) << 24) |
                   (static_cast<uint32_t>(data[17]) << 16) |
                   (static_cast<uint32_t>(data[18]) << 8) |
                   static_cast<uint32_t>(data[19]);
  uint32_t png_h = (static_cast<uint32_t>(data[20]) << 24) |
                   (static_cast<uint32_t>(data[21]) << 16) |
                   (static_cast<uint32_t>(data[22]) << 8) |
                   static_cast<uint32_t>(data[23]);
  if (png_w == 0 || png_h == 0 || png_w > UINT16_MAX || png_h > UINT16_MAX) return false;
  w = static_cast<uint16_t>(png_w);
  h = static_cast<uint16_t>(png_h);
  return true;
}

static bool read_media_cover_jpeg_size(const uint8_t* data, size_t len, uint16_t& w, uint16_t& h) {
  if (!is_media_cover_jpeg(data, len) || len < 32) return false;

  uint8_t* work = static_cast<uint8_t*>(alloc_media_cover_memory(4096));
  if (!work) return false;

  MediaCoverDecodeCtx ctx{};
  ctx.data = data;
  ctx.len = len;

  JDEC jd;
  JRESULT rc = jd_prepare(&jd, media_cover_jpeg_input, work, 4096, &ctx);
  free(work);
  if (rc != JDR_OK || jd.width == 0 || jd.height == 0 ||
      jd.width > UINT16_MAX || jd.height > UINT16_MAX) {
    Serial.printf("[MediaCover] JPEG info fehlgeschlagen: %d\n", static_cast<int>(rc));
    return false;
  }

  w = static_cast<uint16_t>(jd.width);
  h = static_cast<uint16_t>(jd.height);
  return true;
}

static lv_image_dsc_t* make_media_cover_raw_dsc(const uint8_t* data, size_t len) {
  if (!data || len < 32) return nullptr;

  uint16_t img_w = 0;
  uint16_t img_h = 0;
  const bool is_png = read_media_cover_png_size(data, len, img_w, img_h);
  const bool is_jpeg = !is_png && read_media_cover_jpeg_size(data, len, img_w, img_h);
  if (!is_png && !is_jpeg) return nullptr;

  uint8_t* copy = static_cast<uint8_t*>(alloc_media_cover_memory(len, true));
  if (!copy) return nullptr;
  memcpy(copy, data, len);

  lv_image_dsc_t* dsc = static_cast<lv_image_dsc_t*>(alloc_media_cover_memory(sizeof(lv_image_dsc_t)));
  if (!dsc) {
    free(copy);
    return nullptr;
  }
  memset(dsc, 0, sizeof(*dsc));
  dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc->header.cf = is_png ? LV_COLOR_FORMAT_RAW_ALPHA : LV_COLOR_FORMAT_RAW;
  dsc->header.w = img_w;
  dsc->header.h = img_h;
  dsc->header.stride = 0;
  dsc->data_size = len;
  dsc->data = copy;
  return dsc;
}

static lv_image_dsc_t* make_media_cover_dsc_from_bytes(const uint8_t* data, size_t len) {
  if (!data || len < 32) return nullptr;
  if (is_media_cover_jpeg(data, len)) {
    lv_image_dsc_t* dsc = make_media_cover_raw_dsc(data, len);
    if (dsc) {
      Serial.printf("[MediaCover] MQTT JPEG an LVGL-Decoder uebergeben: %u Bytes\n",
                    static_cast<unsigned>(len));
    }
    return dsc;
  }
  if (is_media_cover_png(data, len)) {
    lv_image_dsc_t* dsc = make_media_cover_raw_dsc(data, len);
    if (dsc) {
      Serial.printf("[MediaCover] MQTT PNG an LVGL-Decoder uebergeben: %ux%u\n",
                    static_cast<unsigned>(dsc->header.w),
                    static_cast<unsigned>(dsc->header.h));
    }
    return dsc;
  }
  Serial.printf("[MediaCover] MQTT Bildformat unbekannt: %02X %02X %02X %02X\n",
                data[0], data[1], data[2], data[3]);
  return nullptr;
}

static lv_image_dsc_t* make_media_cover_dsc_from_base64(const String& encoded) {
  if (!encoded.length()) return nullptr;
  const size_t cap = base64_decode_expected_len(encoded.length()) + 4;
  if (cap < 32 || cap > (96U * 1024U)) return nullptr;

  uint8_t* decoded = static_cast<uint8_t*>(alloc_media_cover_memory(cap, true));
  if (!decoded) return nullptr;

  base64_decodestate state;
  base64_init_decodestate(&state);
  const int decoded_len = base64_decode_block(
      encoded.c_str(),
      static_cast<int>(encoded.length()),
      reinterpret_cast<char*>(decoded),
      &state);

  if (decoded_len < 32) {
    free(decoded);
    return nullptr;
  }

  Serial.printf("[MediaCover] MQTT Cover Base64: encoded=%u decoded=%d bytes magic=%02X %02X %02X %02X\n",
                static_cast<unsigned>(encoded.length()),
                decoded_len,
                decoded[0], decoded[1], decoded[2], decoded[3]);

  lv_image_dsc_t* dsc = make_media_cover_dsc_from_bytes(decoded, static_cast<size_t>(decoded_len));
  free(decoded);
  return dsc;
}

static uint8_t* alloc_media_cover_download_buffer(size_t bytes) {
  return static_cast<uint8_t*>(alloc_media_cover_memory(bytes, true));
}

static bool media_cover_network_budget_ok() {
  constexpr size_t kMinInternalFree = 96U * 1024U;
  constexpr size_t kMinInternalLargest = 24U * 1024U;
  const size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largest_internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return free_internal >= kMinInternalFree && largest_internal >= kMinInternalLargest;
}

static bool media_cover_download_allowed(const String& url) {
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (url.startsWith("https://")) return false;
#endif
  return true;
}

static void log_media_cover_download_blocked() {
  static uint32_t last_log_ms = 0;
  const uint32_t now = millis();
  if (last_log_ms != 0 && static_cast<uint32_t>(now - last_log_ms) < 30000) return;
  last_log_ms = now;
  Serial.println("[MediaCover] HTTPS Cover Download auf ESP32-P4 uebersprungen (WiFi-SDIO Stabilitaet)");
}

static lv_image_dsc_t* download_media_cover_jpeg(const String& url, bool* deferred) {
  if (deferred) *deferred = false;
  if (!url.startsWith("http://") && !url.startsWith("https://")) return nullptr;
  if (!media_cover_download_allowed(url)) {
    log_media_cover_download_blocked();
    return nullptr;
  }
  if (webAdminRecentlyActive(3500) || !media_cover_network_budget_ok()) {
    if (deferred) *deferred = true;
    return nullptr;
  }

  constexpr size_t kMaxDownloadBytes = 96U * 1024U;
  constexpr size_t kReadChunkBytes = 512U;
  Serial.printf("[MediaCover] lade: %s\n", url.c_str());
  HTTPClient http;
  http.setTimeout(1200);
  http.setReuse(false);
  http.useHTTP10(true);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  WiFiClient plain_client;
  WiFiClientSecure secure_client;
  bool ok_begin = false;
  if (url.startsWith("https://")) {
    secure_client.setInsecure();
    ok_begin = http.begin(secure_client, url);
  } else {
    ok_begin = http.begin(plain_client, url);
  }
  if (!ok_begin) return nullptr;
  http.addHeader("Connection", "close");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[MediaCover] HTTP %d fuer Cover\n", code);
    http.end();
    plain_client.stop();
    secure_client.stop();
    return nullptr;
  }

  int content_len = http.getSize();
  if (content_len > 0 && static_cast<size_t>(content_len) > kMaxDownloadBytes) {
    Serial.printf("[MediaCover] Cover zu gross: %d Bytes\n", content_len);
    http.end();
    plain_client.stop();
    secure_client.stop();
    return nullptr;
  }

  size_t buffer_cap = (content_len > 0) ? static_cast<size_t>(content_len) : kMaxDownloadBytes;
  if (buffer_cap < 1024) buffer_cap = 1024;
  uint8_t* data = alloc_media_cover_download_buffer(buffer_cap);
  if (!data) {
    Serial.println("[MediaCover] Download-Puffer fehlt");
    http.end();
    plain_client.stop();
    secure_client.stop();
    return nullptr;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t total = 0;
  uint32_t deadline = millis() + 2200;
  while (http.connected() && millis() < deadline) {
    if (webAdminRecentlyActive(1000)) {
      if (deferred) *deferred = true;
      free(data);
      http.end();
      plain_client.stop();
      secure_client.stop();
      delay(20);
      return nullptr;
    }
    int available = stream ? stream->available() : 0;
    if (available > 0) {
      size_t room = buffer_cap - total;
      if (!room) {
        Serial.println("[MediaCover] Cover Download-Limit erreicht");
        free(data);
        http.end();
        plain_client.stop();
        secure_client.stop();
        return nullptr;
      }
      size_t to_read = static_cast<size_t>(available);
      if (to_read > room) to_read = room;
      if (to_read > kReadChunkBytes) to_read = kReadChunkBytes;
      int read_bytes = stream->readBytes(data + total, to_read);
      if (read_bytes > 0) {
        total += static_cast<size_t>(read_bytes);
        deadline = millis() + 900;
        delay(2);
        yield();
      }
    } else {
      if (content_len >= 0 && total >= static_cast<size_t>(content_len)) break;
      delay(5);
      yield();
    }
  }
  http.end();
  plain_client.stop();
  secure_client.stop();
  delay(20);

  if (total < 32) {
    Serial.printf("[MediaCover] zu wenig Daten: %u Bytes\n", static_cast<unsigned>(total));
    free(data);
    return nullptr;
  }

  lv_image_dsc_t* dsc = nullptr;
  if (is_media_cover_jpeg(data, total)) {
    dsc = make_media_cover_raw_dsc(data, total);
  } else if (is_media_cover_png(data, total)) {
    dsc = make_media_cover_raw_dsc(data, total);
    if (dsc) {
      Serial.printf("[MediaCover] PNG an LVGL-Decoder uebergeben: %ux%u\n",
                    static_cast<unsigned>(dsc->header.w),
                    static_cast<unsigned>(dsc->header.h));
    }
  } else {
    Serial.printf("[MediaCover] unbekanntes Bildformat: %02X %02X %02X %02X\n",
                  data[0], data[1], data[2], data[3]);
  }
  if (dsc) {
    if (dsc->header.w && dsc->header.h) {
      Serial.printf("[MediaCover] OK: %ux%u, %u Bytes\n",
                    static_cast<unsigned>(dsc->header.w),
                    static_cast<unsigned>(dsc->header.h),
                    static_cast<unsigned>(total));
    } else {
      Serial.printf("[MediaCover] OK: RAW, %u Bytes\n", static_cast<unsigned>(total));
    }
  }
  free(data);
  return dsc;
}

static bool media_cover_has_hidden_ancestor(lv_obj_t* obj) {
  obj = obj ? lv_obj_get_parent(obj) : nullptr;
  while (obj) {
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return true;
    obj = lv_obj_get_parent(obj);
  }
  return false;
}

static void set_media_cover_text_layout(MediaTileWidgets& widgets, bool cover_visible) {
  const bool has_subtitle = widgets.media_subtitle_label &&
                            !lv_obj_has_flag(widgets.media_subtitle_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t* text_parent = widgets.media_title_label ? lv_obj_get_parent(widgets.media_title_label) : nullptr;
  if (text_parent) {
    lv_obj_update_layout(text_parent);
  }
  const lv_coord_t parent_w = text_parent ? lv_obj_get_width(text_parent) : 720;
  const lv_coord_t parent_h = text_parent ? lv_obj_get_height(text_parent) : 240;
  const bool large_cover = widgets.cover_clip && lv_obj_get_width(widgets.cover_clip) >= 120;
  const lv_coord_t text_x = cover_visible ? (large_cover ? 138 : 112) : 20;
  const lv_coord_t text_right_margin = cover_visible ? 42 : 28;
  lv_coord_t text_w = parent_w - text_x - text_right_margin;
  if (text_w < 120) {
    text_w = parent_w > 40 ? parent_w - 40 : parent_w;
  }

  lv_coord_t title_h = 32;
  lv_coord_t subtitle_h = 0;
  if (widgets.media_title_label) {
    lv_obj_set_width(widgets.media_title_label, text_w);
    lv_obj_set_style_text_align(widgets.media_title_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_update_layout(widgets.media_title_label);
    title_h = lv_obj_get_height(widgets.media_title_label);
    if (title_h < 1) title_h = 32;
  }
  if (widgets.media_subtitle_label) {
    lv_obj_set_width(widgets.media_subtitle_label, text_w);
    lv_obj_set_style_text_align(widgets.media_subtitle_label, LV_TEXT_ALIGN_LEFT, 0);
    if (has_subtitle) {
      lv_obj_update_layout(widgets.media_subtitle_label);
      subtitle_h = lv_obj_get_height(widgets.media_subtitle_label);
      if (subtitle_h < 1) subtitle_h = 20;
    }
  }

  lv_coord_t center_y = parent_h / 2;
  if (widgets.cover_clip) {
    lv_obj_update_layout(widgets.cover_clip);
    center_y = lv_obj_get_y(widgets.cover_clip) + (lv_obj_get_height(widgets.cover_clip) / 2);
  }

  const lv_coord_t subtitle_gap = has_subtitle ? 6 : 0;
  const lv_coord_t block_h = title_h + subtitle_gap + subtitle_h;
  lv_coord_t title_y = center_y - (block_h / 2);
  const lv_coord_t min_title_y = cover_visible ? (large_cover ? 58 : 42) : 76;
  const lv_coord_t max_title_y = parent_h - block_h - 68;
  if (title_y < min_title_y) title_y = min_title_y;
  if (max_title_y > min_title_y && title_y > max_title_y) title_y = max_title_y;

  if (widgets.media_title_label) {
    lv_obj_align(widgets.media_title_label, LV_ALIGN_TOP_LEFT, text_x, title_y);
  }
  if (widgets.media_subtitle_label) {
    if (widgets.media_title_label && has_subtitle) {
      lv_obj_align_to(widgets.media_subtitle_label,
                      widgets.media_title_label,
                      LV_ALIGN_OUT_BOTTOM_LEFT,
                      0,
                      subtitle_gap);
    } else {
      lv_obj_align(widgets.media_subtitle_label, LV_ALIGN_TOP_LEFT, text_x, title_y + title_h);
    }
  }
}

static bool set_media_cover_visible(MediaTileWidgets& widgets, bool visible) {
  bool changed = false;
  if (widgets.cover_clip) {
    const bool hidden = lv_obj_has_flag(widgets.cover_clip, LV_OBJ_FLAG_HIDDEN);
    if (visible && hidden) {
      lv_obj_clear_flag(widgets.cover_clip, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    } else if (!visible && !hidden) {
      lv_obj_add_flag(widgets.cover_clip, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    }
  }
  if (widgets.cover_image) {
    const bool hidden = lv_obj_has_flag(widgets.cover_image, LV_OBJ_FLAG_HIDDEN);
    if (visible && hidden) {
      lv_obj_clear_flag(widgets.cover_image, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    } else if (!visible && !hidden) {
      lv_obj_add_flag(widgets.cover_image, LV_OBJ_FLAG_HIDDEN);
      changed = true;
    }
  }
  return changed;
}

struct MediaCoverRequest {
  GridType grid_type;
  uint8_t grid_index = 0;
  uint32_t url_hash = 0;
  char url[512] = {};
};

struct MediaCoverResult {
  GridType grid_type;
  uint8_t grid_index = 0;
  uint32_t url_hash = 0;
  char url[512] = {};
  lv_image_dsc_t* dsc = nullptr;
  bool ok = false;
  bool deferred = false;
};

static QueueHandle_t g_media_cover_request_queue = nullptr;
static QueueHandle_t g_media_cover_result_queue = nullptr;
static TaskHandle_t g_media_cover_task = nullptr;
static uint32_t g_media_cover_request_full_count = 0;
static uint32_t g_media_cover_result_full_count = 0;
static uint32_t g_media_cover_last_download_ms = 0;
static constexpr bool kMediaCoverDownloadsEnabled = true;
static constexpr uint32_t kMediaCoverRetryCooldownMs = 10000;

static void media_cover_worker_task(void*) {
  for (;;) {
    MediaCoverRequest req{};
    if (xQueueReceive(g_media_cover_request_queue, &req, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    MediaCoverResult result{};
    result.grid_type = req.grid_type;
    result.grid_index = req.grid_index;
    result.url_hash = req.url_hash;
    strncpy(result.url, req.url, sizeof(result.url) - 1);
    result.url[sizeof(result.url) - 1] = '\0';
    while (webAdminRecentlyActive(3500)) {
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    const uint32_t now = millis();
    if (g_media_cover_last_download_ms != 0 &&
        static_cast<uint32_t>(now - g_media_cover_last_download_ms) < 1500) {
      vTaskDelay(pdMS_TO_TICKS(1500 - static_cast<uint32_t>(now - g_media_cover_last_download_ms)));
    }
    bool deferred = false;
    result.dsc = download_media_cover_jpeg(String(req.url), &deferred);
    result.ok = result.dsc != nullptr;
    result.deferred = deferred;
    g_media_cover_last_download_ms = millis();

    if (!g_media_cover_result_queue ||
        xQueueSend(g_media_cover_result_queue, &result, 0) != pdTRUE) {
      if ((g_media_cover_result_full_count++ % 5) == 0) {
        Serial.println("[MediaCover] Ergebnis-Queue voll, Cover verworfen");
      }
      free_media_cover_dsc(result.dsc);
    }
  }
}

static bool ensure_media_cover_worker() {
  if (!g_media_cover_request_queue) {
    g_media_cover_request_queue = xQueueCreate(1, sizeof(MediaCoverRequest));
  }
  if (!g_media_cover_result_queue) {
    g_media_cover_result_queue = xQueueCreate(1, sizeof(MediaCoverResult));
  }
  if (!g_media_cover_request_queue || !g_media_cover_result_queue) {
    return false;
  }
  if (!g_media_cover_task) {
    BaseType_t ok = xTaskCreate(
        media_cover_worker_task,
        "media_cover",
        16384,
        nullptr,
        1,
        &g_media_cover_task);
    if (ok != pdPASS) {
      g_media_cover_task = nullptr;
      return false;
    }
  }
  return true;
}

static void queue_media_cover_request(GridType grid_type,
                                      uint8_t grid_index,
                                      MediaCoverRef* ref,
                                      const String& url,
                                      uint32_t hash) {
  if (!ref || !url.length()) return;
  if (ref->requested_url_hash == hash) return;
  if (!ensure_media_cover_worker()) {
    if ((g_media_cover_request_full_count++ % 5) == 0) {
      Serial.println("[MediaCover] Worker konnte nicht gestartet werden");
    }
    return;
  }

  MediaCoverRequest req{};
  req.grid_type = grid_type;
  req.grid_index = grid_index;
  req.url_hash = hash;
  strncpy(req.url, url.c_str(), sizeof(req.url) - 1);
  req.url[sizeof(req.url) - 1] = '\0';

  if (xQueueSend(g_media_cover_request_queue, &req, 0) != pdTRUE) {
    if ((g_media_cover_request_full_count++ % 5) == 0) {
      Serial.println("[MediaCover] Request-Queue voll, Cover wird spaeter erneut versucht");
    }
    return;
  }

  ref->requested_url_hash = hash;
}

static void process_media_cover_results() {
  if (!g_media_cover_result_queue) return;

  MediaCoverResult result{};
  while (xQueueReceive(g_media_cover_result_queue, &result, 0) == pdTRUE) {
    MediaTileWidgets* target = tile_renderer_get_media_widgets(result.grid_type);
    if (!target || result.grid_index >= TILES_PER_GRID) {
      free_media_cover_dsc(result.dsc);
      continue;
    }

    MediaTileWidgets& widgets = target[result.grid_index];
    MediaCoverRef* ref = widgets.cover_ref;
    if (!widgets.cover_image || !ref || ref->requested_url_hash != result.url_hash) {
      free_media_cover_dsc(result.dsc);
      continue;
    }

    ref->requested_url_hash = 0;
    if (result.deferred) {
      if (result.url[0]) {
        queue_media_cover_request(result.grid_type, result.grid_index, ref, String(result.url), result.url_hash);
      }
      free_media_cover_dsc(result.dsc);
      continue;
    }
    if (!result.ok || !result.dsc) {
      ref->failed_url_hash = result.url_hash;
      ref->failed_at_ms = millis();
      if (ref->url_hash != result.url_hash) {
        const bool cover_visibility_changed = set_media_cover_visible(widgets, false);
        if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
        if (cover_visibility_changed) {
          set_media_cover_text_layout(widgets, false);
        }
        String entity_id = media_entity_for_grid_index(result.grid_type, result.grid_index);
        if (entity_id.length()) {
          update_media_popup_cover(entity_id.c_str(), nullptr, 0);
        }
      }
      free_media_cover_dsc(result.dsc);
      continue;
    }

    lv_image_dsc_t* old = ref->dsc;
    lv_image_set_src(widgets.cover_image, result.dsc);
    const bool cover_visibility_changed = set_media_cover_visible(widgets, true);
    if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    if (cover_visibility_changed) {
      set_media_cover_text_layout(widgets, true);
    }

    ref->dsc = result.dsc;
    ref->url_hash = result.url_hash;
    ref->failed_url_hash = 0;
    ref->failed_at_ms = 0;
    {
      String entity_id = media_entity_for_grid_index(result.grid_type, result.grid_index);
      if (entity_id.length()) {
        update_media_popup_cover(entity_id.c_str(), ref->dsc, ref->url_hash);
      }
    }
    result.dsc = nullptr;
    free_media_cover_dsc(old);
  }
}

static void update_media_cover(GridType grid_type,
                               uint8_t grid_index,
                               MediaTileWidgets& widgets,
                               const String& raw_url) {
  String url = raw_url;
  url.trim();
  if (!widgets.cover_image || !widgets.cover_ref) return;
  MediaCoverRef* ref = widgets.cover_ref;

  if (!url.length()) {
    ref->source_url = "";
    ref->requested_url_hash = 0;
    ref->failed_url_hash = 0;
    ref->failed_at_ms = 0;
    const bool cover_visibility_changed = set_media_cover_visible(widgets, false);
    if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    if (cover_visibility_changed) {
      set_media_cover_text_layout(widgets, false);
    }
    return;
  }

  uint32_t hash = fnv1a_hash(url.c_str());
  if (ref->source_url != url) {
    ref->source_url = url;
    ref->failed_url_hash = 0;
    ref->failed_at_ms = 0;
  }

  if (!kMediaCoverDownloadsEnabled) {
    ref->requested_url_hash = 0;
    const bool cover_visibility_changed = set_media_cover_visible(widgets, false);
    if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    if (cover_visibility_changed) {
      set_media_cover_text_layout(widgets, false);
    }
    return;
  }

  if (ref->url_hash == hash && ref->dsc) {
    ref->requested_url_hash = 0;
    const bool cover_visibility_changed = set_media_cover_visible(widgets, true);
    if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    if (cover_visibility_changed) {
      set_media_cover_text_layout(widgets, true);
    }
    return;
  }
  if (!media_cover_download_allowed(url)) {
    ref->requested_url_hash = 0;
    ref->failed_url_hash = 0;
    ref->failed_at_ms = 0;
    const bool cover_visibility_changed = set_media_cover_visible(widgets, false);
    if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    if (cover_visibility_changed) {
      set_media_cover_text_layout(widgets, false);
    }
    log_media_cover_download_blocked();
    return;
  }
  if (ref->failed_url_hash == hash) {
    const uint32_t failed_age = static_cast<uint32_t>(millis() - ref->failed_at_ms);
    if (ref->failed_at_ms != 0 && failed_age < kMediaCoverRetryCooldownMs) return;
    ref->failed_url_hash = 0;
    ref->failed_at_ms = 0;
  }
  if (media_cover_has_hidden_ancestor(widgets.cover_clip ? widgets.cover_clip : widgets.cover_image)) return;

  queue_media_cover_request(grid_type, grid_index, ref, url, hash);
}

static bool update_media_cover_from_base64(MediaTileWidgets& widgets, const String& raw_data) {
  String encoded = raw_data;
  encoded.trim();
  if (!widgets.cover_image || !widgets.cover_ref || !encoded.length()) return false;

  MediaCoverRef* ref = widgets.cover_ref;
  const uint32_t hash = fnv1a_hash(encoded.c_str());
  if (ref->url_hash == hash && ref->dsc) {
    ref->requested_url_hash = 0;
    const bool cover_visibility_changed = set_media_cover_visible(widgets, true);
    if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
    if (cover_visibility_changed) {
      set_media_cover_text_layout(widgets, true);
    }
    return true;
  }

  lv_image_dsc_t* dsc = make_media_cover_dsc_from_base64(encoded);
  if (!dsc) {
    Serial.println("[MediaCover] MQTT Cover konnte nicht dekodiert werden");
    return false;
  }

  lv_image_dsc_t* old = ref->dsc;
  lv_image_set_src(widgets.cover_image, dsc);
  const bool cover_visibility_changed = set_media_cover_visible(widgets, true);
  if (widgets.icon_label) lv_obj_clear_flag(widgets.icon_label, LV_OBJ_FLAG_HIDDEN);
  if (cover_visibility_changed) {
    set_media_cover_text_layout(widgets, true);
  }

  ref->dsc = dsc;
  ref->source_url = "mqtt";
  ref->url_hash = hash;
  ref->requested_url_hash = 0;
  ref->failed_url_hash = 0;
  ref->failed_at_ms = 0;
  free_media_cover_dsc(old);
  Serial.println("[MediaCover] MQTT Cover geladen");
  return true;
}

static void retry_failed_media_covers_for_grid(GridType grid_type, MediaTileWidgets* target) {
  if (!target) return;
  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    MediaTileWidgets& widgets = target[i];
    MediaCoverRef* ref = widgets.cover_ref;
    if (!ref || !widgets.cover_image || ref->requested_url_hash != 0) continue;
    if (ref->failed_url_hash == 0 || ref->failed_at_ms == 0 || !ref->source_url.length()) continue;

    const uint32_t failed_age = static_cast<uint32_t>(millis() - ref->failed_at_ms);
    if (failed_age < kMediaCoverRetryCooldownMs) continue;
    if (media_cover_has_hidden_ancestor(widgets.cover_clip ? widgets.cover_clip : widgets.cover_image)) continue;
    if (!media_cover_download_allowed(ref->source_url)) {
      ref->failed_url_hash = 0;
      ref->failed_at_ms = 0;
      log_media_cover_download_blocked();
      continue;
    }

    const uint32_t hash = fnv1a_hash(ref->source_url.c_str());
    ref->failed_url_hash = 0;
    ref->failed_at_ms = 0;
    queue_media_cover_request(grid_type, i, ref, ref->source_url, hash);
  }
}

static void process_pending_media_cover_retries() {
  retry_failed_media_covers_for_grid(GridType::TAB0, g_tab0_media);
  retry_failed_media_covers_for_grid(GridType::TAB1, g_tab1_media);
  retry_failed_media_covers_for_grid(GridType::TAB2, g_tab2_media);
}

void update_media_tile_state(GridType grid_type, uint8_t grid_index, const char* payload) {
  if (grid_index >= TILES_PER_GRID || !payload) return;
  MediaTileWidgets* target = g_tab0_media;
  if (grid_type == GridType::TAB1) target = g_tab1_media;
  else if (grid_type == GridType::TAB2) target = g_tab2_media;

  MediaTileWidgets& widgets = target[grid_index];
  if (!widgets.icon_label && !widgets.media_title_label &&
      !widgets.media_subtitle_label && !widgets.state_label) {
    return;
  }

  const char* payload_start = payload;
  while (*payload_start == ' ' || *payload_start == '\t' ||
         *payload_start == '\r' || *payload_start == '\n') {
    ++payload_start;
  }
  if (!*payload_start) return;

  const uint32_t payload_hash = fnv1a_hash(payload_start);
  if (widgets.last_payload_hash == payload_hash) return;
  widgets.last_payload_hash = payload_hash;

  String state;
  String title;
  String artist;
  String album;
  String app;
  String source;
  String channel;
  float volume_level = 0.0f;
  bool has_volume_level = false;
  bool is_muted = false;
  bool has_muted = false;

  const bool is_json_payload = *payload_start == '{';
  if (is_json_payload) {
    extract_json_string_field_cstr(payload_start, "state", state);
    extract_json_string_field_cstr(payload_start, "media_title", title);
    extract_json_string_field_cstr(payload_start, "media_artist", artist);
    extract_json_string_field_cstr(payload_start, "media_album_name", album);
    extract_json_string_field_cstr(payload_start, "app_name", app);
    extract_json_string_field_cstr(payload_start, "source", source);
    extract_json_string_field_cstr(payload_start, "media_channel", channel);
    has_volume_level = extract_json_number_field_cstr(payload_start, "volume_level", volume_level);
    if (!has_volume_level) {
      has_volume_level = extract_json_number_field_cstr(payload_start, "volume", volume_level);
    }
    has_muted = extract_json_bool_field_cstr(payload_start, "is_volume_muted", is_muted);
    if (!has_muted) {
      has_muted = extract_json_bool_field_cstr(payload_start, "muted", is_muted);
    }
  } else {
    state = payload_start;
    state.trim();
  }

  decode_basic_json_escapes(state);
  decode_basic_json_escapes(title);
  decode_basic_json_escapes(artist);
  decode_basic_json_escapes(album);
  decode_basic_json_escapes(app);
  decode_basic_json_escapes(source);
  decode_basic_json_escapes(channel);
  sanitize_media_display_text(title);
  sanitize_media_display_text(artist);
  sanitize_media_display_text(album);
  sanitize_media_display_text(app);
  sanitize_media_display_text(source);
  sanitize_media_display_text(channel);

  if (has_volume_level) {
    if (volume_level > 1.0f && volume_level <= 100.0f) {
      volume_level /= 100.0f;
    }
    if (volume_level < 0.0f) volume_level = 0.0f;
    if (volume_level > 1.0f) volume_level = 1.0f;
  }
  widgets.has_media_volume = has_volume_level;
  widgets.media_volume_level = has_volume_level ? volume_level : 0.0f;
  if (has_muted) widgets.media_is_muted = is_muted;

  String main_text = media_first_non_empty(title, channel);
  const bool has_real_media_title = main_text.length() > 0;
  if (!main_text.length()) {
    main_text = media_empty_title_label(state);
  }
  String subtitle = media_first_non_empty(artist, album, app, source);
  if (media_text_same(subtitle, main_text)) {
    subtitle = "";
  }
  String media_text_key = main_text;
  media_text_key += '\x1f';
  media_text_key += subtitle;
  const uint32_t media_text_hash = fnv1a_hash(media_text_key.c_str());
  const bool media_text_changed = widgets.last_media_text_hash != media_text_hash;
  widgets.last_media_text_hash = media_text_hash;

  MediaCoverRef* cover_ref = widgets.cover_ref;
  const bool cover_ready_or_pending = cover_ref &&
                                      (cover_ref->dsc || cover_ref->requested_url_hash != 0);
  const bool should_update_cover = is_json_payload &&
                                   (media_text_changed || !cover_ready_or_pending);

  if (widgets.play_pause_label) {
    String icon = getMdiChar(media_icon_for_state(state));
    if (icon.length()) {
      set_label_text_if_changed(widgets.play_pause_label, icon);
      lv_obj_clear_flag(widgets.play_pause_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  bool text_layout_dirty = false;
  bool title_text_changed = false;
  bool subtitle_text_changed = false;
  if (widgets.media_title_label) {
    text_layout_dirty |= set_label_long_mode_if_changed(widgets.media_title_label,
                                                        has_real_media_title ? LV_LABEL_LONG_SCROLL : LV_LABEL_LONG_DOT);
    title_text_changed = set_label_text_if_changed(widgets.media_title_label, main_text);
    text_layout_dirty |= title_text_changed;
  }
  if (widgets.media_subtitle_label) {
    const bool was_hidden = lv_obj_has_flag(widgets.media_subtitle_label, LV_OBJ_FLAG_HIDDEN);
    if (subtitle.length()) {
      text_layout_dirty |= set_label_long_mode_if_changed(widgets.media_subtitle_label, LV_LABEL_LONG_SCROLL);
      subtitle_text_changed = set_label_text_if_changed(widgets.media_subtitle_label, subtitle);
      text_layout_dirty |= subtitle_text_changed;
      lv_obj_clear_flag(widgets.media_subtitle_label, LV_OBJ_FLAG_HIDDEN);
      if (was_hidden) text_layout_dirty = true;
    } else {
      text_layout_dirty |= set_label_text_if_changed(widgets.media_subtitle_label, "");
      lv_obj_add_flag(widgets.media_subtitle_label, LV_OBJ_FLAG_HIDDEN);
      if (!was_hidden) text_layout_dirty = true;
    }
    if (text_layout_dirty) {
      set_media_cover_text_layout(widgets, widgets.cover_clip && !lv_obj_has_flag(widgets.cover_clip, LV_OBJ_FLAG_HIDDEN));
    }
  }
  if (title_text_changed) restart_visible_media_text_scroll(widgets.media_title_label);
  if (subtitle_text_changed) restart_visible_media_text_scroll(widgets.media_subtitle_label);
  if (widgets.state_label) {
    set_label_text_if_changed(widgets.state_label, "");
    lv_obj_add_flag(widgets.state_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (should_update_cover) {
    String cover_url;
    String cover_data;
    if (!extract_json_string_field_cstr(payload_start, "entity_picture", cover_url)) {
      extract_json_string_field_cstr(payload_start, "media_image_url", cover_url);
    }
    extract_json_string_field_cstr(payload_start, "entity_picture_data", cover_data);
    decode_basic_json_escapes(cover_url);
    decode_basic_json_escapes(cover_data);

    if (cover_data.length()) {
      update_media_cover_from_base64(widgets, cover_data);
    } else {
      update_media_cover(grid_type, grid_index, widgets, cover_url);
    }
    if (!cover_url.length() && !cover_data.length()) {
      Serial.println("[MediaCover] keine Cover-URL im Media-Payload");
    }
  }

  update_media_popup_from_widgets(grid_type, grid_index, widgets, state);
}

void queue_media_tile_update(GridType grid_type, uint8_t grid_index, const char* payload) {
  if (grid_index >= TILES_PER_GRID || !payload) return;

  uint8_t idx = g_media_tail;
  while (idx != g_media_head) {
    MediaUpdate& pending = g_media_queue[idx];
    if (pending.valid &&
        pending.grid_type == grid_type &&
        pending.grid_index == grid_index) {
      if (pending.payload.equals(payload)) return;
      pending.payload = String(payload);
      return;
    }
    idx = (idx + 1) % MEDIA_QUEUE_SIZE;
  }

  uint8_t next_head = (g_media_head + 1) % MEDIA_QUEUE_SIZE;
  if (next_head == g_media_tail) {
    if ((g_media_overflow_count++ % 10) == 0) {
      Serial.println("[Queue] VOLL! Aeltestes Media-Update wird ueberschrieben");
    }
    g_media_tail = (g_media_tail + 1) % MEDIA_QUEUE_SIZE;
  }

  g_media_queue[g_media_head].grid_type = grid_type;
  g_media_queue[g_media_head].grid_index = grid_index;
  g_media_queue[g_media_head].payload = String(payload);
  g_media_queue[g_media_head].valid = true;
  g_media_head = next_head;
}

void process_media_update_queue(uint8_t max_updates) {
  uint8_t processed = 0;
  while (g_media_tail != g_media_head && (max_updates == 0 || processed < max_updates)) {
    MediaUpdate& upd = g_media_queue[g_media_tail];
    if (upd.valid) {
      update_media_tile_state(upd.grid_type, upd.grid_index, upd.payload.c_str());
      upd.valid = false;
      ++processed;
    }
    g_media_tail = (g_media_tail + 1) % MEDIA_QUEUE_SIZE;
  }
  process_media_cover_results();
  process_pending_media_cover_retries();
}

/* === Thread-Safe Queue fuer Tile Graph History (MQTT -> Main Loop) === */
struct TileGraphHistoryUpdate {
  String entity_id;
  String payload;
  bool valid = false;
};

static TileGraphHistoryUpdate g_tile_graph_history;

static bool extract_numeric_for_chart(const String& text, float& out) {
  String normalized = text;
  normalized.trim();
  if (!normalized.length()) return false;
  normalized.replace(",", ".");
  char* end = nullptr;
  float f = strtof(normalized.c_str(), &end);
  if (!end || end == normalized.c_str()) return false;
  if (isnan(f) || isinf(f)) return false;
  out = f;
  return true;
}

static void apply_tile_graph_history(const char* target_entity, const char* payload) {
  if (!payload || !*payload) return;

  // Parse JSON to get entity_id and values
  String json = payload;
  String entity_id;
  if (!extract_json_string_field(json, "entity_id", entity_id)) return;

  // Find matching tile with graph in active grid
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  SensorTileWidgets* sensors = g_tab0_sensors;  // Active grid is always TAB0

  for (uint8_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (tile.type != TILE_SENSOR) continue;
    if (tile.sensor_display_mode != 2) continue;  // Only graph mode
    if (!tile.sensor_entity.equalsIgnoreCase(entity_id)) continue;

    SensorTileWidgets& w = sensors[i];
    if (!w.chart || !w.series) continue;

    // Extract values array
    String values_str;
    int start = json.indexOf("\"values\"");
    if (start < 0) continue;
    int arr_start = json.indexOf('[', start);
    int arr_end = json.indexOf(']', arr_start);
    if (arr_start < 0 || arr_end < arr_start) continue;
    values_str = json.substring(arr_start + 1, arr_end);

    // Count and parse values
    std::vector<float> values;
    values.reserve(100);
    const char* ptr = values_str.c_str();
    while (*ptr) {
      while (*ptr && (*ptr == ' ' || *ptr == ',')) ++ptr;
      if (!*ptr) break;

      // Handle "null" or "unavailable"
      if (strncmp(ptr, "null", 4) == 0) {
        values.push_back(NAN);
        ptr += 4;
        continue;
      }
      if (*ptr == '"') {
        // Skip string values like "unavailable"
        ++ptr;
        while (*ptr && *ptr != '"') ++ptr;
        if (*ptr == '"') ++ptr;
        values.push_back(NAN);
        continue;
      }

      char* end = nullptr;
      float val = strtof(ptr, &end);
      if (end == ptr) {
        ++ptr;
        continue;
      }
      ptr = end;
      if (isfinite(val)) {
        values.push_back(val);
      } else {
        values.push_back(NAN);
      }
    }

    if (values.empty()) {
      lv_chart_set_point_count(w.chart, 1);
      lv_chart_set_value_by_id(w.chart, w.series, 0, LV_CHART_POINT_NONE);
      lv_chart_refresh(w.chart);
      Serial.printf("[TileGraph] History cleared for %s (0 points)\n", entity_id.c_str());
      continue;
    }

    // Luecken wie in HA schliessen: fehlende Buckets mit letztem gueltigen
    // Wert auffuellen (inkl. Prefix mit erstem gueltigen Wert).
    size_t first_valid = values.size();
    for (size_t j = 0; j < values.size(); ++j) {
      if (isfinite(values[j])) {
        first_valid = j;
        break;
      }
    }
    if (first_valid < values.size()) {
      float last = values[first_valid];
      for (size_t j = 0; j < first_valid; ++j) {
        values[j] = last;
      }
      for (size_t j = first_valid + 1; j < values.size(); ++j) {
        if (!isfinite(values[j])) {
          values[j] = last;
        } else {
          last = values[j];
        }
      }
    }

    // Use all points (same as popup - no downsampling)
    // Calculate min/max for range
    float min_v = 0.0f, max_v = 0.0f;
    bool has_range = false;
    for (float v : values) {
      if (!isfinite(v)) continue;
      if (!has_range) {
        min_v = max_v = v;
        has_range = true;
      } else {
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
      }
    }

    // Determine scale factor
    int scale = 1;
    if (has_range) {
      float span = max_v - min_v;
      if (span <= 10.0f) {
        scale = 100;
      } else if (span <= 100.0f) {
        scale = 10;
      }
      float max_abs = fmaxf(fabsf(min_v), fabsf(max_v));
      while (scale > 1 && (max_abs * scale) > 30000.0f) {
        scale /= 10;
      }
    }

    // Update chart
    lv_chart_set_point_count(w.chart, static_cast<uint16_t>(values.size()));
    for (size_t j = 0; j < values.size(); ++j) {
      float v = values[j];
      if (!isfinite(v)) {
        lv_chart_set_value_by_id(w.chart, w.series, static_cast<uint16_t>(j), LV_CHART_POINT_NONE);
      } else {
        lv_chart_set_value_by_id(w.chart, w.series, static_cast<uint16_t>(j),
                                 static_cast<lv_coord_t>(lroundf(v * scale)));
      }
    }

    // Set range
    if (has_range) {
      if (min_v == max_v) {
        min_v -= 1.0f;
        max_v += 1.0f;
      }
      lv_chart_set_range(w.chart, LV_CHART_AXIS_PRIMARY_Y,
                         static_cast<lv_coord_t>(floorf(min_v * scale)),
                         static_cast<lv_coord_t>(ceilf(max_v * scale)));
    }

    lv_chart_refresh(w.chart);
    Serial.printf("[TileGraph] History applied for %s (%zu points)\n", entity_id.c_str(), values.size());
  }
}

void queue_tile_graph_history(const char* entity_id, const char* payload, size_t len) {
  if (!payload || len == 0) return;
  g_tile_graph_history.entity_id = entity_id ? entity_id : "";
  g_tile_graph_history.payload = String(payload).substring(0, len);
  g_tile_graph_history.valid = true;
}

void process_tile_graph_queue() {
  if (!g_tile_graph_history.valid) return;

  apply_tile_graph_history(g_tile_graph_history.entity_id.c_str(),
                           g_tile_graph_history.payload.c_str());
  g_tile_graph_history.valid = false;
}

void request_tile_graph_history(const char* entity_id) {
  if (!entity_id || !*entity_id) return;
  if (String(entity_id).startsWith("__")) return;  // Skip preload entities
  mqttPublishHistoryRequest(entity_id);
  Serial.printf("[TileGraph] History requested for %s\n", entity_id);
}

/* === Helfer === */
void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f) {
  lv_obj_set_style_text_color(lbl, c, 0);
  lv_obj_set_style_text_font(lbl, f, 0);
}

void set_tile_grid_cell(lv_obj_t* obj, uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h) {
  if (!obj) return;
  uint8_t w = span_w < 1 ? 1 : span_w;
  uint8_t h = span_h < 1 ? 1 : span_h;
  if (w > GRID_COLS - col) w = GRID_COLS - col;
  if (h > GRID_ROWS - row) h = GRID_ROWS - row;
  lv_obj_set_grid_cell(obj,
      LV_GRID_ALIGN_STRETCH, col, w,
      LV_GRID_ALIGN_STRETCH, row, h);
}

static bool get_tile_layout(const Tile& tile, uint8_t& col, uint8_t& row, uint8_t& span_w, uint8_t& span_h) {
  if (tile.col >= GRID_COLS || tile.row >= GRID_ROWS) return false;
  col = tile.col;
  row = tile.row;
  span_w = tile.span_w < 1 ? 1 : tile.span_w;
  span_h = tile.span_h < 1 ? 1 : tile.span_h;
  if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
  if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;
  return true;
}

static void mark_occupied(bool occupied[GRID_ROWS][GRID_COLS], uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h) {
  for (uint8_t r = row; r < row + span_h; ++r) {
    for (uint8_t c = col; c < col + span_w; ++c) {
      if (r < GRID_ROWS && c < GRID_COLS) {
        occupied[r][c] = true;
      }
    }
  }
}

void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type,
                      scene_publish_cb_t scene_cb, lv_obj_t** out_tile_objs) {
  // Memory Monitoring - Vorher
  uint32_t heap_before = ESP.getFreeHeap();
  uint32_t psram_before = ESP.getFreePsram();
  Serial.printf("[TileRenderer] Lade %d Tiles... | Heap: %u KB | PSRAM: %u KB\n",
                TILES_PER_GRID, heap_before / 1024, psram_before / 1024);

  // Reset sensor widget pointers for this grid to avoid stale references
  clear_sensor_widgets(grid_type);
  clear_switch_widgets(grid_type);
  clear_weather_widgets(grid_type);
  clear_media_widgets(grid_type);

  if (parent == nullptr) {
    Serial.println("[TileRenderer] ERROR: Parent ist NULL!");
    return;
  }

  if (out_tile_objs) {
    for (size_t i = 0; i < TILES_PER_GRID; ++i) {
      out_tile_objs[i] = nullptr;
    }
  }

  bool occupied[GRID_ROWS][GRID_COLS] = {};
  struct TileLayout {
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t span_w = 1;
    uint8_t span_h = 1;
    bool valid = false;
  };
  TileLayout layouts[TILES_PER_GRID]{};

  for (int i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = config.tiles[i];
    if (tile.type == TILE_EMPTY) continue;
    uint8_t col = 0;
    uint8_t row = 0;
    uint8_t span_w = 1;
    uint8_t span_h = 1;
    if (!get_tile_layout(tile, col, row, span_w, span_h)) continue;
    layouts[i] = {col, row, span_w, span_h, true};
    mark_occupied(occupied, col, row, span_w, span_h);
  }

  for (uint8_t r = 0; r < GRID_ROWS; ++r) {
    for (uint8_t c = 0; c < GRID_COLS; ++c) {
      if (!occupied[r][c]) {
        render_empty_tile(parent, c, r);
      }
    }
    yield();
  }

  size_t render_count = 0;
  for (int i = 0; i < TILES_PER_GRID; ++i) {
    if (!layouts[i].valid) continue;
    Serial.printf("[TileRenderer] Erstelle Tile %d/%d...\n", i + 1, TILES_PER_GRID);

    Tile layout_tile = config.tiles[i];
    layout_tile.col = layouts[i].col;
    layout_tile.row = layouts[i].row;
    layout_tile.span_w = layouts[i].span_w;
    layout_tile.span_h = layouts[i].span_h;
    lv_obj_t* tile_obj = render_tile(parent, layouts[i].col, layouts[i].row, layout_tile, i, grid_type, scene_cb);
    if (out_tile_objs) {
      out_tile_objs[i] = tile_obj;
    }

    if ((++render_count % GRID_COLS) == 0) {
      yield();
      delay(1);
      yield();
    }

    Serial.printf("[TileRenderer] Tile %d/%d fertig\n", i + 1, TILES_PER_GRID);
  }
  // Memory Monitoring - Nachher
  uint32_t heap_after = ESP.getFreeHeap();
  uint32_t psram_after = ESP.getFreePsram();
  int32_t heap_used = heap_before - heap_after;
  int32_t psram_used = psram_before - psram_after;

  Serial.printf("[TileRenderer] ԣ� Alle Tiles geladen | Heap: %u KB (-%d KB) | PSRAM: %u KB (-%d KB)\n",
                heap_after / 1024, heap_used / 1024,
                psram_after / 1024, psram_used / 1024);
  Serial.printf("[TileRenderer] Min Free Heap seit Boot: %u KB\n", ESP.getMinFreeHeap() / 1024);
}

lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb) {
  // Serial.printf("[render_tile] Index=%d, Type=%d, Title='%s'\n", index, tile.type, tile.title.c_str());

  const TileTypeDescriptor* desc = get_tile_type_descriptor(tile.type);
  if (desc && desc->render) {
    return desc->render(parent, col, row, tile, index, grid_type, scene_cb);
  }
  return render_empty_tile(parent, col, row);
}

void update_sensor_tile_value(GridType grid_type, uint8_t grid_index, const char* value, const char* unit) {
  if (grid_index >= TILES_PER_GRID) {
    return;
  }

  SensorTileWidgets* target = (grid_type == GridType::TAB1) ? g_tab1_sensors : g_tab0_sensors;
  if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  lv_obj_t* value_label = target[grid_index].value_label;
  if (!value_label) {
    return;
  }

  if (target[grid_index].gauge && value) {
    float numeric = 0.0f;
    if (parse_sensor_number(String(value), numeric)) {
      const int32_t mapped = map_gauge_arc_value(numeric,
                                                 target[grid_index].gauge_min,
                                                 target[grid_index].gauge_max);
      lv_arc_set_value(target[grid_index].gauge, mapped);
    }
  }

  String displayValue = value ? String(value) : String();
  displayValue.trim();

  String lower = displayValue;
  lower.toLowerCase();
  if (lower == "unavailable" || lower == "unknown" || lower == "none" || lower == "null") {
    displayValue = "--";
  }

  // Formatierung nach Wunsch der Kachel (Nachkommastellen)
  if (displayValue.length() > 0 &&
      displayValue != "--" &&
      !displayValue.equalsIgnoreCase("unavailable")) {
    uint8_t decimals = get_sensor_decimals(grid_type, grid_index);
    apply_decimals(displayValue, decimals);
  }

  // Zeige "--" wenn leer oder unavailable
  if (displayValue.length() == 0 || displayValue.equalsIgnoreCase("unavailable")) {
    displayValue = "--";
  }

  // Kombiniere Wert + Einheit in einem Label (gleiche Gr+�+�e)
  String combined = displayValue;
  if (unit && strlen(unit) > 0 && displayValue != "--") {
    combined += " ";
    combined += unit;
  }
  lv_label_set_text(value_label, combined.c_str());
}

#include "src/tiles/tile_renderer.h"
#include "src/network/ha_bridge_config.h"
#include "src/network/mqtt_handlers.h"
#include "src/game/game_ws_server.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/ui_manager.h"
#include "src/ui/light_popup.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/image_popup.h"
#include "src/types/types_registry.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include <Arduino.h>
#include <cstring>
#include <math.h>
#include <stdlib.h>
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

void tile_renderer_snapshot_tab0(TileWidgetCache* out) {
  if (!out) return;
  memcpy(out->sensors, g_tab0_sensors, sizeof(g_tab0_sensors));
  memcpy(out->switches, g_tab0_switches, sizeof(g_tab0_switches));
  memcpy(out->switch_states, g_tab0_switch_states, sizeof(g_tab0_switch_states));
  memcpy(out->weather, g_tab0_weather, sizeof(g_tab0_weather));
}

void tile_renderer_restore_tab0(const TileWidgetCache* in) {
  if (!in) return;
  memcpy(g_tab0_sensors, in->sensors, sizeof(g_tab0_sensors));
  memcpy(g_tab0_switches, in->switches, sizeof(g_tab0_switches));
  memcpy(g_tab0_switch_states, in->switch_states, sizeof(g_tab0_switch_states));
  memcpy(g_tab0_weather, in->weather, sizeof(g_tab0_weather));
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
    case 1:
      return &ui_font_20;
    case 2:
      return &ui_font_24;
    default:
      return FONT_VALUE;
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
void process_sensor_update_queue() {
  while (g_queue_tail != g_queue_head) {
    SensorUpdate& upd = g_update_queue[g_queue_tail];

    if (upd.valid) {
      update_sensor_tile_value(upd.grid_type, upd.grid_index, upd.value.c_str(),
                              upd.unit.length() > 0 ? upd.unit.c_str() : nullptr);
      upd.valid = false;
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

static void decode_basic_json_escapes(String& text) {
  if (!text.length()) return;
  text.replace("\\u00b0", "\xC2\xB0");
  text.replace("\\u00B0", "\xC2\xB0");
  text.replace("\\/", "/");
  text.replace("\\\"", "\"");
  text.replace("\\\\", "\\");
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

static LightPopupInit build_popup_init_from_state(const Tile& tile, const SwitchState& state) {
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
  init.has_state = state.has_state;
  init.has_color = state.has_color;
  init.has_brightness = state.has_brightness;
  init.has_hs = state.has_hs;
  init.hs_h = state.hs_h;
  init.hs_s = state.hs_s;

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
  } else {
    init.supports_color = false;
    init.supports_brightness = false;
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
      bool has_onoff = list_contains_mode(supported_modes, "onoff");
      out.supports_brightness = has_brightness;
      out.supports_color = has_color;
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
        bool has_onoff = list_contains_mode(supported_modes, "onoff");
        out.supports_brightness = has_brightness;
        out.supports_color = has_color;
        out.supported_onoff_only = has_onoff && !has_brightness && !has_color;
      }

      if (extract_json_string_field(attributes, "color_mode", color_mode)) {
        String mode = color_mode;
        mode.toLowerCase();
        if (mode == "brightness") {
          out.supports_brightness = true;
          out.supported_onoff_only = false;
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

  if (out.has_color) {
    out.supports_color = true;
  }
  if (out.has_hs) {
    out.supports_color = true;
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
  if (is_light_entity) {
    if (state.supported_modes_known && state.supported_onoff_only) {
      state.supports_color = false;
      state.supports_brightness = false;
    }
  }

  state_target[grid_index] = state;

  if (tile.sensor_entity.length()) {
    LightPopupInit init = build_popup_init_from_state(tile, state);
    update_light_popup(init);
  }

  SwitchTileWidgets& widgets = target[grid_index];
  if (!widgets.icon_label && !widgets.title_label && !widgets.switch_obj) return;

  static const uint32_t kIconOn = 0xFFD54F;
  static const uint32_t kIconOff = 0xB0B0B0;

  uint32_t icon_color = kIconOff;
  if (!state.has_state || state.is_on) {
    icon_color = state.has_color ? state.color : kIconOn;
  }

  lv_color_t lv_color = lv_color_hex(icon_color);
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
    lv_obj_set_style_bg_color(widgets.switch_obj, lv_color_hex(kIconOff), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(widgets.switch_obj, lv_color_hex(icon_color), LV_PART_INDICATOR | LV_STATE_CHECKED);
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

void process_switch_update_queue() {
  while (g_switch_tail != g_switch_head) {
    SwitchUpdate& upd = g_switch_queue[g_switch_tail];
    if (upd.valid) {
      update_switch_tile_state(upd.grid_type, upd.grid_index, upd.payload.c_str());
      upd.valid = false;
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
  String forecast_raw;
  uint8_t forecast_count = 0;
  bool has_forecast = false;
  if (extract_json_array_field(json, "forecast", forecast_raw)) {
    if (forecast_raw.indexOf('{') >= 0) {
      has_forecast = true;
    }
  }
  if (has_forecast && forecast_limit > 0) {
    bool in_string = false;
    int depth = 0;
    int start = -1;
    for (int i = 0; i < forecast_raw.length() && forecast_count < forecast_limit; ++i) {
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
          float f_temp = 0.0f;
          float f_low = 0.0f;
          bool f_has_temp = extract_json_number_or_string_field(obj, "temperature", f_temp);
          bool f_has_low = false;
          if (extract_json_number_or_string_field(obj, "templow", f_low)) f_has_low = true;
          if (!f_has_low && extract_json_number_or_string_field(obj, "temperature_low", f_low)) f_has_low = true;
          if (!f_has_low && extract_json_number_or_string_field(obj, "temp_low", f_low)) f_has_low = true;
          if (!f_has_low && extract_json_number_or_string_field(obj, "low", f_low)) f_has_low = true;
          extract_json_string_field(obj, "condition", f_condition);
          extract_json_string_field(obj, "icon", f_icon);
          String datetime;
          if (extract_json_string_field(obj, "datetime", datetime)) {
            f_day = weekday_from_iso(datetime);
          }
          if (!f_day.length() && forecast_count == 0) {
            f_day = "Morgen";
          }
          if (!f_icon.length() && f_condition.length()) {
            f_icon = weather_icon_from_condition(f_condition);
          }

          WeatherForecastWidgets& fw = widgets.forecast[forecast_count];
          if (fw.day_label) {
            String day_text = f_day.length() ? f_day : "--";
            if (f_icon.length()) day_text += " |";
            lv_label_set_text(fw.day_label, day_text.c_str());
            lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
          }
          if (fw.icon_label) {
            if (f_icon.length()) {
              String iconChar = getMdiChar(f_icon);
              if (iconChar.length()) {
                lv_label_set_text(fw.icon_label, iconChar.c_str());
                lv_obj_clear_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
              } else {
                lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
              }
            } else {
              lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
            }
          }
          if (fw.temp_label) {
            String hi_text = f_has_temp ? format_weather_temp(f_temp, unit) : String("--");
            String lo_text = f_has_low ? format_weather_temp(f_low, unit) : String("--");
            String temp_text = hi_text + "\n" + lo_text;
            lv_label_set_text(fw.temp_label, temp_text.c_str());
            lv_obj_clear_flag(fw.temp_label, LV_OBJ_FLAG_HIDDEN);
          }

          forecast_count++;
          start = -1;
        }
      }
    }
  }

  if (has_forecast && forecast_limit > 0) {
    for (uint8_t i = forecast_count; i < forecast_limit; ++i) {
      WeatherForecastWidgets& fw = widgets.forecast[i];
      if (fw.day_label) {
        lv_label_set_text(fw.day_label, "--");
        lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (fw.sep_label) {
        lv_obj_add_flag(fw.sep_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (fw.icon_label) {
        lv_label_set_text(fw.icon_label, "");
        lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (fw.temp_label) {
        lv_label_set_text(fw.temp_label, "--\n--");
        lv_obj_clear_flag(fw.temp_label, LV_OBJ_FLAG_HIDDEN);
      }
    }
  } else if (forecast_limit > 0) {
    for (uint8_t i = 0; i < forecast_limit; ++i) {
      WeatherForecastWidgets& fw = widgets.forecast[i];
      if (fw.day_label) {
        lv_label_set_text(fw.day_label, "--");
        lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (fw.sep_label) {
        lv_obj_add_flag(fw.sep_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (fw.icon_label) {
        lv_label_set_text(fw.icon_label, "");
        lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
      }
      if (fw.temp_label) {
        lv_label_set_text(fw.temp_label, "--\n--");
        lv_obj_clear_flag(fw.temp_label, LV_OBJ_FLAG_HIDDEN);
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

void process_weather_update_queue() {
  while (g_weather_tail != g_weather_head) {
    WeatherUpdate& upd = g_weather_queue[g_weather_tail];
    if (upd.valid) {
      update_weather_tile_state(upd.grid_type, upd.grid_index, upd.payload.c_str());
      upd.valid = false;
    }
    g_weather_tail = (g_weather_tail + 1) % WEATHER_QUEUE_SIZE;
  }
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

void render_tile_grid(lv_obj_t* parent, const TileGridConfig& config, GridType grid_type, scene_publish_cb_t scene_cb) {
  // Memory Monitoring - Vorher
  uint32_t heap_before = ESP.getFreeHeap();
  uint32_t psram_before = ESP.getFreePsram();
  Serial.printf("[TileRenderer] Lade %d Tiles... | Heap: %u KB | PSRAM: %u KB\n",
                TILES_PER_GRID, heap_before / 1024, psram_before / 1024);

  // Reset sensor widget pointers for this grid to avoid stale references
  clear_sensor_widgets(grid_type);
  clear_switch_widgets(grid_type);
  clear_weather_widgets(grid_type);

  if (parent == nullptr) {
    Serial.println("[TileRenderer] ERROR: Parent ist NULL!");
    return;
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
    render_tile(parent, layouts[i].col, layouts[i].row, layout_tile, i, grid_type, scene_cb);

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

#include "src/tiles/tile_renderer.h"
#include "src/network/ha_bridge_config.h"
#include "src/network/mqtt_handlers.h"
#include "src/game/game_ws_server.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/ui_manager.h"
#include "src/ui/light_popup.h"
#include "src/fonts/ui_fonts.h"
#include <Arduino.h>
#include <math.h>
#include <stdlib.h>

/* === Layout-Konstanten === */
static const int CARD_H = 150;

/* === Fonts === */
#define FONT_TITLE (&ui_font_24)

#if defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  #define FONT_VALUE (&lv_font_montserrat_48)
#elif defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  #define FONT_VALUE (&lv_font_montserrat_40)
#else
  #define FONT_VALUE (LV_FONT_DEFAULT)
#endif

#define FONT_UNIT (&ui_font_24)

/* === Globale State für Updates === */
struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  lv_obj_t* unit_label = nullptr;
};

struct SwitchTileWidgets {
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* switch_obj = nullptr;
};

static SensorTileWidgets g_tab0_sensors[TILES_PER_GRID];
static SensorTileWidgets g_tab1_sensors[TILES_PER_GRID];
static SensorTileWidgets g_tab2_sensors[TILES_PER_GRID];

static SwitchTileWidgets g_tab0_switches[TILES_PER_GRID];
static SwitchTileWidgets g_tab1_switches[TILES_PER_GRID];
static SwitchTileWidgets g_tab2_switches[TILES_PER_GRID];

struct SwitchState {
  bool has_state = false;
  bool is_on = false;
  bool has_color = false;
  uint32_t color = 0;
  bool has_brightness = false;
  uint8_t brightness_pct = 100;
  bool supports_color = false;
  bool supports_brightness = false;
  bool supported_modes_known = false;
  bool supported_onoff_only = false;
};

static SwitchState g_tab0_switch_states[TILES_PER_GRID];
static SwitchState g_tab1_switch_states[TILES_PER_GRID];
static SwitchState g_tab2_switch_states[TILES_PER_GRID];

static void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f);
static bool is_light_entity_id(const String& entity_id);

static void clear_sensor_widgets(GridType grid_type) {
  SensorTileWidgets* target = g_tab0_sensors;
  if (grid_type == GridType::TAB1) target = g_tab1_sensors;
  else if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    target[i].value_label = nullptr;
    target[i].unit_label = nullptr;
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

/* === Thread-Safe Update Queue (MQTT → Main Loop) === */
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
  const TileGridConfig& grid = (grid_type == GridType::TAB1)
                                 ? tileConfig.getTab1Grid()
                                 : (grid_type == GridType::TAB2 ? tileConfig.getTab2Grid() : tileConfig.getTab0Grid());
  return grid.tiles[grid_index].sensor_decimals;
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

    if (!out.has_color) {
      String hs_list;
      if (extract_json_array_field(text, "hs_color", hs_list)) {
        float h = 0.0f, s = 0.0f;
        if (parse_hs_list(hs_list, h, s)) {
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

      if (!out.has_color) {
        String hs_list;
        if (extract_json_array_field(attributes, "hs_color", hs_list)) {
          float h = 0.0f, s = 0.0f;
          if (parse_hs_list(hs_list, h, s)) {
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

static void update_switch_tile_state(GridType grid_type, uint8_t grid_index, const char* payload) {
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

  const TileGridConfig& grid = (grid_type == GridType::TAB1)
                                 ? tileConfig.getTab1Grid()
                                 : (grid_type == GridType::TAB2 ? tileConfig.getTab2Grid() : tileConfig.getTab0Grid());
  const String& entity_id = grid.tiles[grid_index].sensor_entity;
  const bool is_light_entity = is_light_entity_id(entity_id);
  if (is_light_entity) {
    if (state.supported_modes_known && state.supported_onoff_only) {
      state.supports_color = false;
      state.supports_brightness = false;
    }
  }

  state_target[grid_index] = state;

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

/* === Helfer === */
static void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f) {
  lv_obj_set_style_text_color(lbl, c, 0);
  lv_obj_set_style_text_font(lbl, f, 0);
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

  for (int i = 0; i < TILES_PER_GRID; ++i) {
    int row = i / 3;
    int col = i % 3;

    // Fehlerbehandlung: Ein defektes Tile crasht nicht das ganze System
    if (parent == nullptr) {
      Serial.println("[TileRenderer] ERROR: Parent ist NULL!");
      return;
    }

    Serial.printf("[TileRenderer] Erstelle Tile %d/%d...\n", i + 1, TILES_PER_GRID);

    render_tile(parent, col, row, config.tiles[i], i, grid_type, scene_cb);

    // PROGRESSIVES RENDERING: Pause zwischen Tiles (verhindert Crash)
    yield();                    // Watchdog füttern
    delay(10);                 // 10ms Pause für System Processing (120ms total)
    yield();                   // Nochmal Watchdog
    // KEIN lv_timer_handler() hier! Sonst werden unfertige Tiles gezeichnet!

    Serial.printf("[TileRenderer] ✓ Tile %d/%d fertig\n", i + 1, TILES_PER_GRID);
  }

  // Memory Monitoring - Nachher
  uint32_t heap_after = ESP.getFreeHeap();
  uint32_t psram_after = ESP.getFreePsram();
  int32_t heap_used = heap_before - heap_after;
  int32_t psram_used = psram_before - psram_after;

  Serial.printf("[TileRenderer] ✓ Alle Tiles geladen | Heap: %u KB (-%d KB) | PSRAM: %u KB (-%d KB)\n",
                heap_after / 1024, heap_used / 1024,
                psram_after / 1024, psram_used / 1024);
  Serial.printf("[TileRenderer] Min Free Heap seit Boot: %u KB\n", ESP.getMinFreeHeap() / 1024);
}

lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb) {
  switch (tile.type) {
    case TILE_SENSOR:
      return render_sensor_tile(parent, col, row, tile, index, grid_type);
    case TILE_SCENE:
      return render_scene_tile(parent, col, row, tile, index, scene_cb);
    case TILE_KEY:
      return render_key_tile(parent, col, row, tile, index, grid_type);
    case TILE_NAVIGATE:
      return render_navigate_tile(parent, col, row, tile, index);
    case TILE_SWITCH:
      return render_switch_tile(parent, col, row, tile, index, grid_type);
    default:
      return render_empty_tile(parent, col, row);
  }
  return nullptr;
}

lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  if (!parent) {
    Serial.println("[TileRenderer] ERROR: parent NULL bei Sensor-Tile");
    return nullptr;
  }

  lv_obj_t* card = lv_obj_create(parent);
  if (!card) {
    Serial.println("[TileRenderer] ERROR: Konnte Sensor-Card nicht erstellen");
    return nullptr;
  }

  // Farbe verwenden (Standard: 0x2A2A2A wenn color = 0)
  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x2A2A2A;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_set_height(card, CARD_H);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(card,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Icon Label (optional, falls icon_name vorhanden) - rechtsbündig
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      lv_obj_t* icon_lbl = lv_label_create(card);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());
        lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);  // Rechtsbündig (4px rechts, 8px hoch)
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden) - linksbündig
  if (tile.title.length() > 0) {
    lv_obj_t* t = lv_label_create(card);
    if (t) {
      set_label_style(t, lv_color_hex(0xFFFFFF), FONT_TITLE);
      lv_label_set_text(t, tile.title.c_str());
      lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 4);  // Linksbündig
    }
  }

  // Value Label (Wert + Einheit kombiniert)
  lv_obj_t* v = lv_label_create(card);
  if (!v) {
    Serial.println("[TileRenderer] ERROR: Konnte Value-Label nicht erstellen");
    return card;
  }
  set_label_style(v, lv_color_white(), get_sensor_value_font(tile));
  lv_label_set_text(v, "--");
  lv_obj_align(v, LV_ALIGN_CENTER, 0, 28);  // Nach unten verschoben (war 18)

  // Speichern für spätere Updates
  SensorTileWidgets* target = (grid_type == GridType::TAB0) ? g_tab0_sensors : g_tab1_sensors;
  if (grid_type == GridType::TAB2) target = g_tab2_sensors;
  target[index].value_label = v;
  target[index].unit_label = nullptr;

  return card;
}

struct SceneEventData {
  String scene_alias;
  scene_publish_cb_t callback;
};

lv_obj_t* render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Icon Label (optional, falls icon_name vorhanden)
  lv_obj_t* icon_lbl = nullptr;
  bool has_icon = tile.icon_name.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(btn);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());

        // Flexible Positionierung: Icon + Title = 2 Zeilen mittig, nur Icon = 1 Zeile mittig
        if (has_title) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);  // Icon oben (mit Title)
        } else {
          lv_obj_center(icon_lbl);  // Icon mittig (ohne Title)
        }
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden)
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    if (l) {
      set_label_style(l, lv_color_white(), FONT_TITLE);
      lv_label_set_text(l, tile.title.c_str());

      // Flexible Positionierung: mit Icon unten, ohne Icon mittig
      if (icon_lbl) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);  // Title unten (mit Icon)
      } else {
        lv_obj_center(l);  // Title mittig (ohne Icon)
      }
    }
  }

  // Event-Handler für Scene-Aktivierung
  if (scene_cb && tile.scene_alias.length()) {
    // Allocate permanent storage for event data
    SceneEventData* event_data = new SceneEventData{tile.scene_alias, scene_cb};

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          SceneEventData* data = static_cast<SceneEventData*>(lv_event_get_user_data(e));
          if (data && data->callback) {
            Serial.printf("[Tile] Szene aktiviert: %s\n", data->scene_alias.c_str());
            data->callback(data->scene_alias.c_str());
          }
        },
        LV_EVENT_CLICKED,
        event_data);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          SceneEventData* data = static_cast<SceneEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  }

  return btn;
}

struct KeyEventData {
  String title;
  uint8_t key_code;
  uint8_t modifier;
  uint8_t index;
};

lv_obj_t* render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Icon Label (optional, falls icon_name vorhanden) - wie bei Scene
  lv_obj_t* icon_lbl = nullptr;
  bool has_icon = tile.icon_name.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(btn);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());

        // Flexible Positionierung: Icon + Title = 2 Zeilen mittig, nur Icon = 1 Zeile mittig
        if (has_title) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);  // Icon oben (mit Title)
        } else {
          lv_obj_center(icon_lbl);  // Icon mittig (ohne Title)
        }
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden)
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    if (l) {
      set_label_style(l, lv_color_white(), FONT_TITLE);
      lv_label_set_text(l, tile.title.c_str());

      // Flexible Positionierung: mit Icon unten, ohne Icon mittig
      if (icon_lbl) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);  // Title unten (mit Icon)
      } else {
        lv_obj_center(l);  // Title mittig (ohne Icon)
      }
    }
  }

  // Event-Handler für WebSocket Broadcast
  if (tile.key_code != 0) {
    // Allocate permanent storage for event data
    KeyEventData* event_data = new KeyEventData{
      tile.title,
      tile.key_code,
      tile.key_modifier,
      index
    };

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          KeyEventData* data = static_cast<KeyEventData*>(lv_event_get_user_data(e));
          if (data) {
            Serial.printf("[Tile] Key '%s' gedrückt - Code: 0x%02X Mod: 0x%02X\n",
                          data->title.c_str(), data->key_code, data->modifier);

            // WebSocket Broadcast an alle verbundenen Clients
            gameWSServer.broadcastButtonPress(
              data->index,
              data->title.c_str(),
              data->key_code,
              data->modifier
            );
          }
        },
        LV_EVENT_CLICKED,
        event_data);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          KeyEventData* data = static_cast<KeyEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  }

  return btn;
}

struct NavigateEventData {
  uint8_t target_tab;
  String title;
};

lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_height(btn, CARD_H);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Icon Label (optional, falls icon_name vorhanden)
  lv_obj_t* icon_lbl = nullptr;
  bool has_icon = tile.icon_name.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(btn);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());

        // Flexible Positionierung: Icon + Title = 2 Zeilen mittig, nur Icon = 1 Zeile mittig
        if (has_title) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);  // Icon oben (mit Title)
        } else {
          lv_obj_center(icon_lbl);  // Icon mittig (ohne Title)
        }
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden)
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    if (l) {
      set_label_style(l, lv_color_white(), FONT_TITLE);
      lv_label_set_text(l, tile.title.c_str());

      // Flexible Positionierung: mit Icon unten, ohne Icon mittig
      if (icon_lbl) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);  // Title unten (mit Icon)
      } else {
        lv_obj_center(l);  // Title mittig (ohne Icon)
      }
    }
  }

  // Event-Handler für Tab-Navigation
  // Element-Pool: sensor_decimals = target tab (0=Tab0, 1=Tab1, 2=Tab2)
  uint8_t target_tab = tile.sensor_decimals;
  Serial.printf("[Navigate] Render Navigation-Tile - sensor_decimals=%d, target_tab=%d\n", tile.sensor_decimals, target_tab);

  if (target_tab <= 2) {  // Nur gültige Tabs
    Serial.printf("[Navigate] Event-Handler wird registriert für Tab %d\n", target_tab);
    // Allocate permanent storage for event data
    NavigateEventData* event_data = new NavigateEventData{
      target_tab,
      tile.title
    };

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          NavigateEventData* data = static_cast<NavigateEventData*>(lv_event_get_user_data(e));
          if (data) {
            Serial.printf("[Tile] Navigation CLICKED! Ziel-Tab: %d, Titel: %s\n", data->target_tab, data->title.c_str());
            uiManager.switchToTab(data->target_tab);
            Serial.printf("[Tile] switchToTab(%d) aufgerufen\n", data->target_tab);
          }
        },
        LV_EVENT_CLICKED,
        event_data);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          NavigateEventData* data = static_cast<NavigateEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  } else {
    Serial.printf("[Navigate] WARNUNG: target_tab=%d ist ungültig (>2), Event-Handler NICHT registriert!\n", target_tab);
  }

  return btn;
}

struct SwitchEventData {
  String entity_id;
  String title;
  GridType grid_type;
  uint8_t index = 0;
  bool suppress_click = false;
};

struct SwitchWidgetEventData {
  String entity_id;
};

static SwitchState* get_switch_state_array(GridType grid_type) {
  if (grid_type == GridType::TAB1) return g_tab1_switch_states;
  if (grid_type == GridType::TAB2) return g_tab2_switch_states;
  return g_tab0_switch_states;
}

static SwitchState get_switch_state(GridType grid_type, uint8_t index) {
  if (index >= TILES_PER_GRID) return {};
  return get_switch_state_array(grid_type)[index];
}

static bool is_light_entity_id(const String& entity_id) {
  return entity_id.length() >= 6 && entity_id.startsWith("light.");
}

static LightPopupInit build_light_popup_init(const SwitchEventData* data) {
  LightPopupInit init;
  if (!data) return init;
  init.entity_id = data->entity_id;
  init.title = data->title;
  init.is_light = is_light_entity_id(data->entity_id);

  const SwitchState state = get_switch_state(data->grid_type, data->index);
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

static bool is_switch_widget_tile(const Tile& tile) {
  return tile.sensor_decimals == 1;
}

lv_obj_t* render_switch_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  const bool use_switch_widget = is_switch_widget_tile(tile);
  lv_obj_t* container = use_switch_widget ? lv_obj_create(parent) : lv_button_create(parent);
  lv_obj_set_style_radius(container, 22, 0);
  lv_obj_set_style_border_width(container, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t tile_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(container, lv_color_hex(tile_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  if (!use_switch_widget) {
    // Pressed-State: 10% heller
    uint32_t pressed_color = tile_color + 0x101010;
    lv_obj_set_style_bg_color(container, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  }

  lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(container, 0, 0);
  if (use_switch_widget) {
    lv_obj_set_style_pad_hor(container, 20, 0);
    lv_obj_set_style_pad_ver(container, 24, 0);
    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_set_height(container, CARD_H);
  lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_set_grid_cell(container,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Icon Label (optional, falls icon_name vorhanden)
  lv_obj_t* icon_lbl = nullptr;
  lv_obj_t* title_lbl = nullptr;
  bool has_icon = tile.icon_name.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(container);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());

        if (use_switch_widget) {
          lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);
        } else {
          // Flexible Positionierung: Icon + Title = 2 Zeilen mittig, nur Icon = 1 Zeile mittig
          if (has_title) {
            lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);
          } else {
            lv_obj_center(icon_lbl);
          }
        }
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden)
  if (has_title) {
    title_lbl = lv_label_create(container);
    if (title_lbl) {
      set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
      lv_label_set_text(title_lbl, tile.title.c_str());

      if (use_switch_widget) {
        lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 4);
      } else {
        // Flexible Positionierung: mit Icon unten, ohne Icon mittig
        if (icon_lbl) {
          lv_obj_align(title_lbl, LV_ALIGN_CENTER, 0, 35);
        } else {
          lv_obj_center(title_lbl);
        }
      }
    }
  }

  lv_obj_t* switch_obj = nullptr;
  if (use_switch_widget) {
    switch_obj = lv_switch_create(container);
    if (switch_obj) {
      lv_obj_set_size(switch_obj, 90, 44);
      lv_obj_align(switch_obj, LV_ALIGN_CENTER, 0, 28);
      lv_obj_set_ext_click_area(switch_obj, 18);
      lv_obj_add_flag(switch_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
      lv_obj_set_style_bg_color(switch_obj, lv_color_hex(0xB0B0B0), LV_PART_INDICATOR | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_color(switch_obj, lv_color_hex(0xFFD54F), LV_PART_INDICATOR | LV_STATE_CHECKED);
      SwitchWidgetEventData* widget_data = new SwitchWidgetEventData{tile.sensor_entity};
      lv_obj_add_event_cb(
          switch_obj,
          [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
            SwitchWidgetEventData* data = static_cast<SwitchWidgetEventData*>(lv_event_get_user_data(e));
            if (!data || !data->entity_id.length()) return;
            lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            bool is_on = target && lv_obj_has_state(target, LV_STATE_CHECKED);
            mqttPublishSwitchCommand(data->entity_id.c_str(), is_on ? "on" : "off");
          },
          LV_EVENT_VALUE_CHANGED,
          widget_data);
      lv_obj_add_event_cb(
          switch_obj,
          [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
            SwitchWidgetEventData* data = static_cast<SwitchWidgetEventData*>(lv_event_get_user_data(e));
            delete data;
          },
          LV_EVENT_DELETE,
          widget_data);
    }
  }

  SwitchTileWidgets* target = g_tab0_switches;
  if (grid_type == GridType::TAB1) target = g_tab1_switches;
  else if (grid_type == GridType::TAB2) target = g_tab2_switches;
  if (index < TILES_PER_GRID) {
    target[index].icon_label = icon_lbl;
    target[index].title_label = title_lbl;
    target[index].switch_obj = switch_obj;
  }

  if (tile.sensor_entity.length()) {
    String initial = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
    if (initial.length()) {
      update_switch_tile_state(grid_type, index, initial.c_str());
    }
  }

  if (tile.sensor_entity.length()) {
    SwitchEventData* event_data = new SwitchEventData{
      tile.sensor_entity,
      tile.title,
      grid_type,
      index,
      false
    };

    if (!use_switch_widget) {
      lv_obj_add_event_cb(
          container,
          [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            SwitchEventData* data = static_cast<SwitchEventData*>(lv_event_get_user_data(e));
            if (!data) return;
            if (data->suppress_click) {
              data->suppress_click = false;
              return;
            }
            Serial.printf("[Tile] Switch toggle: %s\n", data->entity_id.c_str());
            mqttPublishSwitchCommand(data->entity_id.c_str(), "toggle");
          },
          LV_EVENT_CLICKED,
          event_data);
    }

    lv_obj_add_event_cb(
        container,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
          SwitchEventData* data = static_cast<SwitchEventData*>(lv_event_get_user_data(e));
          if (!data) return;
          data->suppress_click = true;
          LightPopupInit init = build_light_popup_init(data);
          show_light_popup(init);
        },
        LV_EVENT_LONG_PRESSED,
        event_data);

    lv_obj_add_event_cb(
        container,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          SwitchEventData* data = static_cast<SwitchEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  }

  return container;
}

lv_obj_t* render_empty_tile(lv_obj_t* parent, int col, int row) {
  lv_obj_t* placeholder = lv_obj_create(parent);
  lv_obj_set_style_bg_opa(placeholder, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(placeholder, 0, 0);
  lv_obj_set_height(placeholder, CARD_H);
  lv_obj_set_grid_cell(placeholder,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);
  return placeholder;
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

  // Kombiniere Wert + Einheit in einem Label (gleiche Größe)
  String combined = displayValue;
  if (unit && strlen(unit) > 0 && displayValue != "--") {
    combined += " ";
    combined += unit;
  }
  lv_label_set_text(value_label, combined.c_str());
}

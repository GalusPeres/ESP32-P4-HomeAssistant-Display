#include "src/ui/weather_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/image_popup.h"
#include "src/ui/radar_popup.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer.h"
#include <math.h>
#include <stdlib.h>

namespace {

// Match the popup width to the full tile grid so left/right margins align.
constexpr int kCardWidth = (GRID_CELL_W * GRID_COLS) + (GRID_GAP * (GRID_COLS - 1));
constexpr int kCardHeight = kCardWidth;
constexpr int kCardPad = 20;
constexpr int kForecastGap = 16;
constexpr int kCols = 5;
constexpr int kRows = 2;
constexpr int kCellW = (kCardWidth - (kCardPad * 2) - (kForecastGap * (kCols - 1))) / kCols;
constexpr int kCellH = (kCardHeight - (kCardPad * 2) - (kForecastGap * (kRows - 1))) / kRows;
constexpr int kHeaderPadTop = 4;
constexpr int kHeaderIconOffsetX = 4;
constexpr int kHeaderIconOffsetY = -8;

struct ForecastWidgets {
  lv_obj_t* day_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* temp_label = nullptr;
};

struct WeatherPopupContext {
  String entity_id;
  String title;
  uint32_t bg_color = 0;
  String unit;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* location_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* value_row = nullptr;
  lv_obj_t* condition_label = nullptr;
  lv_obj_t* condition_sep_label = nullptr;
  lv_obj_t* temp_label = nullptr;
  ForecastWidgets forecast[kCols];
};

struct PendingWeatherUpdate {
  String entity_id;
  String payload;
  bool valid = false;
};

static WeatherPopupContext* g_weather_popup_ctx = nullptr;
static PendingWeatherUpdate g_pending_weather;

static bool is_popup_visible(WeatherPopupContext* ctx) {
  if (!ctx || !ctx->card) return false;
  return !lv_obj_has_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
}

static void decode_basic_json_escapes(String& text) {
  if (!text.length()) return;
  text.replace("\\u00b0", "\xC2\xB0");
  text.replace("\\u00B0", "\xC2\xB0");
  text.replace("\\/", "/");
  text.replace("\\\"", "\"");
  text.replace("\\\\", "\\");
}

static bool extract_json_string_field(const String& src, const char* key, String& out) {
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
  if (pos >= src.length() || src.charAt(pos) != '"') return false;
  int start = pos + 1;
  bool escaped = false;
  for (int i = start; i < src.length(); ++i) {
    char c = src.charAt(i);
    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      out = src.substring(start, i);
      return true;
    }
  }
  return false;
}

static bool extract_json_array_field(const String& src, const char* key, String& out) {
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
  if (pos >= src.length() || src.charAt(pos) != '[') return false;
  int depth = 0;
  bool in_string = false;
  for (int i = pos; i < src.length(); ++i) {
    char c = src.charAt(i);
    if (c == '"' && (i == 0 || src.charAt(i - 1) != '\\')) {
      in_string = !in_string;
    }
    if (in_string) continue;
    if (c == '[') {
      ++depth;
    } else if (c == ']') {
      --depth;
      if (depth == 0) {
        out = src.substring(pos, i + 1);
        return true;
      }
    }
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

static bool extract_json_number_field(const String& src, const char* key, float& out) {
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
  if (pos >= src.length()) return false;
  const char* start = src.c_str() + pos;
  char* end = nullptr;
  float val = strtof(start, &end);
  if (!end || end == start) return false;
  out = val;
  return true;
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
  if (key == "cloudy") return "Bew\xC3\xB6lkt";
  if (key == "exceptional") return "Ausnahme";
  if (key == "fog") return "Nebel";
  if (key == "hail") return "Hagel";
  if (key == "lightning") return "Gewitter";
  if (key == "lightning-rainy") return "Gewitterregen";
  if (key == "partlycloudy") return "Teilw. bew\xC3\xB6lkt";
  if (key == "pouring") return "Starkregen";
  if (key == "rainy") return "Regen";
  if (key == "snowy") return "Schnee";
  if (key == "snowy-rainy") return "Schneeregen";
  if (key == "sunny") return "Sonnig";
  if (key == "windy") return "Windig";
  if (key == "windy-variant") return "B\xC3\xB6ig";
  String text = condition;
  text.replace("-", " ");
  text.replace("_", " ");
  text.trim();
  return text.length() ? text : "--";
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
  int dow = (h + 6) % 7;
  static const char* kDaysDe[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  if (dow < 0 || dow > 6) return "";
  return String(kDaysDe[dow]);
}

static void set_label_style(lv_obj_t* lbl, lv_color_t color, const lv_font_t* font) {
  if (!lbl) return;
  lv_obj_set_style_text_color(lbl, color, 0);
  if (font) {
    lv_obj_set_style_text_font(lbl, font, 0);
  }
}

static void clear_forecast(WeatherPopupContext* ctx) {
  if (!ctx) return;

  for (int i = 0; i < kCols; ++i) {
    ForecastWidgets& fw = ctx->forecast[i];
    if (fw.day_label) {
      lv_label_set_text(fw.day_label, "--");
      lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
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

static void align_header_row(lv_obj_t* card, lv_obj_t* title_label, lv_obj_t* icon_label);

static void apply_weather_payload(WeatherPopupContext* ctx, const char* payload) {
  if (!ctx || !payload || !*payload) return;
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
  if (unit.length()) {
    ctx->unit = unit;
  } else {
    unit = ctx->unit;
  }

  if (ctx->icon_label) {
    if (icon_name.length()) {
      String iconChar = getMdiChar(icon_name);
      if (iconChar.length()) {
        lv_label_set_text(ctx->icon_label, iconChar.c_str());
        lv_obj_clear_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      lv_obj_add_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  String condition_text = weather_condition_to_german(condition);
  bool show_condition = (condition_text.length() && condition_text != "--");

  if (ctx->temp_label) {
    String temp_text = has_temp ? format_weather_temp(temperature, unit) : String("--");
    lv_label_set_text(ctx->temp_label, temp_text.c_str());
    lv_obj_clear_flag(ctx->temp_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ctx->condition_label) {
    if (show_condition) {
      lv_label_set_text(ctx->condition_label, condition_text.c_str());
      lv_obj_clear_flag(ctx->condition_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(ctx->condition_label, "");
      lv_obj_add_flag(ctx->condition_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (ctx->condition_sep_label) {
    if (show_condition) {
      lv_label_set_text(ctx->condition_sep_label, "|");
      lv_obj_clear_flag(ctx->condition_sep_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(ctx->condition_sep_label, "");
      lv_obj_add_flag(ctx->condition_sep_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (ctx->location_label) {
    String name = ctx->title;
    name.trim();
    if (!name.length() || name == "--") {
      String payload_name;
      if (extract_json_string_field(json, "name", payload_name)) {
        decode_basic_json_escapes(payload_name);
        name = payload_name;
      }
    }
    if (!name.length()) name = "--";
    lv_label_set_text(ctx->location_label, name.c_str());
    lv_obj_clear_flag(ctx->location_label, LV_OBJ_FLAG_HIDDEN);
  }
  align_header_row(ctx->card, ctx->location_label, ctx->icon_label);

  String forecast_raw;
  uint8_t forecast_count = 0;
  bool has_forecast = false;
  if (extract_json_array_field(json, "forecast", forecast_raw)) {
    if (forecast_raw.indexOf('{') >= 0) {
      has_forecast = true;
    }
  }
  if (has_forecast) {
    bool in_string = false;
    int depth = 0;
    int start = -1;
    for (int i = 0; i < forecast_raw.length() && forecast_count < kCols; ++i) {
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

          ForecastWidgets& fw = ctx->forecast[forecast_count];
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

  if (has_forecast) {
    for (uint8_t i = forecast_count; i < kCols; ++i) {
      ForecastWidgets& fw = ctx->forecast[i];
      if (fw.day_label) {
        lv_label_set_text(fw.day_label, "--");
        lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
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
  } else {
    clear_forecast(ctx);
  }
}

static void align_header_row(lv_obj_t* card, lv_obj_t* title_label, lv_obj_t* icon_label) {
  if (!card) return;
  lv_obj_update_layout(card);
  lv_coord_t header_center_y = 60 - lv_obj_get_style_pad_top(card, LV_PART_MAIN);
  if (header_center_y < 0) header_center_y = 0;
  if (icon_label) {
    lv_coord_t icon_y = header_center_y - (lv_obj_get_height(icon_label) / 2);
    if (icon_y < 0) icon_y = 0;
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 8, icon_y);
  }
  if (title_label) {
    lv_coord_t title_y = header_center_y - (lv_obj_get_height(title_label) / 2);
    if (title_y < 0) title_y = 0;
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 78, title_y);
  }
}

static void apply_init_to_context(WeatherPopupContext* ctx, const WeatherPopupInit& init) {
  if (!ctx) return;
  ctx->entity_id = init.entity_id;
  ctx->title = init.title;
  ctx->bg_color = init.bg_color;
  if (ctx->card) {
    uint32_t color = ctx->bg_color ? ctx->bg_color : 0x2A2A2A;
    lv_obj_set_style_bg_color(ctx->card, lv_color_hex(color), 0);
  }
  if (ctx->location_label) {
    String title = ctx->title;
    title.trim();
    if (!title.length()) title = "--";
    lv_label_set_text(ctx->location_label, title.c_str());
  }
  align_header_row(ctx->card, ctx->location_label, ctx->icon_label);
}

static void on_overlay_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  (void)e;
}

static void on_close_click(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;
  WeatherPopupContext* ctx = static_cast<WeatherPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->overlay || !ctx->card) return;
  lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

static void on_overlay_delete(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  WeatherPopupContext* ctx = static_cast<WeatherPopupContext*>(lv_event_get_user_data(e));
  if (!ctx) return;
  if (g_weather_popup_ctx == ctx) {
    g_weather_popup_ctx = nullptr;
  }
  delete ctx;
}

static void build_popup_ui(WeatherPopupContext* ctx, const WeatherPopupInit& init) {
  lv_obj_t* overlay = lv_obj_create(lv_layer_top());
  ctx->overlay = overlay;
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* card = lv_obj_create(overlay);
  ctx->card = card;
  lv_obj_set_size(card, kCardWidth, kCardHeight);
  lv_obj_center(card);
  uint32_t card_color = init.bg_color ? init.bg_color : 0x2A2A2A;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, kCardPad, 0);
  lv_obj_set_style_shadow_width(card, 28, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
  lv_obj_set_style_shadow_spread(card, 2, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* location = lv_label_create(card);
  ctx->location_label = location;
  set_label_style(location, lv_color_white(), FONT_TITLE);
  lv_label_set_long_mode(location, LV_LABEL_LONG_DOT);
  lv_obj_set_width(location, LV_PCT(62));
  lv_obj_align(location, LV_ALIGN_TOP_LEFT, 78, 10);

  lv_obj_t* icon = lv_label_create(card);
  ctx->icon_label = icon;
  set_label_style(icon, lv_color_white(), FONT_MDI_ICONS);
  lv_label_set_text(icon, "");
  lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* close_btn = lv_button_create(card);
  lv_obj_set_size(close_btn, 96, 96);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close_btn, 16, 0);
  lv_obj_set_style_pad_all(close_btn, 0, 0);
  lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 12, -12);
  lv_obj_set_ext_click_area(close_btn, 28);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(close_btn, on_close_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(close_btn, on_close_click, LV_EVENT_RELEASED, ctx);
  lv_obj_t* close_label = lv_label_create(close_btn);
  set_label_style(close_label, lv_color_white(), FONT_MDI_ICONS);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);

  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 8, 0);

  lv_obj_t* value_row = lv_obj_create(card);
  ctx->value_row = value_row;
  lv_obj_remove_style_all(value_row);
  lv_obj_set_size(value_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(value_row, 14, 0);
  lv_obj_set_style_bg_opa(value_row, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* condition_label = lv_label_create(value_row);
  ctx->condition_label = condition_label;
  set_label_style(condition_label, lv_color_hex(0xD0D0D0), FONT_TITLE);
  lv_label_set_long_mode(condition_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(condition_label, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(condition_label, 220, 0);
  lv_label_set_text(condition_label, "--");
  lv_obj_add_flag(condition_label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* sep_label = lv_label_create(value_row);
  ctx->condition_sep_label = sep_label;
  set_label_style(sep_label, lv_color_hex(0xB0B0B0), FONT_TITLE);
  lv_label_set_text(sep_label, "|");
  lv_obj_add_flag(sep_label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* temp_label = lv_label_create(value_row);
  ctx->temp_label = temp_label;
  set_label_style(temp_label, lv_color_white(), FONT_VALUE);
  lv_label_set_text(temp_label, "--");

  lv_obj_update_layout(value_row);
  lv_coord_t row_h = lv_obj_get_height(value_row);
  lv_coord_t base_center = ((kCellH - 48) / 2) + 28;
  lv_coord_t value_row_y = base_center - (row_h / 2) + 18;
  lv_obj_align(value_row, LV_ALIGN_TOP_MID, 0, value_row_y);

  const lv_coord_t forecast_total_w = kCardWidth;
  const lv_coord_t cols_total = kCols * WEATHER_FORECAST_COL_W;
  const lv_coord_t forecast_remaining = forecast_total_w - cols_total;
  const lv_coord_t forecast_spacing = forecast_remaining / (kCols + 1);
  const lv_coord_t forecast_col_w = WEATHER_FORECAST_COL_W;

  lv_obj_t* forecast_row = lv_obj_create(card);
  lv_obj_remove_style_all(forecast_row);
  lv_obj_set_size(forecast_row, forecast_total_w, GRID_CELL_H);
  lv_obj_set_style_bg_opa(forecast_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(forecast_row, 0, 0);
  lv_obj_remove_flag(forecast_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(forecast_row, -kCardPad, kCellH + kForecastGap + 18);

  lv_obj_move_foreground(icon);
  lv_obj_move_foreground(location);
  lv_obj_move_foreground(close_btn);

  for (int i = 0; i < kCols; ++i) {
    lv_obj_t* col = lv_obj_create(forecast_row);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, forecast_col_w, GRID_CELL_H);
    lv_obj_set_style_pad_hor(col, kCardPad, 0);
    lv_obj_set_style_pad_ver(col, 24, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(col, forecast_spacing + i * (forecast_col_w + forecast_spacing), 0);

    lv_obj_t* day = lv_label_create(col);
    set_label_style(day, lv_color_white(), FONT_TITLE);
    lv_label_set_long_mode(day, LV_LABEL_LONG_DOT);
    lv_obj_set_width(day, LV_PCT(70));
    lv_label_set_text(day, "--");
    lv_obj_align(day, LV_ALIGN_TOP_LEFT, 4, 4);

    lv_obj_t* icon_day = lv_label_create(col);
    set_label_style(icon_day, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(icon_day, "");
    lv_obj_add_flag(icon_day, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(icon_day, LV_ALIGN_TOP_RIGHT, -4, -8);

    lv_obj_t* temp = lv_label_create(col);
    set_label_style(temp, lv_color_white(), &ui_font_24);
    lv_label_set_long_mode(temp, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(temp, LV_PCT(100));
    lv_obj_set_style_text_align(temp, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(temp, 8, 0);
    lv_label_set_text(temp, "--\n--");
    lv_obj_align(temp, LV_ALIGN_CENTER, 0, 28);

    ctx->forecast[i].day_label = day;
    ctx->forecast[i].icon_label = icon_day;
    ctx->forecast[i].temp_label = temp;
  }

  apply_init_to_context(ctx, init);

  lv_obj_add_event_cb(overlay, on_overlay_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
}

}  // namespace

void show_weather_popup(const WeatherPopupInit& init) {
  if (!init.entity_id.length()) return;

  hide_light_popup();
  hide_sensor_popup();
  hide_image_popup();
  hide_radar_popup();

  if (g_weather_popup_ctx && g_weather_popup_ctx->overlay && g_weather_popup_ctx->card) {
    apply_init_to_context(g_weather_popup_ctx, init);
    lv_obj_clear_flag(g_weather_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_weather_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  } else {
    WeatherPopupContext* ctx = new WeatherPopupContext();
    g_weather_popup_ctx = ctx;
    build_popup_ui(ctx, init);
  }

  String cached;
  if (tiles_get_cached_entity_payload(init.entity_id.c_str(), cached)) {
    apply_weather_payload(g_weather_popup_ctx, cached.c_str());
  } else {
    clear_forecast(g_weather_popup_ctx);
  }
}

void preload_weather_popup() {
  if (g_weather_popup_ctx && g_weather_popup_ctx->overlay && g_weather_popup_ctx->card) return;
  WeatherPopupInit init;
  init.entity_id = "__preload__";
  init.title = "";
  init.bg_color = 0;
  show_weather_popup(init);
  if (g_weather_popup_ctx && g_weather_popup_ctx->card && g_weather_popup_ctx->overlay) {
    lv_obj_add_flag(g_weather_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_weather_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  }
}

void hide_weather_popup() {
  if (!g_weather_popup_ctx || !g_weather_popup_ctx->card || !g_weather_popup_ctx->overlay) return;
  lv_obj_add_flag(g_weather_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_weather_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

void queue_weather_popup_payload(const char* entity_id, const char* payload) {
  if (!entity_id || !*entity_id || !payload) return;
  g_pending_weather.entity_id = entity_id;
  g_pending_weather.payload = payload;
  g_pending_weather.valid = true;
}

void process_weather_popup_queue() {
  if (!g_weather_popup_ctx || !g_weather_popup_ctx->card) {
    g_pending_weather.valid = false;
    return;
  }

  if (g_pending_weather.valid) {
    if (g_weather_popup_ctx->entity_id.equalsIgnoreCase(g_pending_weather.entity_id) &&
        is_popup_visible(g_weather_popup_ctx)) {
      apply_weather_payload(g_weather_popup_ctx, g_pending_weather.payload.c_str());
    }
    g_pending_weather.valid = false;
  }
}





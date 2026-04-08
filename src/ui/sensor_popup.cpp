#include "src/ui/sensor_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/weather_popup.h"
#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/types/clock/clock_format.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

namespace {

constexpr int kCardMargin = 4;
constexpr int kCardWidth =
    (SCREEN_WIDTH > SCREEN_HEIGHT)
        ? (SCREEN_HEIGHT - (kCardMargin * 2))
        : (SCREEN_WIDTH - (kCardMargin * 2));
constexpr int kCardHeight = SCREEN_HEIGHT - (kCardMargin * 2);
constexpr int kCardPad = 20;
constexpr int kHeaderPadTop = 4;
constexpr int kHeaderIconOffsetX = 4;
constexpr int kHeaderIconOffsetY = -8;
constexpr int kContentPadTop = 76;
constexpr int kContentRowGap = 15;
constexpr int kChartHeight = 325;
constexpr int kTimeAxisHeight = 20;  // space for time labels below chart
constexpr int kTimeAxisMarkerCount = 8;
constexpr int kChartLineWidth = 4;
constexpr int kChartGroupOffsetY = 63;
constexpr int kRangeRowOffsetY = 136;
constexpr int kValueOffsetY = 31;
constexpr uint16_t kHistoryHours24h = 24;
constexpr uint16_t kHistoryPeriodMinutes24h = 5;
constexpr uint16_t kHistoryPoints24h = 288;
constexpr uint16_t kHistoryHours7d = 168;
constexpr uint16_t kHistoryPeriodMinutes7d = 60;
constexpr uint16_t kHistoryPoints7d = 168;
constexpr int kRangeButtonWidth = 96;
constexpr int kRangeButtonHeight = 64;
constexpr int kRangeButtonGap = 12;

enum class SensorHistoryRange : uint8_t {
  Day24,
  Day7,
};

struct HistoryRangeConfig {
  uint16_t hours;
  uint16_t period_minutes;
  uint16_t points;
};

struct SensorPopupContext {
  String entity_id;
  String unit;
  bool lock_unit = false;
  uint8_t decimals = 0xFF;
  uint32_t bg_color = 0;
  SensorHistoryRange history_range = SensorHistoryRange::Day24;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* value_label = nullptr;
  lv_obj_t* range_row = nullptr;
  lv_obj_t* range_day_btn = nullptr;
  lv_obj_t* range_week_btn = nullptr;
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
  lv_obj_t* y_max_label = nullptr;
  lv_obj_t* y_min_label = nullptr;
  lv_obj_t* y_max_line = nullptr;
  lv_obj_t* y_min_line = nullptr;
  lv_obj_t* time_labels[kTimeAxisMarkerCount] = {};
  lv_obj_t* time_lines[kTimeAxisMarkerCount] = {};
  lv_obj_t* chart_wrap = nullptr;
  uint16_t point_count = kHistoryPoints24h;
};

struct PendingValueUpdate {
  String entity_id;
  String value;
  String unit;
  uint8_t decimals = 0xFF;
  bool valid = false;
};

struct PendingHistoryUpdate {
  String entity_id;
  String payload;
  bool valid = false;
};

static SensorPopupContext* g_sensor_popup_ctx = nullptr;
static PendingValueUpdate g_pending_value;
static PendingHistoryUpdate g_pending_history;

static HistoryRangeConfig get_history_range_config(SensorHistoryRange range) {
  switch (range) {
    case SensorHistoryRange::Day7:
      return {kHistoryHours7d, kHistoryPeriodMinutes7d, kHistoryPoints7d};
    case SensorHistoryRange::Day24:
    default:
      return {kHistoryHours24h, kHistoryPeriodMinutes24h, kHistoryPoints24h};
  }
}

static const char* get_weekday_abbrev(uint8_t wday, bool german) {
  static constexpr const char* kWeekdaysEn[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static constexpr const char* kWeekdaysDe[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  if (wday > 6) wday = 0;
  return german ? kWeekdaysDe[wday] : kWeekdaysEn[wday];
}

static const lv_font_t* get_value_font() {
  return &ui_font_40;
}

static void set_label_style(lv_obj_t* lbl, lv_color_t color, const lv_font_t* font) {
  if (!lbl) return;
  lv_obj_set_style_text_color(lbl, color, 0);
  if (font) {
    lv_obj_set_style_text_font(lbl, font, 0);
  }
}

static bool is_popup_visible(SensorPopupContext* ctx) {
  if (!ctx || !ctx->card) return false;
  return !lv_obj_has_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
}

static bool apply_decimals(String& value, uint8_t decimals) {
  if (decimals == 0xFF) return false;
  String normalized = value;
  normalized.replace(",", ".");
  char* end = nullptr;
  float f = strtof(normalized.c_str(), &end);
  if (!end || end == normalized.c_str()) return false;
  if (isnan(f) || isinf(f)) return false;
  uint8_t d = decimals > 6 ? 6 : decimals;
  value = String(f, static_cast<unsigned int>(d));
  return true;
}

static void update_value_label(SensorPopupContext* ctx, const String& value, const String& unit) {
  if (!ctx || !ctx->value_label) return;
  String display = value;
  display.trim();
  String lower = display;
  lower.toLowerCase();
  if (lower == "unavailable" || lower == "unknown" || lower == "none" || lower == "null") {
    display = "--";
  }
  if (display.isEmpty()) {
    display = "--";
  }
  if (display.length() > 0 && display != "--" && !display.equalsIgnoreCase("unavailable")) {
    apply_decimals(display, ctx->decimals);
  }
  String display_unit = unit;
  display_unit.trim();
  if (!display_unit.isEmpty() && display != "--") {
    display += " ";
    display += display_unit;
  }
  lv_label_set_text(ctx->value_label, display.c_str());
  ctx->unit = display_unit;
}

static void style_range_button(lv_obj_t* btn, bool active) {
  if (!btn) return;
  lv_color_t active_text_color = lv_color_hex(0x2A2A2A);
  lv_obj_t* row = lv_obj_get_parent(btn);
  lv_obj_t* card = row ? lv_obj_get_parent(row) : nullptr;
  if (card) {
    active_text_color = lv_obj_get_style_bg_color(card, LV_PART_MAIN);
  }
  auto apply_selector = [&](lv_style_selector_t selector) {
    lv_obj_set_style_bg_color(btn, active ? lv_color_white() : lv_color_hex(0xFFFFFF), selector);
    lv_obj_set_style_bg_opa(btn, active ? LV_OPA_COVER : LV_OPA_TRANSP, selector);
    lv_obj_set_style_border_color(btn, lv_color_white(), selector);
    lv_obj_set_style_border_width(btn, 2, selector);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, selector);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, selector);
    lv_obj_set_style_transform_width(btn, 0, selector);
    lv_obj_set_style_transform_height(btn, 0, selector);
    lv_obj_set_style_translate_y(btn, 0, selector);
  };

  apply_selector(0);
  apply_selector(LV_STATE_PRESSED);

  lv_obj_t* label = lv_obj_get_child(btn, 0);
  if (label) {
    lv_obj_set_style_text_color(label, active ? active_text_color : lv_color_white(), 0);
    lv_obj_set_style_text_color(label, active ? active_text_color : lv_color_white(), LV_STATE_PRESSED);
  }
}

static void update_range_buttons(SensorPopupContext* ctx) {
  if (!ctx) return;
  style_range_button(ctx->range_day_btn, ctx->history_range == SensorHistoryRange::Day24);
  style_range_button(ctx->range_week_btn, ctx->history_range == SensorHistoryRange::Day7);
}

static void set_range_buttons_visible(SensorPopupContext* ctx, bool visible) {
  if (!ctx || !ctx->range_row) return;
  if (visible) {
    lv_obj_clear_flag(ctx->range_row, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ctx->range_row, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool extract_numeric(JsonVariant v, float& out) {
  if (v.isNull()) return false;

  if (v.is<const char*>()) {
    const char* text = v.as<const char*>();
    if (!text || !*text) return false;
    char* end = nullptr;
    out = strtof(text, &end);
    if (end == text) return false;
    return isfinite(out);
  }

  if (v.is<float>() || v.is<double>() || v.is<int>() || v.is<long>() ||
      v.is<unsigned long>() || v.is<long long>() || v.is<unsigned long long>()) {
    out = v.as<float>();
    return isfinite(out);
  }

  return false;
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

static void apply_init_to_context(SensorPopupContext* ctx, const SensorPopupInit& init) {
  if (!ctx) return;
  ctx->entity_id = init.entity_id;
  ctx->lock_unit = init.lock_unit;
  ctx->decimals = init.decimals;
  ctx->bg_color = init.bg_color;
  if (ctx->card) {
    uint32_t color = ctx->bg_color ? ctx->bg_color : 0x2A2A2A;
    lv_obj_set_style_bg_color(ctx->card, lv_color_hex(color), 0);
  }
  if (ctx->title_label) {
    lv_label_set_text(ctx->title_label, init.title.c_str());
  }
  if (ctx->icon_label) {
    String icon_name = init.icon_name;
    icon_name.trim();
    if (!icon_name.length() || isMdiIconDisabled(icon_name)) {
      lv_label_set_text(ctx->icon_label, "");
      lv_obj_add_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      String icon_char = getMdiChar(icon_name);
      if (!icon_char.isEmpty()) {
        lv_label_set_text(ctx->icon_label, icon_char.c_str());
        lv_obj_clear_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(ctx->icon_label, "");
        lv_obj_add_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
  align_header_row(ctx->card, ctx->title_label, ctx->icon_label);
  update_value_label(ctx, init.value, init.unit);
}

// Measure actual rendered text width of a label by temporarily setting it to content-size.
static lv_coord_t measure_label_text_width(lv_obj_t* label) {
  if (!label) return 0;
  const char* txt = lv_label_get_text(label);
  if (!txt || !*txt) return 0;
  lv_obj_set_width(label, LV_SIZE_CONTENT);
  lv_obj_update_layout(label);
  return lv_obj_get_width(label);
}

static bool get_valid_local_time(struct tm& out) {
  if (!getLocalTime(&out, 0)) return false;
  const int year = out.tm_year + 1900;
  const int month = out.tm_mon + 1;
  const int day = out.tm_mday;
  if (year < 2024 || year > 2100) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  return true;
}

static String format_time_axis_label(int hour24) {
  int normalized = hour24 % 24;
  if (normalized < 0) normalized += 24;
  const DeviceConfig& cfg = configManager.getConfig();
  const uint8_t time_format =
      clock_tile::resolve_time_format(clock_tile::TIME_FORMAT_AUTO, cfg.global_time_format, cfg.language);
  if (time_format == clock_tile::TIME_FORMAT_24H) {
    const bool is_de = clock_tile::language_prefers_german_locale(cfg.language);
    if (is_de) {
      return String(normalized) + " Uhr";
    }
    return String(normalized) + ":00";
  }

  int hour12 = normalized % 12;
  if (hour12 == 0) hour12 = 12;
  return String(hour12) + (normalized < 12 ? " AM" : " PM");
}

static String format_day_axis_label(time_t ts) {
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &ts);
#else
  localtime_r(&ts, &timeinfo);
#endif
  const bool is_de = configManager.getConfig().language[0] == 'd';
  return String(get_weekday_abbrev(static_cast<uint8_t>(timeinfo.tm_wday), is_de));
}

static bool get_day7_axis_base(time_t& now_local, time_t& range_start, struct tm& today_midnight_tm) {
  struct tm now_tm;
  if (!get_valid_local_time(now_tm)) return false;

  now_local = mktime(&now_tm);
  if (now_local <= 0) return false;

  range_start = now_local - static_cast<time_t>(kHistoryHours7d) * 60 * 60;
  today_midnight_tm = now_tm;
  today_midnight_tm.tm_hour = 0;
  today_midnight_tm.tm_min = 0;
  today_midnight_tm.tm_sec = 0;
  return true;
}

static time_t next_local_midnight_after(time_t ts) {
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &ts);
#else
  localtime_r(&ts, &timeinfo);
#endif
  timeinfo.tm_hour = 0;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  timeinfo.tm_mday += 1;
  return mktime(&timeinfo);
}

static time_t next_local_6h_boundary_after(time_t ts) {
  struct tm timeinfo;
#ifdef _WIN32
  localtime_s(&timeinfo, &ts);
#else
  localtime_r(&ts, &timeinfo);
#endif

  const int current_hour = timeinfo.tm_hour;
  int next_hour = ((current_hour / 6) + 1) * 6;
  if (next_hour >= 24) {
    next_hour -= 24;
    timeinfo.tm_mday += 1;
  }

  timeinfo.tm_hour = next_hour;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  return mktime(&timeinfo);
}

static int calc_day24_axis(String out_labels[], float out_frac[], int max_markers) {
  struct tm now_tm;
  if (!get_valid_local_time(now_tm)) return 0;

  time_t now_local = mktime(&now_tm);
  if (now_local <= 0) return 0;

  const time_t range_start = now_local - static_cast<time_t>(kHistoryHours24h) * 60 * 60;
  const float span = static_cast<float>(now_local - range_start);
  if (span <= 0.0f) return 0;

  int count = 0;
  time_t boundary = next_local_6h_boundary_after(range_start);
  while (boundary < now_local && count < max_markers) {
    struct tm boundary_tm;
#ifdef _WIN32
    localtime_s(&boundary_tm, &boundary);
#else
    localtime_r(&boundary, &boundary_tm);
#endif
    out_labels[count] = format_time_axis_label(boundary_tm.tm_hour);
    out_frac[count] = static_cast<float>(boundary - range_start) / span;
    ++count;
    boundary = next_local_6h_boundary_after(boundary);
  }

  return count;
}

static int calc_day7_label_axis(String out_labels[], float out_frac[], int max_markers) {
  time_t now_local = 0;
  time_t range_start = 0;
  struct tm today_midnight_tm;
  if (!get_day7_axis_base(now_local, range_start, today_midnight_tm)) return 0;

  const float span = static_cast<float>(now_local - range_start);
  if (span <= 0.0f) return 0;

  int count = 0;
  time_t segment_start = range_start;
  while (segment_start < now_local && count < max_markers) {
    time_t segment_end = next_local_midnight_after(segment_start);
    if (segment_end > now_local) segment_end = now_local;
    if (segment_end <= segment_start) break;

    const float center = static_cast<float>(segment_start - range_start) +
                         (static_cast<float>(segment_end - segment_start) * 0.5f);
    out_labels[count] = format_day_axis_label(segment_start);
    out_frac[count] = center / span;
    ++count;
    segment_start = segment_end;
  }

  return count;
}

static int calc_day7_boundary_axis(float out_frac[], int max_markers) {
  time_t now_local = 0;
  time_t range_start = 0;
  struct tm today_midnight_tm;
  if (!get_day7_axis_base(now_local, range_start, today_midnight_tm)) return 0;

  const float span = static_cast<float>(now_local - range_start);
  if (span <= 0.0f) return 0;

  int count = 0;
  time_t boundary = next_local_midnight_after(range_start);
  while (boundary < now_local && count < max_markers) {
    if (boundary > range_start) {
      out_frac[count] = static_cast<float>(boundary - range_start) / span;
      ++count;
    }
    boundary = next_local_midnight_after(boundary);
  }

  return count;
}

// Calculate time axis marker positions and labels.
static int calc_time_axis(const SensorPopupContext* ctx, String out_labels[], float out_frac[], int max_markers) {
  if (!ctx || max_markers < 1) return 0;

  if (ctx->history_range == SensorHistoryRange::Day7) {
    return calc_day7_label_axis(out_labels, out_frac, max_markers);
  }

  return calc_day24_axis(out_labels, out_frac, max_markers);
}

// Recalculate Y-axis layout based on actual label text widths.
// Measures max/min labels, repositions guide lines and chart padding.
static void update_y_axis_layout(SensorPopupContext* ctx) {
  if (!ctx || !ctx->chart || !ctx->chart_wrap) return;
  constexpr int kLineOverlap = 6;
  constexpr int kLabelGap = 16;   // gap between label right edge and guide line start
  constexpr int kMinAxisW = 10;   // minimum width even if labels empty
  constexpr int kTimeAxisEdgeGap = 6;
  const int kAvailW = kCardWidth - (kCardPad * 2);

  // Measure actual rendered text widths
  lv_coord_t max_w = measure_label_text_width(ctx->y_max_label);
  lv_coord_t min_w = measure_label_text_width(ctx->y_min_label);

  lv_coord_t text_w = LV_MAX(max_w, min_w);
  if (text_w < kMinAxisW) text_w = kMinAxisW;
  int label_w = text_w + 2;  // text width + small safety margin
  int axis_w = label_w + kLabelGap;  // label + visible gap before lines

  // Reposition labels â€” width must fit full text
  if (ctx->y_max_label) {
    lv_obj_set_width(ctx->y_max_label, label_w);
  }
  if (ctx->y_min_label) {
    lv_obj_set_width(ctx->y_min_label, label_w);
  }

  // Chart drawing starts directly after label area
  int chart_left = axis_w;

  // Prepare time axis labels first so we can reserve enough room on the right.
  constexpr int kLabelOverhang = 12;

  String labels[kTimeAxisMarkerCount];
  float fracs[kTimeAxisMarkerCount];
  int n = calc_time_axis(ctx, labels, fracs, kTimeAxisMarkerCount);
  lv_coord_t max_time_label_w = 0;

  for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
    if (i < n && ctx->time_labels[i]) {
      lv_label_set_text(ctx->time_labels[i], labels[i].c_str());
      lv_obj_update_layout(ctx->time_labels[i]);
      lv_coord_t lbl_w = lv_obj_get_width(ctx->time_labels[i]);
      if (lbl_w > max_time_label_w) max_time_label_w = lbl_w;
    }
  }

  int chart_right_reserve = (max_time_label_w > 0) ? (max_time_label_w / 2 + kTimeAxisEdgeGap) : 0;

  // Reposition guide lines (keep Y unchanged, only adjust X and width)
  int line_start = axis_w - kLineOverlap;
  int line_w = kAvailW - line_start - chart_right_reserve;
  if (line_w < 10) line_w = 10;
  if (ctx->y_max_line) {
    lv_obj_set_width(ctx->y_max_line, line_w);
    lv_obj_set_x(ctx->y_max_line, line_start);
  }
  if (ctx->y_min_line) {
    lv_obj_set_width(ctx->y_min_line, line_w);
    lv_obj_set_x(ctx->y_min_line, line_start);
  }

  // Adjust chart padding so the graph itself ends slightly earlier on the right.
  lv_obj_set_style_pad_left(ctx->chart, chart_left, 0);
  lv_obj_set_style_pad_right(ctx->chart, chart_right_reserve, 0);

  // Reposition time axis markers based on the reduced chart area.
  int chart_draw_w = kAvailW - chart_left - chart_right_reserve;
  if (chart_draw_w < 10) return;

  const bool is_week_range = ctx->history_range == SensorHistoryRange::Day7;
  float week_boundary_fracs[kTimeAxisMarkerCount] = {};
  int week_boundary_count = 0;
  if (is_week_range) {
    week_boundary_count = calc_day7_boundary_axis(week_boundary_fracs, kTimeAxisMarkerCount);
  }

  for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
    if (i < n) {
      int label_x_anchor = chart_left + static_cast<int>(fracs[i] * chart_draw_w);
      int line_x = label_x_anchor;
      if (is_week_range) {
        if (i < week_boundary_count) {
          line_x = chart_left + static_cast<int>(week_boundary_fracs[i] * chart_draw_w);
        }
      }
      if (ctx->time_lines[i]) {
        if (is_week_range && i >= week_boundary_count) {
          lv_obj_add_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_obj_set_x(ctx->time_lines[i], line_x);
          lv_obj_clear_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
        }
      }
      if (ctx->time_labels[i]) {
        lv_coord_t lbl_w = lv_obj_get_width(ctx->time_labels[i]);
        int label_x = label_x_anchor - lbl_w / 2;
        int min_x = chart_left - (lbl_w / 2);
        if (min_x < 0) min_x = 0;
        int max_x = kAvailW - lbl_w;
        if (max_x < min_x) max_x = min_x;
        if (label_x < min_x) label_x = min_x;
        if (label_x > max_x) label_x = max_x;
        lv_obj_set_pos(ctx->time_labels[i], label_x, kLabelOverhang + kChartHeight + 8);
        lv_obj_clear_flag(ctx->time_labels[i], LV_OBJ_FLAG_HIDDEN);
      }
    } else {
      if (ctx->time_lines[i]) lv_obj_add_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
      if (ctx->time_labels[i]) lv_obj_add_flag(ctx->time_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void clear_chart(SensorPopupContext* ctx, uint16_t points) {
  if (!ctx || !ctx->chart || !ctx->series) return;
  ctx->point_count = points;
  lv_chart_set_point_count(ctx->chart, points);
  lv_chart_set_all_value(ctx->chart, ctx->series, LV_CHART_POINT_NONE);
  lv_chart_refresh(ctx->chart);
  if (ctx->y_max_label) lv_label_set_text(ctx->y_max_label, "");
  if (ctx->y_min_label) lv_label_set_text(ctx->y_min_label, "");
  if (ctx->y_max_line) lv_obj_add_flag(ctx->y_max_line, LV_OBJ_FLAG_HIDDEN);
  if (ctx->y_min_line) lv_obj_add_flag(ctx->y_min_line, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
    if (ctx->time_lines[i]) lv_obj_add_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
    if (ctx->time_labels[i]) lv_obj_add_flag(ctx->time_labels[i], LV_OBJ_FLAG_HIDDEN);
  }
}

static void apply_history_payload(SensorPopupContext* ctx, const char* payload) {
  if (!ctx || !payload || !*payload) return;

  DynamicJsonDocument doc(12288);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[SensorPopup] History JSON error: %s\n", err.c_str());
    return;
  }

  const char* entity = doc["entity_id"] | "";
  if (!ctx->entity_id.equalsIgnoreCase(entity)) {
    return;
  }

  if (!ctx->lock_unit && doc.containsKey("unit")) {
    String unit = String(doc["unit"].as<const char*>());
    unit.trim();
    if (!unit.isEmpty()) {
      ctx->unit = unit;
    }
  }

  const HistoryRangeConfig range_cfg = get_history_range_config(ctx->history_range);
  const uint16_t payload_hours = doc["hours"] | range_cfg.hours;
  const uint16_t payload_period_minutes = doc["period_minutes"] | range_cfg.period_minutes;
  if (payload_hours != range_cfg.hours || payload_period_minutes != range_cfg.period_minutes) {
    return;
  }

  if (doc.containsKey("current")) {
    String current = String(doc["current"].as<const char*>());
    update_value_label(ctx, current, ctx->unit);
  }

  JsonArray values = doc["values"].as<JsonArray>();
  if (values.isNull()) {
    return;
  }

  size_t count = values.size();
  if (count == 0) {
    set_range_buttons_visible(ctx, false);
    clear_chart(ctx, range_cfg.points);
    return;
  }

  uint16_t points = static_cast<uint16_t>(count);
  clear_chart(ctx, points);

  std::vector<float> plot_values(count, NAN);
  for (size_t i = 0; i < count; ++i) {
    JsonVariant v = values[i];
    float val = 0.0f;
    if (extract_numeric(v, val)) {
      plot_values[i] = val;
    }
  }

  // Luecken wie in HA schliessen: fehlende Buckets mit letztem gueltigen
  // Wert auffuellen (inkl. Prefix mit erstem gueltigen Wert).
  size_t first_numeric = count;
  for (size_t i = 0; i < count; ++i) {
    if (isfinite(plot_values[i])) {
      first_numeric = i;
      break;
    }
  }
  if (first_numeric < count) {
    float last = plot_values[first_numeric];
    for (size_t i = 0; i < first_numeric; ++i) {
      plot_values[i] = last;
    }
    for (size_t i = first_numeric + 1; i < count; ++i) {
      if (!isfinite(plot_values[i])) {
        plot_values[i] = last;
      } else {
        last = plot_values[i];
      }
    }
  }

  bool has_range = false;
  float min_v = 0.0f;
  float max_v = 0.0f;
  size_t numeric_count = 0;

  for (float val : plot_values) {
    if (!isfinite(val)) continue;
    ++numeric_count;
    if (!has_range) {
      min_v = max_v = val;
      has_range = true;
    } else {
      if (val < min_v) min_v = val;
      if (val > max_v) max_v = val;
    }
  }

  set_range_buttons_visible(ctx, numeric_count > 1);

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

  for (size_t i = 0; i < count; ++i) {
    float val = plot_values[i];
    if (!isfinite(val)) {
      lv_chart_set_value_by_id(ctx->chart, ctx->series, static_cast<uint16_t>(i), LV_CHART_POINT_NONE);
      continue;
    }
    lv_chart_set_value_by_id(
      ctx->chart,
      ctx->series,
      static_cast<uint16_t>(i),
      static_cast<lv_coord_t>(lroundf(val * scale))
    );
  }

  // Bei genau einem Messpunkt gibt es noch keine Linie; deshalb Punkt sichtbar machen.
  if (numeric_count <= 1) {
    lv_obj_set_style_size(ctx->chart, 8, 8, LV_PART_INDICATOR);
  } else {
    lv_obj_set_style_size(ctx->chart, 0, 0, LV_PART_INDICATOR);
  }

  if (has_range && ctx->chart) {
    if (min_v == max_v) {
      min_v -= 1.0f;
      max_v += 1.0f;
    }
    lv_chart_set_range(
      ctx->chart,
      LV_CHART_AXIS_PRIMARY_Y,
      static_cast<lv_coord_t>(floorf(min_v * scale)),
      static_cast<lv_coord_t>(ceilf(max_v * scale))
    );
  }
  lv_chart_refresh(ctx->chart);

  // Update Y-axis labels with min/max values
  if (has_range) {
    auto format_y = [](float val, uint8_t decimals) -> String {
      if (decimals != 0xFF && decimals <= 6) {
        if (decimals == 0) return String(static_cast<int>(roundf(val)));
        return String(val, static_cast<unsigned int>(decimals));
      }
      // Auto: use integer if close to whole number, else 1 decimal
      if (fabsf(val - roundf(val)) < 0.05f) {
        return String(static_cast<int>(roundf(val)));
      }
      return String(val, 1u);
    };
    String unit_suffix = "";
    if (ctx->unit.length()) {
      unit_suffix = " " + ctx->unit;
    }
    if (ctx->y_max_label) {
      lv_label_set_text(ctx->y_max_label, (format_y(max_v, ctx->decimals) + unit_suffix).c_str());
    }
    if (ctx->y_min_label) {
      lv_label_set_text(ctx->y_min_label, (format_y(min_v, ctx->decimals) + unit_suffix).c_str());
    }
    update_y_axis_layout(ctx);
    if (ctx->y_max_line) lv_obj_clear_flag(ctx->y_max_line, LV_OBJ_FLAG_HIDDEN);
    if (ctx->y_min_line) lv_obj_clear_flag(ctx->y_min_line, LV_OBJ_FLAG_HIDDEN);
  } else {
    if (ctx->y_max_label) lv_label_set_text(ctx->y_max_label, "");
    if (ctx->y_min_label) lv_label_set_text(ctx->y_min_label, "");
    if (ctx->y_max_line) lv_obj_add_flag(ctx->y_max_line, LV_OBJ_FLAG_HIDDEN);
    if (ctx->y_min_line) lv_obj_add_flag(ctx->y_min_line, LV_OBJ_FLAG_HIDDEN);
    // No valid range â†’ hide time axis too
    for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
      if (ctx->time_lines[i]) lv_obj_add_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
      if (ctx->time_labels[i]) lv_obj_add_flag(ctx->time_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static bool should_request_history(const String& entity_id) {
  return entity_id.length() && !entity_id.startsWith("__");
}

static void request_history_for_context(SensorPopupContext* ctx) {
  if (!ctx || !should_request_history(ctx->entity_id)) return;
  const HistoryRangeConfig cfg = get_history_range_config(ctx->history_range);
  mqttPublishHistoryRequest(ctx->entity_id.c_str(), cfg.hours, cfg.period_minutes, cfg.points);
}

static void on_overlay_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  (void)e;
}

static void on_close_click(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;
  SensorPopupContext* ctx = static_cast<SensorPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->overlay || !ctx->card) return;
  lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

static void on_overlay_delete(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  SensorPopupContext* ctx = static_cast<SensorPopupContext*>(lv_event_get_user_data(e));
  if (!ctx) return;
  if (g_sensor_popup_ctx == ctx) {
    g_sensor_popup_ctx = nullptr;
  }
  delete ctx;
}

static void on_range_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  SensorPopupContext* ctx = static_cast<SensorPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!ctx || !target) return;

  SensorHistoryRange next_range =
      (target == ctx->range_week_btn) ? SensorHistoryRange::Day7 : SensorHistoryRange::Day24;
  if (ctx->history_range == next_range) return;

  ctx->history_range = next_range;
  update_range_buttons(ctx);
  clear_chart(ctx, get_history_range_config(ctx->history_range).points);
  request_history_for_context(ctx);
}

static void build_popup_ui(SensorPopupContext* ctx, const SensorPopupInit& init) {
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

  lv_obj_t* title = lv_label_create(card);
  ctx->title_label = title;
  set_label_style(title, lv_color_white(), &ui_font_20);
  lv_label_set_text(title, init.title.c_str());
  lv_obj_set_width(title, LV_PCT(62));
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 78, 10);

  lv_obj_t* icon = lv_label_create(card);
  ctx->icon_label = icon;
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);

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
  lv_obj_set_style_text_font(close_label, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);

  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 8, 0);

  lv_obj_t* range_row = lv_obj_create(card);
  ctx->range_row = range_row;
  lv_obj_remove_style_all(range_row);
  lv_obj_set_size(range_row, (kRangeButtonWidth * 2) + kRangeButtonGap, kRangeButtonHeight);
  lv_obj_align(range_row, LV_ALIGN_TOP_RIGHT, -4, kRangeRowOffsetY);
  lv_obj_set_style_bg_opa(range_row, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(range_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(range_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(range_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(range_row, kRangeButtonGap, 0);
  lv_obj_clear_flag(range_row, LV_OBJ_FLAG_CLICKABLE);

  auto make_range_button = [&](const char* text) -> lv_obj_t* {
    lv_obj_t* btn = lv_button_create(range_row);
    lv_obj_set_size(btn, kRangeButtonWidth, kRangeButtonHeight);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_anim_time(btn, 0, 0);
    lv_obj_set_style_anim_time(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 0, 0);
    lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, 0);
    lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(btn, 0, 0);
    lv_obj_set_style_translate_y(btn, 0, LV_STATE_PRESSED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_t* label = lv_label_create(btn);
    set_label_style(label, lv_color_white(), &ui_font_28);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
  };

  ctx->range_day_btn = make_range_button("24H");
  ctx->range_week_btn = make_range_button("7D");
  lv_obj_add_event_cb(ctx->range_day_btn, on_range_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->range_week_btn, on_range_click, LV_EVENT_CLICKED, ctx);
  update_range_buttons(ctx);
  set_range_buttons_visible(ctx, false);

  lv_obj_t* content = lv_obj_create(card);
  lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
  lv_obj_align(content, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_set_style_pad_top(content, kContentPadTop, 0);
  lv_obj_set_layout(content, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_bottom(content, 24, 0);
  lv_obj_set_style_pad_row(content, kContentRowGap, 0);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* value = lv_label_create(content);
  ctx->value_label = value;
  set_label_style(value, lv_color_white(), get_value_font());
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(value, LV_PCT(100));
  lv_obj_set_style_translate_y(value, kValueOffsetY, 0);

  // Chart wrapper: Y-axis labels on the left, chart on the right, time labels below.
  // Extra vertical space (kLabelOverhang) at top/bottom so labels don't get clipped.
  constexpr int kLabelOverhang = 12;  // half of ui_font_20 line height + margin
  lv_obj_t* chart_wrap = lv_obj_create(content);
  ctx->chart_wrap = chart_wrap;
  lv_obj_remove_style_all(chart_wrap);
  lv_obj_set_size(chart_wrap, LV_PCT(100), kChartHeight + 2 * kLabelOverhang + kTimeAxisHeight);
  lv_obj_set_style_margin_top(chart_wrap, kChartGroupOffsetY, 0);
  lv_obj_set_style_bg_opa(chart_wrap, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(chart_wrap, LV_OBJ_FLAG_SCROLLABLE);

  // Layout: [labels] [chart fills rest]
  constexpr int kYAxisWidth = 48;
  const int kChartLeft = kYAxisWidth;
  const lv_font_t* y_font = &ui_font_20;

  // Max label: vertically centered on top guide line (at y = kLabelOverhang)
  lv_obj_t* y_max = lv_label_create(chart_wrap);
  ctx->y_max_label = y_max;
  set_label_style(y_max, lv_color_white(), y_font);
  lv_obj_set_style_text_align(y_max, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(y_max, kYAxisWidth - 10);
  lv_label_set_text(y_max, "");
  lv_obj_set_pos(y_max, 0, 0);  // font center ~kLabelOverhang = on top guide line

  // Min label: vertically centered on bottom guide line (at y = kLabelOverhang + kChartHeight - 1)
  lv_obj_t* y_min = lv_label_create(chart_wrap);
  ctx->y_min_label = y_min;
  set_label_style(y_min, lv_color_white(), y_font);
  lv_obj_set_style_text_align(y_min, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(y_min, kYAxisWidth - 10);
  lv_label_set_text(y_min, "");
  lv_obj_set_pos(y_min, 0, kChartHeight);  // font center ~kChartHeight+kLabelOverhang = on bottom guide line

  // Horizontal guide lines at max/min â€” extend 6px left of Y-axis for label alignment
  constexpr int kLineOverlap = 6;
  const int kLineStart = kYAxisWidth - kLineOverlap;
  const int kLineWidth = kCardWidth - (kCardPad * 2) - kLineStart;
  auto make_guide_line = [&](int16_t y_pos) -> lv_obj_t* {
    lv_obj_t* line = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, kLineWidth, 1);
    lv_obj_set_style_bg_color(line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_30, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(line, kLineStart, y_pos);
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    return line;
  };
  ctx->y_max_line = make_guide_line(kLabelOverhang);
  ctx->y_min_line = make_guide_line(kLabelOverhang + kChartHeight - 1);

  // Vertical time marker lines (thin, same opacity as horizontal guides)
  // Created hidden; positioned and shown in apply_history_payload.
  for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
    lv_obj_t* vline = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(vline);
    lv_obj_set_size(vline, 1, kChartHeight);
    lv_obj_set_style_bg_color(vline, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(vline, LV_OPA_30, 0);
    lv_obj_remove_flag(vline, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(vline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(vline, 0, kLabelOverhang);
    lv_obj_add_flag(vline, LV_OBJ_FLAG_HIDDEN);
    ctx->time_lines[i] = vline;

    lv_obj_t* tlbl = lv_label_create(chart_wrap);
    set_label_style(tlbl, lv_color_white(), &ui_font_20);
    lv_obj_set_style_text_align(tlbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(tlbl, LV_SIZE_CONTENT);
    lv_label_set_text(tlbl, "");
    lv_obj_set_pos(tlbl, 0, kLabelOverhang + kChartHeight + 2);
    lv_obj_add_flag(tlbl, LV_OBJ_FLAG_HIDDEN);
    ctx->time_labels[i] = tlbl;
  }

  lv_obj_t* chart = lv_chart_create(chart_wrap);
  ctx->chart = chart;
  lv_obj_set_size(chart, LV_PCT(100), kChartHeight);
  lv_obj_set_style_pad_left(chart, kChartLeft, 0);
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_pad_top(chart, 0, 0);
  lv_obj_set_style_pad_right(chart, 0, 0);
  lv_obj_set_style_pad_bottom(chart, 0, 0);
  lv_obj_set_pos(chart, 0, kLabelOverhang);
  lv_chart_set_div_line_count(chart, 0, 0);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_obj_set_style_line_width(chart, kChartLineWidth, LV_PART_ITEMS);
  lv_obj_set_style_line_color(chart, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(chart, true, LV_PART_ITEMS);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);

  ctx->series = lv_chart_add_series(chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);
  clear_chart(ctx, get_history_range_config(ctx->history_range).points);

  lv_obj_move_foreground(icon);
  lv_obj_move_foreground(title);
  lv_obj_move_foreground(range_row);
  lv_obj_move_foreground(close_btn);

  apply_init_to_context(ctx, init);

  lv_obj_add_event_cb(overlay, on_overlay_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
}

}  // namespace

void show_sensor_popup(const SensorPopupInit& init) {
  if (!init.entity_id.length()) return;

  // Hide other popups if visible
  hide_light_popup();
  hide_weather_popup();

  if (g_sensor_popup_ctx && g_sensor_popup_ctx->overlay && g_sensor_popup_ctx->card) {
    apply_init_to_context(g_sensor_popup_ctx, init);
    g_sensor_popup_ctx->history_range = SensorHistoryRange::Day24;
    update_range_buttons(g_sensor_popup_ctx);
    clear_chart(g_sensor_popup_ctx, get_history_range_config(g_sensor_popup_ctx->history_range).points);
    lv_obj_clear_flag(g_sensor_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_sensor_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  } else {
    SensorPopupContext* ctx = new SensorPopupContext();
    g_sensor_popup_ctx = ctx;
    build_popup_ui(ctx, init);
  }

  g_pending_history.valid = false;
  set_range_buttons_visible(g_sensor_popup_ctx, false);
  request_history_for_context(g_sensor_popup_ctx);
}

void preload_sensor_popup() {
  if (g_sensor_popup_ctx && g_sensor_popup_ctx->overlay && g_sensor_popup_ctx->card) return;
  SensorPopupInit init;
  init.entity_id = "__preload__";
  init.title = "";
  init.icon_name = "";
  init.value = "";
  init.unit = "";
  init.decimals = 0xFF;
  show_sensor_popup(init);
  if (g_sensor_popup_ctx && g_sensor_popup_ctx->card && g_sensor_popup_ctx->overlay) {
    lv_obj_add_flag(g_sensor_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_sensor_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  }
}

void hide_sensor_popup() {
  if (!g_sensor_popup_ctx || !g_sensor_popup_ctx->card || !g_sensor_popup_ctx->overlay) return;
  lv_obj_add_flag(g_sensor_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_sensor_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

void queue_sensor_popup_value(const char* entity_id, const char* value, const char* unit, uint8_t decimals) {
  if (!entity_id || !*entity_id || !value) return;
  g_pending_value.entity_id = entity_id;
  g_pending_value.value = value;
  g_pending_value.unit = unit ? unit : "";
  g_pending_value.decimals = decimals;
  g_pending_value.valid = true;
}

static String extract_history_entity_id(const String& payload) {
  int key = payload.indexOf("\"entity_id\"");
  if (key < 0) return "";
  int colon = payload.indexOf(':', key);
  if (colon < 0) return "";
  int q1 = payload.indexOf('"', colon + 1);
  if (q1 < 0) return "";
  int q2 = payload.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  String entity = payload.substring(q1 + 1, q2);
  entity.trim();
  return entity;
}

void queue_sensor_popup_history(const char* entity_id, const char* payload, size_t len) {
  if (!payload || len == 0) return;
  if (!g_sensor_popup_ctx || !is_popup_visible(g_sensor_popup_ctx)) return;

  String payload_text = String(payload).substring(0, len);
  String incoming_entity = entity_id ? entity_id : "";
  incoming_entity.trim();
  if (!incoming_entity.length()) {
    incoming_entity = extract_history_entity_id(payload_text);
  }

  // Ignore history updates for other entities while this popup is open.
  if (incoming_entity.length() &&
      !g_sensor_popup_ctx->entity_id.equalsIgnoreCase(incoming_entity)) {
    return;
  }

  g_pending_history.entity_id = incoming_entity;
  g_pending_history.payload = payload_text;
  g_pending_history.valid = true;
}

void process_sensor_popup_queue() {
  if (!g_sensor_popup_ctx || !g_sensor_popup_ctx->card) {
    g_pending_value.valid = false;
    g_pending_history.valid = false;
    return;
  }

  if (g_pending_value.valid) {
    if (g_sensor_popup_ctx->entity_id.equalsIgnoreCase(g_pending_value.entity_id) &&
        is_popup_visible(g_sensor_popup_ctx)) {
      g_sensor_popup_ctx->decimals = g_pending_value.decimals;
      String live_unit = g_sensor_popup_ctx->lock_unit ? g_sensor_popup_ctx->unit : g_pending_value.unit;
      update_value_label(g_sensor_popup_ctx, g_pending_value.value, live_unit);
    }
    g_pending_value.valid = false;
  }

  if (g_pending_history.valid) {
    bool same_entity = true;
    if (g_pending_history.entity_id.length()) {
      same_entity = g_sensor_popup_ctx->entity_id.equalsIgnoreCase(g_pending_history.entity_id);
    }
    if (same_entity && is_popup_visible(g_sensor_popup_ctx)) {
      apply_history_payload(g_sensor_popup_ctx, g_pending_history.payload.c_str());
    }
    g_pending_history.valid = false;
  }
}






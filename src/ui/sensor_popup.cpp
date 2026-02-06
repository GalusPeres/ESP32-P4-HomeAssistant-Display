#include "src/ui/sensor_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/image_popup.h"
#include "src/ui/weather_popup.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

namespace {

constexpr int kCardWidth = 760;
constexpr int kCardHeight = 420;
constexpr int kCardPad = 20;
constexpr int kHeaderPadTop = 4;
constexpr int kHeaderIconOffsetX = 4;
constexpr int kHeaderIconOffsetY = -8;
constexpr int kContentPadTop = 65;
constexpr int kContentRowGap = 15;
constexpr int kChartHeight = 190;
constexpr int kTimeAxisHeight = 20;  // space for time labels below chart
constexpr int kTimeAxisMarkerCount = 4;  // 4 time markers (every 6h)
constexpr int kChartLineWidth = 4;
constexpr int kHistoryPointsDefault = 288;

struct SensorPopupContext {
  String entity_id;
  String unit;
  uint8_t decimals = 0xFF;
  uint32_t bg_color = 0;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* value_label = nullptr;
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
  lv_obj_t* y_max_label = nullptr;
  lv_obj_t* y_min_label = nullptr;
  lv_obj_t* y_max_line = nullptr;
  lv_obj_t* y_min_line = nullptr;
  lv_obj_t* time_labels[kTimeAxisMarkerCount] = {};
  lv_obj_t* time_lines[kTimeAxisMarkerCount] = {};
  lv_obj_t* chart_wrap = nullptr;
  uint16_t point_count = kHistoryPointsDefault;
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

static const lv_font_t* get_value_font() {
#if defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  return &lv_font_montserrat_40;
#elif defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#else
  return LV_FONT_DEFAULT;
#endif
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
  if (!unit.isEmpty() && display != "--") {
    display += " ";
    display += unit;
  }
  lv_label_set_text(ctx->value_label, display.c_str());
  ctx->unit = unit;
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

static void apply_init_to_context(SensorPopupContext* ctx, const SensorPopupInit& init) {
  if (!ctx) return;
  ctx->entity_id = init.entity_id;
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

// Get current local hour (0-23). Returns -1 if time not available.
static int get_local_hour() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    return timeinfo.tm_hour;
  }
  return -1;
}

// Calculate time axis marker positions and labels.
// History covers the last 24h ending at "now".  We place markers at full
// 6-hour boundaries (0:00, 6:00, 12:00, 18:00) that fall within that window.
// out_labels[i] receives the hour string ("0","6","12","18").
// out_frac[i] receives the fractional X position (0.0 = left, 1.0 = right).
// Returns number of markers placed (0-4).
static int calc_time_axis(String out_labels[], float out_frac[], int max_markers) {
  int hour = get_local_hour();
  if (hour < 0 || max_markers < 1) return 0;

  // The chart spans 24h ending at the current hour.
  // Position 0.0 = 24h ago, position 1.0 = now.
  // A clock-hour H occurred (hour - H) hours ago  (mod 24).
  // Its fractional position = 1.0 - (hours_ago / 24.0).
  int count = 0;
  for (int m = 0; m < 4 && count < max_markers; ++m) {
    int h = m * 6;  // 0, 6, 12, 18
    int hours_ago = (hour - h + 24) % 24;
    if (hours_ago == 0) hours_ago = 24;  // "now" maps to 24h ago at same hour
    float frac = 1.0f - (static_cast<float>(hours_ago) / 24.0f);
    if (frac < 0.02f || frac > 0.98f) continue;  // skip if too close to edges
    out_labels[count] = String(h) + " Uhr";
    out_frac[count] = frac;
    ++count;
  }
  return count;
}

// Recalculate Y-axis layout based on actual label text widths.
// Measures max/min labels, repositions guide lines and chart padding.
static void update_y_axis_layout(SensorPopupContext* ctx) {
  if (!ctx || !ctx->chart || !ctx->chart_wrap) return;
  constexpr int kLineOverlap = 6;
  constexpr int kLabelGap = 16;   // gap between label right edge and guide line start
  constexpr int kMinAxisW = 10;   // minimum width even if labels empty
  const int kAvailW = kCardWidth - (kCardPad * 2);

  // Measure actual rendered text widths
  lv_coord_t max_w = measure_label_text_width(ctx->y_max_label);
  lv_coord_t min_w = measure_label_text_width(ctx->y_min_label);

  lv_coord_t text_w = LV_MAX(max_w, min_w);
  if (text_w < kMinAxisW) text_w = kMinAxisW;
  int label_w = text_w + 2;  // text width + small safety margin
  int axis_w = label_w + kLabelGap;  // label + visible gap before lines

  // Reposition labels — width must fit full text
  if (ctx->y_max_label) {
    lv_obj_set_width(ctx->y_max_label, label_w);
  }
  if (ctx->y_min_label) {
    lv_obj_set_width(ctx->y_min_label, label_w);
  }

  // Chart drawing starts directly after label area
  int chart_left = axis_w;

  // Reposition guide lines (keep Y unchanged, only adjust X and width)
  int line_start = axis_w - kLineOverlap;
  int line_w = kAvailW - line_start;
  if (ctx->y_max_line) {
    lv_obj_set_width(ctx->y_max_line, line_w);
    lv_obj_set_x(ctx->y_max_line, line_start);
  }
  if (ctx->y_min_line) {
    lv_obj_set_width(ctx->y_min_line, line_w);
    lv_obj_set_x(ctx->y_min_line, line_start);
  }

  // Adjust chart left padding so graph starts after labels
  lv_obj_set_style_pad_left(ctx->chart, chart_left, 0);

  // Reposition time axis markers based on new chart area
  int chart_draw_w = kAvailW - chart_left;
  if (chart_draw_w < 10) return;
  constexpr int kLabelOverhang = 12;

  String labels[kTimeAxisMarkerCount];
  float fracs[kTimeAxisMarkerCount];
  int n = calc_time_axis(labels, fracs, kTimeAxisMarkerCount);

  for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
    if (i < n) {
      int x = chart_left + static_cast<int>(fracs[i] * chart_draw_w);
      if (ctx->time_lines[i]) {
        lv_obj_set_x(ctx->time_lines[i], x);
        lv_obj_clear_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
      }
      if (ctx->time_labels[i]) {
        lv_label_set_text(ctx->time_labels[i], labels[i].c_str());
        lv_obj_update_layout(ctx->time_labels[i]);
        lv_coord_t lbl_w = lv_obj_get_width(ctx->time_labels[i]);
        lv_obj_set_pos(ctx->time_labels[i], x - lbl_w / 2, kLabelOverhang + kChartHeight + 8);
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

  if (doc.containsKey("unit")) {
    String unit = String(doc["unit"].as<const char*>());
    unit.trim();
    if (!unit.isEmpty()) {
      ctx->unit = unit;
    }
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
    clear_chart(ctx, kHistoryPointsDefault);
    return;
  }

  uint16_t points = static_cast<uint16_t>(count);
  clear_chart(ctx, points);

  bool has_range = false;
  float min_v = 0.0f;
  float max_v = 0.0f;

  for (size_t i = 0; i < count; ++i) {
    JsonVariant v = values[i];
    float val = 0.0f;
    if (!extract_numeric(v, val)) {
      continue;
    }
    if (!has_range) {
      min_v = max_v = val;
      has_range = true;
    } else {
      if (val < min_v) min_v = val;
      if (val > max_v) max_v = val;
    }
  }

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
    JsonVariant v = values[i];
    float val = 0.0f;
    if (!extract_numeric(v, val)) {
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
    // No valid range → hide time axis too
    for (int i = 0; i < kTimeAxisMarkerCount; ++i) {
      if (ctx->time_lines[i]) lv_obj_add_flag(ctx->time_lines[i], LV_OBJ_FLAG_HIDDEN);
      if (ctx->time_labels[i]) lv_obj_add_flag(ctx->time_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void on_overlay_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
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
  set_label_style(title, lv_color_white(), &ui_font_24);
  lv_label_set_text(title, init.title.c_str());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, kHeaderPadTop);

  lv_obj_t* icon = lv_label_create(card);
  ctx->icon_label = icon;
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_align(icon, LV_ALIGN_TOP_RIGHT, kHeaderIconOffsetX, kHeaderIconOffsetY);

  lv_obj_t* content = lv_obj_create(card);
  lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
  lv_obj_align(content, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_set_style_pad_top(content, kContentPadTop, 0);
  lv_obj_set_layout(content, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(content, kContentRowGap, 0);

  lv_obj_t* value = lv_label_create(content);
  ctx->value_label = value;
  set_label_style(value, lv_color_white(), get_value_font());
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_width(value, LV_PCT(100));

  // Chart wrapper: Y-axis labels on the left, chart on the right, time labels below.
  // Extra vertical space (kLabelOverhang) at top/bottom so labels don't get clipped.
  constexpr int kLabelOverhang = 12;  // half of ui_font_20 line height + margin
  lv_obj_t* chart_wrap = lv_obj_create(content);
  ctx->chart_wrap = chart_wrap;
  lv_obj_remove_style_all(chart_wrap);
  lv_obj_set_size(chart_wrap, LV_PCT(100), kChartHeight + 2 * kLabelOverhang + kTimeAxisHeight);
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

  // Horizontal guide lines at max/min — extend 6px left of Y-axis for label alignment
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
  clear_chart(ctx, kHistoryPointsDefault);

  apply_init_to_context(ctx, init);

  lv_obj_add_event_cb(overlay, on_overlay_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
}

static bool should_request_history(const SensorPopupInit& init) {
  return init.entity_id.length() && !init.entity_id.startsWith("__");
}

}  // namespace

void show_sensor_popup(const SensorPopupInit& init) {
  if (!init.entity_id.length()) return;

  // Hide other popups if visible
  hide_light_popup();
  hide_image_popup();
  hide_weather_popup();

  if (g_sensor_popup_ctx && g_sensor_popup_ctx->overlay && g_sensor_popup_ctx->card) {
    apply_init_to_context(g_sensor_popup_ctx, init);
    clear_chart(g_sensor_popup_ctx, kHistoryPointsDefault);
    lv_obj_clear_flag(g_sensor_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_sensor_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  } else {
    SensorPopupContext* ctx = new SensorPopupContext();
    g_sensor_popup_ctx = ctx;
    build_popup_ui(ctx, init);
  }

  if (should_request_history(init)) {
    mqttPublishHistoryRequest(init.entity_id.c_str());
  }
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

void queue_sensor_popup_history(const char* entity_id, const char* payload, size_t len) {
  if (!payload || len == 0) return;
  g_pending_history.entity_id = entity_id ? entity_id : "";
  g_pending_history.payload = String(payload).substring(0, len);
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
      update_value_label(g_sensor_popup_ctx, g_pending_value.value, g_pending_value.unit);
    }
    g_pending_value.valid = false;
  }

  if (g_pending_history.valid) {
    if (is_popup_visible(g_sensor_popup_ctx)) {
      apply_history_payload(g_sensor_popup_ctx, g_pending_history.payload.c_str());
    }
    g_pending_history.valid = false;
  }
}

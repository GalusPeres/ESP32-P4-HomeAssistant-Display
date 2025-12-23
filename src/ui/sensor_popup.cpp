#include "src/ui/sensor_popup.h"
#include "src/ui/light_popup.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include <ArduinoJson.h>
#include <math.h>
#include <stdlib.h>

namespace {

constexpr int kCardWidth = 760;
constexpr int kCardHeight = 420;
constexpr int kCardPad = 20;
constexpr int kHeaderPadTop = 4;
constexpr int kHeaderIconOffsetX = 4;
constexpr int kHeaderIconOffsetY = -8;
constexpr int kContentPadTop = 85;
constexpr int kContentRowGap = 35;
constexpr int kChartHeight = 190;
constexpr int kChartLineWidth = 4;
constexpr int kHistoryPointsDefault = 288;

struct SensorPopupContext {
  String entity_id;
  String unit;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* value_label = nullptr;
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
  uint16_t point_count = kHistoryPointsDefault;
};

struct PendingValueUpdate {
  String entity_id;
  String value;
  String unit;
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
  if (ctx->title_label) {
    lv_label_set_text(ctx->title_label, init.title.c_str());
  }
  if (ctx->icon_label) {
    String icon_name = init.icon_name;
    if (icon_name.isEmpty()) {
      icon_name = "home-analytics";
    }
    String icon_char = getMdiChar(icon_name);
    if (!icon_char.isEmpty()) {
      lv_label_set_text(ctx->icon_label, icon_char.c_str());
    }
  }
  update_value_label(ctx, init.value, init.unit);
}

static void clear_chart(SensorPopupContext* ctx, uint16_t points) {
  if (!ctx || !ctx->chart || !ctx->series) return;
  ctx->point_count = points;
  lv_chart_set_point_count(ctx->chart, points);
  lv_chart_set_all_value(ctx->chart, ctx->series, LV_CHART_POINT_NONE);
  lv_chart_refresh(ctx->chart);
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
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
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

  lv_obj_t* chart = lv_chart_create(content);
  ctx->chart = chart;
  lv_obj_set_width(chart, LV_PCT(100));
  lv_obj_set_height(chart, kChartHeight);
  lv_obj_set_style_bg_opa(chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(chart, 0, 0);
  lv_obj_set_style_pad_all(chart, 0, 0);
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

  // Hide light popup if visible
  hide_light_popup();

  if (g_sensor_popup_ctx && g_sensor_popup_ctx->overlay && g_sensor_popup_ctx->card) {
    apply_init_to_context(g_sensor_popup_ctx, init);
    clear_chart(g_sensor_popup_ctx, kHistoryPointsDefault);
    lv_obj_clear_flag(g_sensor_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_sensor_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(g_sensor_popup_ctx->overlay);
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
  init.icon_name = "home-analytics";
  init.value = "";
  init.unit = "";
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

void queue_sensor_popup_value(const char* entity_id, const char* value, const char* unit) {
  if (!entity_id || !*entity_id || !value) return;
  g_pending_value.entity_id = entity_id;
  g_pending_value.value = value;
  g_pending_value.unit = unit ? unit : "";
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

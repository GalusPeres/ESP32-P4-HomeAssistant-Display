#include "src/ui/weather_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/sensor_popup.h"
#include "src/network/mqtt_handlers.h"
#include "src/core/display_manager.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/types/clock/clock_format.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

namespace {

constexpr int kCardMargin = 4;
constexpr int kCardWidth =
    (SCREEN_WIDTH > SCREEN_HEIGHT)
        ? (SCREEN_HEIGHT - (kCardMargin * 2))
        : (SCREEN_WIDTH - (kCardMargin * 2));
constexpr int kCardHeight = SCREEN_HEIGHT - (kCardMargin * 2);
constexpr int kCardPad = 20;
constexpr int kCols = 7;
constexpr int kHourlyForecastMax = 168;
constexpr int kDetailMarkerCount = 5;
constexpr int kDetailChartPointCount = 25;
constexpr int kModeRowOffsetY = 124;
constexpr int kModeButtonWidth = 96;
constexpr int kModeButtonHeight = 54;
constexpr int kModeButtonGap = 12;
constexpr int kSummaryRowTop = 108;
constexpr int kDetailHeaderSubrowTop = 170;
constexpr int kForecastRowTop = 230;
constexpr int kForecastRowHeight = kCardHeight - kForecastRowTop - kCardPad - 10;
constexpr int kForecastSidePad = 4;
constexpr int kForecastColGap = 4;
constexpr int kForecastDayTop = 2;
constexpr int kForecastDayIconGap = 6;
constexpr int kForecastIconTop = 30;
constexpr int kForecastTempChartTop = 102;
constexpr int kForecastTempChartHeight = 164;
constexpr int kForecastHighLabelGap = 6;
constexpr int kForecastLowLabelGap = 8;
constexpr int kForecastPrecipChartTop = 302;
constexpr int kForecastPrecipChartHeight = 54;
constexpr int kForecastAmountTop = 376;
constexpr int kForecastProbabilityTop = 408;
constexpr int kForecastBarWidth = 10;
constexpr int kForecastChartSideInset = 12;
constexpr int kForecastPlotTotalW = kCardWidth - (kCardPad * 2);
constexpr int kForecastPlotColW =
    (kForecastPlotTotalW - (kForecastSidePad * 2) - (kForecastColGap * (kCols - 1))) / kCols;
constexpr int kForecastFirstCenter = kForecastSidePad + (kForecastPlotColW / 2);
constexpr int kForecastLastCenter =
    kForecastSidePad + ((kCols - 1) * (kForecastPlotColW + kForecastColGap)) + (kForecastPlotColW / 2);
constexpr int kDetailRowTop = kForecastRowTop;
constexpr int kDetailTitleTop = kDetailHeaderSubrowTop;
constexpr int kDetailChartWrapTop = 0;
constexpr int kDetailLabelOverhang = 12;
constexpr int kDetailYAxisWidth = 18;
constexpr int kDetailTempChartHeight = kForecastTempChartHeight - 20;
constexpr int kDetailPrecipChartHeight = kForecastPrecipChartHeight + 20;
constexpr int kDetailChartGap =
    kForecastPrecipChartTop - kForecastTempChartTop - kForecastTempChartHeight;
constexpr int kDetailProbabilityGap = 4;
constexpr int kDetailProbabilityHeight = 22;
constexpr int kDetailTempValueGap = 14;
constexpr int kDetailTempMarkerSize = 10;
constexpr int kDetailCurrentHourDotSize = 14;
constexpr int kDetailPrecipBarWidth = 12;
constexpr int kDetailSectionGutter = 16;
constexpr int kDetailChartLeftInset = kForecastFirstCenter;
constexpr int kDetailChartRightInset = kForecastPlotTotalW - kForecastLastCenter;
constexpr int kDetailTimeLabelY = kForecastDayTop;
constexpr int kDetailIconY = kForecastIconTop;
constexpr int kDetailPastOverlayRadius = 16;
constexpr int kDetailDisabledBlockGap = 5;
constexpr int kDetailDisabledLineGap = 10;
constexpr int kDetailTempChartTop = kForecastTempChartTop;
constexpr int kDetailPrecipChartTop = kForecastPrecipChartTop - 20;
constexpr int kDetailAmountTop = kForecastAmountTop;
constexpr int kDetailProbabilityTop = kForecastProbabilityTop;
constexpr int kDetailDisabledIconBlockTop = kDetailIconY + 4;
constexpr int kDetailDisabledTempBlockTop = kDetailTempChartTop - 6;
constexpr int kDetailDisabledIconBlockHeight =
    kDetailDisabledTempBlockTop - kDetailDisabledBlockGap - kDetailDisabledIconBlockTop;
constexpr int kDetailDisabledPrecipBlockTop = kDetailPrecipChartTop - 6;
constexpr int kDetailDisabledTempBlockHeight =
    kDetailDisabledPrecipBlockTop - kDetailDisabledBlockGap - kDetailDisabledTempBlockTop;
constexpr int kDetailDisabledPrecipBlockHeight =
    (kDetailProbabilityTop + kDetailProbabilityHeight + 4) - kDetailDisabledPrecipBlockTop;
constexpr int kDetailDisabledOverlayTop = kDetailDisabledIconBlockTop;
constexpr int kDetailDisabledOverlayHeight =
    (kDetailProbabilityTop + kDetailProbabilityHeight + 4) - kDetailDisabledOverlayTop;
constexpr int kDetailDisabledSeparator1Top = kDetailDisabledIconBlockHeight;
constexpr int kDetailDisabledSeparator2Top =
    (kDetailDisabledTempBlockTop + kDetailDisabledTempBlockHeight) - kDetailDisabledOverlayTop;
constexpr int kDetailDisabledLineGapDelta = kDetailDisabledLineGap - kDetailDisabledBlockGap;
constexpr int kDetailDisabledLineInsetTop = (kDetailDisabledLineGapDelta + 1) / 2;
constexpr int kDetailDisabledLineInsetBottom = kDetailDisabledLineGapDelta / 2;
constexpr int kDetailTempGuideTop = kDetailDisabledTempBlockTop + kDetailDisabledLineInsetTop;
constexpr int kDetailTempGuideHeight =
    kDetailDisabledTempBlockHeight - kDetailDisabledLineGapDelta;
constexpr int kDetailPrecipGuideTop = kDetailPrecipChartTop + kDetailDisabledLineInsetTop;
constexpr int kDetailPrecipGuideHeight =
    kDetailPrecipChartHeight - kDetailDisabledLineInsetTop;
constexpr int kDetailChartWrapHeight =
    kDetailProbabilityTop + kDetailProbabilityHeight + kDetailLabelOverhang;
constexpr int kHeaderPadTop = 4;
constexpr int kHeaderIconOffsetX = 4;
constexpr int kHeaderIconOffsetY = -8;

struct ForecastWidgets {
  lv_obj_t* column = nullptr;
  lv_obj_t* day_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* high_label = nullptr;
  lv_obj_t* low_label = nullptr;
  lv_obj_t* precip_bar = nullptr;
  lv_obj_t* precip_amount_label = nullptr;
  lv_obj_t* precip_probability_label = nullptr;
};

struct ForecastData {
  bool active = false;
  String day;
  String date_local;
  String icon;
  bool has_high = false;
  float high = 0.0f;
  bool has_low = false;
  float low = 0.0f;
  bool has_precipitation = false;
  float precipitation = 0.0f;
  bool has_precipitation_probability = false;
  float precipitation_probability = 0.0f;
};

struct HourlyForecastData {
  bool active = false;
  String date_local;
  int hour_local = -1;
  String icon;
  bool has_temp = false;
  float temp = 0.0f;
  bool has_precipitation = false;
  float precipitation = 0.0f;
  bool has_precipitation_probability = false;
  float precipitation_probability = 0.0f;
};

enum class WeatherPopupViewMode {
  Week,
  Day,
};

struct WeatherPopupContext {
  String entity_id;
  String title;
  uint32_t bg_color = 0;
  String unit;
  String precipitation_unit = "mm";
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* location_label = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* header_today_btn = nullptr;
  lv_obj_t* header_week_btn = nullptr;
  lv_obj_t* value_row = nullptr;
  lv_obj_t* condition_label = nullptr;
  lv_obj_t* condition_sep_label = nullptr;
  lv_obj_t* temp_label = nullptr;
  lv_obj_t* week_range_label = nullptr;
  lv_obj_t* mode_row = nullptr;
  lv_obj_t* mode_week_btn = nullptr;
  lv_obj_t* mode_day_btn = nullptr;
  lv_obj_t* forecast_row = nullptr;
  lv_obj_t* forecast_temp_chart = nullptr;
  lv_obj_t* forecast_precip_base = nullptr;
  lv_obj_t* forecast_dividers[kCols - 1] = {};
  lv_chart_series_t* forecast_high_series = nullptr;
  lv_chart_series_t* forecast_low_series = nullptr;
  int forecast_temp_min = 0;
  int forecast_temp_max = 100;
  lv_obj_t* detail_wrap = nullptr;
  lv_obj_t* detail_title_label = nullptr;
  lv_obj_t* detail_prev_btn = nullptr;
  lv_obj_t* detail_next_btn = nullptr;
  lv_obj_t* detail_past_overlay = nullptr;
  lv_obj_t* detail_end_overlay = nullptr;
  lv_obj_t* detail_disabled_separators[4] = {};
  int detail_disabled_separator_count = 0;
  lv_obj_t* detail_y_max_label = nullptr;
  lv_obj_t* detail_y_min_label = nullptr;
  lv_obj_t* detail_y_max_line = nullptr;
  lv_obj_t* detail_y_min_line = nullptr;
  lv_obj_t* detail_icon_guides[kDetailMarkerCount] = {};
  lv_obj_t* detail_time_lines[kDetailMarkerCount] = {};
  lv_obj_t* detail_time_labels[kDetailMarkerCount] = {};
  lv_obj_t* detail_icon_labels[kDetailMarkerCount] = {};
  lv_obj_t* detail_temp_point_dots[kDetailMarkerCount] = {};
  lv_obj_t* detail_current_dot = nullptr;
  lv_obj_t* detail_temp_value_labels[kDetailMarkerCount] = {};
  lv_obj_t* detail_precip_amount_labels[kDetailMarkerCount] = {};
  lv_obj_t* detail_probability_labels[kDetailMarkerCount] = {};
  lv_obj_t* detail_precip_guides[kDetailMarkerCount] = {};
  lv_obj_t* detail_precip_bars[kDetailChartPointCount] = {};
  lv_obj_t* detail_temp_chart = nullptr;
  lv_chart_series_t* detail_temp_series = nullptr;
  lv_obj_t* detail_precip_chart = nullptr;
  lv_chart_series_t* detail_precip_series = nullptr;
  int detail_temp_min = 0;
  int detail_temp_max = 100;
  WeatherPopupViewMode view_mode = WeatherPopupViewMode::Week;
  int selected_day_index = -1;
  ForecastWidgets forecast[kCols];
  ForecastData forecast_data[kCols];
  HourlyForecastData hourly[kHourlyForecastMax];
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

static String format_precipitation_amount(float amount, const String& unit) {
  String text = String(amount, 1);
  if (text.endsWith(".0")) {
    text.remove(text.length() - 2);
  }
  text += " ";
  text += unit.length() ? unit : "mm";
  return text;
}

static String format_precipitation_probability(float probability) {
  return String(static_cast<int>(lroundf(probability))) + " %";
}

static String format_forecast_chart_temp(float temp) {
  String text = String(temp, 1);
  if (text.endsWith(".0")) {
    text.remove(text.length() - 2);
  }
  text += "\xC2\xB0";
  return text;
}

static int scale_forecast_temp(float temp) {
  return static_cast<int>(lroundf(temp * 10.0f));
}

static lv_coord_t map_forecast_chart_y(const WeatherPopupContext* ctx, int scaled_temp) {
  if (!ctx) return kForecastTempChartTop + (kForecastTempChartHeight / 2);
  int min_temp = ctx->forecast_temp_min;
  int max_temp = ctx->forecast_temp_max;
  if (max_temp <= min_temp) return kForecastTempChartTop + (kForecastTempChartHeight / 2);
  const lv_coord_t pad = 4;
  const lv_coord_t draw_h = kForecastTempChartHeight - pad * 2;
  const float ratio = static_cast<float>(max_temp - scaled_temp) /
                      static_cast<float>(max_temp - min_temp);
  return kForecastTempChartTop + pad + static_cast<lv_coord_t>(lroundf(ratio * draw_h));
}

static lv_coord_t map_detail_chart_y(const WeatherPopupContext* ctx, int scaled_temp) {
  if (!ctx) return kDetailTempChartTop + (kDetailTempChartHeight / 2);
  int min_temp = ctx->detail_temp_min;
  int max_temp = ctx->detail_temp_max;
  if (max_temp <= min_temp) return kDetailTempChartTop + (kDetailTempChartHeight / 2);
  const lv_coord_t pad = 6;
  const lv_coord_t draw_h = kDetailTempChartHeight - pad * 2;
  const float ratio = static_cast<float>(max_temp - scaled_temp) /
                      static_cast<float>(max_temp - min_temp);
  return kDetailTempChartTop + pad + static_cast<lv_coord_t>(lroundf(ratio * draw_h));
}

static lv_coord_t map_detail_chart_x(const WeatherPopupContext* ctx, int point_id) {
  if (!ctx || !ctx->detail_temp_chart || !ctx->detail_temp_series) return 0;
  const int clamped_point =
      (point_id < 0) ? 0 : ((point_id >= kDetailChartPointCount) ? (kDetailChartPointCount - 1) : point_id);
  lv_point_t point;
  lv_chart_get_point_pos_by_id(ctx->detail_temp_chart, ctx->detail_temp_series, clamped_point, &point);
  return lv_obj_get_x(ctx->detail_temp_chart) + point.x;
}

static void clear_forecast_chart(WeatherPopupContext* ctx) {
  if (!ctx || !ctx->forecast_temp_chart || !ctx->forecast_high_series || !ctx->forecast_low_series) return;
  lv_chart_set_point_count(ctx->forecast_temp_chart, kCols);
  lv_chart_set_all_value(ctx->forecast_temp_chart, ctx->forecast_high_series, LV_CHART_POINT_NONE);
  lv_chart_set_all_value(ctx->forecast_temp_chart, ctx->forecast_low_series, LV_CHART_POINT_NONE);
  lv_chart_set_range(ctx->forecast_temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_refresh(ctx->forecast_temp_chart);
}

static void update_forecast_graph(WeatherPopupContext* ctx) {
  if (!ctx) return;

  bool has_temp = false;
  int min_temp = 0;
  int max_temp = 0;
  bool has_precipitation = false;
  float max_precipitation = 0.0f;

  if (ctx->forecast_temp_chart && ctx->forecast_high_series && ctx->forecast_low_series) {
    lv_chart_set_point_count(ctx->forecast_temp_chart, kCols);
  }

  for (int i = 0; i < kCols; ++i) {
    const ForecastData& data = ctx->forecast_data[i];

    if (ctx->forecast_temp_chart && ctx->forecast_high_series) {
      if (data.has_high) {
        int scaled = scale_forecast_temp(data.high);
        lv_chart_set_value_by_id(ctx->forecast_temp_chart, ctx->forecast_high_series, i, scaled);
        if (!has_temp) {
          min_temp = scaled;
          max_temp = scaled;
          has_temp = true;
        } else {
          if (scaled < min_temp) min_temp = scaled;
          if (scaled > max_temp) max_temp = scaled;
        }
      } else {
        lv_chart_set_value_by_id(ctx->forecast_temp_chart, ctx->forecast_high_series, i, LV_CHART_POINT_NONE);
      }
    }

    if (ctx->forecast_temp_chart && ctx->forecast_low_series) {
      if (data.has_low) {
        int scaled = scale_forecast_temp(data.low);
        lv_chart_set_value_by_id(ctx->forecast_temp_chart, ctx->forecast_low_series, i, scaled);
        if (!has_temp) {
          min_temp = scaled;
          max_temp = scaled;
          has_temp = true;
        } else {
          if (scaled < min_temp) min_temp = scaled;
          if (scaled > max_temp) max_temp = scaled;
        }
      } else {
        lv_chart_set_value_by_id(ctx->forecast_temp_chart, ctx->forecast_low_series, i, LV_CHART_POINT_NONE);
      }
    }

    if (data.has_precipitation) {
      if (!has_precipitation || data.precipitation > max_precipitation) {
        max_precipitation = data.precipitation;
      }
      has_precipitation = true;
    }
  }

  if (has_temp) {
    int padding = (max_temp - min_temp) / 6;
    if (padding < 6) padding = 6;
    if (max_temp == min_temp) padding = 10;
    ctx->forecast_temp_min = min_temp - padding;
    ctx->forecast_temp_max = max_temp + padding;
  } else {
    ctx->forecast_temp_min = 0;
    ctx->forecast_temp_max = 100;
  }

  if (ctx->forecast_temp_chart) {
    lv_chart_set_range(ctx->forecast_temp_chart,
                       LV_CHART_AXIS_PRIMARY_Y,
                       ctx->forecast_temp_min,
                       ctx->forecast_temp_max);
    lv_chart_refresh(ctx->forecast_temp_chart);
    lv_obj_update_layout(ctx->forecast_temp_chart);
  }

  for (int i = 0; i < kCols; ++i) {
    ForecastWidgets& fw = ctx->forecast[i];
    const ForecastData& data = ctx->forecast_data[i];
    if (!fw.column) continue;

    if (fw.day_label) {
      if (data.active) {
        String text = data.day.length() ? data.day : "--";
        lv_label_set_text(fw.day_label, text.c_str());
        lv_obj_clear_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(fw.day_label);
      } else {
        lv_label_set_text(fw.day_label, "");
        lv_obj_add_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (fw.icon_label) {
      if (data.active && data.icon.length()) {
        String icon_char = getMdiChar(data.icon);
        if (icon_char.length()) {
          lv_label_set_text(fw.icon_label, icon_char.c_str());
          lv_obj_clear_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
          if (fw.day_label && !lv_obj_has_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_align_to(fw.icon_label,
                            fw.day_label,
                            LV_ALIGN_OUT_BOTTOM_MID,
                            0,
                            kForecastDayIconGap);
          } else {
            lv_obj_align(fw.icon_label, LV_ALIGN_TOP_MID, 0, kForecastIconTop);
          }
        } else {
          lv_label_set_text(fw.icon_label, "");
          lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
        }
      } else {
        lv_label_set_text(fw.icon_label, "");
        lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
      }
    }

    lv_coord_t high_y = kForecastTempChartTop;
    lv_coord_t low_y = kForecastTempChartTop + kForecastTempChartHeight - 18;
    lv_coord_t high_h = 0;
    lv_coord_t low_h = 0;

    if (fw.high_label) {
      if (data.active && data.has_high) {
        String text = format_forecast_chart_temp(data.high);
        lv_label_set_text(fw.high_label, text.c_str());
        lv_obj_clear_flag(fw.high_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(fw.high_label);
        high_h = lv_obj_get_height(fw.high_label);
        if (ctx->forecast_temp_chart && ctx->forecast_high_series) {
          lv_point_t point;
          lv_chart_get_point_pos_by_id(ctx->forecast_temp_chart, ctx->forecast_high_series, i, &point);
          high_y = lv_obj_get_y(ctx->forecast_temp_chart) + point.y - high_h - kForecastHighLabelGap;
        } else {
          high_y = map_forecast_chart_y(ctx, scale_forecast_temp(data.high)) - high_h - kForecastHighLabelGap;
        }
      } else {
        lv_label_set_text(fw.high_label, "");
        lv_obj_add_flag(fw.high_label, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (fw.low_label) {
      if (data.active && data.has_low) {
        String text = format_forecast_chart_temp(data.low);
        lv_label_set_text(fw.low_label, text.c_str());
        lv_obj_clear_flag(fw.low_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(fw.low_label);
        low_h = lv_obj_get_height(fw.low_label);
        if (ctx->forecast_temp_chart && ctx->forecast_low_series) {
          lv_point_t point;
          lv_chart_get_point_pos_by_id(ctx->forecast_temp_chart, ctx->forecast_low_series, i, &point);
          low_y = lv_obj_get_y(ctx->forecast_temp_chart) + point.y + kForecastLowLabelGap;
        } else {
          low_y = map_forecast_chart_y(ctx, scale_forecast_temp(data.low)) + kForecastLowLabelGap;
        }
      } else {
        lv_label_set_text(fw.low_label, "");
        lv_obj_add_flag(fw.low_label, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (fw.high_label && fw.low_label && data.active && data.has_high && data.has_low) {
      if (high_y + high_h > low_y - 2) {
        const lv_coord_t overlap = (high_y + high_h) - low_y + 2;
        high_y -= (overlap + 1) / 2;
        low_y += overlap / 2;
      }
    }

    const lv_coord_t high_min_y = kForecastIconTop + 18;
    const lv_coord_t high_max_y = kForecastPrecipChartTop - 42;
    const lv_coord_t low_min_y = kForecastTempChartTop + 10;
    const lv_coord_t low_max_y = kForecastPrecipChartTop - 22;

    if (fw.high_label && !lv_obj_has_flag(fw.high_label, LV_OBJ_FLAG_HIDDEN)) {
      if (high_y < high_min_y) high_y = high_min_y;
      if (high_y > high_max_y) high_y = high_max_y;
      lv_obj_set_y(fw.high_label, high_y);
    }

    if (fw.low_label && !lv_obj_has_flag(fw.low_label, LV_OBJ_FLAG_HIDDEN)) {
      if (low_y < low_min_y) low_y = low_min_y;
      if (low_y > low_max_y) low_y = low_max_y;
      lv_obj_set_y(fw.low_label, low_y);
    }

    if (fw.precip_amount_label) {
      if (data.active && data.has_precipitation) {
        String text = format_precipitation_amount(data.precipitation, ctx->precipitation_unit);
        lv_label_set_text(fw.precip_amount_label, text.c_str());
        lv_obj_clear_flag(fw.precip_amount_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(fw.precip_amount_label, "");
        lv_obj_add_flag(fw.precip_amount_label, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (fw.precip_probability_label) {
      if (data.active && data.has_precipitation_probability) {
        String text = format_precipitation_probability(data.precipitation_probability);
        lv_label_set_text(fw.precip_probability_label, text.c_str());
        lv_obj_clear_flag(fw.precip_probability_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(fw.precip_probability_label, "");
        lv_obj_add_flag(fw.precip_probability_label, LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (fw.precip_bar) {
      if (data.active && data.has_precipitation && data.precipitation > 0.0f &&
          has_precipitation && max_precipitation > 0.0f) {
        lv_coord_t col_w = lv_obj_get_width(fw.column);
        lv_coord_t bar_h = static_cast<lv_coord_t>(lroundf(
            (data.precipitation / max_precipitation) * kForecastPrecipChartHeight));
        if (bar_h < 4) bar_h = 4;
        if (bar_h > kForecastPrecipChartHeight) bar_h = kForecastPrecipChartHeight;
        lv_obj_set_size(fw.precip_bar, kForecastBarWidth, bar_h);
        lv_obj_set_pos(fw.precip_bar,
                       (col_w - kForecastBarWidth) / 2,
                       kForecastPrecipChartTop + kForecastPrecipChartHeight - bar_h);
        lv_obj_clear_flag(fw.precip_bar, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(fw.precip_bar, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
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
  int dow = (h + 6) % 7;
  static const char* kDaysDe[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  if (dow < 0 || dow > 6) return "";
  return String(kDaysDe[dow]);
}

static String weekday_long_from_iso(const String& iso) {
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
  const bool is_de = configManager.getConfig().language[0] == 'd';
  static const char* kDaysDe[] = {
      "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};
  static const char* kDaysEn[] = {
      "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  if (dow < 0 || dow > 6) return "";
  return String(is_de ? kDaysDe[dow] : kDaysEn[dow]);
}

static String format_detail_time_axis_label(int hour24) {
  int normalized = hour24 % 24;
  if (normalized < 0) normalized += 24;
  const DeviceConfig& cfg = configManager.getConfig();
  const uint8_t time_format =
      clock_tile::resolve_time_format(clock_tile::TIME_FORMAT_AUTO, cfg.global_time_format, cfg.language);
  if (time_format == clock_tile::TIME_FORMAT_24H) {
    return String(normalized) + ":00";
  }

  int hour12 = normalized % 12;
  if (hour12 == 0) hour12 = 12;
  return String(hour12) + (normalized < 12 ? "am" : "pm");
}

static String format_localized_date_from_iso(const String& iso) {
  if (iso.length() < 10) return iso;
  const DeviceConfig& cfg = configManager.getConfig();
  const uint8_t date_format =
      clock_tile::resolve_date_format(clock_tile::DATE_FORMAT_AUTO, cfg.global_date_format, cfg.language);
  const String yyyy = iso.substring(0, 4);
  const String mm = iso.substring(5, 7);
  const String dd = iso.substring(8, 10);
  switch (date_format) {
    case clock_tile::DATE_FORMAT_MDY:
      return mm + "/" + dd + "/" + yyyy;
    case clock_tile::DATE_FORMAT_YMD:
      return yyyy + "/" + mm + "/" + dd;
    default:
      return dd + "." + mm + "." + yyyy;
  }
}

static const char* weather_today_button_text() {
  return (configManager.getConfig().language[0] == 'd') ? "Heute" : "Today";
}

static String iso_date_part(const String& text) {
  if (text.length() < 10) return "";
  return text.substring(0, 10);
}

static int iso_hour_part(const String& text) {
  if (text.length() < 13) return -1;
  return text.substring(11, 13).toInt();
}

static bool get_local_now_parts(String& date_out, int& hour_out) {
  time_t now = time(nullptr);
  if (now < 1704067200) return false;

  struct tm local_tm;
  if (!localtime_r(&now, &local_tm)) return false;

  char date_buf[11];
  if (strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &local_tm) == 0) return false;

  date_out = date_buf;
  hour_out = local_tm.tm_hour;
  return true;
}

static String format_detail_day_title(const ForecastData& data) {
  String title = weekday_long_from_iso(data.date_local);
  if (!title.length()) title = data.day;
  if (!data.date_local.length()) return title.length() ? title : String("--");
  if (data.date_local.length() < 10) return title.length() ? title : data.date_local;

  String date_text = format_localized_date_from_iso(data.date_local);

  if (!title.length()) return date_text;

  title += " | ";
  title += date_text;
  return title;
}

static String format_forecast_range_title(const WeatherPopupContext* ctx) {
  if (!ctx) return "";
  String start_date;
  String end_date;
  for (int i = 0; i < kCols; ++i) {
    if (ctx->forecast_data[i].active && ctx->forecast_data[i].date_local.length()) {
      if (!start_date.length()) start_date = ctx->forecast_data[i].date_local;
      end_date = ctx->forecast_data[i].date_local;
    }
  }
  if (!start_date.length()) return "";
  String start_text = format_localized_date_from_iso(start_date);
  if (!end_date.length() || end_date == start_date) return start_text;
  return start_text + " - " + format_localized_date_from_iso(end_date);
}

static void set_label_style(lv_obj_t* lbl, lv_color_t color, const lv_font_t* font) {
  if (!lbl) return;
  lv_obj_set_style_text_color(lbl, color, 0);
  if (font) {
    lv_obj_set_style_text_font(lbl, font, 0);
  }
}

static void align_header_row(lv_obj_t* card, lv_obj_t* title_label, lv_obj_t* icon_label);
static int find_active_day_index(const WeatherPopupContext* ctx, const String& date_local);
static int find_prev_active_day_index(const WeatherPopupContext* ctx, int from_index);
static int find_next_active_day_index(const WeatherPopupContext* ctx, int from_index);

static void update_week_range_label(WeatherPopupContext* ctx) {
  if (!ctx || !ctx->week_range_label) return;
  String text = format_forecast_range_title(ctx);
  lv_label_set_text(ctx->week_range_label, text.c_str());
  if (text.length()) {
    lv_obj_clear_flag(ctx->week_range_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ctx->week_range_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static bool next_json_object_in_array(const String& array, int& cursor, String& out) {
  bool in_string = false;
  int depth = 0;
  int start = -1;

  for (int i = cursor; i < array.length(); ++i) {
    char c = array.charAt(i);
    if (c == '"' && (i == 0 || array.charAt(i - 1) != '\\')) {
      in_string = !in_string;
    }
    if (in_string) continue;
    if (c == '{') {
      if (depth == 0) start = i;
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0 && start >= 0) {
        out = array.substring(start, i + 1);
        cursor = i + 1;
        return true;
      }
    }
  }

  cursor = array.length();
  return false;
}

static void style_mode_button(lv_obj_t* btn, bool active) {
  if (!btn) return;
  lv_color_t active_text_color = lv_color_hex(0x2A2A2A);
  lv_obj_t* row = lv_obj_get_parent(btn);
  lv_obj_t* card = row ? lv_obj_get_parent(row) : nullptr;
  if (card) {
    active_text_color = lv_obj_get_style_bg_color(card, LV_PART_MAIN);
  }

  auto apply_selector = [&](lv_style_selector_t selector) {
    lv_obj_set_style_bg_color(btn, lv_color_white(), selector);
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

static void style_header_action_button(WeatherPopupContext* ctx, lv_obj_t* btn, bool active) {
  if (!ctx || !btn) return;
  lv_color_t active_text_color = lv_color_hex(0x2A2A2A);
  if (ctx->card) {
    active_text_color = lv_obj_get_style_bg_color(ctx->card, LV_PART_MAIN);
  }

  auto apply_selector = [&](lv_style_selector_t selector) {
    lv_obj_set_style_bg_color(btn, lv_color_white(), selector);
    lv_obj_set_style_bg_opa(btn, active ? LV_OPA_COVER : LV_OPA_TRANSP, selector);
    lv_obj_set_style_border_color(btn, lv_color_white(), selector);
    lv_obj_set_style_border_width(btn, 2, selector);
    lv_obj_set_style_border_opa(btn, active ? LV_OPA_TRANSP : LV_OPA_COVER, selector);
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
    lv_obj_set_style_text_font(label, FONT_UNIT, 0);
    lv_obj_set_style_text_font(label, FONT_UNIT, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(label, active ? active_text_color : lv_color_white(), 0);
    lv_obj_set_style_text_color(label, active ? active_text_color : lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_text_outline_stroke_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_outline_stroke_opa(label, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_text_outline_stroke_width(label, 0, 0);
    lv_obj_set_style_text_outline_stroke_width(label, 0, LV_STATE_PRESSED);
  }
}

static void update_mode_buttons(WeatherPopupContext* ctx) {
  if (!ctx) return;
  if (ctx->mode_week_btn) {
    lv_obj_t* label = lv_obj_get_child(ctx->mode_week_btn, 0);
    if (label) {
      set_label_style(label, lv_color_white(), FONT_MDI_ICONS);
      lv_label_set_text(label, getMdiChar("arrow-left").c_str());
    }
    style_mode_button(ctx->mode_week_btn, true);
  }
  if (ctx->mode_row) {
    lv_obj_add_flag(ctx->mode_row, LV_OBJ_FLAG_HIDDEN);
  }
  String today_date;
  int today_hour = 0;
  const int today_day =
      get_local_now_parts(today_date, today_hour) ? find_active_day_index(ctx, today_date) : -1;
  if (ctx->header_today_btn) {
    lv_obj_t* label = lv_obj_get_child(ctx->header_today_btn, 0);
    if (label) {
      set_label_style(label, lv_color_white(), FONT_UNIT);
      lv_label_set_text(label, weather_today_button_text());
    }
    style_header_action_button(ctx,
                               ctx->header_today_btn,
                               ctx->view_mode == WeatherPopupViewMode::Day &&
                                   today_day >= 0 &&
                                   ctx->selected_day_index == today_day);
    if (today_day >= 0) {
      lv_obj_clear_flag(ctx->header_today_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->header_today_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->header_week_btn) {
    lv_obj_t* label = lv_obj_get_child(ctx->header_week_btn, 0);
    if (label) {
      set_label_style(label, lv_color_white(), FONT_UNIT);
      lv_label_set_text(label, "7D");
    }
    style_header_action_button(ctx, ctx->header_week_btn, ctx->view_mode == WeatherPopupViewMode::Week);
    lv_obj_clear_flag(ctx->header_week_btn, LV_OBJ_FLAG_HIDDEN);
  }
  const bool show_detail_nav = ctx->view_mode == WeatherPopupViewMode::Day;
  const int prev_day = find_prev_active_day_index(ctx, ctx->selected_day_index);
  const int next_day = find_next_active_day_index(ctx, ctx->selected_day_index);
  if (ctx->detail_prev_btn) {
    if (show_detail_nav && prev_day >= 0) {
      lv_obj_clear_flag(ctx->detail_prev_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->detail_prev_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->detail_next_btn) {
    if (show_detail_nav && next_day >= 0) {
      lv_obj_clear_flag(ctx->detail_next_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->detail_next_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void clear_hourly(WeatherPopupContext* ctx) {
  if (!ctx) return;
  for (int i = 0; i < kHourlyForecastMax; ++i) {
    ctx->hourly[i] = HourlyForecastData();
  }
}

static int find_active_day_index(const WeatherPopupContext* ctx, const String& date_local) {
  if (!ctx || !date_local.length()) return -1;
  for (int i = 0; i < kCols; ++i) {
    if (ctx->forecast_data[i].active && ctx->forecast_data[i].date_local == date_local) {
      return i;
    }
  }
  return -1;
}

static int find_first_active_day_index(const WeatherPopupContext* ctx) {
  if (!ctx) return -1;
  for (int i = 0; i < kCols; ++i) {
    if (ctx->forecast_data[i].active) return i;
  }
  return -1;
}

static int find_prev_active_day_index(const WeatherPopupContext* ctx, int from_index) {
  if (!ctx) return -1;
  for (int i = from_index - 1; i >= 0; --i) {
    if (ctx->forecast_data[i].active) return i;
  }
  return -1;
}

static int find_next_active_day_index(const WeatherPopupContext* ctx, int from_index) {
  if (!ctx) return -1;
  for (int i = from_index + 1; i < kCols; ++i) {
    if (ctx->forecast_data[i].active) return i;
  }
  return -1;
}

static int get_default_day_index(const WeatherPopupContext* ctx) {
  if (!ctx) return -1;
  String today_date;
  int today_hour = 0;
  if (get_local_now_parts(today_date, today_hour)) {
    int today_index = find_active_day_index(ctx, today_date);
    if (today_index >= 0) return today_index;
  }
  return find_first_active_day_index(ctx);
}

static void set_popup_view_mode(WeatherPopupContext* ctx, WeatherPopupViewMode mode) {
  if (!ctx) return;
  if (ctx->forecast_row) {
    if (mode == WeatherPopupViewMode::Week) {
      lv_obj_clear_flag(ctx->forecast_row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->forecast_row, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->detail_wrap) {
    if (mode == WeatherPopupViewMode::Day) {
      lv_obj_clear_flag(ctx->detail_wrap, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->detail_wrap, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->value_row) {
    if (mode == WeatherPopupViewMode::Week || mode == WeatherPopupViewMode::Day) {
      lv_obj_clear_flag(ctx->value_row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->value_row, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->week_range_label) {
    if (mode == WeatherPopupViewMode::Week) {
      update_week_range_label(ctx);
      lv_obj_clear_flag(ctx->week_range_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->week_range_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->detail_title_label) {
    if (mode == WeatherPopupViewMode::Day) {
      lv_obj_clear_flag(ctx->detail_title_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ctx->detail_title_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
  ctx->view_mode = mode;
  update_mode_buttons(ctx);
}

static void clear_detail_view(WeatherPopupContext* ctx) {
  if (!ctx) return;

  if (ctx->detail_title_label) {
    lv_label_set_text(ctx->detail_title_label, "");
    lv_obj_add_flag(ctx->detail_title_label, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->week_range_label) {
    lv_label_set_text(ctx->week_range_label, "");
    lv_obj_add_flag(ctx->week_range_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ctx->detail_temp_chart && ctx->detail_temp_series) {
    lv_chart_set_point_count(ctx->detail_temp_chart, kDetailChartPointCount);
    lv_chart_set_all_value(ctx->detail_temp_chart, ctx->detail_temp_series, LV_CHART_POINT_NONE);
    lv_chart_set_range(ctx->detail_temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_refresh(ctx->detail_temp_chart);
  }

  if (ctx->detail_precip_chart && ctx->detail_precip_series) {
    lv_chart_set_point_count(ctx->detail_precip_chart, kDetailChartPointCount);
    lv_chart_set_all_value(ctx->detail_precip_chart, ctx->detail_precip_series, 0);
    lv_chart_set_range(ctx->detail_precip_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 10);
    lv_chart_refresh(ctx->detail_precip_chart);
  }
  if (ctx->detail_past_overlay) {
    lv_obj_add_flag(ctx->detail_past_overlay, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->detail_end_overlay) {
    lv_obj_add_flag(ctx->detail_end_overlay, LV_OBJ_FLAG_HIDDEN);
  }
  for (int i = 0; i < kDetailChartPointCount; ++i) {
    if (ctx->detail_precip_bars[i]) {
      lv_obj_add_flag(ctx->detail_precip_bars[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (ctx->detail_y_max_label) {
    lv_label_set_text(ctx->detail_y_max_label, "");
    lv_obj_add_flag(ctx->detail_y_max_label, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->detail_y_min_label) {
    lv_label_set_text(ctx->detail_y_min_label, "");
    lv_obj_add_flag(ctx->detail_y_min_label, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->detail_y_max_line) lv_obj_add_flag(ctx->detail_y_max_line, LV_OBJ_FLAG_HIDDEN);
  if (ctx->detail_y_min_line) lv_obj_add_flag(ctx->detail_y_min_line, LV_OBJ_FLAG_HIDDEN);
  for (int i = 0; i < kDetailMarkerCount; ++i) {
    if (ctx->detail_icon_guides[i]) lv_obj_add_flag(ctx->detail_icon_guides[i], LV_OBJ_FLAG_HIDDEN);
    if (ctx->detail_precip_guides[i]) lv_obj_add_flag(ctx->detail_precip_guides[i], LV_OBJ_FLAG_HIDDEN);
  }

  for (int i = 0; i < kDetailMarkerCount; ++i) {
    if (ctx->detail_time_lines[i]) lv_obj_add_flag(ctx->detail_time_lines[i], LV_OBJ_FLAG_HIDDEN);
    if (ctx->detail_time_labels[i]) {
      lv_label_set_text(ctx->detail_time_labels[i], "");
      lv_obj_add_flag(ctx->detail_time_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->detail_icon_labels[i]) {
      lv_label_set_text(ctx->detail_icon_labels[i], "");
      lv_obj_add_flag(ctx->detail_icon_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->detail_temp_point_dots[i]) {
      lv_obj_add_flag(ctx->detail_temp_point_dots[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->detail_temp_value_labels[i]) {
      lv_label_set_text(ctx->detail_temp_value_labels[i], "");
      lv_obj_add_flag(ctx->detail_temp_value_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->detail_precip_amount_labels[i]) {
      lv_label_set_text(ctx->detail_precip_amount_labels[i], "");
      lv_obj_add_flag(ctx->detail_precip_amount_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (ctx->detail_probability_labels[i]) {
      lv_label_set_text(ctx->detail_probability_labels[i], "");
      lv_obj_add_flag(ctx->detail_probability_labels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ctx->detail_current_dot) {
    lv_obj_add_flag(ctx->detail_current_dot, LV_OBJ_FLAG_HIDDEN);
  }
}

static void clear_forecast(WeatherPopupContext* ctx) {
  if (!ctx) return;

  for (int i = 0; i < kCols; ++i) {
    ctx->forecast_data[i] = ForecastData();
  }

  clear_forecast_chart(ctx);

  for (int i = 0; i < kCols; ++i) {
    ForecastWidgets& fw = ctx->forecast[i];
    if (fw.day_label) {
      lv_label_set_text(fw.day_label, "");
      lv_obj_add_flag(fw.day_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.icon_label) {
      lv_label_set_text(fw.icon_label, "");
      lv_obj_add_flag(fw.icon_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.high_label) {
      lv_label_set_text(fw.high_label, "");
      lv_obj_add_flag(fw.high_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.low_label) {
      lv_label_set_text(fw.low_label, "");
      lv_obj_add_flag(fw.low_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.precip_bar) {
      lv_obj_add_flag(fw.precip_bar, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.precip_amount_label) {
      lv_label_set_text(fw.precip_amount_label, "");
      lv_obj_add_flag(fw.precip_amount_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (fw.precip_probability_label) {
      lv_label_set_text(fw.precip_probability_label, "");
      lv_obj_add_flag(fw.precip_probability_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static bool update_detail_view(WeatherPopupContext* ctx, int day_index) {
  if (!ctx) return false;
  clear_detail_view(ctx);

  if (day_index < 0 || day_index >= kCols) return false;
  const ForecastData& day = ctx->forecast_data[day_index];
  if (!day.active || !day.date_local.length()) return false;

  if (ctx->detail_title_label) {
    String title = format_detail_day_title(day);
    if (!title.length()) title = day.day.length() ? day.day : String("--");
    lv_label_set_text(ctx->detail_title_label, title.c_str());
  }

  if (!ctx->detail_temp_chart || !ctx->detail_temp_series ||
      !ctx->detail_precip_chart || !ctx->detail_precip_series) {
    return true;
  }

  const lv_color_t popup_bg_color =
      ctx->card ? lv_obj_get_style_bg_color(ctx->card, LV_PART_MAIN) : lv_color_hex(ctx->bg_color);
  const lv_color_t past_overlay_label_color = lv_color_mix(lv_color_white(), popup_bg_color, 96);

  constexpr int kDetailDataHourCount = 24;
  const int marker_hours[kDetailMarkerCount] = {0, 6, 12, 18, 24};
  bool has_temp = false;
  int min_temp = 0;
  int max_temp = 0;
  bool has_precipitation = false;
  int max_precipitation = 0;
  bool marker_has_icon[kDetailMarkerCount] = {};
  String marker_icon[kDetailMarkerCount];
  bool marker_has_temp[kDetailMarkerCount] = {};
  float marker_temp[kDetailMarkerCount] = {};
  bool marker_has_precipitation[kDetailMarkerCount] = {};
  float marker_precipitation[kDetailMarkerCount] = {};
  bool marker_has_probability[kDetailMarkerCount] = {};
  float marker_probability[kDetailMarkerCount] = {};
  bool point_has_temp[kDetailChartPointCount] = {};
  bool point_has_precipitation[kDetailChartPointCount] = {};
  float point_precipitation[kDetailChartPointCount] = {};
  String next_date_local;

  for (int i = day_index + 1; i < kCols; ++i) {
    if (ctx->forecast_data[i].active && ctx->forecast_data[i].date_local.length()) {
      next_date_local = ctx->forecast_data[i].date_local;
      break;
    }
  }

  String now_date;
  int now_hour = 0;
  const bool clip_today = get_local_now_parts(now_date, now_hour) && now_date == day.date_local;

  lv_chart_set_point_count(ctx->detail_temp_chart, kDetailChartPointCount);
  lv_chart_set_all_value(ctx->detail_temp_chart, ctx->detail_temp_series, LV_CHART_POINT_NONE);
  lv_chart_set_point_count(ctx->detail_precip_chart, kDetailChartPointCount);
  lv_chart_set_all_value(ctx->detail_precip_chart, ctx->detail_precip_series, 0);

  for (int i = 0; i < kHourlyForecastMax; ++i) {
    const HourlyForecastData& hour = ctx->hourly[i];
    if (!hour.active) continue;
    if (hour.hour_local < 0 || hour.hour_local >= kDetailDataHourCount) continue;
    const bool is_same_day = hour.date_local == day.date_local;
    const bool is_next_day_midnight =
        next_date_local.length() && hour.date_local == next_date_local && hour.hour_local == 0;
    if (is_same_day) {
      if (clip_today && hour.hour_local < now_hour) continue;
    } else if (!is_next_day_midnight) {
      continue;
    }

    if (hour.has_temp) {
      const int scaled = scale_forecast_temp(hour.temp);
      const int point_id = is_next_day_midnight ? 24 : hour.hour_local;
      lv_chart_set_value_by_id(ctx->detail_temp_chart, ctx->detail_temp_series, point_id, scaled);
      point_has_temp[point_id] = true;
      if (!has_temp) {
        min_temp = scaled;
        max_temp = scaled;
        has_temp = true;
      } else {
        if (scaled < min_temp) min_temp = scaled;
        if (scaled > max_temp) max_temp = scaled;
      }
    }

    if (hour.has_precipitation) {
      const int point_id = is_next_day_midnight ? 24 : hour.hour_local;
      const int scaled_precip = static_cast<int>(lroundf(hour.precipitation * 10.0f));
      point_has_precipitation[point_id] = true;
      point_precipitation[point_id] = hour.precipitation;
      if (scaled_precip > 0) {
        has_precipitation = true;
        if (scaled_precip > max_precipitation) max_precipitation = scaled_precip;
      }
    }

    for (int marker = 0; marker < kDetailMarkerCount; ++marker) {
      const bool is_marker_same_day =
          is_same_day && hour.hour_local == marker_hours[marker];
      const bool is_marker_next_day =
          is_next_day_midnight && marker == (kDetailMarkerCount - 1);
      if (is_marker_same_day || is_marker_next_day) {
        if (hour.icon.length()) {
          marker_has_icon[marker] = true;
          marker_icon[marker] = hour.icon;
        }
        if (hour.has_temp) {
          marker_has_temp[marker] = true;
          marker_temp[marker] = hour.temp;
        }
        if (hour.has_precipitation) {
          marker_has_precipitation[marker] = true;
          marker_precipitation[marker] = hour.precipitation;
        }
        if (hour.has_precipitation_probability) {
          marker_has_probability[marker] = true;
          marker_probability[marker] = hour.precipitation_probability;
        }
        break;
      }
    }
  }

  if (has_temp) {
    int padding = (max_temp - min_temp) / 4;
    if (padding < 10) padding = 10;
    if (max_temp == min_temp) padding = 14;
    ctx->detail_temp_min = min_temp - padding;
    ctx->detail_temp_max = max_temp + padding;
    lv_chart_set_range(ctx->detail_temp_chart,
                       LV_CHART_AXIS_PRIMARY_Y,
                       ctx->detail_temp_min,
                       ctx->detail_temp_max);
    lv_chart_refresh(ctx->detail_temp_chart);
  } else {
    ctx->detail_temp_min = 0;
    ctx->detail_temp_max = 100;
    lv_chart_set_range(ctx->detail_temp_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_refresh(ctx->detail_temp_chart);
  }

  if (!has_precipitation) {
    max_precipitation = 20;
  } else if (max_precipitation < 30) {
    max_precipitation = 30;
  }
  lv_chart_set_range(ctx->detail_precip_chart, LV_CHART_AXIS_PRIMARY_Y, 0, max_precipitation);
  lv_chart_refresh(ctx->detail_precip_chart);

  lv_obj_t* chart_wrap = lv_obj_get_parent(ctx->detail_temp_chart);
  if (!chart_wrap) return true;
  lv_obj_update_layout(chart_wrap);
  lv_obj_update_layout(ctx->detail_temp_chart);
  lv_obj_update_layout(ctx->detail_precip_chart);
  const lv_coord_t first_point_x = map_detail_chart_x(ctx, 0);
  const lv_coord_t second_point_x = map_detail_chart_x(ctx, 1);
  const lv_coord_t last_point_x = map_detail_chart_x(ctx, kDetailChartPointCount - 1);
  const lv_coord_t prev_point_x = map_detail_chart_x(ctx, kDetailChartPointCount - 2);
  const lv_coord_t left_overlay_bound =
      first_point_x - ((second_point_x - first_point_x) / 2);
  const lv_coord_t right_overlay_bound =
      last_point_x + ((last_point_x - prev_point_x) / 2);
  if (ctx->detail_past_overlay) {
    lv_obj_add_flag(ctx->detail_past_overlay, LV_OBJ_FLAG_HIDDEN);
    if (clip_today && now_hour > 0) {
      lv_coord_t overlay_right = map_detail_chart_x(ctx, now_hour);
      const lv_coord_t prev_x = map_detail_chart_x(ctx, now_hour - 1);
      overlay_right = prev_x + ((overlay_right - prev_x) / 2);
      const lv_coord_t overlay_left = (left_overlay_bound > 0) ? left_overlay_bound : 0;
      if (overlay_right > overlay_left) {
        lv_obj_set_pos(ctx->detail_past_overlay, overlay_left, kDetailDisabledOverlayTop);
        lv_obj_set_size(ctx->detail_past_overlay,
                        overlay_right - overlay_left,
                        kDetailDisabledOverlayHeight);
        lv_obj_clear_flag(ctx->detail_past_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ctx->detail_past_overlay);
      }
    }
  }
  const lv_coord_t chart_wrap_w = lv_obj_get_width(chart_wrap);
  const bool missing_end_marker =
      !point_has_temp[kDetailChartPointCount - 1] &&
      !point_has_precipitation[kDetailChartPointCount - 1] &&
      !marker_has_icon[kDetailMarkerCount - 1] &&
      !marker_has_temp[kDetailMarkerCount - 1] &&
      !marker_has_precipitation[kDetailMarkerCount - 1] &&
      !marker_has_probability[kDetailMarkerCount - 1];
  if (ctx->detail_end_overlay) {
    lv_obj_add_flag(ctx->detail_end_overlay, LV_OBJ_FLAG_HIDDEN);
    if (missing_end_marker) {
      lv_coord_t overlay_left = prev_point_x + ((last_point_x - prev_point_x) / 2);
      lv_coord_t overlay_right =
          (right_overlay_bound < chart_wrap_w) ? right_overlay_bound : chart_wrap_w;
      if (overlay_left < overlay_right) {
        lv_obj_set_pos(ctx->detail_end_overlay, overlay_left, kDetailDisabledOverlayTop);
        lv_obj_set_size(ctx->detail_end_overlay,
                        overlay_right - overlay_left,
                        kDetailDisabledOverlayHeight);
        lv_obj_clear_flag(ctx->detail_end_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ctx->detail_end_overlay);
      }
    }
  }
  for (int i = 0; i < kDetailMarkerCount; ++i) {
    if (!ctx->detail_precip_guides[i]) continue;
    lv_obj_add_flag(ctx->detail_precip_guides[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (int point_id = 0; point_id < kDetailChartPointCount; ++point_id) {
    if (!ctx->detail_precip_bars[point_id]) continue;
    lv_obj_add_flag(ctx->detail_precip_bars[point_id], LV_OBJ_FLAG_HIDDEN);
    if (!point_has_precipitation[point_id] || point_precipitation[point_id] <= 0.0f || max_precipitation <= 0) {
      continue;
    }

    const lv_coord_t center_x = map_detail_chart_x(ctx, point_id);
    const float scaled = (point_precipitation[point_id] * 10.0f) / static_cast<float>(max_precipitation);
    lv_coord_t bar_h = static_cast<lv_coord_t>(lroundf(scaled * kDetailPrecipChartHeight));
    if (bar_h < 1) bar_h = 1;
    if (bar_h > kDetailPrecipChartHeight) bar_h = kDetailPrecipChartHeight;
    lv_obj_set_pos(ctx->detail_precip_bars[point_id],
                   center_x - (kDetailPrecipBarWidth / 2),
                   kDetailPrecipChartTop + kDetailPrecipChartHeight - bar_h);
    lv_obj_set_size(ctx->detail_precip_bars[point_id], kDetailPrecipBarWidth, bar_h);
    lv_obj_clear_flag(ctx->detail_precip_bars[point_id], LV_OBJ_FLAG_HIDDEN);
  }

  for (int marker = 0; marker < kDetailMarkerCount; ++marker) {
    lv_coord_t marker_x = map_detail_chart_x(ctx, marker_hours[marker]);
    lv_coord_t marker_y = kDetailTempChartTop + (kDetailTempChartHeight / 2);
    const bool end_missing_marker =
        missing_end_marker && marker == (kDetailMarkerCount - 1);
    if (ctx->detail_temp_chart && ctx->detail_temp_series) {
      lv_point_t point;
      lv_chart_get_point_pos_by_id(ctx->detail_temp_chart,
                                   ctx->detail_temp_series,
                                   marker_hours[marker],
                                   &point);
      if (marker_has_temp[marker]) {
        marker_y = lv_obj_get_y(ctx->detail_temp_chart) + point.y;
      }
    }

    if (ctx->detail_time_lines[marker]) {
      if ((!clip_today || marker_hours[marker] >= now_hour) &&
          !(end_missing_marker && marker == (kDetailMarkerCount - 1))) {
        lv_obj_set_pos(ctx->detail_time_lines[marker], marker_x, kDetailTempGuideTop);
        lv_obj_set_size(ctx->detail_time_lines[marker], 1, kDetailTempGuideHeight);
        lv_obj_clear_flag(ctx->detail_time_lines[marker], LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (ctx->detail_precip_guides[marker]) {
      if ((!clip_today || marker_hours[marker] >= now_hour) &&
          !(end_missing_marker && marker == (kDetailMarkerCount - 1))) {
        lv_obj_set_pos(ctx->detail_precip_guides[marker], marker_x, kDetailPrecipGuideTop);
        lv_obj_set_size(ctx->detail_precip_guides[marker], 1, kDetailPrecipGuideHeight);
        lv_obj_clear_flag(ctx->detail_precip_guides[marker], LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (ctx->detail_time_labels[marker]) {
      const bool past_marker = clip_today && marker_hours[marker] < now_hour;
      lv_obj_set_style_text_color(ctx->detail_time_labels[marker],
                                  (past_marker || end_missing_marker)
                                      ? past_overlay_label_color
                                      : lv_color_white(),
                                  0);
      String label = format_detail_time_axis_label(marker_hours[marker]);
      lv_label_set_text(ctx->detail_time_labels[marker], label.c_str());
      lv_obj_update_layout(ctx->detail_time_labels[marker]);
      lv_coord_t lbl_w = lv_obj_get_width(ctx->detail_time_labels[marker]);
      lv_coord_t x = marker_x - (lbl_w / 2);
      if (x < 0) x = 0;
      if (x > chart_wrap_w - lbl_w) x = chart_wrap_w - lbl_w;
      lv_obj_set_pos(ctx->detail_time_labels[marker], x, kDetailTimeLabelY);
      lv_obj_clear_flag(ctx->detail_time_labels[marker], LV_OBJ_FLAG_HIDDEN);
    }

    if (ctx->detail_icon_labels[marker]) {
      if (marker_has_icon[marker]) {
        String icon_char = getMdiChar(marker_icon[marker]);
        if (icon_char.length()) {
          lv_label_set_text(ctx->detail_icon_labels[marker], icon_char.c_str());
          lv_obj_update_layout(ctx->detail_icon_labels[marker]);
          lv_coord_t icon_w = lv_obj_get_width(ctx->detail_icon_labels[marker]);
          lv_coord_t x = marker_x - (icon_w / 2);
          if (x < 0) x = 0;
          if (x > chart_wrap_w - icon_w) x = chart_wrap_w - icon_w;
          lv_obj_set_pos(ctx->detail_icon_labels[marker], x, kDetailIconY);
          lv_obj_clear_flag(ctx->detail_icon_labels[marker], LV_OBJ_FLAG_HIDDEN);
        } else {
          lv_label_set_text(ctx->detail_icon_labels[marker], "");
          lv_obj_add_flag(ctx->detail_icon_labels[marker], LV_OBJ_FLAG_HIDDEN);
        }
      } else {
        lv_label_set_text(ctx->detail_icon_labels[marker], "");
        lv_obj_add_flag(ctx->detail_icon_labels[marker], LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (ctx->detail_temp_point_dots[marker]) {
      lv_obj_add_flag(ctx->detail_temp_point_dots[marker], LV_OBJ_FLAG_HIDDEN);
    }

    if (ctx->detail_temp_value_labels[marker]) {
      if (marker_has_temp[marker]) {
        String label = format_forecast_chart_temp(marker_temp[marker]);
        lv_label_set_text(ctx->detail_temp_value_labels[marker], label.c_str());
        lv_obj_update_layout(ctx->detail_temp_value_labels[marker]);
        lv_coord_t lbl_w = lv_obj_get_width(ctx->detail_temp_value_labels[marker]);
        lv_coord_t lbl_h = lv_obj_get_height(ctx->detail_temp_value_labels[marker]);
        lv_coord_t x = marker_x - (lbl_w / 2);
        if (x < 0) x = 0;
        if (x > chart_wrap_w - lbl_w) x = chart_wrap_w - lbl_w;
        lv_coord_t y = marker_y - lbl_h - kDetailTempValueGap;
        const lv_coord_t min_y = kDetailIconY + 28;
        const lv_coord_t max_y = kDetailPrecipChartTop - lbl_h - 6;
        if (y < min_y) y = min_y;
        if (y > max_y) y = max_y;
        lv_obj_set_pos(ctx->detail_temp_value_labels[marker], x, y);
        lv_obj_clear_flag(ctx->detail_temp_value_labels[marker], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(ctx->detail_temp_value_labels[marker], "");
        lv_obj_add_flag(ctx->detail_temp_value_labels[marker], LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (ctx->detail_precip_amount_labels[marker]) {
      if (marker_has_precipitation[marker]) {
        String label = format_precipitation_amount(marker_precipitation[marker], ctx->precipitation_unit);
        lv_label_set_text(ctx->detail_precip_amount_labels[marker], label.c_str());
        lv_obj_update_layout(ctx->detail_precip_amount_labels[marker]);
        lv_coord_t lbl_w = lv_obj_get_width(ctx->detail_precip_amount_labels[marker]);
        lv_coord_t x = marker_x - (lbl_w / 2);
        if (x < 0) x = 0;
        if (x > chart_wrap_w - lbl_w) x = chart_wrap_w - lbl_w;
        lv_obj_set_pos(ctx->detail_precip_amount_labels[marker], x, kDetailAmountTop);
        lv_obj_clear_flag(ctx->detail_precip_amount_labels[marker], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(ctx->detail_precip_amount_labels[marker], "");
        lv_obj_add_flag(ctx->detail_precip_amount_labels[marker], LV_OBJ_FLAG_HIDDEN);
      }
    }

    if (ctx->detail_probability_labels[marker]) {
      if (marker_has_probability[marker] && (!clip_today || marker_hours[marker] >= now_hour)) {
        String label = format_precipitation_probability(marker_probability[marker]);
        lv_label_set_text(ctx->detail_probability_labels[marker], label.c_str());
        lv_obj_update_layout(ctx->detail_probability_labels[marker]);
        lv_coord_t lbl_w = lv_obj_get_width(ctx->detail_probability_labels[marker]);
        lv_coord_t x = marker_x - (lbl_w / 2);
        if (x < 0) x = 0;
        if (x > chart_wrap_w - lbl_w) x = chart_wrap_w - lbl_w;
        lv_obj_set_pos(ctx->detail_probability_labels[marker], x, kDetailProbabilityTop);
        lv_obj_clear_flag(ctx->detail_probability_labels[marker], LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(ctx->detail_probability_labels[marker], "");
        lv_obj_add_flag(ctx->detail_probability_labels[marker], LV_OBJ_FLAG_HIDDEN);
      }
    }
  }

  if (ctx->detail_current_dot) {
    lv_obj_add_flag(ctx->detail_current_dot, LV_OBJ_FLAG_HIDDEN);
    if (clip_today && now_hour >= 0 && now_hour < kDetailDataHourCount && point_has_temp[now_hour]) {
      lv_point_t point;
      lv_chart_get_point_pos_by_id(ctx->detail_temp_chart,
                                   ctx->detail_temp_series,
                                   now_hour,
                                   &point);
      const lv_coord_t center_x = map_detail_chart_x(ctx, now_hour);
      const lv_coord_t center_y = lv_obj_get_y(ctx->detail_temp_chart) + point.y;
      lv_obj_set_pos(ctx->detail_current_dot,
                     center_x - (kDetailCurrentHourDotSize / 2),
                     center_y - (kDetailCurrentHourDotSize / 2));
      lv_obj_clear_flag(ctx->detail_current_dot, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(ctx->detail_current_dot);
    }
  }

  if ((ctx->detail_past_overlay && !lv_obj_has_flag(ctx->detail_past_overlay, LV_OBJ_FLAG_HIDDEN)) ||
      (ctx->detail_end_overlay && !lv_obj_has_flag(ctx->detail_end_overlay, LV_OBJ_FLAG_HIDDEN))) {
    for (int i = 0; i < kDetailMarkerCount; ++i) {
      if (ctx->detail_time_labels[i]) lv_obj_move_foreground(ctx->detail_time_labels[i]);
      if (ctx->detail_icon_labels[i]) lv_obj_move_foreground(ctx->detail_icon_labels[i]);
      if (ctx->detail_temp_value_labels[i]) lv_obj_move_foreground(ctx->detail_temp_value_labels[i]);
      if (ctx->detail_precip_amount_labels[i]) lv_obj_move_foreground(ctx->detail_precip_amount_labels[i]);
      if (ctx->detail_probability_labels[i]) lv_obj_move_foreground(ctx->detail_probability_labels[i]);
    }
  }

  return true;
}

static void show_week_view(WeatherPopupContext* ctx) {
  if (!ctx) return;
  set_popup_view_mode(ctx, WeatherPopupViewMode::Week);
}

static void show_day_view(WeatherPopupContext* ctx, int day_index) {
  if (!ctx) return;
  if (day_index < 0 || day_index >= kCols || !ctx->forecast_data[day_index].active) {
    day_index = get_default_day_index(ctx);
  }
  if (day_index < 0 || !update_detail_view(ctx, day_index)) {
    show_week_view(ctx);
    return;
  }
  ctx->selected_day_index = day_index;
  set_popup_view_mode(ctx, WeatherPopupViewMode::Day);
}

static void reset_weather_popup_content(WeatherPopupContext* ctx) {
  if (!ctx) return;

  if (ctx->icon_label) {
    lv_label_set_text(ctx->icon_label, "");
    lv_obj_add_flag(ctx->icon_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ctx->condition_label) {
    lv_label_set_text(ctx->condition_label, "");
    lv_obj_add_flag(ctx->condition_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ctx->condition_sep_label) {
    lv_label_set_text(ctx->condition_sep_label, "");
    lv_obj_add_flag(ctx->condition_sep_label, LV_OBJ_FLAG_HIDDEN);
  }

  if (ctx->temp_label) {
    lv_label_set_text(ctx->temp_label, "--");
    lv_obj_clear_flag(ctx->temp_label, LV_OBJ_FLAG_HIDDEN);
  }

  ctx->selected_day_index = -1;
  clear_hourly(ctx);
  clear_forecast(ctx);
  clear_detail_view(ctx);
  show_week_view(ctx);
  align_header_row(ctx->card, ctx->location_label, ctx->icon_label);
}

static void request_weather_for_context(WeatherPopupContext* ctx) {
  if (!ctx || !ctx->entity_id.length()) return;
  if (ctx->entity_id.startsWith("__")) return;
  mqttPublishWeatherRequest(ctx->entity_id.c_str());
}

static void apply_weather_payload(WeatherPopupContext* ctx, const char* payload) {
  if (!ctx || !payload || !*payload) return;
  String json = payload;
  json.trim();
  if (!json.length()) return;

  String previous_selected_date;
  if (ctx->selected_day_index >= 0 && ctx->selected_day_index < kCols) {
    previous_selected_date = ctx->forecast_data[ctx->selected_day_index].date_local;
  }
  WeatherPopupViewMode previous_mode = ctx->view_mode;

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
  String precipitation_unit;
  String units_obj;
  if (extract_json_object_field(json, "units", units_obj)) {
    extract_json_string_field(units_obj, "temperature", unit);
    extract_json_string_field(units_obj, "precipitation", precipitation_unit);
  } else {
    extract_json_string_field(json, "temperature_unit", unit);
    extract_json_string_field(json, "precipitation_unit", precipitation_unit);
  }
  decode_basic_json_escapes(unit);
  decode_basic_json_escapes(precipitation_unit);
  if (unit.length()) {
    ctx->unit = unit;
  } else {
    unit = ctx->unit;
  }
  if (precipitation_unit.length()) {
    ctx->precipitation_unit = precipitation_unit;
  } else {
    precipitation_unit = ctx->precipitation_unit;
  }
  if (!precipitation_unit.length()) {
    precipitation_unit = "mm";
    ctx->precipitation_unit = precipitation_unit;
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

  clear_forecast(ctx);
  clear_hourly(ctx);

  String forecast_raw;
  if (extract_json_array_field(json, "forecast", forecast_raw) && forecast_raw.indexOf('{') >= 0) {
    int cursor = 0;
    int forecast_count = 0;
    String obj;
    while (forecast_count < kCols && next_json_object_in_array(forecast_raw, cursor, obj)) {
      String f_condition;
      String f_icon;
      String f_day;
      String f_datetime;
      String f_date_local;
      float f_high = 0.0f;
      float f_low = 0.0f;
      float f_precipitation = 0.0f;
      float f_probability = 0.0f;
      bool f_has_high = extract_json_number_or_string_field(obj, "temperature", f_high);
      bool f_has_low = false;
      bool f_has_precipitation =
          extract_json_number_or_string_field(obj, "precipitation", f_precipitation);
      bool f_has_probability =
          extract_json_number_or_string_field(obj, "precipitation_probability", f_probability);
      if (extract_json_number_or_string_field(obj, "templow", f_low)) f_has_low = true;
      if (!f_has_low && extract_json_number_or_string_field(obj, "temperature_low", f_low)) f_has_low = true;
      if (!f_has_low && extract_json_number_or_string_field(obj, "temp_low", f_low)) f_has_low = true;
      if (!f_has_low && extract_json_number_or_string_field(obj, "low", f_low)) f_has_low = true;

      extract_json_string_field(obj, "condition", f_condition);
      extract_json_string_field(obj, "icon", f_icon);
      extract_json_string_field(obj, "datetime", f_datetime);
      extract_json_string_field(obj, "date_local", f_date_local);

      if (!f_date_local.length() && f_datetime.length()) {
        f_date_local = iso_date_part(f_datetime);
      }
      if (f_date_local.length()) {
        f_day = weekday_from_iso(f_date_local);
      } else if (f_datetime.length()) {
        f_day = weekday_from_iso(f_datetime);
      }
      if (!f_icon.length() && f_condition.length()) {
        f_icon = weather_icon_from_condition(f_condition);
      }

      ForecastData& data = ctx->forecast_data[forecast_count];
      data.active = true;
      data.day = f_day.length() ? f_day : String("--");
      data.date_local = f_date_local;
      data.icon = f_icon;
      data.has_high = f_has_high;
      data.high = f_high;
      data.has_low = f_has_low;
      data.low = f_low;
      data.has_precipitation = f_has_precipitation;
      data.precipitation = f_precipitation;
      data.has_precipitation_probability = f_has_probability;
      data.precipitation_probability = f_probability;
      ++forecast_count;
    }
  }

  String hourly_raw;
  if (extract_json_array_field(json, "forecast_hourly", hourly_raw) && hourly_raw.indexOf('{') >= 0) {
    int cursor = 0;
    int hourly_count = 0;
    String obj;
    while (hourly_count < kHourlyForecastMax && next_json_object_in_array(hourly_raw, cursor, obj)) {
      String h_condition;
      String h_icon;
      String h_datetime;
      String h_date_local;
      float h_hour_local = -1.0f;
      float h_temp = 0.0f;
      float h_precipitation = 0.0f;
      float h_probability = 0.0f;

      extract_json_string_field(obj, "condition", h_condition);
      if (!h_condition.length()) {
        extract_json_string_field(obj, "c", h_condition);
      }
      extract_json_string_field(obj, "icon", h_icon);
      if (!h_icon.length()) {
        extract_json_string_field(obj, "i", h_icon);
      }
      extract_json_string_field(obj, "datetime", h_datetime);
      extract_json_string_field(obj, "date_local", h_date_local);
      if (!h_date_local.length()) {
        extract_json_string_field(obj, "d", h_date_local);
      }
      if (!extract_json_number_or_string_field(obj, "hour_local", h_hour_local)) {
        extract_json_number_or_string_field(obj, "h", h_hour_local);
      }

      if (!h_icon.length() && h_condition.length()) {
        h_icon = weather_icon_from_condition(h_condition);
      }
      if (!h_date_local.length() && h_datetime.length()) {
        h_date_local = iso_date_part(h_datetime);
      }
      if (h_hour_local < 0.0f && h_datetime.length()) {
        h_hour_local = static_cast<float>(iso_hour_part(h_datetime));
      }
      if (!h_date_local.length() || h_hour_local < 0.0f) continue;

      HourlyForecastData& hour = ctx->hourly[hourly_count];
      hour = HourlyForecastData();
      hour.active = true;
      hour.date_local = h_date_local;
      hour.hour_local = static_cast<int>(lroundf(h_hour_local));
      hour.icon = h_icon;
      hour.has_temp = extract_json_number_or_string_field(obj, "temperature", h_temp) ||
                      extract_json_number_or_string_field(obj, "t", h_temp);
      hour.temp = h_temp;
      hour.has_precipitation = extract_json_number_or_string_field(obj, "precipitation", h_precipitation) ||
                               extract_json_number_or_string_field(obj, "p", h_precipitation);
      hour.precipitation = h_precipitation;
      hour.has_precipitation_probability =
          extract_json_number_or_string_field(obj, "precipitation_probability", h_probability) ||
          extract_json_number_or_string_field(obj, "pp", h_probability);
      hour.precipitation_probability = h_probability;
      ++hourly_count;
    }
  }

  update_forecast_graph(ctx);
  update_week_range_label(ctx);

  int selected_day_index = ctx->selected_day_index;
  if (previous_selected_date.length()) {
    int by_date = find_active_day_index(ctx, previous_selected_date);
    if (by_date >= 0) selected_day_index = by_date;
  }
  if (selected_day_index < 0 || selected_day_index >= kCols ||
      !ctx->forecast_data[selected_day_index].active) {
    selected_day_index = get_default_day_index(ctx);
  }
  ctx->selected_day_index = selected_day_index;

  if (previous_mode == WeatherPopupViewMode::Day) {
    show_day_view(ctx, selected_day_index);
  } else {
    show_week_view(ctx);
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
  uint32_t color = ctx->bg_color ? ctx->bg_color : 0x2A2A2A;
  if (ctx->card) {
    lv_obj_set_style_bg_color(ctx->card, lv_color_hex(color), 0);
  }
  for (int i = 0; i < kDetailMarkerCount; ++i) {
    if (ctx->detail_temp_value_labels[i]) {
      lv_obj_set_style_bg_color(ctx->detail_temp_value_labels[i], lv_color_hex(color), 0);
    }
  }
  for (int i = 0; i < ctx->detail_disabled_separator_count; ++i) {
    if (ctx->detail_disabled_separators[i]) {
      lv_obj_set_style_bg_color(ctx->detail_disabled_separators[i], lv_color_hex(color), 0);
    }
  }
  if (ctx->location_label) {
    String title = ctx->title;
    title.trim();
    if (!title.length()) title = "--";
    lv_label_set_text(ctx->location_label, title.c_str());
  }
  update_mode_buttons(ctx);
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

static void on_mode_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  WeatherPopupContext* ctx = static_cast<WeatherPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!ctx || !target) return;

  if (target == ctx->mode_week_btn) {
    show_week_view(ctx);
  }
}

static void on_detail_nav_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  WeatherPopupContext* ctx = static_cast<WeatherPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!ctx || !target) return;

  if (target == ctx->detail_prev_btn) {
    int prev_day = find_prev_active_day_index(ctx, ctx->selected_day_index);
    if (prev_day >= 0) show_day_view(ctx, prev_day);
  } else if (target == ctx->detail_next_btn) {
    int next_day = find_next_active_day_index(ctx, ctx->selected_day_index);
    if (next_day >= 0) show_day_view(ctx, next_day);
  }
}

static void on_header_action_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  WeatherPopupContext* ctx = static_cast<WeatherPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!ctx || !target) return;

  if (target == ctx->header_week_btn) {
    show_week_view(ctx);
    return;
  }

  if (target == ctx->header_today_btn) {
    String today_date;
    int today_hour = 0;
    if (get_local_now_parts(today_date, today_hour)) {
      const int today_day = find_active_day_index(ctx, today_date);
      if (today_day >= 0) {
        show_day_view(ctx, today_day);
        return;
      }
    }
    const int default_day = get_default_day_index(ctx);
    if (default_day >= 0) show_day_view(ctx, default_day);
  }
}

static void on_day_column_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  WeatherPopupContext* ctx = static_cast<WeatherPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!ctx || !target) return;

  for (int i = 0; i < kCols; ++i) {
    if (ctx->forecast[i].column == target) {
      show_day_view(ctx, i);
      return;
    }
  }
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
  uint32_t popup_tile_bg_color = init.bg_color ? init.bg_color : 0x2A2A2A;
  ctx->bg_color = popup_tile_bg_color;
  lv_obj_set_style_bg_color(card, lv_color_hex(popup_tile_bg_color), 0);
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
  set_label_style(location, lv_color_white(), &ui_font_24);
  lv_label_set_long_mode(location, LV_LABEL_LONG_DOT);
  lv_obj_set_width(location, LV_PCT(38));
  lv_obj_align(location, LV_ALIGN_TOP_LEFT, 78, 10);

  lv_obj_t* icon = lv_label_create(card);
  ctx->icon_label = icon;
  set_label_style(icon, lv_color_white(), FONT_MDI_ICONS);
  lv_label_set_text(icon, "");
  lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);

  auto make_header_action_button = [&](const char* text, int x_ofs, const lv_font_t* font) -> lv_obj_t* {
    lv_obj_t* btn = lv_button_create(card);
    lv_obj_set_size(btn, 96, 58);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_anim_time(btn, 0, 0);
    lv_obj_set_style_anim_time(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 0, 0);
    lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, 0);
    lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(btn, 0, 0);
    lv_obj_set_style_translate_y(btn, 0, LV_STATE_PRESSED);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, x_ofs, 13);
    lv_obj_set_ext_click_area(btn, 12);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, on_header_action_click, LV_EVENT_CLICKED, ctx);
    lv_obj_t* label = lv_label_create(btn);
    set_label_style(label, lv_color_white(), font);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
  };

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
  lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 6, -6);
  lv_obj_set_ext_click_area(close_btn, 8);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(close_btn, on_close_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(close_btn, on_close_click, LV_EVENT_RELEASED, ctx);
  lv_obj_t* close_label = lv_label_create(close_btn);
  set_label_style(close_label, lv_color_white(), FONT_MDI_ICONS);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);

  ctx->header_week_btn = make_header_action_button("7D", -106, FONT_UNIT);
  ctx->header_today_btn = make_header_action_button(weather_today_button_text(), -214, FONT_UNIT);

  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 8, 0);

  lv_obj_t* mode_row = lv_obj_create(card);
  ctx->mode_row = mode_row;
  lv_obj_remove_style_all(mode_row);
  lv_obj_set_size(mode_row, kModeButtonWidth, kModeButtonHeight);
  lv_obj_align(mode_row, LV_ALIGN_TOP_LEFT, 0, kModeRowOffsetY);
  lv_obj_set_style_bg_opa(mode_row, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(mode_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(mode_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(mode_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(mode_row, kModeButtonGap, 0);
  lv_obj_clear_flag(mode_row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(mode_row, LV_OBJ_FLAG_HIDDEN);

  auto make_mode_button = [&](const char* text) -> lv_obj_t* {
    lv_obj_t* btn = lv_button_create(mode_row);
    lv_obj_set_size(btn, kModeButtonWidth, kModeButtonHeight);
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
    set_label_style(label, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
  };

  ctx->mode_week_btn = make_mode_button(getMdiChar("arrow-left").c_str());
  ctx->mode_day_btn = nullptr;
  lv_obj_add_event_cb(ctx->mode_week_btn, on_mode_click, LV_EVENT_CLICKED, ctx);

  lv_obj_t* value_row = lv_obj_create(card);
  ctx->value_row = value_row;
  lv_obj_remove_style_all(value_row);
  lv_obj_set_size(value_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(value_row, 14, 0);
  lv_obj_set_style_bg_opa(value_row, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(value_row, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* condition_label = lv_label_create(value_row);
  ctx->condition_label = condition_label;
  set_label_style(condition_label, lv_color_white(), FONT_VALUE);
  lv_label_set_long_mode(condition_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(condition_label, LV_SIZE_CONTENT);
  lv_obj_set_style_max_width(condition_label, 250, 0);
  lv_label_set_text(condition_label, "--");
  lv_obj_add_flag(condition_label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* sep_label = lv_label_create(value_row);
  ctx->condition_sep_label = sep_label;
  set_label_style(sep_label, lv_color_white(), FONT_VALUE);
  lv_label_set_text(sep_label, "|");
  lv_obj_add_flag(sep_label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* temp_label = lv_label_create(value_row);
  ctx->temp_label = temp_label;
  set_label_style(temp_label, lv_color_white(), FONT_VALUE);
  lv_label_set_text(temp_label, "--");
  lv_obj_align(value_row, LV_ALIGN_TOP_MID, 0, kSummaryRowTop);

  lv_obj_t* week_range_label = lv_label_create(card);
  ctx->week_range_label = week_range_label;
  set_label_style(week_range_label, lv_color_white(), &ui_font_24);
  lv_label_set_long_mode(week_range_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(week_range_label, (kCardWidth - (kCardPad * 2)) - 260);
  lv_obj_set_style_text_align(week_range_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(week_range_label, "");
  lv_obj_align(week_range_label, LV_ALIGN_TOP_MID, 0, kDetailHeaderSubrowTop);
  lv_obj_add_flag(week_range_label, LV_OBJ_FLAG_HIDDEN);

  const lv_coord_t forecast_total_w = kCardWidth - (kCardPad * 2);
  const lv_coord_t forecast_col_w =
      (forecast_total_w - (kForecastSidePad * 2) - (kForecastColGap * (kCols - 1))) / kCols;
  const lv_coord_t forecast_first_center = kForecastSidePad + (forecast_col_w / 2);
  const lv_coord_t forecast_last_center =
      kForecastSidePad + ((kCols - 1) * (forecast_col_w + kForecastColGap)) + (forecast_col_w / 2);
  const lv_coord_t forecast_chart_x = forecast_first_center - kForecastChartSideInset;
  const lv_coord_t forecast_chart_w =
      (kCols > 1)
          ? (forecast_last_center - forecast_first_center) + (kForecastChartSideInset * 2)
          : forecast_col_w + (kForecastChartSideInset * 2);

  lv_obj_t* forecast_row = lv_obj_create(card);
  ctx->forecast_row = forecast_row;
  lv_obj_remove_style_all(forecast_row);
  lv_obj_set_size(forecast_row, forecast_total_w, kForecastRowHeight);
  lv_obj_set_style_bg_opa(forecast_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(forecast_row, 0, 0);
  lv_obj_remove_flag(forecast_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(forecast_row, 0, kForecastRowTop);

  lv_obj_t* temp_chart = lv_chart_create(forecast_row);
  ctx->forecast_temp_chart = temp_chart;
  lv_obj_set_size(temp_chart, forecast_chart_w, kForecastTempChartHeight);
  lv_obj_set_pos(temp_chart, forecast_chart_x, kForecastTempChartTop);
  lv_obj_set_style_bg_opa(temp_chart, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(temp_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_top(temp_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(temp_chart, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_left(temp_chart, kForecastChartSideInset, LV_PART_MAIN);
  lv_obj_set_style_pad_right(temp_chart, kForecastChartSideInset, LV_PART_MAIN);
  lv_obj_set_style_line_width(temp_chart, 4, LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(temp_chart, true, LV_PART_ITEMS);
  lv_obj_set_style_size(temp_chart, 8, 8, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(temp_chart, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(temp_chart, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(temp_chart, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_remove_flag(temp_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(temp_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_chart_set_div_line_count(temp_chart, 0, 0);
  lv_chart_set_type(temp_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(temp_chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(temp_chart, kCols);
  ctx->forecast_high_series =
      lv_chart_add_series(temp_chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);
  ctx->forecast_low_series =
      lv_chart_add_series(temp_chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);

  for (int i = 1; i < kCols; ++i) {
    lv_obj_t* divider = lv_obj_create(forecast_row);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, 1, kForecastRowHeight - 12);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_10, 0);
    lv_obj_set_style_radius(divider, 0, 0);
    lv_obj_remove_flag(divider, LV_OBJ_FLAG_SCROLLABLE);
    lv_coord_t divider_x = kForecastSidePad +
                           i * forecast_col_w +
                           (i - 1) * kForecastColGap +
                           (kForecastColGap / 2);
    lv_obj_set_pos(divider, divider_x, 10);
  }

  lv_obj_t* precip_base = lv_obj_create(forecast_row);
  lv_obj_remove_style_all(precip_base);
  lv_obj_set_size(precip_base,
                  forecast_total_w - (kForecastSidePad * 2),
                  1);
  lv_obj_set_style_bg_color(precip_base, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_bg_opa(precip_base, LV_OPA_20, 0);
  lv_obj_set_style_radius(precip_base, 0, 0);
  lv_obj_remove_flag(precip_base, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(precip_base, kForecastSidePad, kForecastPrecipChartTop + kForecastPrecipChartHeight);

  lv_obj_move_foreground(icon);
  lv_obj_move_foreground(location);
  lv_obj_move_foreground(close_btn);

  for (int i = 0; i < kCols; ++i) {
    lv_obj_t* col = lv_obj_create(forecast_row);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, forecast_col_w, kForecastRowHeight);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(col, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(col, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_ext_click_area(col, 4);
    lv_obj_set_pos(col, kForecastSidePad + i * (forecast_col_w + kForecastColGap), 0);
    lv_obj_add_event_cb(col, on_day_column_click, LV_EVENT_CLICKED, ctx);
    ctx->forecast[i].column = col;

    lv_obj_t* day = lv_label_create(col);
    set_label_style(day, lv_color_white(), FONT_TITLE);
    lv_label_set_long_mode(day, LV_LABEL_LONG_DOT);
    lv_obj_set_width(day, LV_PCT(100));
    lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(day, "");
    lv_obj_add_flag(day, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(day, 0, kForecastDayTop);

    lv_obj_t* icon_day = lv_label_create(col);
    set_label_style(icon_day, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(icon_day, "");
    lv_obj_add_flag(icon_day, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(icon_day, LV_ALIGN_TOP_MID, 0, kForecastIconTop);

    lv_obj_t* high = lv_label_create(col);
    set_label_style(high, lv_color_white(), FONT_TITLE);
    lv_obj_set_width(high, LV_PCT(100));
    lv_obj_set_style_text_align(high, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(high, "");
    lv_obj_add_flag(high, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(high, 0, kForecastTempChartTop);

    lv_obj_t* low = lv_label_create(col);
    set_label_style(low, lv_color_white(), FONT_TITLE);
    lv_obj_set_width(low, LV_PCT(100));
    lv_obj_set_style_text_align(low, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(low, "");
    lv_obj_add_flag(low, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(low, 0, kForecastTempChartTop + 24);

    lv_obj_t* precip_bar = lv_obj_create(col);
    lv_obj_remove_style_all(precip_bar);
    lv_obj_set_size(precip_bar, kForecastBarWidth, 0);
    lv_obj_set_style_bg_color(precip_bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(precip_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(precip_bar, 6, 0);
    lv_obj_add_flag(precip_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(precip_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* amount = lv_label_create(col);
    set_label_style(amount, lv_color_white(), FONT_TITLE);
    lv_obj_set_width(amount, LV_PCT(100));
    lv_obj_set_style_text_align(amount, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(amount, "");
    lv_obj_add_flag(amount, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(amount, 0, kForecastAmountTop);

    lv_obj_t* probability = lv_label_create(col);
    set_label_style(probability, lv_color_white(), FONT_TITLE);
    lv_obj_set_width(probability, LV_PCT(100));
    lv_obj_set_style_text_align(probability, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(probability, "");
    lv_obj_add_flag(probability, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(probability, 0, kForecastProbabilityTop);

    ctx->forecast[i].day_label = day;
    ctx->forecast[i].icon_label = icon_day;
    ctx->forecast[i].high_label = high;
    ctx->forecast[i].low_label = low;
    ctx->forecast[i].precip_bar = precip_bar;
    ctx->forecast[i].precip_amount_label = amount;
    ctx->forecast[i].precip_probability_label = probability;
  }

  lv_obj_t* detail_wrap = lv_obj_create(card);
  ctx->detail_wrap = detail_wrap;
  lv_obj_remove_style_all(detail_wrap);
  lv_obj_set_size(detail_wrap, forecast_total_w, kForecastRowHeight);
  lv_obj_set_style_bg_opa(detail_wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(detail_wrap, 0, 0);
  lv_obj_remove_flag(detail_wrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(detail_wrap, 0, kDetailRowTop);
  lv_obj_add_flag(detail_wrap, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* detail_title = lv_label_create(card);
  ctx->detail_title_label = detail_title;
  set_label_style(detail_title, lv_color_white(), &ui_font_24);
  lv_label_set_long_mode(detail_title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(detail_title, forecast_total_w - 300);
  lv_obj_set_style_text_align(detail_title, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(detail_title, "");
  lv_obj_align(detail_title, LV_ALIGN_TOP_MID, 0, kDetailTitleTop);
  lv_obj_add_flag(detail_title, LV_OBJ_FLAG_HIDDEN);

  auto make_detail_nav_button = [&](const char* mdi_icon, int x_ofs) -> lv_obj_t* {
    lv_obj_t* btn = lv_button_create(card);
    lv_obj_set_size(btn, 56, 56);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_anim_time(btn, 0, 0);
    lv_obj_set_style_anim_time(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_width(btn, 0, 0);
    lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(btn, 0, 0);
    lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(btn, 0, 0);
    lv_obj_set_style_translate_y(btn, 0, LV_STATE_PRESSED);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, x_ofs, kDetailTitleTop - 10);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t* label = lv_label_create(btn);
    set_label_style(label, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(label, getMdiChar(mdi_icon).c_str());
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, on_detail_nav_click, LV_EVENT_CLICKED, ctx);
    return btn;
  };

  ctx->detail_prev_btn = make_detail_nav_button("chevron-left", -240);
  ctx->detail_next_btn = make_detail_nav_button("chevron-right", 240);

  lv_obj_t* chart_wrap = lv_obj_create(detail_wrap);
  lv_obj_remove_style_all(chart_wrap);
  lv_obj_set_size(chart_wrap, forecast_total_w - kDetailSectionGutter, kDetailChartWrapHeight);
  lv_obj_set_pos(chart_wrap, kDetailSectionGutter, kDetailChartWrapTop);
  lv_obj_set_style_bg_opa(chart_wrap, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(chart_wrap, LV_OBJ_FLAG_SCROLLABLE);
  const lv_color_t popup_bg_color = lv_obj_get_style_bg_color(card, LV_PART_MAIN);
  lv_obj_t* past_overlay = lv_obj_create(chart_wrap);
  ctx->detail_past_overlay = past_overlay;
  lv_obj_remove_style_all(past_overlay);
  lv_obj_set_size(past_overlay, 0, kDetailDisabledOverlayHeight);
  lv_obj_set_pos(past_overlay, 0, kDetailDisabledOverlayTop);
  lv_obj_set_style_bg_opa(past_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(past_overlay, 0, 0);
  lv_obj_remove_flag(past_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(past_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(past_overlay, LV_OBJ_FLAG_HIDDEN);

  auto build_detail_overlay = [&](lv_obj_t* parent) {
    lv_obj_t* block = lv_obj_create(parent);
    lv_obj_remove_style_all(block);
    lv_obj_set_size(block, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(block, 0, 0);
    lv_obj_set_style_bg_color(block, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(block, 32, 0);
    lv_obj_set_style_border_width(block, 0, 0);
    lv_obj_set_style_radius(block, kDetailPastOverlayRadius, 0);
    lv_obj_remove_flag(block, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(block, LV_OBJ_FLAG_SCROLLABLE);

    auto add_separator = [&](lv_coord_t y) {
      lv_obj_t* sep = lv_obj_create(parent);
      lv_obj_remove_style_all(sep);
      lv_obj_set_size(sep, LV_PCT(100), kDetailDisabledBlockGap);
      lv_obj_set_pos(sep, 0, y);
      lv_obj_set_style_bg_color(sep, popup_bg_color, 0);
      lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(sep, 0, 0);
      lv_obj_set_style_radius(sep, 0, 0);
      lv_obj_remove_flag(sep, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
      if (ctx->detail_disabled_separator_count <
          static_cast<int>(sizeof(ctx->detail_disabled_separators) /
                           sizeof(ctx->detail_disabled_separators[0]))) {
        ctx->detail_disabled_separators[ctx->detail_disabled_separator_count++] = sep;
      }
    };

    add_separator(kDetailDisabledSeparator1Top);
    add_separator(kDetailDisabledSeparator2Top);
  };

  build_detail_overlay(past_overlay);

  lv_obj_t* end_overlay = lv_obj_create(chart_wrap);
  ctx->detail_end_overlay = end_overlay;
  lv_obj_remove_style_all(end_overlay);
  lv_obj_set_size(end_overlay, 0, kDetailDisabledOverlayHeight);
  lv_obj_set_pos(end_overlay, 0, kDetailDisabledOverlayTop);
  lv_obj_set_style_bg_opa(end_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(end_overlay, 0, 0);
  lv_obj_remove_flag(end_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(end_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(end_overlay, LV_OBJ_FLAG_HIDDEN);

  build_detail_overlay(end_overlay);

  lv_obj_t* y_max = lv_label_create(chart_wrap);
  ctx->detail_y_max_label = y_max;
  set_label_style(y_max, lv_color_white(), FONT_TITLE);
  lv_obj_set_style_text_align(y_max, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(y_max, kDetailYAxisWidth - 10);
  lv_label_set_text(y_max, "");
  lv_obj_set_pos(y_max, 0, 0);
  lv_obj_add_flag(y_max, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* y_min = lv_label_create(chart_wrap);
  ctx->detail_y_min_label = y_min;
  set_label_style(y_min, lv_color_white(), FONT_TITLE);
  lv_obj_set_style_text_align(y_min, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_width(y_min, kDetailYAxisWidth - 10);
  lv_label_set_text(y_min, "");
  lv_obj_set_pos(y_min, 0, kDetailTempChartHeight);
  lv_obj_add_flag(y_min, LV_OBJ_FLAG_HIDDEN);

  constexpr int kLineOverlap = 6;
  const int line_start = kDetailYAxisWidth - kLineOverlap;
  const int line_width = (forecast_total_w - kDetailSectionGutter) - line_start;
  auto make_guide_line = [&](int16_t y_pos) -> lv_obj_t* {
    lv_obj_t* line = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(line);
    lv_obj_set_size(line, line_width, 1);
    lv_obj_set_style_bg_color(line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_30, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(line, line_start, y_pos);
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    return line;
  };
  ctx->detail_y_max_line = make_guide_line(kDetailLabelOverhang);
  ctx->detail_y_min_line = make_guide_line(kDetailLabelOverhang + kDetailTempChartHeight - 1);

  for (int i = 0; i < kDetailMarkerCount; ++i) {
    lv_obj_t* icon_line = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(icon_line);
    lv_obj_set_size(icon_line, 1, 0);
    lv_obj_set_style_bg_color(icon_line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(icon_line, LV_OPA_30, 0);
    lv_obj_remove_flag(icon_line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(icon_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(icon_line, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_icon_guides[i] = icon_line;

    lv_obj_t* vline = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(vline);
    lv_obj_set_size(vline, 1, 0);
    lv_obj_set_style_bg_color(vline, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(vline, LV_OPA_30, 0);
    lv_obj_remove_flag(vline, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(vline, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(vline, 0, kDetailTempChartTop);
    lv_obj_add_flag(vline, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_time_lines[i] = vline;

    lv_obj_t* tlbl = lv_label_create(chart_wrap);
    set_label_style(tlbl, lv_color_white(), FONT_TITLE);
    lv_obj_set_style_text_align(tlbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(tlbl, "");
    lv_obj_set_pos(tlbl, 0, kDetailTimeLabelY);
    lv_obj_add_flag(tlbl, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_time_labels[i] = tlbl;

    lv_obj_t* ilbl = lv_label_create(chart_wrap);
    set_label_style(ilbl, lv_color_white(), FONT_MDI_ICONS);
    lv_obj_set_style_text_align(ilbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(ilbl, "");
    lv_obj_set_pos(ilbl, 0, kDetailIconY);
    lv_obj_add_flag(ilbl, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_icon_labels[i] = ilbl;

    lv_obj_t* dot = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, kDetailTempMarkerSize, kDetailTempMarkerSize);
    lv_obj_set_style_bg_color(dot, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_temp_point_dots[i] = dot;

    lv_obj_t* vlbl = lv_label_create(chart_wrap);
    set_label_style(vlbl, lv_color_white(), FONT_TITLE);
    lv_obj_set_style_text_align(vlbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(vlbl, lv_color_hex(ctx->bg_color ? ctx->bg_color : 0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(vlbl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(vlbl, 6, 0);
    lv_obj_set_style_pad_left(vlbl, 4, 0);
    lv_obj_set_style_pad_right(vlbl, 4, 0);
    lv_obj_set_style_pad_top(vlbl, 1, 0);
    lv_obj_set_style_pad_bottom(vlbl, 1, 0);
    lv_label_set_text(vlbl, "");
    lv_obj_add_flag(vlbl, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_temp_value_labels[i] = vlbl;

    lv_obj_t* albl = lv_label_create(chart_wrap);
    set_label_style(albl, lv_color_white(), FONT_TITLE);
    lv_obj_set_style_text_align(albl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(albl, "");
    lv_obj_set_pos(albl, 0, kDetailAmountTop);
    lv_obj_add_flag(albl, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_precip_amount_labels[i] = albl;

    lv_obj_t* plbl = lv_label_create(chart_wrap);
    set_label_style(plbl, lv_color_white(), FONT_TITLE);
    lv_obj_set_style_text_align(plbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(plbl, "");
    lv_obj_set_pos(plbl, 0, kDetailProbabilityTop);
    lv_obj_add_flag(plbl, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_probability_labels[i] = plbl;

    lv_obj_t* precip_line = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(precip_line);
    lv_obj_set_size(precip_line, 1, 0);
    lv_obj_set_style_bg_color(precip_line, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(precip_line, LV_OPA_30, 0);
    lv_obj_remove_flag(precip_line, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(precip_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(precip_line, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_precip_guides[i] = precip_line;
  }

  lv_obj_t* detail_temp_chart = lv_chart_create(chart_wrap);
  ctx->detail_temp_chart = detail_temp_chart;
  lv_obj_set_size(detail_temp_chart, LV_PCT(100), kDetailTempChartHeight);
  lv_obj_set_pos(detail_temp_chart, 0, kDetailTempChartTop);
  lv_obj_set_style_bg_opa(detail_temp_chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(detail_temp_chart, 0, 0);
  lv_obj_set_style_pad_left(detail_temp_chart, kDetailChartLeftInset, 0);
  lv_obj_set_style_pad_right(detail_temp_chart, kDetailChartRightInset, 0);
  lv_obj_set_style_pad_top(detail_temp_chart, 0, 0);
  lv_obj_set_style_pad_bottom(detail_temp_chart, 0, 0);
  lv_obj_set_style_line_width(detail_temp_chart, 3, LV_PART_ITEMS);
  lv_obj_set_style_line_opa(detail_temp_chart, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_line_color(detail_temp_chart, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_line_rounded(detail_temp_chart, true, LV_PART_ITEMS);
  lv_obj_set_style_size(detail_temp_chart, 6, 6, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(detail_temp_chart, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(detail_temp_chart, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(detail_temp_chart, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_remove_flag(detail_temp_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(detail_temp_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_chart_set_div_line_count(detail_temp_chart, 0, 0);
  lv_chart_set_type(detail_temp_chart, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(detail_temp_chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(detail_temp_chart, kDetailChartPointCount);
  ctx->detail_temp_series =
      lv_chart_add_series(detail_temp_chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);

  lv_obj_t* current_dot = lv_obj_create(chart_wrap);
  ctx->detail_current_dot = current_dot;
  lv_obj_remove_style_all(current_dot);
  lv_obj_set_size(current_dot, kDetailCurrentHourDotSize, kDetailCurrentHourDotSize);
  lv_obj_set_style_bg_color(current_dot, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(current_dot, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(current_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_remove_flag(current_dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(current_dot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(current_dot, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t* detail_precip_chart = lv_chart_create(chart_wrap);
  ctx->detail_precip_chart = detail_precip_chart;
  lv_obj_set_size(detail_precip_chart, LV_PCT(100), kDetailPrecipChartHeight);
  lv_obj_set_pos(detail_precip_chart, 0, kDetailPrecipChartTop);
  lv_obj_set_style_bg_opa(detail_precip_chart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(detail_precip_chart, 0, 0);
  lv_obj_set_style_pad_left(detail_precip_chart, kDetailChartLeftInset, 0);
  lv_obj_set_style_pad_right(detail_precip_chart, kDetailChartRightInset, 0);
  lv_obj_set_style_pad_top(detail_precip_chart, 0, 0);
  lv_obj_set_style_pad_bottom(detail_precip_chart, 0, 0);
  lv_obj_set_style_bg_color(detail_precip_chart, lv_color_white(), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(detail_precip_chart, LV_OPA_TRANSP, LV_PART_ITEMS);
  lv_obj_set_style_radius(detail_precip_chart, 0, LV_PART_ITEMS);
  lv_obj_set_style_line_width(detail_precip_chart, 0, LV_PART_ITEMS);
  lv_obj_set_style_size(detail_precip_chart, 4, 0, LV_PART_INDICATOR);
  lv_obj_remove_flag(detail_precip_chart, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(detail_precip_chart, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(detail_precip_chart, LV_OBJ_FLAG_HIDDEN);
  lv_chart_set_div_line_count(detail_precip_chart, 0, 0);
  lv_chart_set_type(detail_precip_chart, LV_CHART_TYPE_BAR);
  lv_chart_set_update_mode(detail_precip_chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(detail_precip_chart, kDetailChartPointCount);
  ctx->detail_precip_series =
      lv_chart_add_series(detail_precip_chart, lv_color_white(), LV_CHART_AXIS_PRIMARY_Y);

  for (int i = 0; i < kDetailChartPointCount; ++i) {
    lv_obj_t* bar = lv_obj_create(chart_wrap);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, kDetailPrecipBarWidth, 0);
    lv_obj_set_style_bg_color(bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
    ctx->detail_precip_bars[i] = bar;
  }

  auto make_detail_section_icon =
      [&](const char* mdi_icon, lv_coord_t section_top, lv_coord_t section_height, lv_coord_t y_offset = 0) {
    lv_obj_t* icon = lv_label_create(detail_wrap);
    set_label_style(icon, lv_color_white(), FONT_MDI_ICONS);
    lv_obj_set_style_text_opa(icon, LV_OPA_70, 0);
    lv_label_set_text(icon, getMdiChar(mdi_icon).c_str());
    lv_obj_update_layout(icon);
    const lv_coord_t icon_x = (kDetailSectionGutter - lv_obj_get_width(icon)) / 2;
    const lv_coord_t icon_y =
        section_top + ((section_height - lv_obj_get_height(icon)) / 2) + y_offset;
    lv_obj_set_pos(icon, icon_x < 0 ? 0 : icon_x, icon_y);
  };

  make_detail_section_icon("thermometer",
                           kDetailDisabledTempBlockTop,
                           kDetailDisabledTempBlockHeight);
  make_detail_section_icon("water-outline",
                           kDetailDisabledPrecipBlockTop,
                           kDetailDisabledPrecipBlockHeight,
                           -4);

  for (int i = 0; i < kDetailMarkerCount; ++i) {
    if (ctx->detail_time_labels[i]) lv_obj_move_foreground(ctx->detail_time_labels[i]);
    if (ctx->detail_icon_labels[i]) lv_obj_move_foreground(ctx->detail_icon_labels[i]);
    if (ctx->detail_temp_point_dots[i]) lv_obj_move_foreground(ctx->detail_temp_point_dots[i]);
    if (ctx->detail_temp_value_labels[i]) lv_obj_move_foreground(ctx->detail_temp_value_labels[i]);
    if (ctx->detail_precip_amount_labels[i]) lv_obj_move_foreground(ctx->detail_precip_amount_labels[i]);
    if (ctx->detail_probability_labels[i]) lv_obj_move_foreground(ctx->detail_probability_labels[i]);
  }
  if (ctx->detail_past_overlay) lv_obj_move_foreground(ctx->detail_past_overlay);

  clear_forecast_chart(ctx);
  clear_detail_view(ctx);
  show_week_view(ctx);

  apply_init_to_context(ctx, init);

  lv_obj_move_foreground(icon);
  lv_obj_move_foreground(location);
  lv_obj_move_foreground(mode_row);
  lv_obj_move_foreground(value_row);
  if (ctx->week_range_label) lv_obj_move_foreground(ctx->week_range_label);
  if (ctx->header_today_btn) lv_obj_move_foreground(ctx->header_today_btn);
  if (ctx->header_week_btn) lv_obj_move_foreground(ctx->header_week_btn);
  lv_obj_move_foreground(detail_title);
  if (ctx->detail_prev_btn) lv_obj_move_foreground(ctx->detail_prev_btn);
  if (ctx->detail_next_btn) lv_obj_move_foreground(ctx->detail_next_btn);
  lv_obj_move_foreground(close_btn);
  lv_obj_add_event_cb(overlay, on_overlay_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
}

}  // namespace

void show_weather_popup(const WeatherPopupInit& init) {
  if (!init.entity_id.length()) return;

  hide_light_popup();
  hide_sensor_popup();

  if (g_weather_popup_ctx && g_weather_popup_ctx->overlay && g_weather_popup_ctx->card) {
    apply_init_to_context(g_weather_popup_ctx, init);
    lv_obj_clear_flag(g_weather_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_weather_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  } else {
    WeatherPopupContext* ctx = new WeatherPopupContext();
    g_weather_popup_ctx = ctx;
    build_popup_ui(ctx, init);
  }

  reset_weather_popup_content(g_weather_popup_ctx);
  g_pending_weather.valid = false;

  String cached;
  if (tiles_get_cached_entity_payload(init.entity_id.c_str(), cached)) {
    apply_weather_payload(g_weather_popup_ctx, cached.c_str());
  }

  request_weather_for_context(g_weather_popup_ctx);
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


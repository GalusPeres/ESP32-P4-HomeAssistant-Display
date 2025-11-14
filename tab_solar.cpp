#include "tab_solar.h"
#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>

static lv_obj_t *chart = nullptr;
static lv_chart_series_t *ser_pv = nullptr;
static lv_obj_t *lbl_now = nullptr;

static const uint16_t SOLAR_HISTORY_POINTS = 288;  // 24h bei 5min Intervall
static int16_t chart_min = 0;
static int16_t chart_max = 4000;

static void ensure_chart_range(int16_t value) {
    if (!chart) return;
    if (value > chart_max - 100) {
        chart_max = ((value / 500) + 1) * 500;
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, chart_min, chart_max);
    }
    if (value < chart_min) {
        chart_min = (value / 500) * 500;
        lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, chart_min, chart_max);
    }
}

void build_solar_tab(lv_obj_t *parent) {
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_anim_duration(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    lbl_now = lv_label_create(parent);
    lv_label_set_text(lbl_now, "PV Garage: -- W");
#if defined(LV_FONT_MONTSERRAT_32) && LV_FONT_MONTSERRAT_32
    lv_obj_set_style_text_font(lbl_now, &lv_font_montserrat_32, 0);
#endif
    lv_obj_set_style_text_color(lbl_now, lv_color_white(), 0);
    lv_obj_align(lbl_now, LV_ALIGN_TOP_MID, 0, 20);

    chart = lv_chart_create(parent);
    lv_obj_set_size(chart, lv_pct(92), 280);
    lv_obj_align_to(chart, lbl_now, LV_ALIGN_OUT_BOTTOM_MID, 0, 24);

    lv_obj_set_style_bg_color(chart, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chart, 0, 0);
    lv_obj_set_style_radius(chart, 12, 0);

    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(chart, 4, 8);
    lv_chart_set_point_count(chart, SOLAR_HISTORY_POINTS);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, chart_min, chart_max);

    lv_obj_set_style_line_width(chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(chart, true, LV_PART_ITEMS);

    ser_pv = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_YELLOW), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_value(chart, ser_pv, 0);
}

void solar_update_power(float watts) {
    if (lbl_now) {
        char buf[48];
        snprintf(buf, sizeof(buf), "PV Garage: %.0f W", watts);
        lv_label_set_text(lbl_now, buf);
    }

    if (!chart || !ser_pv) return;

    int16_t value = static_cast<int16_t>(roundf(watts));
    ensure_chart_range(value);
    lv_chart_set_next_value(chart, ser_pv, value);
}

void solar_set_history_csv(const char* csv) {
    if (!csv || !chart || !ser_pv) {
        Serial.println("❌ solar_set_history_csv: Invalid params");
        return;
    }

    Serial.printf("📊 solar_set_history_csv: CSV length=%d, first 100 chars: %.100s\n", strlen(csv), csv);

    float temp[SOLAR_HISTORY_POINTS];
    uint16_t parsed = 0;
    const char* ptr = csv;
    while (*ptr && parsed < SOLAR_HISTORY_POINTS) {
        while (isspace(static_cast<unsigned char>(*ptr)) || *ptr == ',') ++ptr;
        if (*ptr == '\0') break;
        char* endptr;
        float val = strtof(ptr, &endptr);
        if (ptr == endptr) {
            ++ptr;
            continue;
        }
        temp[parsed++] = val;
        ptr = endptr;
    }

    Serial.printf("✅ Parsed %d values from CSV\n", parsed);
    if (parsed == 0) {
        Serial.println("❌ No values parsed from CSV!");
        return;
    }

    lv_chart_set_all_value(chart, ser_pv, 0);
    uint16_t use = parsed;
    if (use > SOLAR_HISTORY_POINTS) use = SOLAR_HISTORY_POINTS;
    uint16_t start = parsed - use;

    Serial.printf("📈 Loading %d values into chart (from index %d to %d)\n", use, start, parsed-1);

    int16_t max_candidate = chart_max;
    for (uint16_t i = 0; i < use; ++i) {
        int16_t v = static_cast<int16_t>(roundf(temp[start + i]));
        if (v > max_candidate) max_candidate = v;
        lv_chart_set_value_by_id(chart, ser_pv, SOLAR_HISTORY_POINTS - use + i, v);
    }
    ensure_chart_range(max_candidate);

    float last = temp[parsed - 1];
    Serial.printf("🔄 Chart updated with %d points, last value: %.0f W, max: %d W\n", use, last, max_candidate);

    if (lbl_now) {
        char buf[48];
        snprintf(buf, sizeof(buf), "PV Garage: %.0f W", last);
        lv_label_set_text(lbl_now, buf);
    }
}

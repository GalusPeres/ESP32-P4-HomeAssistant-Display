#include "src/types/clock/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/fonts/font_roboto_mono_digits_20.h"
#include "src/fonts/font_roboto_mono_digits_24.h"
#include "src/fonts/font_roboto_mono_digits_32.h"
#include "src/fonts/font_roboto_mono_digits_40.h"
#include "src/fonts/font_roboto_mono_digits_48.h"
#include <Arduino.h>
#include <time.h>

static uint8_t normalize_clock_font_size(uint8_t raw, uint8_t fallback) {
  switch (raw) {
    case 20:
    case 24:
    case 32:
    case 40:
    case 48:
      return raw;
    default:
      return fallback;
  }
}

static const lv_font_t* get_clock_time_font(const Tile& tile) {
  switch (normalize_clock_font_size(tile.key_code, 48)) {
    case 20:
      return &font_roboto_mono_digits_20;
    case 24:
      return &font_roboto_mono_digits_24;
    case 32:
      return &font_roboto_mono_digits_32;
    case 40:
      return &font_roboto_mono_digits_40;
    default:
      return &font_roboto_mono_digits_48;
  }
}

static const lv_font_t* get_clock_date_font(const Tile& tile) {
  switch (normalize_clock_font_size(tile.key_modifier, 24)) {
    case 20:
      return &font_roboto_mono_digits_20;
    case 32:
      return &font_roboto_mono_digits_32;
    case 40:
      return &font_roboto_mono_digits_40;
    case 48:
      return &font_roboto_mono_digits_48;
    default:
      return &font_roboto_mono_digits_24;
  }
}

struct ClockTileData {
  lv_obj_t* time_label = nullptr;
  lv_obj_t* date_label = nullptr;
  lv_timer_t* timer = nullptr;
};

static uint8_t get_clock_flags(const Tile& tile) {
  uint8_t flags = tile.sensor_decimals;
  if (flags == 0xFF) flags = 1;
  flags &= 0x03;
  if (flags == 0) flags = 1;
  return flags;
}

static void update_clock_labels(ClockTileData* data) {
  if (!data) return;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 0)) {
    if (data->time_label) {
      char buf[6];
      snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      lv_label_set_text(data->time_label, buf);
    }
    if (data->date_label) {
      char buf[12];
      snprintf(buf, sizeof(buf), "%02d.%02d.%04d",
               timeinfo.tm_mday,
               timeinfo.tm_mon + 1,
               timeinfo.tm_year + 1900);
      lv_label_set_text(data->date_label, buf);
    }
  }
}

static void clock_timer_cb(lv_timer_t* timer) {
  ClockTileData* data = static_cast<ClockTileData*>(lv_timer_get_user_data(timer));
  if (!data) return;
  update_clock_labels(data);
}

lv_obj_t* render_clock_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  (void)index;
  lv_obj_t* card = lv_button_create(parent);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);

  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
  uint32_t pressed_color = brighten_rgb_color(card_color, 0x10);
  lv_obj_set_style_bg_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
lv_obj_set_style_bg_grad_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(card);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  // Icon Label (optional)
  String iconChar;
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    iconChar = getMdiChar(tile.icon_name);
  }
  const bool has_icon = iconChar.length() > 0;
  if (has_icon) {
    lv_obj_t* icon_lbl = lv_label_create(card);
    if (icon_lbl) {
      set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
      lv_label_set_text(icon_lbl, iconChar.c_str());
      lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);
    }
  }

  // Title Label (optional)
  if (tile.title.length() > 0) {
    lv_obj_t* title_lbl = lv_label_create(card);
    if (title_lbl) {
      set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
      lv_label_set_text(title_lbl, tile.title.c_str());
      lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 4);
    }
  }

  uint8_t flags = get_clock_flags(tile);
  const bool show_time = (flags & 1) != 0;
  const bool show_date = (flags & 2) != 0;
  const bool has_header = tile.title.length() > 0 || has_icon;

  lv_obj_t* stack = lv_obj_create(card);
  lv_obj_remove_style_all(stack);
  lv_obj_set_size(stack, LV_PCT(100), LV_PCT(100));
  lv_obj_set_flex_flow(stack, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(stack, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(stack, 12, 0);
  lv_obj_set_style_bg_opa(stack, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(stack, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(stack, LV_ALIGN_CENTER, 0, has_header ? 18 : 0);

  lv_obj_t* time_lbl = nullptr;
  if (show_time) {
    time_lbl = lv_label_create(stack);
    if (time_lbl) {
      set_label_style(time_lbl, lv_color_white(), get_clock_time_font(tile));
      lv_obj_set_width(time_lbl, LV_PCT(100));
      lv_obj_set_style_text_align(time_lbl, LV_TEXT_ALIGN_CENTER, 0);
      lv_label_set_text(time_lbl, "");
    }
  }

  lv_obj_t* date_lbl = nullptr;
  if (show_date) {
    date_lbl = lv_label_create(stack);
    if (date_lbl) {
      set_label_style(date_lbl, lv_color_hex(0xD0D0D0), get_clock_date_font(tile));
      lv_obj_set_width(date_lbl, LV_PCT(100));
      lv_obj_set_style_text_align(date_lbl, LV_TEXT_ALIGN_CENTER, 0);
      lv_label_set_text(date_lbl, "");
    }
  }

  ClockTileData* data = new ClockTileData{};
  data->time_label = time_lbl;
  data->date_label = date_lbl;
  update_clock_labels(data);
  data->timer = lv_timer_create(clock_timer_cb, 1000, data);

  lv_obj_add_event_cb(
      card,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
        ClockTileData* data = static_cast<ClockTileData*>(lv_event_get_user_data(e));
        if (!data) return;
        if (data->timer) {
          lv_timer_delete(data->timer);
          data->timer = nullptr;
        }
        delete data;
      },
      LV_EVENT_DELETE,
      data);

  return card;
}



#include "src/types/clock/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/fonts/font_roboto_mono_digits_48.h"
#include <Arduino.h>
#include <time.h>

struct ClockTileData {
  lv_obj_t* time_label = nullptr;
  lv_timer_t* timer = nullptr;
};

static void update_clock_label(lv_obj_t* label) {
  if (!label) return;
  struct tm timeinfo;
  char buf[6];
  if (getLocalTime(&timeinfo, 0)) {
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    snprintf(buf, sizeof(buf), "--:--");
  }
  lv_label_set_text(label, buf);
}

static void clock_timer_cb(lv_timer_t* timer) {
  ClockTileData* data = static_cast<ClockTileData*>(lv_timer_get_user_data(timer));
  if (!data) return;
  update_clock_label(data->time_label);
}

lv_obj_t* render_clock_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  (void)index;
  lv_obj_t* card = lv_button_create(parent);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);

  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  uint32_t pressed_color = card_color + 0x101010;
  lv_obj_set_style_bg_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  // Icon Label (optional)
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      lv_obj_t* icon_lbl = lv_label_create(card);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());
        lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);
      }
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

  lv_obj_t* time_lbl = lv_label_create(card);
  if (time_lbl) {
    set_label_style(time_lbl, lv_color_white(), &font_roboto_mono_digits_48);
    update_clock_label(time_lbl);
    const bool has_header = tile.title.length() > 0 || tile.icon_name.length() > 0;
    lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, has_header ? 18 : 0);
  }

  ClockTileData* data = new ClockTileData{};
  data->time_label = time_lbl;
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

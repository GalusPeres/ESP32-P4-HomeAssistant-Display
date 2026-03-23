#include "src/types/radar/renderer.h"

#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/radar_popup.h"
#include <Arduino.h>

namespace {

const char* radar_preset_label(const String& preset_raw) {
  String preset = preset_raw;
  preset.trim();
  preset.toLowerCase();
  if (preset == "germany") return "Deutschland";
  if (preset == "south") return "Sueddeutschland";
  if (preset == "custom") return "Custom BBOX";
  return "Bayern";
}

struct RadarEventData {
  RadarPopupInit init;
};

}  // namespace

lv_obj_t* render_radar_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t) {
  if (!parent) return nullptr;

  lv_obj_t* card = lv_button_create(parent);
  if (!card) return nullptr;

  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x2A2A2A;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
  uint32_t pressed_color = brighten_rgb_color(card_color, 0x10);
  lv_obj_set_style_bg_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_PRESS_LOCK);
  disable_pressed_button_animation(card);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  String icon_name = normalizeMdiIconName(tile.icon_name.length() ? tile.icon_name : "radar");
  String icon_char;
  if (!isMdiIconDisabled(icon_name) && icon_name.length() && FONT_MDI_ICONS != nullptr) {
    icon_char = getMdiChar(icon_name);
  }

  if (icon_char.length()) {
    lv_obj_t* icon = lv_label_create(card);
    set_label_style(icon, lv_color_white(), FONT_MDI_ICONS);
    lv_label_set_text(icon, icon_char.c_str());
    lv_obj_align(icon, LV_ALIGN_TOP_LEFT, -8, -8);
  }

  String title = tile.title;
  if (!title.length()) title = "Radar";
  lv_obj_t* title_label = lv_label_create(card);
  set_label_style(title_label, lv_color_white(), FONT_TITLE);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title_label, LV_PCT(70));
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(title_label, title.c_str());
  lv_obj_align(title_label, LV_ALIGN_TOP_RIGHT, 4, 4);

  lv_obj_t* center_label = lv_label_create(card);
  set_label_style(center_label, lv_color_white(), &ui_font_24);
  lv_obj_set_style_text_align(center_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(center_label, radar_preset_label(tile.sensor_entity));
  lv_obj_align(center_label, LV_ALIGN_CENTER, 0, -8);

  lv_obj_t* foot_label = lv_label_create(card);
  set_label_style(foot_label, lv_color_hex(0xC8D4E8), &ui_font_20);
  lv_obj_set_style_text_align(foot_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(foot_label, "DWD Radar");
  lv_obj_align(foot_label, LV_ALIGN_BOTTOM_MID, 0, -6);

  RadarEventData* data = new RadarEventData;
  data->init.title = title;
  data->init.icon_name = icon_name.length() ? icon_name : String("radar");
  data->init.preset = tile.sensor_entity.length() ? tile.sensor_entity : String("bayern");
  data->init.bbox = tile.sensor_unit;
  data->init.frame_count = (tile.sensor_gauge_min >= 2 && tile.sensor_gauge_min <= 8)
                               ? static_cast<uint8_t>(tile.sensor_gauge_min)
                               : 6;
  data->init.frame_delay_ms = (tile.sensor_gauge_max >= 150 && tile.sensor_gauge_max <= 3000)
                                  ? static_cast<uint16_t>(tile.sensor_gauge_max)
                                  : 450;
  data->init.refresh_sec = tile.image_slideshow_sec ? tile.image_slideshow_sec : 300;
  data->init.bg_color = card_color;

  const lv_event_code_t popup_event =
      (getTilePopupOpenMode(tile) == TILE_POPUP_OPEN_SHORT_PRESS)
          ? LV_EVENT_SHORT_CLICKED
          : LV_EVENT_LONG_PRESSED;

  lv_obj_add_event_cb(
      card,
      [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED) return;
        RadarEventData* data = static_cast<RadarEventData*>(lv_event_get_user_data(e));
        if (!data) return;
        show_radar_popup(data->init);
      },
      popup_event,
      data);

  lv_obj_add_event_cb(
      card,
      [](lv_event_t* e) {
        RadarEventData* data = static_cast<RadarEventData*>(lv_event_get_user_data(e));
        delete data;
      },
      LV_EVENT_DELETE,
      data);

  return card;
}

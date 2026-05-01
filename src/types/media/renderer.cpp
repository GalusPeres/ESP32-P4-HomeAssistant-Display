#include "src/types/media/renderer.h"

#include <Arduino.h>

#include "src/network/ha_bridge_config.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"

#include <stdlib.h>

namespace {

struct MediaEventData {
  String entity_id;
};

static void free_cover_dsc(MediaCoverRef* ref) {
  if (!ref || !ref->dsc) return;
  if (ref->dsc->data) {
    free(const_cast<uint8_t*>(ref->dsc->data));
  }
  free(ref->dsc);
  ref->dsc = nullptr;
  ref->url_hash = 0;
  ref->requested_url_hash = 0;
  ref->failed_url_hash = 0;
}

static void cover_ref_delete_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  MediaCoverRef* ref = static_cast<MediaCoverRef*>(lv_event_get_user_data(e));
  free_cover_dsc(ref);
  delete ref;
}

void enable_event_bubble(lv_obj_t* obj) {
  if (!obj) return;
  lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
}

}  // namespace

lv_obj_t* render_media_tile(lv_obj_t* parent,
                            int col,
                            int row,
                            const Tile& tile,
                            uint8_t index,
                            GridType grid_type) {
  if (!parent) {
    Serial.println("[TileRenderer] ERROR: parent NULL bei Media-Tile");
    return nullptr;
  }

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
  disable_pressed_button_animation(card);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  MediaCoverRef* cover_ref = new MediaCoverRef();
  lv_obj_add_event_cb(card, cover_ref_delete_cb, LV_EVENT_DELETE, cover_ref);

  lv_obj_t* cover_img = lv_img_create(card);
  if (cover_img) {
    const lv_coord_t cover_size = (tile.span_w > 1 || tile.span_h > 1) ? 96 : 78;
    lv_obj_set_size(cover_img, cover_size, cover_size);
    lv_obj_align(cover_img, LV_ALIGN_LEFT_MID, -2, 8);
    lv_image_set_inner_align(cover_img, LV_IMAGE_ALIGN_COVER);
    lv_obj_set_style_radius(cover_img, 12, 0);
    lv_obj_set_style_clip_corner(cover_img, true, 0);
    lv_obj_set_style_opa(cover_img, LV_OPA_COVER, 0);
    lv_obj_add_flag(cover_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(cover_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(cover_img, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(cover_img, LV_OBJ_FLAG_IGNORE_LAYOUT);
  }

  String icon_name = tile.icon_name;
  bool icon_disabled = isMdiIconDisabled(icon_name);
  const bool custom_icon = tile.icon_name.length() && !icon_disabled;
  icon_name = normalizeMdiIconName(icon_name);
  if (!icon_disabled && !icon_name.length() && tile.sensor_entity.length()) {
    icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
  }
  if (!icon_disabled && !icon_name.length()) {
    icon_name = "music";
  }

  lv_obj_t* icon_label = lv_label_create(card);
  if (icon_label) {
    set_label_style(icon_label, lv_color_white(), FONT_MDI_ICONS);
    String icon_char = (!icon_disabled && icon_name.length()) ? getMdiChar(icon_name) : "";
    if (icon_char.length()) {
      lv_label_set_text(icon_label, icon_char.c_str());
      lv_obj_clear_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(icon_label, "");
      lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, -8, -8);
    enable_event_bubble(icon_label);
  }

  String title_text = tile.title;
  if (!title_text.length() && tile.sensor_entity.length()) {
    title_text = haBridgeConfig.findSensorName(tile.sensor_entity);
  }
  if (!title_text.length()) title_text = tile.sensor_entity;
  if (!title_text.length()) title_text = "Media";

  lv_obj_t* title_label = lv_label_create(card);
  if (title_label) {
    set_label_style(title_label, lv_color_white(), FONT_TITLE);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title_label, LV_PCT(70));
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(title_label, title_text.c_str());
    lv_obj_align(title_label, LV_ALIGN_TOP_RIGHT, 4, 4);
    enable_event_bubble(title_label);
  }

  const lv_font_t* media_font = (tile.span_w > 1 || tile.span_h > 1) ? FONT_VALUE : &ui_font_24;

  lv_obj_t* media_title = lv_label_create(card);
  if (media_title) {
    set_label_style(media_title, lv_color_white(), media_font);
    lv_label_set_long_mode(media_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(media_title, LV_PCT(100));
    lv_obj_set_style_text_align(media_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(media_title, "--");
    lv_obj_align(media_title, LV_ALIGN_CENTER, 0, 8);
    enable_event_bubble(media_title);
  }

  lv_obj_t* subtitle = lv_label_create(card);
  if (subtitle) {
    set_label_style(subtitle, lv_color_hex(0xD8DEE9), FONT_SMALL);
    lv_label_set_long_mode(subtitle, LV_LABEL_LONG_DOT);
    lv_obj_set_width(subtitle, LV_PCT(92));
    lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(subtitle, "--");
    lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 50);
    enable_event_bubble(subtitle);
  }

  lv_obj_t* state_label = lv_label_create(card);
  if (state_label) {
    set_label_style(state_label, lv_color_hex(0xB7C1D6), FONT_SMALL);
    lv_label_set_long_mode(state_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(state_label, LV_PCT(72));
    lv_obj_set_style_text_align(state_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(state_label, "--");
    lv_obj_align(state_label, LV_ALIGN_BOTTOM_RIGHT, 4, 4);
    enable_event_bubble(state_label);
  }

  MediaTileWidgets* target = tile_renderer_get_media_widgets(grid_type);
  if (target && index < TILES_PER_GRID) {
    target[index].cover_image = cover_img;
    target[index].cover_ref = cover_ref;
    target[index].icon_label = icon_label;
    target[index].title_label = title_label;
    target[index].media_title_label = media_title;
    target[index].media_subtitle_label = subtitle;
    target[index].state_label = state_label;
    target[index].last_payload_hash = 0;
    target[index].dynamic_icon = !custom_icon;
  }

  if (tile.sensor_entity.length()) {
    String initial = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
    if (initial.length()) {
      update_media_tile_state(grid_type, index, initial.c_str());
    }

    MediaEventData* data = new MediaEventData{tile.sensor_entity};
    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_SHORT_CLICKED) return;
          MediaEventData* data = static_cast<MediaEventData*>(lv_event_get_user_data(e));
          if (!data || !data->entity_id.length()) return;
          mqttPublishMediaCommand(data->entity_id.c_str(), "play_pause");
        },
        LV_EVENT_SHORT_CLICKED,
        data);

    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          MediaEventData* data = static_cast<MediaEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        data);
  }

  return card;
}

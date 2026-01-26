#include "src/types/image/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/image_popup.h"
#include <Arduino.h>
#include <SD.h>

namespace {

static bool is_url_path(const String& value) {
  String v = value;
  v.trim();
  return v.startsWith("http://") || v.startsWith("https://");
}

static bool is_slideshow_token(const String& value) {
  return value.startsWith("__");
}

static uint32_t hash_url(const String& url) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < url.length(); ++i) {
    h ^= static_cast<uint8_t>(url[i]);
    h *= 16777619u;
  }
  return h;
}

static String make_url_cache_bin_path(const String& url) {
  uint32_t h = hash_url(url);
  char buf[64];
  snprintf(buf, sizeof(buf), "/_url_cache/u%08lX.bin", static_cast<unsigned long>(h));
  return String(buf);
}

static bool resolve_preview_path(const String& raw, String& out_path) {
  String path = raw;
  path.trim();
  if (!path.length()) return false;
  if (is_slideshow_token(path)) return false;
  if (path.startsWith("S:")) path = path.substring(2);
  if (is_url_path(path)) {
    String cached = make_url_cache_bin_path(path);
    if (!SD.exists(cached)) return false;
    out_path = cached;
    return true;
  }
  if (!path.startsWith("/")) path = "/" + path;
  if (!SD.exists(path)) return false;
  out_path = path;
  return true;
}

}  // namespace

struct ImageEventData {
  String image_path;
  uint16_t slideshow_sec;
};

lv_obj_t* render_image_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  Serial.printf("[TileRenderer] render_image_tile: title='%s', image_path='%s'\n", tile.title.c_str(), tile.image_path.c_str());

  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, LV_STATE_PRESSED);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(btn, col, row, tile.span_w, tile.span_h);

  bool has_preview = false;
  const bool has_icon = tile.icon_name.length() > 0;
  const bool has_title = tile.title.length() > 0;
  if (tile.sensor_display_mode != 0 && tile.image_path.length() > 0) {
    String full_path;
    if (resolve_preview_path(tile.image_path, full_path)) {
      String src = "S:" + full_path;
      lv_image_header_t header{};
      if (lv_image_decoder_get_info(src.c_str(), &header) == LV_RESULT_OK) {
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_clip_corner(btn, true, 0);
        lv_obj_set_style_clip_corner(btn, true, LV_STATE_PRESSED);
        lv_obj_update_layout(btn);
        lv_obj_t* img = lv_img_create(btn);
        if (img) {
          lv_img_set_src(img, src.c_str());
          lv_coord_t btn_w = lv_obj_get_width(btn);
          lv_coord_t btn_h = lv_obj_get_height(btn);
          const lv_coord_t bleed = 2;
          if (btn_w > 0 && btn_h > 0) {
            lv_obj_set_size(img, btn_w + bleed * 2, btn_h + bleed * 2);
            lv_obj_set_pos(img, -bleed, -bleed);
          } else {
            lv_obj_set_size(img, LV_PCT(100), LV_PCT(100));
            lv_obj_align(img, LV_ALIGN_TOP_LEFT, 0, 0);
          }
          lv_image_set_inner_align(img, LV_IMAGE_ALIGN_COVER);
          lv_obj_add_flag(img, LV_OBJ_FLAG_IGNORE_LAYOUT);
          lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
          lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
          has_preview = true;
        }
      }
    }
  }

  if (has_preview && (has_icon || has_title)) {
    const lv_color_t badge_color = lv_color_hex(0x1E1E1E);
    const lv_opa_t badge_opa = LV_OPA_70;
    const int16_t title_pad_x = 12;
    const int16_t title_pad_y = 6;
    const int16_t icon_pad = 6;
    lv_obj_t* title_lbl = nullptr;
    lv_obj_t* icon_lbl = nullptr;

    if (has_title) {
      title_lbl = lv_label_create(btn);
      if (title_lbl) {
        set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
        lv_label_set_text(title_lbl, tile.title.c_str());
        lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 20, 28);
      }
    }

    if (has_icon && FONT_MDI_ICONS != nullptr) {
      String iconChar = getMdiChar(tile.icon_name);
      if (iconChar.length() > 0) {
        icon_lbl = lv_label_create(btn);
        if (icon_lbl) {
          set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
          lv_label_set_text(icon_lbl, iconChar.c_str());
          lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, -16, 16);
        }
      }
    }

    if (title_lbl) lv_obj_update_layout(title_lbl);
    if (icon_lbl) lv_obj_update_layout(icon_lbl);

    int16_t badge_h = 0;
    int16_t title_w = 0;
    int16_t title_h = 0;
    int16_t icon_w = 0;
    int16_t icon_h = 0;

    if (title_lbl) {
      title_w = lv_obj_get_width(title_lbl);
      title_h = lv_obj_get_height(title_lbl);
      badge_h = title_h + title_pad_y * 2;
    }
    if (icon_lbl) {
      icon_w = lv_obj_get_width(icon_lbl);
      icon_h = lv_obj_get_height(icon_lbl);
      int16_t icon_box = (icon_w > icon_h ? icon_w : icon_h) + icon_pad * 2;
      if (icon_box > badge_h) badge_h = icon_box;
    }
    if (badge_h < 24) badge_h = 24;

    if (title_lbl) {
      int16_t pad_y = (badge_h - title_h) / 2;
      lv_obj_t* title_bg = lv_obj_create(btn);
      if (title_bg) {
        lv_obj_set_size(title_bg, title_w + title_pad_x * 2, badge_h);
        lv_obj_set_pos(title_bg,
                       lv_obj_get_x(title_lbl) - title_pad_x,
                       lv_obj_get_y(title_lbl) - pad_y);
        lv_obj_set_style_bg_color(title_bg, badge_color, 0);
        lv_obj_set_style_bg_opa(title_bg, badge_opa, 0);
        lv_obj_set_style_radius(title_bg, badge_h / 4, 0);
        lv_obj_set_style_border_width(title_bg, 0, 0);
        lv_obj_set_style_shadow_width(title_bg, 0, 0);
        lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(title_bg, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_add_flag(title_bg, LV_OBJ_FLAG_EVENT_BUBBLE);
      }
    }

    if (icon_lbl) {
      int16_t size = badge_h;
      lv_obj_t* icon_bg = lv_obj_create(btn);
      if (icon_bg) {
        lv_obj_set_size(icon_bg, size, size);
        lv_obj_set_pos(icon_bg,
                       lv_obj_get_x(icon_lbl) + (icon_w / 2) - (size / 2),
                       lv_obj_get_y(icon_lbl) + (icon_h / 2) - (size / 2));
        lv_obj_set_style_bg_color(icon_bg, badge_color, 0);
        lv_obj_set_style_bg_opa(icon_bg, badge_opa, 0);
        lv_obj_set_style_radius(icon_bg, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(icon_bg, 0, 0);
        lv_obj_set_style_shadow_width(icon_bg, 0, 0);
        lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_EVENT_BUBBLE);
      }
    }

    if (title_lbl) lv_obj_move_foreground(title_lbl);
    if (icon_lbl) lv_obj_move_foreground(icon_lbl);
  }

  if (!has_preview) {
    // Icon Label (optional)
    lv_obj_t* icon_lbl = nullptr;

    if (has_icon && FONT_MDI_ICONS != nullptr) {
      String iconChar = getMdiChar(tile.icon_name);
      if (iconChar.length() > 0) {
        icon_lbl = lv_label_create(btn);
        if (icon_lbl) {
          set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
          lv_label_set_text(icon_lbl, iconChar.c_str());

          if (has_title) {
            lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);  // Icon oben
          } else {
            lv_obj_center(icon_lbl);  // Icon mittig
          }
        }
      }
    }

    // Title Label
    if (has_title) {
      lv_obj_t* l = lv_label_create(btn);
      if (l) {
        set_label_style(l, lv_color_white(), FONT_TITLE);
        lv_label_set_text(l, tile.title.c_str());

        if (icon_lbl) {
          lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);  // Title unten
        } else {
          lv_obj_center(l);  // Title mittig
        }
      }
    }
  }

  // Event-Handler für Image-Popup
  if (tile.image_path.length() > 0) {
    Serial.printf("[TileRenderer] Registriere Click-Event für image_path='%s'\n", tile.image_path.c_str());

    // Allocate permanent storage for event data
    ImageEventData* event_data = new ImageEventData{tile.image_path, tile.image_slideshow_sec};

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          ImageEventData* data = static_cast<ImageEventData*>(lv_event_get_user_data(e));
          if (data && data->image_path.length() > 0) {
            Serial.printf("[Tile] Öffne Bild: %s\n", data->image_path.c_str());
            show_image_popup(data->image_path.c_str(), data->slideshow_sec);
          } else {
            Serial.println("[Tile] FEHLER: Keine event_data oder image_path leer!");
          }
        },
        LV_EVENT_CLICKED,
        event_data);

    // Cleanup on delete
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          ImageEventData* data = static_cast<ImageEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  } else {
    Serial.println("[TileRenderer] WARNUNG: image_path ist leer - kein Click-Event registriert!");
  }

  return btn;
}



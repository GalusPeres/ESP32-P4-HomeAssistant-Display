#include "src/types/image/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/image_popup.h"
#include <Arduino.h>
#include "src/core/waveshare_sdmmc.h"

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

static void calc_thumb_pixel_size(const Tile& tile, uint16_t& out_w, uint16_t& out_h) {
  uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;
  if (span_w > GRID_COLS) span_w = GRID_COLS;
  if (span_h > GRID_ROWS) span_h = GRID_ROWS;
  out_w = static_cast<uint16_t>(span_w * GRID_CELL_W + (span_w - 1) * GRID_GAP);
  out_h = static_cast<uint16_t>(span_h * GRID_CELL_H + (span_h - 1) * GRID_GAP);
}

static String wrap_title(const String& title) {
  String t = title;
  t.trim();
  t.replace("\\n", "\n");
  return t;
}

static String normalize_preview_key(String raw) {
  raw.trim();
  if (raw.startsWith("S:")) raw = raw.substring(2);
  if (is_url_path(raw)) return raw;
  if (!raw.startsWith("/")) raw = "/" + raw;
  return raw;
}

static String make_url_cache_bin_path(const String& url) {
  uint32_t h = hash_url(url);
  char buf[64];
  snprintf(buf, sizeof(buf), "/_url_cache/u%08lX.bin", static_cast<unsigned long>(h));
  return String(buf);
}

static String make_thumb_path(const String& key, uint16_t w, uint16_t h) {
  uint32_t hval = hash_url(key);
  char buf[96];
  snprintf(buf, sizeof(buf), "/_thumbs/t%08lX_%ux%u.bin", static_cast<unsigned long>(hval),
           static_cast<unsigned int>(w), static_cast<unsigned int>(h));
  return String(buf);
}

static bool resolve_preview_path(const Tile& tile, String& out_path) {
  String raw = tile.image_path;
  raw.trim();
  if (!raw.length()) return false;
  if (is_slideshow_token(raw)) return false;

  String key = normalize_preview_key(raw);
  String path = key;
  if (is_url_path(key)) {
    path = make_url_cache_bin_path(key);
  }
  if (!path.startsWith("/")) path = "/" + path;
  if (!SD_MMC.exists(path)) return false;

  uint16_t tile_w = 0;
  uint16_t tile_h = 0;
  calc_thumb_pixel_size(tile, tile_w, tile_h);
  if (tile_w > 0 && tile_h > 0) {
    String thumb = make_thumb_path(key, tile_w, tile_h);
    if (SD_MMC.exists(thumb)) {
      out_path = thumb;
      return true;
    }
  }

  return false;
}
}  // namespace

bool image_tile_get_preview_path(const Tile& tile, String& out_path) {
  (void)tile;
  out_path = "";
  return false;
}

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
lv_obj_set_style_bg_grad_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = brighten_rgb_color(btn_color, 0x10);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(btn);

  set_tile_grid_cell(btn, col, row, tile.span_w, tile.span_h);

  const bool wants_preview = false;
  bool has_preview = wants_preview;
  String iconChar;
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    iconChar = getMdiChar(tile.icon_name);
  }
  const bool has_icon = iconChar.length() > 0;
  const bool has_title = tile.title.length() > 0;
  if (wants_preview) {
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_clip_corner(btn, false, 0);
    lv_obj_set_style_clip_corner(btn, false, LV_STATE_PRESSED);
    lv_obj_update_layout(btn);
    lv_obj_t* preview_wrap = lv_obj_create(btn);
    if (preview_wrap) {
      lv_obj_set_size(preview_wrap, LV_PCT(100), LV_PCT(100));
      lv_obj_align(preview_wrap, LV_ALIGN_TOP_LEFT, 0, 0);
      lv_obj_set_style_bg_opa(preview_wrap, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(preview_wrap, 0, 0);
      lv_obj_set_style_pad_all(preview_wrap, 0, 0);
      lv_obj_set_style_radius(preview_wrap, 22, 0);
      lv_obj_set_style_clip_corner(preview_wrap, true, 0);
      lv_obj_clear_flag(preview_wrap, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(preview_wrap, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_flag(preview_wrap, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_add_flag(preview_wrap, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
    lv_obj_t* img = lv_img_create(preview_wrap ? preview_wrap : btn);
    if (img) {
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
      lv_obj_add_flag(img, LV_OBJ_FLAG_USER_1);
      lv_obj_add_flag(img, LV_OBJ_FLAG_IGNORE_LAYOUT);
      lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
      lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
    }
  }

  if (has_preview && (has_icon || has_title)) {
    const lv_color_t badge_color = lv_color_hex(btn_color);
    const lv_opa_t badge_opa = LV_OPA_70;

    // --- Consistent pill positioning constants ---
    const int16_t pill_margin = 6;        // equal spacing from button edge (top, left, right)
    const int16_t title_pad_x = 12;       // horizontal padding inside title pill
    const int16_t title_pad_y = 6;        // vertical padding inside title pill
    const int16_t icon_pad = 6;           // padding inside icon pill

    lv_obj_t* title_lbl = nullptr;
    lv_obj_t* icon_lbl = nullptr;
    int title_lines = 1;

    // --- Create labels and set text (position later) ---
    if (has_title) {
      title_lbl = lv_label_create(btn);
      if (title_lbl) {
        set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
        String wrapped = wrap_title(tile.title);
        for (size_t i = 0; i < wrapped.length(); ++i) {
          if (wrapped[i] == '\n') title_lines++;
        }
        lv_label_set_text(title_lbl, wrapped.c_str());
      }
    }

    if (has_icon) {
      icon_lbl = lv_label_create(btn);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());
      }
    }

    // --- Measure label sizes ---
    if (title_lbl) lv_obj_update_layout(title_lbl);
    if (icon_lbl) lv_obj_update_layout(icon_lbl);

    int16_t title_w = title_lbl ? lv_obj_get_width(title_lbl) : 0;
    int16_t title_h = title_lbl ? lv_obj_get_height(title_lbl) : 0;
    int16_t icon_w = icon_lbl ? lv_obj_get_width(icon_lbl) : 0;
    int16_t icon_h = icon_lbl ? lv_obj_get_height(icon_lbl) : 0;

    // --- Compute pill dimensions ---
    // Fixed title pill height: always sized for two lines (same height regardless of line count)
    int16_t font_line_h = lv_font_get_line_height(FONT_TITLE);
    int16_t fixed_title_pill_h = font_line_h * 2 + title_pad_y * 2;
    int16_t title_pill_h = title_lbl ? fixed_title_pill_h : 0;

    int16_t icon_pill_size = 0;
    if (icon_lbl) {
      icon_pill_size = (icon_w > icon_h ? icon_w : icon_h) + icon_pad * 2;
      if (icon_pill_size < 24) icon_pill_size = 24;
    }

    const bool merge_badge = (tile.span_w <= 1);

    // Equalize heights for separate single-line title + icon
    if (!merge_badge && title_lbl && icon_lbl && title_lines == 1) {
      int16_t equal_h = title_pill_h > icon_pill_size ? title_pill_h : icon_pill_size;
      title_pill_h = equal_h;
      icon_pill_size = equal_h;
    }

    // For merged mode, use the taller of both
    int16_t merged_pill_h = title_pill_h > icon_pill_size ? title_pill_h : icon_pill_size;
    if (merged_pill_h < 24) merged_pill_h = 24;

    // --- Position labels centered within their pills ---
    if (title_lbl) {
      int16_t eff_h = (merge_badge && icon_lbl) ? merged_pill_h : title_pill_h;
      lv_obj_set_pos(title_lbl,
                     pill_margin + title_pad_x,
                     pill_margin + (eff_h - title_h) / 2);
    }

    if (icon_lbl) {
      int16_t eff_h = (merge_badge && title_lbl) ? merged_pill_h : icon_pill_size;
      lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT,
                   -(pill_margin + (icon_pill_size - icon_w) / 2),
                   pill_margin + (eff_h - icon_h) / 2);
      lv_obj_update_layout(icon_lbl);
    }

    // --- Create pill backgrounds ---
    if (merge_badge && title_lbl && icon_lbl) {
      // One merged pill spanning from title left to icon right
      int16_t left = pill_margin;
      int16_t top = pill_margin;
      int16_t right = lv_obj_get_x(icon_lbl) + icon_w + (icon_pill_size - icon_w) / 2;
      int16_t h = merged_pill_h;

      lv_obj_t* combo_bg = lv_obj_create(btn);
      if (combo_bg) {
        lv_obj_set_pos(combo_bg, left, top);
        lv_obj_set_size(combo_bg, right - left, h);
        lv_obj_set_style_bg_color(combo_bg, badge_color, 0);
        lv_obj_set_style_bg_opa(combo_bg, badge_opa, 0);
        lv_obj_set_style_radius(combo_bg, 16, 0);
        lv_obj_set_style_border_width(combo_bg, 0, 0);
        lv_obj_set_style_shadow_width(combo_bg, 0, 0);
        lv_obj_clear_flag(combo_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(combo_bg, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(combo_bg, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_add_flag(combo_bg, LV_OBJ_FLAG_EVENT_BUBBLE);
      }
    } else {
      // Separate pills (or only one present)
      if (title_lbl) {
        lv_obj_t* title_bg = lv_obj_create(btn);
        if (title_bg) {
          lv_obj_set_pos(title_bg, pill_margin, pill_margin);
          lv_obj_set_size(title_bg, title_w + title_pad_x * 2, title_pill_h);
          lv_obj_set_style_bg_color(title_bg, badge_color, 0);
          lv_obj_set_style_bg_opa(title_bg, badge_opa, 0);
          lv_obj_set_style_radius(title_bg, 16, 0);
          lv_obj_set_style_border_width(title_bg, 0, 0);
          lv_obj_set_style_shadow_width(title_bg, 0, 0);
          lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_SCROLLABLE);
          lv_obj_clear_flag(title_bg, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_add_flag(title_bg, LV_OBJ_FLAG_IGNORE_LAYOUT);
          lv_obj_add_flag(title_bg, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
      }

      if (icon_lbl) {
        lv_obj_t* icon_bg = lv_obj_create(btn);
        if (icon_bg) {
          int16_t icon_lbl_x = lv_obj_get_x(icon_lbl);
          lv_obj_set_pos(icon_bg,
                         icon_lbl_x + icon_w / 2 - icon_pill_size / 2,
                         pill_margin);
          lv_obj_set_size(icon_bg, icon_pill_size, icon_pill_size);
          lv_obj_set_style_bg_color(icon_bg, badge_color, 0);
          lv_obj_set_style_bg_opa(icon_bg, badge_opa, 0);
          lv_obj_set_style_radius(icon_bg, 16, 0);
          lv_obj_set_style_border_width(icon_bg, 0, 0);
          lv_obj_set_style_shadow_width(icon_bg, 0, 0);
          lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_SCROLLABLE);
          lv_obj_clear_flag(icon_bg, LV_OBJ_FLAG_CLICKABLE);
          lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_IGNORE_LAYOUT);
          lv_obj_add_flag(icon_bg, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
      }
    }

    if (title_lbl) lv_obj_move_foreground(title_lbl);
    if (icon_lbl) lv_obj_move_foreground(icon_lbl);
  }

  if (!has_preview) {
    // Icon Label (optional)
    lv_obj_t* icon_lbl = nullptr;

    if (has_icon) {
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

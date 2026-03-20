#include "src/types/key/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/game/game_ws_server.h"
#include <Arduino.h>

struct KeyEventData {
  String title;
  uint8_t key_code;
  uint8_t modifier;
  uint8_t index;
};

lv_obj_t* render_key_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

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

  // Icon Label (optional, falls icon_name vorhanden) - wie bei Scene
  lv_obj_t* icon_lbl = nullptr;
  String iconChar;
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    iconChar = getMdiChar(tile.icon_name);
  }
  bool has_icon = iconChar.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon) {
    icon_lbl = lv_label_create(btn);
    if (icon_lbl) {
      set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
      lv_label_set_text(icon_lbl, iconChar.c_str());

      // Flexible Positionierung: Icon + Title = 2 Zeilen mittig, nur Icon = 1 Zeile mittig
      if (has_title) {
        lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);  // Icon oben (mit Title)
      } else {
        lv_obj_center(icon_lbl);  // Icon mittig (ohne Title)
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden)
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    if (l) {
      set_label_style(l, lv_color_white(), FONT_TITLE);
      lv_label_set_text(l, tile.title.c_str());

      // Flexible Positionierung: mit Icon unten, ohne Icon mittig
      if (icon_lbl) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);  // Title unten (mit Icon)
      } else {
        lv_obj_center(l);  // Title mittig (ohne Icon)
      }
    }
  }

  // Event-Handler für WebSocket Broadcast
  if (tile.key_code != 0) {
    // Allocate permanent storage for event data
    KeyEventData* event_data = new KeyEventData{
      tile.title,
      tile.key_code,
      tile.key_modifier,
      index
    };

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          KeyEventData* data = static_cast<KeyEventData*>(lv_event_get_user_data(e));
          if (data) {
            Serial.printf("[Tile] Key '%s' gedrückt - Code: 0x%02X Mod: 0x%02X\n",
                          data->title.c_str(), data->key_code, data->modifier);

            // WebSocket Broadcast an alle verbundenen Clients
            gameWSServer.broadcastButtonPress(
              data->index,
              data->title.c_str(),
              data->key_code,
              data->modifier
            );
          }
        },
        LV_EVENT_CLICKED,
        event_data);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          KeyEventData* data = static_cast<KeyEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  }

  return btn;
}



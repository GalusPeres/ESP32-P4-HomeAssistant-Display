#include "src/types/navigate/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/ui/ui_manager.h"
#include <Arduino.h>

struct NavigateEventData {
  uint8_t target_kind;
  uint16_t target_folder_id;
  String title;
};

static uint16_t navFolderIdFromTile(const Tile& tile) {
  return static_cast<uint16_t>((static_cast<uint16_t>(tile.key_modifier) << 8) | tile.key_code);
}

lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Settings/Back immer im neutralen Standard-Grau rendern.
  uint32_t btn_color = ((tile.type == TILE_SETTINGS) || (tile.type == TILE_BACK))
                           ? 0x2A2A2A
                           : ((tile.bg_color != 0) ? tile.bg_color : 0x353535);
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_FOCUSED);

  // Pressed-State: 10% heller
  uint32_t pressed_color = brighten_rgb_color(btn_color, 0x10);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(btn);

  set_tile_grid_cell(btn, col, row, tile.span_w, tile.span_h);

  // Icon Label (optional, falls icon_name vorhanden)
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

  // Event-Handler für Tab-Navigation
  static constexpr uint8_t NAV_KIND_FOLDER = 0;
  static constexpr uint8_t NAV_KIND_SETTINGS = 1;
  static constexpr uint8_t NAV_KIND_BACK = 2;
  uint8_t target_kind = NAV_KIND_FOLDER;
  uint16_t target_folder = 0;
  if (tile.type == TILE_SETTINGS) {
    target_kind = NAV_KIND_SETTINGS;
  } else if (tile.type == TILE_BACK) {
    target_kind = NAV_KIND_BACK;
  } else {
    target_kind = NAV_KIND_FOLDER;
    target_folder = navFolderIdFromTile(tile);
  }
  Serial.printf("[Navigate] Render Navigation-Tile - kind=%u, folder=%u\n",
                static_cast<unsigned>(target_kind),
                static_cast<unsigned>(target_folder));

  NavigateEventData* event_data = new NavigateEventData{
    target_kind,
    target_folder,
    tile.title
  };

  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        NavigateEventData* data = static_cast<NavigateEventData*>(lv_event_get_user_data(e));
        if (!data) return;
        if (data->target_kind == NAV_KIND_SETTINGS) {
          Serial.printf("[Tile] Navigation CLICKED! Settings, Titel: %s\n", data->title.c_str());
          uiManager.switchToTab(3);
        } else if (data->target_kind == NAV_KIND_BACK) {
          uint16_t current = tileConfig.getActiveFolderId();
          uint16_t parent = tileConfig.getFolderParent(current);
          Serial.printf("[Tile] Navigation CLICKED! Back to %u, Titel: %s\n",
                        static_cast<unsigned>(parent), data->title.c_str());
          uiManager.switchToFolder(parent);
        } else {
          Serial.printf("[Tile] Navigation CLICKED! Folder %u, Titel: %s\n",
                        static_cast<unsigned>(data->target_folder_id), data->title.c_str());
          uiManager.switchToFolder(data->target_folder_id);
        }
      },
      LV_EVENT_CLICKED,
      event_data);
  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
        NavigateEventData* data = static_cast<NavigateEventData*>(lv_event_get_user_data(e));
        delete data;
      },
      LV_EVENT_DELETE,
      event_data);

  return btn;
}



#include "src/types/camera/renderer.h"

#include "src/network/ha_bridge_config.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/camera_popup.h"

namespace {

struct CameraEventData {
  String entity_id;
  String title;
  String icon_name;
  uint32_t bg_color = 0x2A2A2A;
};

static String friendly_camera_name(const String& entity_id) {
  String name = haBridgeConfig.findSensorName(entity_id);
  if (name.length()) return name;
  const int dot = entity_id.indexOf('.');
  name = dot >= 0 ? entity_id.substring(dot + 1) : entity_id;
  name.replace('_', ' ');
  if (name.length() && name[0] >= 'a' && name[0] <= 'z') {
    name.setCharAt(0, static_cast<char>(name[0] - ('a' - 'A')));
  }
  return name;
}

static void camera_tile_event_cb(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_SHORT_CLICKED) return;
  CameraEventData* data =
      static_cast<CameraEventData*>(lv_event_get_user_data(event));
  if (!data || !data->entity_id.length()) return;
  finish_press_before_popup(event);
  CameraPopupInit init;
  init.entity_id = data->entity_id;
  init.title = data->title;
  init.icon_name = data->icon_name;
  init.bg_color = data->bg_color;
  show_camera_popup(init);
}

static void camera_tile_delete_cb(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
  delete static_cast<CameraEventData*>(lv_event_get_user_data(event));
}

}  // namespace

lv_obj_t* render_camera_tile(lv_obj_t* parent,
                             int col,
                             int row,
                             const Tile& tile,
                             uint8_t,
                             GridType grid_type) {
  if (!parent) return nullptr;

  lv_obj_t* card = lv_button_create(parent);
  if (!card) return nullptr;
  const uint32_t card_color = tileBgColorOrDefault(tile, 0x2A2A2A);
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_NONE,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(card,
                            lv_color_hex(brighten_rgb_color(card_color, 0x10)),
                            LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, tile_layout::scale_480(22), 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, tile_layout::scale_480(18), 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(card);
  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  String icon_name = normalizeMdiIconName(tile.icon_name);
  if (!icon_name.length() && tile.sensor_entity.length()) {
    icon_name =
        normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
  }
  if (!icon_name.length()) icon_name = "video";

  lv_obj_t* icon = lv_label_create(card);
  set_label_style(icon, lv_color_white(), FONT_MDI_ICONS);
  const String icon_char = getMdiChar(icon_name);
  lv_label_set_text(icon, icon_char.c_str());
  lv_obj_align(icon, LV_ALIGN_TOP_LEFT, 0, 0);

  String title = tile.title;
  title.trim();
  if (!title.length()) title = friendly_camera_name(tile.sensor_entity);
  if (!title.length()) title = "Kamera";

  lv_obj_t* title_label = lv_label_create(card);
  set_label_style(title_label, lv_color_white(),
                  tile_layout::header_title_font());
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title_label, LV_PCT(72));
  lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(title_label, title.c_str());
  lv_obj_align(title_label, LV_ALIGN_TOP_RIGHT, 0, 0);

  lv_obj_t* live_label = lv_label_create(card);
  set_label_style(live_label, lv_color_hex(0xD8DEE9),
                  tile_layout::content_font_20());
  lv_label_set_text(live_label, "LIVE");
  lv_obj_align(live_label, LV_ALIGN_CENTER, 0,
               tile_layout::scale_480(18));

  if (grid_type != GridType::SCREENSAVER && tile.sensor_entity.length()) {
    CameraEventData* event_data = new CameraEventData{
        tile.sensor_entity, title, icon_name, card_color};
    lv_obj_add_event_cb(card, camera_tile_event_cb, LV_EVENT_SHORT_CLICKED,
                        event_data);
    lv_obj_add_event_cb(card, camera_tile_delete_cb, LV_EVENT_DELETE,
                        event_data);
  }
  return card;
}

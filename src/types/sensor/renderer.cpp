#include "src/types/sensor/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/sensor_popup.h"
#include <Arduino.h>

static const lv_font_t* get_sensor_value_font(const Tile& tile) {
  switch (tile.sensor_value_font) {
    case 1:
      return &ui_font_20;
    case 2:
      return &ui_font_24;
    default:
      return FONT_VALUE;
  }
}

struct SensorEventData {
  String entity_id;
  String title;
  String icon_name;
  String unit;
};

lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  if (!parent) {
    Serial.println("[TileRenderer] ERROR: parent NULL bei Sensor-Tile");
    return nullptr;
  }

  const bool gauge_enabled = tile.sensor_gauge_enabled;
  int32_t gauge_min = tile.sensor_gauge_min;
  int32_t gauge_max = tile.sensor_gauge_max;
  if (gauge_max <= gauge_min) {
    gauge_min = 0;
    gauge_max = 100;
  }

  lv_obj_t* card = lv_button_create(parent);
  if (!card) {
    Serial.println("[TileRenderer] ERROR: Konnte Sensor-Card nicht erstellen");
    return nullptr;
  }

  // Farbe verwenden (Standard: 0x2A2A2A wenn color = 0)
  uint32_t card_color = (tile.bg_color != 0) ? tile.bg_color : 0x2A2A2A;
  lv_obj_set_style_bg_color(card, lv_color_hex(card_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Pressed-State: 10% heller
  uint32_t pressed_color = card_color + 0x101010;
  lv_obj_set_style_bg_color(card, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);

  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 24, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  // Icon Label (optional, falls icon_name vorhanden) - rechtsbündig
  lv_obj_t* icon_lbl = nullptr;
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(card);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());
        lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);  // Rechtsbündig (4px rechts, 8px hoch)
      }
    }
  }

  lv_obj_t* title_label = nullptr;
  // Title Label (nur anzeigen wenn Titel vorhanden) - linksbündig
  if (tile.title.length() > 0) {
    title_label = lv_label_create(card);
    if (title_label) {
      set_label_style(title_label, lv_color_hex(0xFFFFFF), FONT_TITLE);
      lv_label_set_text(title_label, tile.title.c_str());
      if (gauge_enabled) {
        lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 4);
      } else {
        lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 0, 4);  // Linksbündig
      }
    }
  }

  lv_obj_t* gauge = nullptr;
  if (gauge_enabled) {
    const int gauge_size = 350;
    gauge = lv_arc_create(card);
    if (gauge) {
      lv_obj_set_size(gauge, gauge_size, gauge_size);
      lv_obj_align(gauge, LV_ALIGN_TOP_MID, 0, 12);
      lv_obj_remove_flag(gauge, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_remove_flag(gauge, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_set_style_bg_opa(gauge, LV_OPA_TRANSP, LV_PART_MAIN);
      lv_obj_set_style_border_width(gauge, 0, LV_PART_MAIN);
      lv_obj_set_style_shadow_width(gauge, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(gauge, 0, LV_PART_MAIN);

      lv_arc_set_range(gauge, 0, GAUGE_ARC_STEPS);
      lv_arc_set_value(gauge, 0);
      lv_arc_set_rotation(gauge, 220);
      lv_arc_set_bg_angles(gauge, 0, 100);
      lv_arc_set_angles(gauge, 0, 100);

      lv_obj_set_style_arc_width(gauge, 14, LV_PART_MAIN);
      lv_obj_set_style_arc_color(gauge, lv_color_hex(0x2E2E2E), LV_PART_MAIN);
      lv_obj_set_style_arc_rounded(gauge, true, LV_PART_MAIN);

      lv_obj_set_style_arc_width(gauge, 14, LV_PART_INDICATOR);
      lv_obj_set_style_arc_color(gauge, lv_color_hex(0x20A4FF), LV_PART_INDICATOR);
      lv_obj_set_style_arc_rounded(gauge, true, LV_PART_INDICATOR);

      lv_obj_set_style_bg_opa(gauge, LV_OPA_TRANSP, LV_PART_KNOB);
      lv_obj_set_style_border_opa(gauge, LV_OPA_TRANSP, LV_PART_KNOB);
      lv_obj_set_style_shadow_width(gauge, 0, LV_PART_KNOB);
    }
  }
  if (gauge_enabled) {
    if (icon_lbl) lv_obj_move_foreground(icon_lbl);
    if (title_label) lv_obj_move_foreground(title_label);
  }

  // Value Label (Wert + Einheit kombiniert)
  lv_obj_t* v = lv_label_create(card);
  if (!v) {
    Serial.println("[TileRenderer] ERROR: Konnte Value-Label nicht erstellen");
    return card;
  }
  set_label_style(v, lv_color_white(), get_sensor_value_font(tile));
  lv_label_set_long_mode(v, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(v, LV_PCT(100));
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(v, 2, 0);
  lv_label_set_text(v, "--");
  if (gauge_enabled) {
    lv_obj_align(v, LV_ALIGN_BOTTOM_MID, 0, 12);
  } else {
    lv_obj_align(v, LV_ALIGN_CENTER, 0, 28);  // Nach unten verschoben (war 18)
  }

  // Speichern für spätere Updates
  SensorTileWidgets* target = tile_renderer_get_sensor_widgets(grid_type);
  if (target && index < TILES_PER_GRID) {
    target[index].value_label = v;
    target[index].unit_label = nullptr;
    target[index].gauge = gauge;
    target[index].gauge_min = gauge_min;
    target[index].gauge_max = gauge_max;
  }

  if (tile.sensor_entity.length()) {
    SensorEventData* data = new SensorEventData{
      tile.sensor_entity,
      tile.title,
      tile.icon_name,
      tile.sensor_unit
    };

    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
          SensorEventData* data = static_cast<SensorEventData*>(lv_event_get_user_data(e));
          if (!data || !data->entity_id.length()) return;
          SensorPopupInit init;
          init.entity_id = data->entity_id;
          init.title = data->title;
          if (!init.title.length()) {
            init.title = haBridgeConfig.findSensorName(data->entity_id);
          }
          if (!init.title.length()) {
            init.title = data->entity_id;
          }
          init.icon_name = data->icon_name;
          String unit = data->unit;
          if (!unit.length()) {
            unit = haBridgeConfig.findSensorUnit(data->entity_id);
          }
          init.unit = unit;
          init.value = haBridgeConfig.findSensorInitialValue(data->entity_id);
          show_sensor_popup(init);
        },
        LV_EVENT_LONG_PRESSED,
        data);

    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          SensorEventData* data = static_cast<SensorEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        data);
  }

  return card;
}



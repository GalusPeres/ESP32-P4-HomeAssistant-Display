#include "src/types/switch/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/network/mqtt_handlers.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/light_popup.h"
#include "src/tiles/tile_config.h"
#include <Arduino.h>

struct SwitchEventData {
  String entity_id;
  String title;
  GridType grid_type;
  uint8_t index = 0;
  bool suppress_click = false;
};

struct SwitchWidgetEventData {
  String entity_id;
  GridType grid_type = GridType::TAB0;
  uint8_t index = 0;
};

static SwitchState* get_switch_state_array(GridType grid_type) {
  return tile_renderer_get_switch_states(grid_type);
}

static SwitchState get_switch_state(GridType grid_type, uint8_t index) {
  SwitchState* states = get_switch_state_array(grid_type);
  if (!states || index >= TILES_PER_GRID) return {};
  return states[index];
}

static bool is_switch_widget_tile(const Tile& tile);

static LightPopupInit build_light_popup_init(const SwitchEventData* data) {
  LightPopupInit init;
  if (!data) return init;
  init.entity_id = data->entity_id;
  init.title = data->title;
  init.is_light = is_light_entity_id(data->entity_id);

  // Get icon from tile config (fallback to HA icon when empty)
  const TileGridConfig& grid = tileConfig.getActiveGrid();
  if (data->index < TILES_PER_GRID) {
    const Tile& tile = grid.tiles[data->index];
    init.keep_icon_white = is_switch_widget_tile(tile);
    init.has_tile_ref = true;
    init.tile_grid = static_cast<uint8_t>(data->grid_type);
    init.tile_index = data->index;
    bool icon_disabled = isMdiIconDisabled(tile.icon_name);
    init.icon_name = normalizeMdiIconName(tile.icon_name);
    if (!icon_disabled && !init.icon_name.length() && data->entity_id.length()) {
      init.icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(data->entity_id));
    }
  }

  const SwitchState state = get_switch_state(data->grid_type, data->index);
  init.has_state = state.has_state;
  init.has_color = state.has_color;
  init.has_brightness = state.has_brightness;
  init.has_hs = state.has_hs;
  init.hs_h = state.hs_h;
  init.hs_s = state.hs_s;
  if (state.has_state) {
    init.is_on = state.is_on;
  } else if (state.has_brightness) {
    init.is_on = state.brightness_pct > 0;
  } else {
    init.is_on = true;
  }

  if (init.is_light) {
    init.supports_color = state.supports_color;
    init.supports_brightness = state.supports_brightness || state.supports_color;
  } else {
    init.supports_color = false;
    init.supports_brightness = false;
  }
  if (state.has_color) {
    init.color = state.color;
  }
  if (state.has_brightness) {
    init.brightness_pct = state.brightness_pct;
  } else if (state.has_state && !state.is_on) {
    init.brightness_pct = 0;
  } else {
    init.brightness_pct = 100;
  }
  return init;
}

static bool is_switch_widget_tile(const Tile& tile) {
  return tile.sensor_decimals == 1;
}

lv_obj_t* render_switch_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  const bool use_switch_widget = is_switch_widget_tile(tile);
  lv_obj_t* container = use_switch_widget ? lv_obj_create(parent) : lv_button_create(parent);
  lv_obj_set_style_radius(container, 22, 0);
  lv_obj_set_style_border_width(container, 0, 0);

  // Farbe verwenden (Standard: 0x353535 wenn color = 0)
  uint32_t tile_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(container, lv_color_hex(tile_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  if (!use_switch_widget) {
    // Pressed-State: 10% heller
    uint32_t pressed_color = tile_color + 0x101010;
    lv_obj_set_style_bg_color(container, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  }

  lv_obj_set_style_bg_opa(container, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(container, 0, 0);
  if (use_switch_widget) {
    lv_obj_set_style_pad_hor(container, 20, 0);
    lv_obj_set_style_pad_ver(container, 24, 0);
    lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
  }
  lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(container, col, row, tile.span_w, tile.span_h);

  // Icon Label (optional, falls icon_name vorhanden)
  lv_obj_t* icon_lbl = nullptr;
  lv_obj_t* title_lbl = nullptr;
  String icon_name = tile.icon_name;
  bool icon_disabled = isMdiIconDisabled(icon_name);
  icon_name = normalizeMdiIconName(icon_name);
  if (!icon_disabled && !icon_name.length() && tile.sensor_entity.length()) {
    icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
  }
  String iconChar;
  if (icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    iconChar = getMdiChar(icon_name);
  }
  bool has_icon = iconChar.length() > 0;
  bool has_title = tile.title.length() > 0;

  if (has_icon) {
    icon_lbl = lv_label_create(container);
    if (icon_lbl) {
      set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
      lv_label_set_text(icon_lbl, iconChar.c_str());

      if (use_switch_widget) {
        lv_obj_align(icon_lbl, LV_ALIGN_TOP_RIGHT, 4, -8);
      } else {
        // Flexible Positionierung: Icon + Title = 2 Zeilen mittig, nur Icon = 1 Zeile mittig
        if (has_title) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);
        } else {
          lv_obj_center(icon_lbl);
        }
      }
    }
  }

  // Title Label (nur anzeigen wenn Titel vorhanden)
  if (has_title) {
    title_lbl = lv_label_create(container);
    if (title_lbl) {
      set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
      lv_label_set_text(title_lbl, tile.title.c_str());

      if (use_switch_widget) {
        lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 0, 4);
      } else {
        // Flexible Positionierung: mit Icon unten, ohne Icon mittig
        if (icon_lbl) {
          lv_obj_align(title_lbl, LV_ALIGN_CENTER, 0, 35);
        } else {
          lv_obj_center(title_lbl);
        }
      }
    }
  }

  lv_obj_t* switch_obj = nullptr;
  if (use_switch_widget) {
    switch_obj = lv_switch_create(container);
    if (switch_obj) {
      lv_obj_set_size(switch_obj, 90, 44);
      lv_obj_align(switch_obj, LV_ALIGN_CENTER, 0, 28);
      lv_obj_set_ext_click_area(switch_obj, 18);
      lv_obj_add_flag(switch_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
      lv_obj_set_style_bg_color(switch_obj, lv_color_hex(tile_color), LV_PART_KNOB);
      lv_obj_set_style_bg_color(switch_obj, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_color(switch_obj, lv_color_hex(0x3B82F6), LV_PART_INDICATOR | LV_STATE_CHECKED);
      SwitchWidgetEventData* widget_data = new SwitchWidgetEventData{
        tile.sensor_entity,
        grid_type,
        index
      };
      lv_obj_add_event_cb(
          switch_obj,
          [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
            SwitchWidgetEventData* data = static_cast<SwitchWidgetEventData*>(lv_event_get_user_data(e));
            if (!data || !data->entity_id.length()) return;
            lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            bool is_on = target && lv_obj_has_state(target, LV_STATE_CHECKED);
            update_switch_tile_state(data->grid_type, data->index, is_on ? "on" : "off");
            mqttPublishSwitchCommand(data->entity_id.c_str(), is_on ? "on" : "off");
          },
          LV_EVENT_VALUE_CHANGED,
          widget_data);
      lv_obj_add_event_cb(
          switch_obj,
          [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
            SwitchWidgetEventData* data = static_cast<SwitchWidgetEventData*>(lv_event_get_user_data(e));
            delete data;
          },
          LV_EVENT_DELETE,
          widget_data);
    }
  }

  SwitchTileWidgets* target = tile_renderer_get_switch_widgets(grid_type);
  if (target && index < TILES_PER_GRID) {
    target[index].icon_label = icon_lbl;
    target[index].title_label = title_lbl;
    target[index].switch_obj = switch_obj;
  }

  if (tile.sensor_entity.length()) {
    String initial = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
    if (initial.length()) {
      update_switch_tile_state(grid_type, index, initial.c_str());
    }
  }

  if (tile.sensor_entity.length()) {
    SwitchEventData* event_data = new SwitchEventData{
      tile.sensor_entity,
      tile.title,
      grid_type,
      index,
      false
    };

    if (!use_switch_widget) {
      lv_obj_add_event_cb(
          container,
          [](lv_event_t* e) {
            if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
            SwitchEventData* data = static_cast<SwitchEventData*>(lv_event_get_user_data(e));
            if (!data) return;
            if (data->suppress_click) {
              data->suppress_click = false;
              return;
            }
            Serial.printf("[Tile] Switch toggle: %s\n", data->entity_id.c_str());
            mqttPublishSwitchCommand(data->entity_id.c_str(), "toggle");
          },
          LV_EVENT_CLICKED,
          event_data);
    }

    lv_obj_add_event_cb(
        container,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
          SwitchEventData* data = static_cast<SwitchEventData*>(lv_event_get_user_data(e));
          if (!data) return;
          data->suppress_click = true;
          LightPopupInit init = build_light_popup_init(data);
          show_light_popup(init);
        },
        LV_EVENT_LONG_PRESSED,
        event_data);

    lv_obj_add_event_cb(
        container,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          SwitchEventData* data = static_cast<SwitchEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  }

  return container;
}



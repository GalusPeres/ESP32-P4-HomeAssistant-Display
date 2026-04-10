#include "src/types/weather/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/types/types_registry.h"
#include "src/network/ha_bridge_config.h"
#include "src/ui/weather_popup.h"
#include <Arduino.h>

struct WeatherEventData {
  String entity_id;
  String title;
  lv_obj_t* location_label = nullptr;
  uint32_t bg_color = 0;
};

lv_obj_t* render_weather_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type) {
  if (!parent) {
    Serial.println("[TileRenderer] ERROR: parent NULL bei Weather-Tile");
    return nullptr;
  }

  const uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  const uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;
  const bool show_forecast = (span_h >= 2);
  uint8_t forecast_cols = show_forecast ? weather_forecast_count(span_w) : 0;

  lv_obj_t* card = lv_button_create(parent);
  if (!card) return nullptr;
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_PRESS_LOCK);

  uint32_t card_color = tile.bg_color;
  if (card_color == 0) card_color = get_tile_type_default_bg(tile.type);
  if (card_color == 0) card_color = 0x2A2A2A;
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
  const int16_t pad_hor = 20;
  const int16_t pad_ver = 24;
  lv_obj_set_style_pad_hor(card, pad_hor, 0);
  lv_obj_set_style_pad_ver(card, pad_ver, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(card);

  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  // Ensure press animation behaves like other tiles, even when tapping child widgets.
  lv_obj_add_event_cb(
      card,
      [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
        if (!obj) return;
        if (code == LV_EVENT_PRESSED) {
          lv_obj_add_state(obj, LV_STATE_PRESSED);
        } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
          lv_obj_clear_state(obj, LV_STATE_PRESSED);
        }
      },
      LV_EVENT_ALL,
      nullptr);

  auto enable_bubble = [](lv_obj_t* obj) {
    if (!obj) return;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  };

  // Title (location)
  String location = tile.title;
  if (!location.length() && tile.sensor_entity.length()) {
    location = haBridgeConfig.findSensorName(tile.sensor_entity);
  }
  if (!location.length()) location = "--";

  lv_obj_t* icon_label = lv_label_create(card);
  set_label_style(icon_label, lv_color_white(), FONT_MDI_ICONS);
  lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, -8, -8);
  enable_bubble(icon_label);

  lv_obj_t* location_label = lv_label_create(card);
  set_label_style(location_label, lv_color_white(), FONT_TITLE);
  lv_label_set_long_mode(location_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(location_label, LV_PCT(70));
  lv_obj_set_style_text_align(location_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(location_label, location.c_str());
  lv_obj_align(location_label, LV_ALIGN_TOP_RIGHT, 4, 4);
  enable_bubble(location_label);

  String icon_name = tile.icon_name;
  bool icon_disabled = isMdiIconDisabled(icon_name);
  icon_name = normalizeMdiIconName(icon_name);
  if (!icon_disabled && !icon_name.length() && tile.sensor_entity.length()) {
    icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(tile.sensor_entity));
  }
  String iconChar;
  if (!icon_disabled && icon_name.length() && FONT_MDI_ICONS != nullptr) {
    iconChar = getMdiChar(icon_name);
  }
  if (icon_label) {
    if (iconChar.length()) {
      lv_label_set_text(icon_label, iconChar.c_str());
      lv_obj_clear_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_label_set_text(icon_label, "");
      lv_obj_add_flag(icon_label, LV_OBJ_FLAG_HIDDEN);
    }
  }

  lv_obj_t* value_row = lv_obj_create(card);
  lv_obj_remove_style_all(value_row);
  lv_obj_set_size(value_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(value_row, 14, 0);
  lv_obj_set_style_bg_opa(value_row, LV_OPA_TRANSP, 0);
  enable_bubble(value_row);
  lv_obj_remove_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* condition_label = nullptr;
  lv_obj_t* sep_label = nullptr;
  lv_obj_t* temp_label = nullptr;

  if (span_w > 1) {
    const lv_font_t* condition_font = FONT_VALUE;

    condition_label = lv_label_create(value_row);
    set_label_style(condition_label, lv_color_white(), condition_font);
    lv_label_set_long_mode(condition_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(condition_label, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(condition_label, (span_w >= 3) ? 220 : 140, 0);
    lv_label_set_text(condition_label, "--");
    lv_obj_add_flag(condition_label, LV_OBJ_FLAG_HIDDEN);
    enable_bubble(condition_label);

    sep_label = lv_label_create(value_row);
    set_label_style(sep_label, lv_color_hex(0xB0B0B0), FONT_VALUE);
    lv_label_set_text(sep_label, "|");
    lv_obj_add_flag(sep_label, LV_OBJ_FLAG_HIDDEN);
    enable_bubble(sep_label);
  }

  temp_label = lv_label_create(value_row);
  set_label_style(temp_label, lv_color_white(), FONT_VALUE);
  lv_label_set_text(temp_label, "--");
  enable_bubble(temp_label);

  // Position like a 1x1 sensor tile: keep the same top offset regardless of span.
  lv_obj_update_layout(value_row);
  lv_coord_t row_h = lv_obj_get_height(value_row);
  lv_coord_t base_center = ((GRID_CELL_H - 48) / 2) + 28;  // content center + offset (pad_ver=24)
  lv_coord_t value_row_y = base_center - (row_h / 2);
  lv_obj_align(value_row, LV_ALIGN_TOP_MID, 0, value_row_y);

  lv_obj_t* forecast_row = nullptr;
  if (show_forecast) {
    forecast_row = lv_obj_create(card);
    lv_obj_remove_style_all(forecast_row);
    lv_obj_set_size(forecast_row,
                    (span_w * GRID_CELL_W) + ((span_w - 1) * GRID_GAP),
                    GRID_CELL_H);
    lv_obj_set_style_bg_opa(forecast_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(forecast_row, 0, 0);
    enable_bubble(forecast_row);
    lv_obj_remove_flag(forecast_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(forecast_row, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_coord_t forecast_x = -pad_hor;
    lv_coord_t forecast_y = ((span_h - 1) * (GRID_CELL_H + GRID_GAP)) - pad_ver;
    lv_obj_set_pos(forecast_row, forecast_x, forecast_y);
  }

  WeatherTileWidgets* target = tile_renderer_get_weather_widgets(grid_type);
  if (target && index < TILES_PER_GRID) {
    WeatherTileWidgets widgets{};
    widgets.icon_label = icon_label;
    widgets.temp_label = temp_label;
    widgets.condition_label = condition_label;
    widgets.condition_sep_label = sep_label;
    widgets.location_label = location_label;

    if (forecast_row && forecast_cols > 0) {
      const lv_coord_t total_w = (span_w * GRID_CELL_W) + ((span_w - 1) * GRID_GAP);
      const lv_coord_t cols_total = forecast_cols * WEATHER_FORECAST_COL_W;
      const lv_coord_t remaining = total_w - cols_total;
      // Gleichmäßig verteilen: Rand links + Gaps + Rand rechts = (forecast_cols + 1) Abstände
      const lv_coord_t spacing = remaining / (forecast_cols + 1);
      for (uint8_t i = 0; i < forecast_cols; ++i) {
        lv_obj_t* col = lv_obj_create(forecast_row);
        lv_obj_remove_style_all(col);
        lv_obj_set_size(col, WEATHER_FORECAST_COL_W, GRID_CELL_H);
        lv_obj_set_style_pad_hor(col, pad_hor, 0);
        lv_obj_set_style_pad_ver(col, pad_ver, 0);
        lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
        enable_bubble(col);
        lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(col, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_set_pos(col, spacing + i * (WEATHER_FORECAST_COL_W + spacing), 0);

        constexpr lv_coord_t kTileForecastDayTop = -33;
        constexpr lv_coord_t kTileForecastIconTop = -8;
        constexpr lv_coord_t kTileForecastTempTop = 54;

        lv_obj_t* day = lv_label_create(col);
        set_label_style(day, lv_color_white(), FONT_TITLE);
        lv_label_set_long_mode(day, LV_LABEL_LONG_DOT);
        lv_obj_set_width(day, LV_PCT(100));
        lv_obj_set_style_text_align(day, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(day, "--");
        lv_obj_set_pos(day, 0, kTileForecastDayTop);
        enable_bubble(day);

        lv_obj_t* icon = lv_label_create(col);
        set_label_style(icon, lv_color_white(), FONT_MDI_ICONS);
        lv_obj_set_width(icon, LV_PCT(100));
        lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(icon, "");
        lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(icon, 0, kTileForecastIconTop);
        enable_bubble(icon);

        lv_obj_t* temp = lv_label_create(col);
        set_label_style(temp, lv_color_white(), &ui_font_24);
        lv_label_set_long_mode(temp, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(temp, LV_PCT(100));
        lv_obj_set_style_text_align(temp, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_line_space(temp, 6, 0);
        lv_label_set_text(temp, "--\n--");
        lv_obj_set_pos(temp, 0, kTileForecastTempTop);
        enable_bubble(temp);

        widgets.forecast[i].day_label = day;
        widgets.forecast[i].icon_label = icon;
        widgets.forecast[i].temp_label = temp;
      }
    }

    target[index] = widgets;
  }

  if (tile.sensor_entity.length()) {
    WeatherEventData* data = new WeatherEventData{
      tile.sensor_entity,
      location,
      location_label,
      card_color
    };

    const lv_event_code_t popup_event =
        (getTilePopupOpenMode(tile) == TILE_POPUP_OPEN_SHORT_PRESS)
            ? LV_EVENT_SHORT_CLICKED
            : LV_EVENT_LONG_PRESSED;

    auto show_popup = [](lv_event_t* e) {
      lv_event_code_t code = lv_event_get_code(e);
      if (code != LV_EVENT_SHORT_CLICKED && code != LV_EVENT_LONG_PRESSED) return;
      WeatherEventData* data = static_cast<WeatherEventData*>(lv_event_get_user_data(e));
      if (!data || !data->entity_id.length()) return;
      String title = data->title;
      if (data->location_label) {
        const char* label_text = lv_label_get_text(data->location_label);
        if (label_text && *label_text) {
          title = label_text;
        }
      }
      title.trim();
      if (title == "--") title = "";
      WeatherPopupInit init;
      init.entity_id = data->entity_id;
      init.title = title;
      init.bg_color = data->bg_color;
      show_weather_popup(init);
    };

    lv_obj_add_event_cb(card, show_popup, popup_event, data);

    lv_obj_add_event_cb(
        card,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          WeatherEventData* data = static_cast<WeatherEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        data);
  }

  return card;
}

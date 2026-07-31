#pragma once

#include <lvgl.h>
#include "src/tiles/tile_renderer.h"

static constexpr int32_t GAUGE_ARC_STEPS = 1000;

// Extern declarations for global widget arrays
extern SensorTileWidgets g_tab0_sensors[];
extern SensorTileWidgets g_tab1_sensors[];
extern SensorTileWidgets g_tab2_sensors[];
extern SensorTileWidgets g_screensaver_sensors[];

extern SwitchTileWidgets g_tab0_switches[];
extern SwitchTileWidgets g_tab1_switches[];
extern SwitchTileWidgets g_tab2_switches[];
extern SwitchTileWidgets g_screensaver_switches[];

extern WeatherTileWidgets g_tab0_weather[];
extern WeatherTileWidgets g_tab1_weather[];
extern WeatherTileWidgets g_tab2_weather[];

extern MediaTileWidgets g_tab0_media[];
extern MediaTileWidgets g_tab1_media[];
extern MediaTileWidgets g_tab2_media[];
extern MediaTileWidgets g_screensaver_media[];

extern SwitchState g_tab0_switch_states[];
extern SwitchState g_tab1_switch_states[];
extern SwitchState g_tab2_switch_states[];
extern SwitchState g_screensaver_switch_states[];

extern ClimateTileWidgets g_tab0_climate[];
extern ClimateTileWidgets g_tab1_climate[];
extern ClimateTileWidgets g_tab2_climate[];

void set_label_style(lv_obj_t* lbl, lv_color_t c, const lv_font_t* f);
void set_tile_grid_cell(lv_obj_t* obj, uint8_t col, uint8_t row, uint8_t span_w, uint8_t span_h);

SensorTileWidgets* tile_renderer_get_sensor_widgets(GridType grid_type);
SwitchTileWidgets* tile_renderer_get_switch_widgets(GridType grid_type);
WeatherTileWidgets* tile_renderer_get_weather_widgets(GridType grid_type);
MediaTileWidgets* tile_renderer_get_media_widgets(GridType grid_type);
SwitchState* tile_renderer_get_switch_states(GridType grid_type);
ClimateTileWidgets* tile_renderer_get_climate_widgets(GridType grid_type);
ClimateState* tile_renderer_get_climate_states(GridType grid_type);
const Tile* tile_renderer_get_tile_config(GridType grid_type, uint8_t index);

bool is_light_entity_id(const String& entity_id);
void update_switch_tile_state(GridType grid_type, uint8_t grid_index, const char* payload);
void update_media_tile_state(GridType grid_type, uint8_t grid_index, const char* payload);

// Beim Zerstoeren einer Media-Karte (LV_EVENT_DELETE) aufrufen: nullt alle
// Widget-Array-Eintraege, die noch auf diesen (gleich freigegebenen) Ref
// zeigen. Ohne das behaelt z.B. ein per Web-Admin geloeschter Tile-Slot
// seinen Eintrag dauerhaft und der Cover-Retry liest freigegebenen Speicher
// (Absturz "Load access fault" in String::startsWith, v0.4.16-Coredump).
void tile_renderer_forget_media_widgets(const MediaCoverRef* ref);

static inline uint32_t brighten_rgb_color(uint32_t color, uint8_t delta) {
  const uint16_t r = ((color >> 16) & 0xFFu) + delta;
  const uint16_t g = ((color >> 8) & 0xFFu) + delta;
  const uint16_t b = (color & 0xFFu) + delta;
  return ((r > 0xFFu ? 0xFFu : r) << 16) |
         ((g > 0xFFu ? 0xFFu : g) << 8) |
         (b > 0xFFu ? 0xFFu : b);
}

static inline void disable_pressed_button_animation(lv_obj_t* obj) {
  if (!obj) return;
  lv_obj_set_style_transform_width(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_translate_x(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_translate_y(obj, 0, LV_PART_MAIN | LV_STATE_PRESSED);
}

// Ein vorgeladenes Popup wird im Klick-Callback sofort eingeblendet. Ist der
// letzte Press-/Release-Refresh der auslösenden Kachel noch offen, rendert
// LVGL deren rechteckige Dirty-Fläche bereits mit einem einzelnen Ausschnitt
// des Popups. Den Button deshalb zuerst vollständig in den Normalzustand
// zeichnen; anschließend kann das Popup sauber von oben nach unten erscheinen.
static inline void finish_press_before_popup(lv_event_t* event) {
  if (!event) return;
  lv_obj_t* source = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (source) {
    lv_obj_clear_state(source, LV_STATE_PRESSED);
    lv_obj_invalidate(source);
  }
  lv_display_t* display = lv_display_get_default();
  if (display) lv_refr_now(display);
}

// Weather and other large preloaded popups should not force a nested display
// refresh from inside the input callback.  Finish the pressed frame first and
// open the popup only after LVGL has rendered and flushed that frame.  A plain
// lv_async_call() is insufficient here: its zero-delay timer can still run in
// the same lv_timer_handler() pass before the display refresh timer.
template <typename Init>
struct DeferredPopupAfterRefresh {
  Init init{};
  void (*show)(const Init&) = nullptr;
  lv_display_t* registered_display = nullptr;
  bool pending = false;
};

template <typename Init>
static DeferredPopupAfterRefresh<Init>& deferred_popup_after_refresh_state() {
  static DeferredPopupAfterRefresh<Init> state;
  return state;
}

template <typename Init>
static void deferred_popup_after_refresh_cb(lv_event_t* event) {
  auto* state = static_cast<DeferredPopupAfterRefresh<Init>*>(
      lv_event_get_user_data(event));
  if (!state || !state->pending || !state->registered_display) return;

  // Clear the shared state before invoking user code. This keeps the helper
  // correct even if show() synchronously schedules another popup of this type.
  Init init = state->init;
  void (*show)(const Init&) = state->show;
  state->pending = false;
  state->show = nullptr;
  state->init = Init{};
  if (show) show(init);
}

template <typename Init>
static inline void defer_popup_until_source_refreshed(
    lv_event_t* event, const Init& init, void (*show)(const Init&)) {
  if (!event || !show) return;

  // Weather children bubble their events to the card. The card is the current
  // target and owns the pressed style; the original target may only be a label.
  lv_obj_t* source = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
  if (!source) source = static_cast<lv_obj_t*>(lv_event_get_target(event));
  lv_display_t* display = source ? lv_obj_get_display(source)
                                 : lv_display_get_default();
  if (source) {
    lv_obj_clear_state(source, LV_STATE_PRESSED);
    lv_obj_invalidate(source);
  }
  if (!display) {
    show(init);
    return;
  }

  auto& state = deferred_popup_after_refresh_state<Init>();
  if (state.registered_display && state.registered_display != display) {
    lv_display_remove_event_cb_with_user_data(
        state.registered_display, deferred_popup_after_refresh_cb<Init>, &state);
    state.registered_display = nullptr;
  }

  state.init = init;
  state.show = show;
  state.pending = true;
  if (!state.registered_display) {
    // Keep one callback for the static display instead of allocating and
    // freeing an LVGL event descriptor on every Weather click.
    lv_display_add_event_cb(display, deferred_popup_after_refresh_cb<Init>,
                            LV_EVENT_REFR_READY, &state);
    state.registered_display = display;
  }
}

#include <lvgl.h>
#include <WiFi.h>
#include "src/ui/tab_settings.h"
#include "src/core/config_manager.h"
#include "src/core/board_hal.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/ui_manager.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/core/display_manager.h"

static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *display_rotate_btn = nullptr;
static lv_obj_t *display_rotate_label = nullptr;
static lv_obj_t *display_rotate_sub_label = nullptr;
static bool display_rotated_180 = false;
static uint8_t display_rotation_mode = kDisplayRotationNormal;
static lv_obj_t *mains_wake_btn = nullptr;
static lv_obj_t *mains_wake_label = nullptr;
static lv_obj_t *mains_wake_sub_label = nullptr;
// Battery wake removed (no battery on Waveshare)
static lv_obj_t *battery_wake_btn = nullptr;
static lv_obj_t *battery_wake_label = nullptr;
static lv_obj_t *battery_wake_sub_label = nullptr;
static uint8_t wake_mode_mains = kWakeModeTouch;
static uint8_t wake_mode_battery = kWakeModeTouch;
static hotspot_callback_t g_hotspot_callback = nullptr;

// WiFi Status Labels
static lv_obj_t *wifi_status_label = nullptr;
static lv_obj_t *wifi_ssid_label = nullptr;
static lv_obj_t *wifi_ip_label = nullptr;
static lv_obj_t *ap_mode_btn = nullptr;
static lv_obj_t *ap_mode_btn_label = nullptr;
static lv_obj_t *ap_confirm_row = nullptr;
static lv_obj_t *ap_confirm_yes_btn = nullptr;
static lv_obj_t *ap_confirm_no_btn = nullptr;
static lv_obj_t *mqtt_notice_label = nullptr;
static lv_timer_t *ap_confirm_timer = nullptr;
static bool ap_mode_confirm_pending = false;
static bool ap_mode_active = false;
static uint32_t ap_mode_click_block_until = 0;

// Sleep Settings
static lv_obj_t *sleep_slider = nullptr;
static lv_obj_t *sleep_time_label = nullptr;
static lv_obj_t *sleep_label = nullptr;

// Power Status Labels (stubs – no battery display)
static lv_obj_t *power_status_label = nullptr;
static lv_obj_t *power_level_label = nullptr;
static lv_obj_t *battery_icon_label = nullptr;
static lv_obj_t *battery_percent_label = nullptr;

// Layout constants – kompakter fuer 720×720, 4×4 Grid
static const int kSettingsColLeftPct = 15;
static const int kSettingsColRightPct = 85;
static const int kSettingsColGap = 8;
static const int kSettingsColRowGap = 4;
static const int kSettingsBtnHeight = 72;
static const int kSettingsButtonWidthPct = 90;
static const int kSettingsSliderLabelWidth = 160;
static const int kSettingsSliderValueWidth = 70;
static const int kSettingsSliderHeight = 16;
static const int kSettingsSliderKnobSize = 36;
static const int kSettingsSliderClickPad = 20;
static const uint8_t kSettingsCardColStart = 1;

// Forward declarations
void settings_update_ap_mode(bool running);

static void style_settings_button(lv_obj_t *btn, uint32_t base_color) {
  if (!btn) return;
  uint32_t pressed_color = brighten_rgb_color(base_color, 0x10);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(base_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
}
static uint16_t sleep_seconds_from_index(int32_t index) {
  if (index < 0) {
    index = 0;
  } else if (index >= static_cast<int32_t>(kSleepOptionsSecCount)) {
    index = static_cast<int32_t>(kSleepOptionsSecCount) - 1;
  }
  return kSleepOptionsSec[index];
}

static int32_t sleep_index_from_seconds(uint16_t seconds) {
  uint16_t closest = kSleepOptionsSec[0];
  int32_t closest_index = 0;
  uint16_t best_diff = (seconds > closest) ? (seconds - closest) : (closest - seconds);
  for (size_t i = 1; i < kSleepOptionsSecCount; ++i) {
    uint16_t option = kSleepOptionsSec[i];
    uint16_t diff = (seconds > option) ? (seconds - option) : (option - seconds);
    if (diff < best_diff) {
      best_diff = diff;
      closest = option;
      closest_index = static_cast<int32_t>(i);
    }
  }
  return closest_index;
}

static void format_sleep_label(char* buf, size_t len, uint16_t seconds) {
  if (seconds <= 60) {
    snprintf(buf, len, "%u s", static_cast<unsigned>(seconds));
  } else {
    snprintf(buf, len, "%u min", static_cast<unsigned>(seconds / 60));
  }
}

static int32_t sleep_slider_max_index() {
  return static_cast<int32_t>(kSleepOptionsSecCount);
}

static bool sleep_index_is_never(int32_t index) {
  return index >= static_cast<int32_t>(kSleepOptionsSecCount);
}

static void format_sleep_label_for_index(char* buf, size_t len, int32_t index) {
  if (sleep_index_is_never(index)) {
    snprintf(buf, len, "Nie");
    return;
  }
  format_sleep_label(buf, len, sleep_seconds_from_index(index));
}

static const char* rotation_mode_text(uint8_t mode) {
  if (mode == kDisplayRotationFlipped) return "180\xC2\xB0";
  if (mode == kDisplayRotationAuto) return "Auto";
  return "Normal";
}

static void update_display_rotate_label() {
  if (!display_rotate_label) return;
  uint8_t next = (display_rotation_mode == kDisplayRotationNormal)
                 ? kDisplayRotationFlipped
                 : (display_rotation_mode == kDisplayRotationFlipped)
                     ? kDisplayRotationAuto
                     : kDisplayRotationNormal;
  const char* next_text = rotation_mode_text(next);
  const char* cur_text = rotation_mode_text(display_rotation_mode);
  static char main_buf[24];
  static char sub_buf[32];
  snprintf(main_buf, sizeof(main_buf), "%s", next_text);
  snprintf(sub_buf, sizeof(sub_buf), "Aktuell: %s", cur_text);
  lv_label_set_text(display_rotate_label, main_buf);
  if (display_rotate_sub_label) {
    lv_label_set_text(display_rotate_sub_label, sub_buf);
  }
}

static const char* wake_mode_text(uint8_t mode) {
  (void)mode;
  return "Touch";
}

static void update_wake_button(lv_obj_t *main_label, lv_obj_t *sub_label, uint8_t mode) {
  (void)mode;
  if (!main_label) return;
  wake_mode_mains = kWakeModeTouch;
  wake_mode_battery = kWakeModeTouch;
  lv_label_set_text(main_label, wake_mode_text(kWakeModeTouch));
  if (sub_label) {
    lv_label_set_text(sub_label, "GT911 / kein IMU");
  }
}

void settings_sync_display_rotation(bool rotated) {
  display_rotated_180 = rotated;
  display_rotation_mode = rotated ? kDisplayRotationFlipped : kDisplayRotationNormal;
  update_display_rotate_label();
  lv_obj_invalidate(lv_scr_act());
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_refr_now(disp);
  }
}

static void on_display_rotate_clicked(lv_event_t *e) {
  (void)e;
  if (display_rotation_mode == kDisplayRotationNormal) {
    display_rotation_mode = kDisplayRotationFlipped;
    display_rotated_180 = true;
    displayManager.setRotationFlipped(true);
  } else if (display_rotation_mode == kDisplayRotationFlipped) {
    display_rotation_mode = kDisplayRotationAuto;
    display_rotated_180 = displayManager.isRotationFlipped();
  } else {
    display_rotation_mode = kDisplayRotationNormal;
    display_rotated_180 = false;
    displayManager.setRotationFlipped(false);
  }
  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      display_rotation_mode,
      display_rotated_180,
      wake_mode_mains,
      wake_mode_battery);
  mqttPublishDeviceSettings();
  update_display_rotate_label();
  lv_obj_invalidate(lv_scr_act());
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_refr_now(disp);
  }
}

static void on_mains_wake_clicked(lv_event_t *e) {
  (void)e;
  update_wake_button(mains_wake_label, mains_wake_sub_label, kWakeModeTouch);
}

static void on_brightness(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t value = lv_slider_get_value(slider);

  BoardHAL::setBrightness(value);

  static char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)value);
  if (brightness_label) lv_label_set_text(brightness_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    configManager.saveDisplaySettings(
        value,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds,
        display_rotation_mode,
        display_rotated_180,
        wake_mode_mains,
        wake_mode_battery);
    mqttPublishDeviceSettings();
  }
}

static lv_obj_t *create_settings_column(lv_obj_t *parent, lv_coord_t width_pct,
                                        lv_flex_align_t main_align, lv_flex_align_t cross_align) {
  lv_obj_t *col = lv_obj_create(parent);
  lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(col, LV_PCT(width_pct));
  lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(col, 0, 0);
  lv_obj_set_style_pad_row(col, kSettingsColRowGap, 0);
  lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(col, main_align, cross_align, cross_align);
  return col;
}

static void create_icon_block(lv_obj_t *parent, const char *icon_name, const char *label_text) {
  lv_obj_t *icon = lv_label_create(parent);
  String icon_char = getMdiChar(String(icon_name));
  lv_label_set_text(icon, icon_char.c_str());
  if (FONT_MDI_ICONS) {
    lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  }
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);

  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, label_text);
  lv_obj_set_style_text_font(label, &ui_font_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_margin_top(label, 4, 0);
}

static void on_settings_back_clicked(lv_event_t *e) {
  (void)e;
  uiManager.switchToTab(0);
}

static void create_settings_back_button(lv_obj_t *parent) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 18, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, 0, 1,
      LV_GRID_ALIGN_STRETCH, 0, 1);

  uint32_t btn_color = 0x2A2A2A;
  style_settings_button(btn, btn_color);

  lv_obj_add_event_cb(btn, on_settings_back_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *icon = lv_label_create(btn);
  String icon_char = getMdiChar(String("arrow-left"));
  lv_label_set_text(icon, icon_char.c_str());
  if (FONT_MDI_ICONS) {
    lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  }
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_center(icon);
}

static void on_sleep_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  static char buf[16];
  format_sleep_label_for_index(buf, sizeof(buf), index);
  if (sleep_time_label) lv_label_set_text(sleep_time_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    bool enabled = !sleep_index_is_never(index);
    uint16_t seconds = enabled ? sleep_seconds_from_index(index) : cfg.auto_sleep_seconds;
    configManager.saveDisplaySettings(
        cfg.display_brightness,
        enabled,
        seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds,
        display_rotation_mode,
        display_rotated_180,
        wake_mode_mains,
        wake_mode_battery);
    mqttPublishDeviceSettings();
  }
}

// Power Status Update (stub – no battery on Waveshare)
void settings_update_power_status() {
  // No battery to monitor – nothing to update
}

static void clear_ap_confirm_timer() {
  if (ap_confirm_timer) {
    lv_timer_del(ap_confirm_timer);
    ap_confirm_timer = nullptr;
  }
}

static void hide_ap_confirm_row() {
  if (ap_confirm_row) {
    lv_obj_add_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);
  }
  clear_ap_confirm_timer();
  ap_mode_confirm_pending = false;
  if (ap_mode_btn) {
    lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_ap_confirm_timeout(lv_timer_t *timer) {
  if (timer) {
    lv_timer_del(timer);
  }
  ap_confirm_timer = nullptr;
  hide_ap_confirm_row();
}

static void on_confirm_yes_clicked(lv_event_t *e) {
  if (g_hotspot_callback) {
    g_hotspot_callback(true);
  }
  settings_update_ap_mode(true);
  ap_mode_click_block_until = millis() + 400;
  hide_ap_confirm_row();
}

static void on_confirm_no_clicked(lv_event_t *e) {
  ap_mode_click_block_until = millis() + 400;
  hide_ap_confirm_row();
}

static void on_ap_mode_clicked(lv_event_t *e) {
  if (ap_mode_click_block_until != 0 &&
      (int32_t)(millis() - ap_mode_click_block_until) < 0) {
    return;
  }
  lv_obj_t *btn = (lv_obj_t*)lv_event_get_current_target(e);
  if (btn) {
    ap_mode_btn = btn;
    ap_mode_btn_label = lv_obj_get_child(btn, 0);
  }
  if (ap_mode_active) {
    if (g_hotspot_callback) {
      g_hotspot_callback(false);
    }
    return;
  }
  if (ap_confirm_row && !ap_mode_confirm_pending) {
    ap_mode_confirm_pending = true;
    lv_obj_clear_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);
    clear_ap_confirm_timer();
    ap_confirm_timer = lv_timer_create(on_ap_confirm_timeout, 10000, nullptr);
    if (btn) {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (ap_mode_btn) {
      lv_obj_add_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static lv_obj_t *create_settings_card(lv_obj_t *parent, uint8_t col, uint8_t row) {
  lv_obj_t *card = lv_obj_create(parent);
  uint8_t span = (col < GRID_COLS) ? (GRID_COLS - col) : 1;
  lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, span, LV_GRID_ALIGN_STRETCH, row, 1);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(card, 18, 0);
  lv_obj_set_style_pad_hor(card, 12, 0);
  lv_obj_set_style_pad_ver(card, 10, 0);
  lv_obj_set_style_pad_row(card, 4, 0);
  lv_obj_set_style_pad_column(card, kSettingsColGap, 0);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return card;
}

static lv_obj_t *create_card_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t *create_slider_row(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_bottom(row, 2, 0);
  lv_obj_set_style_pad_right(row, 12, 0);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static void style_settings_slider(lv_obj_t *slider) {
  lv_obj_set_height(slider, kSettingsSliderHeight);
  lv_obj_set_style_width(slider, kSettingsSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_style_height(slider, kSettingsSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, kSettingsSliderClickPad);
}

// ========== Public API ==========
void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb) {
  g_hotspot_callback = hotspot_cb;
  ap_mode_confirm_pending = false;

  lv_obj_clean(tab);
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(tab, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(tab, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(tab, GRID_PAD, 0);

  // 4×4 Grid
  static int32_t col_dsc[] = {
    GRID_CELL_W, GRID_CELL_W, GRID_CELL_W, GRID_CELL_W,
    LV_GRID_TEMPLATE_LAST
  };
  static int32_t row_dsc[] = {
    GRID_CELL_H, GRID_CELL_H, GRID_CELL_H, GRID_CELL_H,
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_layout(tab, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(tab, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(tab, GRID_GAP, 0);
  lv_obj_set_style_pad_row(tab, GRID_GAP, 0);

  const DeviceConfig& cfg = configManager.getConfig();
  display_rotated_180 = cfg.display_rotated_180;
  display_rotation_mode = cfg.display_rotation_mode;
  wake_mode_mains = kWakeModeTouch;
  wake_mode_battery = kWakeModeTouch;

  // ===== Row 0: Back + Display (Helligkeit + Rotation) =====
  create_settings_back_button(tab);

  lv_obj_t *card_display = create_settings_card(tab, kSettingsCardColStart, 0);
  lv_obj_t *display_col_left = create_settings_column(
      card_display, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(display_col_left, "monitor", "Display");

  lv_obj_t *display_col_right = create_settings_column(
      card_display, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(display_col_right, 8, 0);

  // Brightness slider row
  lv_obj_t *brightness_row = create_slider_row(display_col_right);
  lv_obj_t *brightness_row_label = lv_label_create(brightness_row);
  lv_label_set_text(brightness_row_label, "Helligkeit");
  lv_obj_set_width(brightness_row_label, kSettingsSliderLabelWidth);
  lv_label_set_long_mode(brightness_row_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(brightness_row_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(brightness_row_label, lv_color_white(), 0);
  lv_obj_t *brightness_slider = lv_slider_create(brightness_row);
  style_settings_slider(brightness_slider);
  lv_obj_set_flex_grow(brightness_slider, 1);
  lv_slider_set_range(brightness_slider, 75, 255);
  int current_brightness = BoardHAL::getBrightness();
  lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_RELEASED, nullptr);

  brightness_label = lv_label_create(brightness_row);
  lv_obj_set_width(brightness_label, kSettingsSliderValueWidth);
  lv_label_set_long_mode(brightness_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(brightness_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(brightness_label, LV_TEXT_ALIGN_RIGHT, 0);

  static char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d", current_brightness);
  lv_label_set_text(brightness_label, bright_buf);

  // Rotation button row
  lv_obj_t *rotate_row = create_slider_row(display_col_right);
  lv_obj_t *rotate_row_label = lv_label_create(rotate_row);
  lv_label_set_text(rotate_row_label, "Rotation");
  lv_obj_set_width(rotate_row_label, kSettingsSliderLabelWidth);
  lv_label_set_long_mode(rotate_row_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(rotate_row_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(rotate_row_label, lv_color_white(), 0);

  display_rotate_btn = lv_button_create(rotate_row);
  lv_obj_set_height(display_rotate_btn, 40);
  lv_obj_set_flex_grow(display_rotate_btn, 1);
  style_settings_button(display_rotate_btn, 0x3B82F6);
  lv_obj_set_style_border_opa(display_rotate_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(display_rotate_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(display_rotate_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(display_rotate_btn, 12, 0);
  lv_obj_add_event_cb(display_rotate_btn, on_display_rotate_clicked, LV_EVENT_CLICKED, nullptr);

  display_rotate_label = lv_label_create(display_rotate_btn);
  lv_obj_set_style_text_font(display_rotate_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(display_rotate_label, lv_color_white(), 0);
  lv_obj_center(display_rotate_label);
  display_rotate_sub_label = nullptr;  // kein Sub-Label im kompakten Layout
  update_display_rotate_label();

  // ===== Row 1: WiFi =====
  lv_obj_t *card_wifi = create_settings_card(tab, 0, 1);
  // WiFi spans all 4 cols
  lv_obj_set_grid_cell(card_wifi, LV_GRID_ALIGN_STRETCH, 0, GRID_COLS, LV_GRID_ALIGN_STRETCH, 1, 1);

  lv_obj_t *wifi_col_left = create_settings_column(
      card_wifi, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(wifi_col_left, "wifi", "WLAN");

  lv_obj_t *wifi_col_mid = create_settings_column(
      card_wifi, 50, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(wifi_col_mid, 8, 0);
  wifi_status_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Offline");
  lv_obj_set_width(wifi_status_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(wifi_status_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);

  wifi_ssid_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(wifi_ssid_label, "SSID: ---");
  lv_obj_set_width(wifi_ssid_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_ssid_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(wifi_ssid_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xC8C8C8), 0);

  wifi_ip_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(wifi_ip_label, "IP: ---");
  lv_obj_set_width(wifi_ip_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_ip_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(wifi_ip_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0xC8C8C8), 0);

  mqtt_notice_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(mqtt_notice_label, "MQTT nicht konfiguriert");
  lv_obj_set_width(mqtt_notice_label, LV_PCT(100));
  lv_label_set_long_mode(mqtt_notice_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(mqtt_notice_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(mqtt_notice_label, lv_color_hex(0xFFC04D), 0);
  lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *wifi_col_right = create_settings_column(
      card_wifi, 35, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  ap_mode_btn = lv_button_create(wifi_col_right);
  lv_obj_set_width(ap_mode_btn, LV_PCT(kSettingsButtonWidthPct));
  lv_obj_set_height(ap_mode_btn, kSettingsBtnHeight);
  style_settings_button(ap_mode_btn, 0xFF9800);
  lv_obj_set_style_border_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_mode_btn, 14, 0);
  lv_obj_add_event_cb(ap_mode_btn, on_ap_mode_clicked, LV_EVENT_CLICKED, nullptr);

  ap_mode_btn_label = lv_label_create(ap_mode_btn);
  lv_label_set_text(ap_mode_btn_label, "AP aktivieren");
  lv_obj_set_style_text_font(ap_mode_btn_label, &ui_font_20, 0);
  lv_obj_center(ap_mode_btn_label);

  ap_confirm_row = create_card_row(wifi_col_right);
  lv_obj_set_width(ap_confirm_row, LV_PCT(kSettingsButtonWidthPct));
  lv_obj_set_height(ap_confirm_row, kSettingsBtnHeight);
  lv_obj_set_style_pad_column(ap_confirm_row, 8, 0);
  lv_obj_add_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);

  ap_confirm_yes_btn = lv_button_create(ap_confirm_row);
  lv_obj_set_height(ap_confirm_yes_btn, kSettingsBtnHeight);
  lv_obj_set_flex_grow(ap_confirm_yes_btn, 1);
  style_settings_button(ap_confirm_yes_btn, 0xC62828);
  lv_obj_set_style_border_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_confirm_yes_btn, 14, 0);
  lv_obj_add_event_cb(ap_confirm_yes_btn, on_confirm_yes_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ap_yes_label = lv_label_create(ap_confirm_yes_btn);
  lv_label_set_text(ap_yes_label, "Ja");
  lv_obj_set_style_text_font(ap_yes_label, &ui_font_20, 0);
  lv_obj_center(ap_yes_label);

  ap_confirm_no_btn = lv_button_create(ap_confirm_row);
  lv_obj_set_height(ap_confirm_no_btn, kSettingsBtnHeight);
  lv_obj_set_flex_grow(ap_confirm_no_btn, 1);
  style_settings_button(ap_confirm_no_btn, 0x555555);
  lv_obj_set_style_border_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_confirm_no_btn, 14, 0);
  lv_obj_add_event_cb(ap_confirm_no_btn, on_confirm_no_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ap_no_label = lv_label_create(ap_confirm_no_btn);
  lv_label_set_text(ap_no_label, "Nein");
  lv_obj_set_style_text_font(ap_no_label, &ui_font_20, 0);
  lv_obj_center(ap_no_label);

  // ===== Row 2: Auto-Sleep =====
  lv_obj_t *card_sleep = create_settings_card(tab, 0, 2);
  lv_obj_set_grid_cell(card_sleep, LV_GRID_ALIGN_STRETCH, 0, GRID_COLS, LV_GRID_ALIGN_STRETCH, 2, 1);

  lv_obj_t *sleep_col_left = create_settings_column(
      card_sleep, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(sleep_col_left, "power-plug", "Sleep");

  lv_obj_t *sleep_col_right = create_settings_column(
      card_sleep, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(sleep_col_right, 8, 0);
  lv_obj_t *sleep_row = create_slider_row(sleep_col_right);
  sleep_label = lv_label_create(sleep_row);
  lv_label_set_text(sleep_label, "Auto-Sleep nach");
  lv_obj_set_width(sleep_label, kSettingsSliderLabelWidth);
  lv_label_set_long_mode(sleep_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(sleep_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(sleep_label, lv_color_white(), 0);
  sleep_slider = lv_slider_create(sleep_row);
  style_settings_slider(sleep_slider);
  lv_obj_set_flex_grow(sleep_slider, 1);
  lv_slider_set_range(sleep_slider, 0, sleep_slider_max_index());
  int32_t sleep_index = cfg.auto_sleep_enabled
                        ? sleep_index_from_seconds(cfg.auto_sleep_seconds)
                        : sleep_slider_max_index();
  lv_slider_set_value(sleep_slider, sleep_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_RELEASED, nullptr);

  sleep_time_label = lv_label_create(sleep_row);
  lv_obj_set_width(sleep_time_label, kSettingsSliderValueWidth);
  lv_label_set_long_mode(sleep_time_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(sleep_time_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(sleep_time_label, lv_color_white(), 0);
  lv_obj_set_style_text_align(sleep_time_label, LV_TEXT_ALIGN_RIGHT, 0);

  static char sleep_buf[16];
  format_sleep_label_for_index(sleep_buf, sizeof(sleep_buf), sleep_index);
  lv_label_set_text(sleep_time_label, sleep_buf);

  // ===== Row 3: Wake Mode =====
  lv_obj_t *card_wake = create_settings_card(tab, 0, 3);
  lv_obj_set_grid_cell(card_wake, LV_GRID_ALIGN_STRETCH, 0, GRID_COLS, LV_GRID_ALIGN_STRETCH, 3, 1);

  lv_obj_t *wake_col_left = create_settings_column(
      card_wake, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(wake_col_left, "gesture-tap", "Wake");

  lv_obj_t *wake_col_right = create_settings_column(
      card_wake, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(wake_col_right, 8, 0);

  mains_wake_btn = lv_button_create(wake_col_right);
  lv_obj_set_width(mains_wake_btn, LV_PCT(60));
  lv_obj_set_height(mains_wake_btn, kSettingsBtnHeight);
  style_settings_button(mains_wake_btn, 0x3B82F6);
  lv_obj_set_style_border_opa(mains_wake_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(mains_wake_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(mains_wake_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(mains_wake_btn, 14, 0);
  lv_obj_remove_flag(mains_wake_btn, LV_OBJ_FLAG_CLICKABLE);

  mains_wake_label = lv_label_create(mains_wake_btn);
  lv_obj_set_style_text_font(mains_wake_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(mains_wake_label, lv_color_white(), 0);
  lv_obj_align(mains_wake_label, LV_ALIGN_TOP_MID, 0, 6);

  mains_wake_sub_label = lv_label_create(mains_wake_btn);
  lv_obj_set_style_text_font(mains_wake_sub_label, &ui_font_20, 0);
  lv_obj_set_style_text_color(mains_wake_sub_label, lv_color_hex(0xE0E0E0), 0);
  lv_obj_align(mains_wake_sub_label, LV_ALIGN_BOTTOM_MID, 0, -6);
  update_wake_button(mains_wake_label, mains_wake_sub_label, wake_mode_mains);

  // Stubs for battery wake (hidden, no battery on Waveshare)
  battery_wake_btn = nullptr;
  battery_wake_label = nullptr;
  battery_wake_sub_label = nullptr;
  power_status_label = nullptr;
  power_level_label = nullptr;
  battery_icon_label = nullptr;
  battery_percent_label = nullptr;

  settings_update_ap_mode(ap_mode_active);
}

void settings_update_wifi_status(bool connected, const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  static char buf[128];

  if (connected) {
    lv_label_set_text(wifi_status_label, "Status: Verbunden");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x51CF66), 0);

    snprintf(buf, sizeof(buf), "SSID: %s", ssid ? ssid : "---");
    lv_label_set_text(wifi_ssid_label, buf);

    snprintf(buf, sizeof(buf), "IP: %s", ip ? ip : "---");
    lv_label_set_text(wifi_ip_label, buf);

  } else {
    lv_label_set_text(wifi_status_label, "Status: Offline");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
    lv_label_set_text(wifi_ssid_label, "SSID: ---");
    lv_label_set_text(wifi_ip_label, "IP: ---");
  }
}

void settings_update_wifi_status_ap(const char* ssid, const char* password) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  if (mqtt_notice_label) lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);

  static char status_buf[96];
  snprintf(status_buf, sizeof(status_buf), "Status: AP (PW: %s)", password ? password : "12345678");
  lv_label_set_text(wifi_status_label, status_buf);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFC04D), 0);

  static char buf[128];
  snprintf(buf, sizeof(buf), "SSID: %s", ssid ? ssid : "WS_P4_Config");
  lv_label_set_text(wifi_ssid_label, buf);

  String ip_str = WiFi.softAPIP().toString();
  snprintf(buf, sizeof(buf), "IP: %s", ip_str.length() ? ip_str.c_str() : "192.168.4.1");
  lv_label_set_text(wifi_ip_label, buf);
}

void settings_show_mqtt_warning(bool show) {
  if (!mqtt_notice_label) return;
  if (show) {
    lv_obj_clear_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_ssid_label) lv_obj_add_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_ip_label) lv_obj_add_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_ssid_label) lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
    if (wifi_ip_label) lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);
  }
}

void settings_update_ap_mode(bool running) {
  ap_mode_active = running;
  if (!ap_mode_btn_label && ap_mode_btn) {
    ap_mode_btn_label = lv_obj_get_child(ap_mode_btn, 0);
  }
  if (ap_mode_btn_label) {
    lv_label_set_text(ap_mode_btn_label, running ? "AP beenden" : "AP aktivieren");
  }
  if (ap_mode_btn) {
    style_settings_button(ap_mode_btn, running ? 0xC62828 : 0xFF9800);
    if (!ap_mode_confirm_pending) {
      lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (running) {
    hide_ap_confirm_row();
  }
}

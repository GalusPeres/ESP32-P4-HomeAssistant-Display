#include <M5Unified.h>
#include <lvgl.h>
#include "src/ui/tab_settings.h"
#include "src/core/config_manager.h"
#include "src/tiles/mdi_icons.h"

static lv_obj_t *brightness_label = nullptr;
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
static lv_obj_t *sleep_battery_slider = nullptr;
static lv_obj_t *sleep_battery_time_label = nullptr;
static lv_obj_t *sleep_battery_label = nullptr;

// Power Status Labels
static lv_obj_t *power_status_label = nullptr;
static lv_obj_t *power_level_label = nullptr;

static const int kSettingsColLeftPct = 12;
static const int kSettingsColMidPct = 34;
static const int kSettingsColRightPct = 54;
static const int kSettingsColGap = 12;
static const int kSettingsColRowGap = 6;
static const int kSettingsBtnHeight = 88;
static const int kSettingsButtonWidthPct = 90;
static const int kSettingsSliderWidthPct = 45;

// Forward declarations
void settings_update_ap_mode(bool running);

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

static void on_brightness(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t value = lv_slider_get_value(slider);

  // Helligkeit sofort ändern
  M5.Display.setBrightness(value);

  // Label sofort updaten
  static char buf[16];
  snprintf(buf, sizeof(buf), "%d", (int)value);
  if (brightness_label) lv_label_set_text(brightness_label, buf);

  // Config NUR beim Loslassen speichern (nicht bei jeder Bewegung!)
  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    configManager.saveDisplaySettings(
        value,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds);
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
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_margin_top(label, 8, 0);
}

static void on_sleep_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  // Label sofort updaten
  static char buf[16];
  format_sleep_label_for_index(buf, sizeof(buf), index);
  if (sleep_time_label) lv_label_set_text(sleep_time_label, buf);

  // Config NUR beim Loslassen speichern (nicht bei jeder Bewegung!)
  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    bool enabled = !sleep_index_is_never(index);
    uint16_t seconds = enabled ? sleep_seconds_from_index(index) : cfg.auto_sleep_seconds;
    configManager.saveDisplaySettings(
        cfg.display_brightness,
        enabled,
        seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds);
  }
}

static void on_sleep_battery_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  static char buf[16];
  format_sleep_label_for_index(buf, sizeof(buf), index);
  if (sleep_battery_time_label) lv_label_set_text(sleep_battery_time_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    bool enabled = !sleep_index_is_never(index);
    uint16_t seconds = enabled ? sleep_seconds_from_index(index) : cfg.auto_sleep_battery_seconds;
    configManager.saveDisplaySettings(
        cfg.display_brightness,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        enabled,
        seconds);
  }
}

// Power Status Update (public, wird von main loop aufgerufen)
void settings_update_power_status() {
  if (!power_status_label || !power_level_label) return;

  int32_t batLevel = M5.Power.getBatteryLevel();
  int32_t batCurrent = M5.Power.getBatteryCurrent();

  static char status_buf[40];
  static char level_buf[32];

  static bool lastPoweredState = false;
  static uint32_t lastStateChange = 0;
  uint32_t now = millis();

  bool currentPowered = (batCurrent <= 50);
  if (currentPowered != lastPoweredState) {
    if (now - lastStateChange > 2000) {
      lastPoweredState = currentPowered;
      lastStateChange = now;
    }
  } else {
    lastStateChange = now;
  }

  int displayLevel = (batLevel >= 0 && batLevel <= 100) ? batLevel : 0;
  const char* statusText = lastPoweredState ? "Netzteil" : "Akku";

  snprintf(status_buf, sizeof(status_buf), "Status: %s", statusText);
  snprintf(level_buf, sizeof(level_buf), "Level: %d%%", displayLevel);

  lv_label_set_text(power_status_label, status_buf);
  lv_label_set_text(power_level_label, level_buf);
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
  lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_pad_hor(card, 20, 0);
  lv_obj_set_style_pad_ver(card, 18, 0);
  lv_obj_set_style_pad_row(card, 8, 0);
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
  lv_obj_set_style_pad_right(row, 16, 0);
  lv_obj_set_style_pad_column(row, 12, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static void style_settings_slider(lv_obj_t *slider) {
  lv_obj_set_height(slider, 18);
  lv_obj_set_style_width(slider, 28, LV_PART_KNOB);
  lv_obj_set_style_height(slider, 28, LV_PART_KNOB);
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
  lv_obj_set_style_pad_all(tab, 24, 0);

  static int32_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
  static int32_t row_dsc[] = {
    LV_GRID_FR(1),
    LV_GRID_FR(1),
    LV_GRID_FR(1),
    LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST
  };
  lv_obj_set_grid_dsc_array(tab, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(tab, 24, 0);
  lv_obj_set_style_pad_row(tab, 24, 0);

  const DeviceConfig& cfg = configManager.getConfig();

  // Display tile
  lv_obj_t *card_display = create_settings_card(tab, 0, 0);
  lv_obj_t *display_col_left = create_settings_column(
      card_display, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(display_col_left, "brightness-5", "Display");

  lv_obj_t *display_col_mid = create_settings_column(
      card_display, kSettingsColMidPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(display_col_mid, 16, 0);
  lv_obj_set_style_pad_right(display_col_mid, 8, 0);

  lv_obj_t *display_col_right = create_settings_column(
      card_display, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(display_col_right, 8, 0);
  lv_obj_set_style_pad_right(display_col_right, 8, 0);
  lv_obj_t *brightness_row = create_slider_row(display_col_right);
  lv_obj_t *brightness_row_label = lv_label_create(brightness_row);
  lv_label_set_text(brightness_row_label, "Helligkeit");
  lv_obj_set_style_text_font(brightness_row_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(brightness_row_label, lv_color_white(), 0);
  lv_obj_t *brightness_slider = lv_slider_create(brightness_row);
  style_settings_slider(brightness_slider);
  lv_obj_set_width(brightness_slider, LV_PCT(kSettingsSliderWidthPct));
  lv_slider_set_range(brightness_slider, 75, 255);
  int current_brightness = M5.Display.getBrightness();
  lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_RELEASED, nullptr);

  brightness_label = lv_label_create(brightness_row);
  lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);

  static char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d", current_brightness);
  lv_label_set_text(brightness_label, bright_buf);

  // WiFi tile
  lv_obj_t *card_wifi = create_settings_card(tab, 0, 1);
  lv_obj_t *wifi_col_left = create_settings_column(
      card_wifi, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(wifi_col_left, "wifi", "WLAN");

  lv_obj_t *wifi_col_mid = create_settings_column(
      card_wifi, kSettingsColMidPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(wifi_col_mid, 16, 0);
  lv_obj_set_style_pad_right(wifi_col_mid, 8, 0);
  wifi_status_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Nicht verbunden");
  lv_obj_set_width(wifi_status_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(wifi_status_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);

  wifi_ssid_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(wifi_ssid_label, "Netzwerk: ---");
  lv_obj_set_width(wifi_ssid_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_ssid_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(wifi_ssid_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0xC8C8C8), 0);

  wifi_ip_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(wifi_ip_label, "IP-Adresse: ---");
  lv_obj_set_width(wifi_ip_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_ip_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(wifi_ip_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(wifi_ip_label, lv_color_hex(0xC8C8C8), 0);

  mqtt_notice_label = lv_label_create(wifi_col_mid);
  lv_label_set_text(mqtt_notice_label, LV_SYMBOL_WARNING " MQTT nicht konfiguriert");
  lv_obj_set_width(mqtt_notice_label, LV_PCT(100));
  lv_label_set_long_mode(mqtt_notice_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(mqtt_notice_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(mqtt_notice_label, lv_color_hex(0xFFC04D), 0);
  lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *wifi_col_right = create_settings_column(
      card_wifi, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_left(wifi_col_right, 8, 0);
  lv_obj_set_style_pad_right(wifi_col_right, 8, 0);
  ap_mode_btn = lv_button_create(wifi_col_right);
  lv_obj_set_width(ap_mode_btn, LV_PCT(kSettingsButtonWidthPct));
  lv_obj_set_height(ap_mode_btn, kSettingsBtnHeight);
  lv_obj_set_style_bg_color(ap_mode_btn, lv_color_hex(0xFF9800), 0);
  lv_obj_set_style_border_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_mode_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_mode_btn, 18, 0);
  lv_obj_add_event_cb(ap_mode_btn, on_ap_mode_clicked, LV_EVENT_CLICKED, nullptr);

  ap_mode_btn_label = lv_label_create(ap_mode_btn);
  lv_label_set_text(ap_mode_btn_label, "AP aktivieren");
  lv_obj_set_style_text_font(ap_mode_btn_label, &lv_font_montserrat_20, 0);
  lv_obj_center(ap_mode_btn_label);

  ap_confirm_row = create_card_row(wifi_col_right);
  lv_obj_set_width(ap_confirm_row, LV_PCT(kSettingsButtonWidthPct));
  lv_obj_set_height(ap_confirm_row, kSettingsBtnHeight);
  lv_obj_set_style_pad_column(ap_confirm_row, 12, 0);
  lv_obj_add_flag(ap_confirm_row, LV_OBJ_FLAG_HIDDEN);

  ap_confirm_yes_btn = lv_button_create(ap_confirm_row);
  lv_obj_set_height(ap_confirm_yes_btn, kSettingsBtnHeight);
  lv_obj_set_flex_grow(ap_confirm_yes_btn, 1);
  lv_obj_set_style_bg_color(ap_confirm_yes_btn, lv_color_hex(0xC62828), 0);
  lv_obj_set_style_border_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_confirm_yes_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_confirm_yes_btn, 18, 0);
  lv_obj_add_event_cb(ap_confirm_yes_btn, on_confirm_yes_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ap_yes_label = lv_label_create(ap_confirm_yes_btn);
  lv_label_set_text(ap_yes_label, "Bestaetigen");
  lv_obj_set_style_text_font(ap_yes_label, &lv_font_montserrat_14, 0);
  lv_obj_center(ap_yes_label);

  ap_confirm_no_btn = lv_button_create(ap_confirm_row);
  lv_obj_set_height(ap_confirm_no_btn, kSettingsBtnHeight);
  lv_obj_set_flex_grow(ap_confirm_no_btn, 1);
  lv_obj_set_style_bg_color(ap_confirm_no_btn, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(ap_confirm_no_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ap_confirm_no_btn, 18, 0);
  lv_obj_add_event_cb(ap_confirm_no_btn, on_confirm_no_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ap_no_label = lv_label_create(ap_confirm_no_btn);
  lv_label_set_text(ap_no_label, "Abbrechen");
  lv_obj_set_style_text_font(ap_no_label, &lv_font_montserrat_14, 0);
  lv_obj_center(ap_no_label);

  // Sleep mains tile
  lv_obj_t *card_mains = create_settings_card(tab, 0, 2);
  lv_obj_t *mains_col_left = create_settings_column(
      card_mains, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(mains_col_left, "power-plug", "Netzteil");

  lv_obj_t *mains_col_mid = create_settings_column(
      card_mains, kSettingsColMidPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(mains_col_mid, 16, 0);
  lv_obj_set_style_pad_right(mains_col_mid, 8, 0);

  lv_obj_t *mains_col_right = create_settings_column(
      card_mains, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(mains_col_right, 8, 0);
  lv_obj_set_style_pad_right(mains_col_right, 8, 0);
  lv_obj_t *sleep_row = create_slider_row(mains_col_right);
  sleep_label = lv_label_create(sleep_row);
  lv_label_set_text(sleep_label, "Auto-Sleep nach");
  lv_obj_set_style_text_font(sleep_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sleep_label, lv_color_white(), 0);
  sleep_slider = lv_slider_create(sleep_row);
  style_settings_slider(sleep_slider);
  lv_obj_set_width(sleep_slider, LV_PCT(kSettingsSliderWidthPct));
  lv_slider_set_range(sleep_slider, 0, sleep_slider_max_index());
  int32_t sleep_index = cfg.auto_sleep_enabled
                        ? sleep_index_from_seconds(cfg.auto_sleep_seconds)
                        : sleep_slider_max_index();
  lv_slider_set_value(sleep_slider, sleep_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_RELEASED, nullptr);

  sleep_time_label = lv_label_create(sleep_row);
  lv_obj_set_style_text_font(sleep_time_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sleep_time_label, lv_color_white(), 0);

  static char sleep_buf[16];
  format_sleep_label_for_index(sleep_buf, sizeof(sleep_buf), sleep_index);
  lv_label_set_text(sleep_time_label, sleep_buf);

  // Sleep battery tile
  lv_obj_t *card_battery = create_settings_card(tab, 0, 3);
  lv_obj_t *battery_col_left = create_settings_column(
      card_battery, kSettingsColLeftPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  create_icon_block(battery_col_left, "battery", "Batterie");

  lv_obj_t *battery_col_mid = create_settings_column(
      card_battery, kSettingsColMidPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(battery_col_mid, 16, 0);
  lv_obj_set_style_pad_right(battery_col_mid, 8, 0);
  power_status_label = lv_label_create(battery_col_mid);
  lv_label_set_text(power_status_label, "Status: ---");
  lv_obj_set_width(power_status_label, LV_PCT(100));
  lv_label_set_long_mode(power_status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(power_status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(power_status_label, lv_color_white(), 0);

  power_level_label = lv_label_create(battery_col_mid);
  lv_label_set_text(power_level_label, "Level: --%");
  lv_obj_set_width(power_level_label, LV_PCT(100));
  lv_label_set_long_mode(power_level_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(power_level_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(power_level_label, lv_color_white(), 0);

  lv_obj_t *battery_col_right = create_settings_column(
      card_battery, kSettingsColRightPct, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_left(battery_col_right, 8, 0);
  lv_obj_set_style_pad_right(battery_col_right, 8, 0);

  lv_obj_t *sleep_battery_row = create_slider_row(battery_col_right);
  sleep_battery_label = lv_label_create(sleep_battery_row);
  lv_label_set_text(sleep_battery_label, "Auto-Sleep nach");
  lv_obj_set_style_text_font(sleep_battery_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sleep_battery_label, lv_color_white(), 0);
  sleep_battery_slider = lv_slider_create(sleep_battery_row);
  style_settings_slider(sleep_battery_slider);
  lv_obj_set_width(sleep_battery_slider, LV_PCT(kSettingsSliderWidthPct));
  lv_slider_set_range(sleep_battery_slider, 0, sleep_slider_max_index());
  int32_t sleep_battery_index = cfg.auto_sleep_battery_enabled
                                ? sleep_index_from_seconds(cfg.auto_sleep_battery_seconds)
                                : sleep_slider_max_index();
  lv_slider_set_value(sleep_battery_slider, sleep_battery_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_battery_slider, on_sleep_battery_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_battery_slider, on_sleep_battery_slider, LV_EVENT_RELEASED, nullptr);

  sleep_battery_time_label = lv_label_create(sleep_battery_row);
  lv_obj_set_style_text_font(sleep_battery_time_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(sleep_battery_time_label, lv_color_white(), 0);

  static char sleep_battery_buf[16];
  format_sleep_label_for_index(sleep_battery_buf, sizeof(sleep_battery_buf), sleep_battery_index);
  lv_label_set_text(sleep_battery_time_label, sleep_battery_buf);

  settings_update_power_status();

  settings_update_ap_mode(ap_mode_active);
}

void settings_update_wifi_status(bool connected, const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  static char buf[128];

  if (connected) {
    lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_OK " Verbunden");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x51CF66), 0);

    snprintf(buf, sizeof(buf), "Netzwerk: %s", ssid ? ssid : "---");
    lv_label_set_text(wifi_ssid_label, buf);

    snprintf(buf, sizeof(buf), "IP-Adresse: %s", ip ? ip : "---");
    lv_label_set_text(wifi_ip_label, buf);

  } else {
    lv_label_set_text(wifi_status_label, "Status: " LV_SYMBOL_CLOSE " Nicht verbunden");
    lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFF6B6B), 0);
    lv_label_set_text(wifi_ssid_label, "Netzwerk: ---");
    lv_label_set_text(wifi_ip_label, "IP-Adresse: ---");
  }
}

void settings_update_wifi_status_ap(const char* ssid, const char* ip) {
  if (!wifi_status_label || !wifi_ssid_label || !wifi_ip_label) return;

  if (mqtt_notice_label) lv_obj_add_flag(mqtt_notice_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_ssid_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_ip_label, LV_OBJ_FLAG_HIDDEN);

  lv_label_set_text(wifi_status_label, "Status: AP Mode");
  lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xFFC04D), 0);

  static char buf[128];
  snprintf(buf, sizeof(buf), "Netzwerk: %s", ssid ? ssid : "Tab5_Config");
  lv_label_set_text(wifi_ssid_label, buf);

  snprintf(buf, sizeof(buf), "IP-Adresse: %s", ip ? ip : "192.168.4.1");
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
    lv_obj_set_style_bg_color(ap_mode_btn,
                              running ? lv_color_hex(0xC62828) : lv_color_hex(0xFF9800),
                              0);
    if (!ap_mode_confirm_pending) {
      lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (running) {
    hide_ap_confirm_row();
  }
}

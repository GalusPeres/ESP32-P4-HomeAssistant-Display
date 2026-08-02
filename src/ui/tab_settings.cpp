#include <lvgl.h>
#include <WiFi.h>
#include <cstring>
#include "src/ui/tab_settings.h"
#include "src/core/config_manager.h"
#include "src/core/board_hal.h"
#include "src/core/power_manager.h"
#include "src/network/network_manager.h"
#include "src/network/network_transport.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/ui_manager.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/fonts/ui_fonts.h"
#include "src/core/firmware_version.h"
#include "src/core/github_update.h"
#include "src/devices/device.h"
#include "src/network/mqtt_handlers.h"
#include "src/core/display_manager.h"
#include "src/core/i18n.h"
#include "src/types/clock/clock_format.h"
#include "src/web/web_config.h"
#include "src/ui/ui_keyboard.h"
#include "src/ui/hometiles_logo.h"
#include "src/ui/popup_layout.h"
#include "src/ui/image_screensaver.h"
#include "src/ui/ui_surface_style.h"
#include "src/ui/weather_popup.h"

static lv_obj_t *brightness_label = nullptr;
static lv_obj_t *screensaver_brightness_value_label = nullptr;
static lv_obj_t *display_rotate_btn = nullptr;
static lv_obj_t *display_rotate_label = nullptr;
static lv_obj_t *display_rotate_text_label = nullptr;
static lv_obj_t *display_rotate_sub_label = nullptr;
static bool display_rotated_180 = false;
static uint8_t display_rotation_quarters = Device::kRotationDefault;
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
static wifi_reconnect_callback_t g_wifi_reconnect_callback = nullptr;
static fw_check_callback_t g_fw_check_callback = nullptr;
static fw_install_callback_t g_fw_install_callback = nullptr;
static system_reboot_callback_t g_system_reboot_callback = nullptr;
static wifi_disconnect_callback_t g_wifi_disconnect_callback = nullptr;
static ha_pair_callback_t g_ha_pair_callback = nullptr;

enum class SettingsPopupKind : uint8_t {
  Display,
  Wifi,
  Localization,
  Firmware,
};

static lv_obj_t *settings_tile_display_title = nullptr;
static lv_obj_t *settings_tile_display_summary = nullptr;
static lv_obj_t *settings_tile_wifi_title = nullptr;
static lv_obj_t *settings_tile_wifi_summary = nullptr;
static lv_obj_t *settings_tile_locale_title = nullptr;
static lv_obj_t *settings_tile_locale_summary = nullptr;
static lv_obj_t *settings_tile_firmware_title = nullptr;
static lv_obj_t *settings_tile_firmware_summary = nullptr;

static lv_obj_t *settings_popup_overlay = nullptr;
static lv_obj_t *settings_popup_card = nullptr;
static lv_obj_t *settings_popup_title = nullptr;
static lv_obj_t *settings_popup_content = nullptr;
static lv_obj_t *settings_popup_keyboard = nullptr;
static lv_obj_t *settings_popup_active_ta = nullptr;
// Icon-Label im X-Button oben rechts: wird in der WLAN-Eingabe-Ansicht zum
// Zurueck-Pfeil umfunktioniert (siehe on_settings_popup_close_clicked).
static lv_obj_t *settings_popup_close_icon = nullptr;
// Platzhalter, der in der Content-Spalte den Bereich der frei positionierten
// Tastatur reserviert - wird in der WLAN-Listen-Ansicht ausgeblendet, damit
// die Netzwerkliste die volle Hoehe nutzen kann.
static lv_obj_t *settings_popup_kb_spacer = nullptr;
static SettingsPopupKind settings_popup_kind = SettingsPopupKind::Display;

static lv_obj_t *wifi_ssid_ta = nullptr;
static lv_obj_t *wifi_pass_ta = nullptr;
static lv_obj_t *wifi_pass_eye_icon = nullptr;
static lv_obj_t *wifi_list_view = nullptr;
static lv_obj_t *wifi_entry_view = nullptr;
static lv_obj_t *wifi_conn_status_label = nullptr;
static lv_obj_t *wifi_scan_status_label = nullptr;
static lv_obj_t *wifi_list_container = nullptr;
static lv_obj_t *wifi_manual_row = nullptr;
static lv_obj_t *wifi_manual_gap = nullptr;
static lv_obj_t *wifi_ap_qr = nullptr;
static lv_obj_t *wifi_info_box = nullptr;
// AP-Infobox: Zugangsdaten als buendig ausgerichtete Beschriftung/Wert-Zeilen
// statt Fliesstext; der QR-Code waechst einmalig auf den freien Platz (der
// Spacer der Listen-Ansicht dient dabei als Mass).
static lv_obj_t *wifi_ap_info_rows = nullptr;
static lv_obj_t *wifi_ap_ssid_val = nullptr;
static lv_obj_t *wifi_ap_pw_val = nullptr;
static lv_obj_t *wifi_ap_ip_val = nullptr;
static lv_obj_t *wifi_list_spacer = nullptr;
static bool wifi_ap_qr_sized = false;
static lv_timer_t *wifi_scan_timer = nullptr;
struct WifiScanEntry { char ssid[33]; int16_t rssi; bool open; };
static WifiScanEntry wifi_scan_results[24];
static size_t wifi_scan_result_count = 0;
static char wifi_selected_ssid[33] = {};
static bool wifi_selected_open = false;
static bool wifi_manual_mode = false;
static char wifi_known_fallback_ssid[33] = {};
// Nach einem angeforderten (Re-)Connect kurz keine Scans starten - ein
// laufender Scan wuerde das WiFi.begin() im Hauptloop wieder stoeren.
static uint32_t wifi_scan_block_until = 0;

static lv_obj_t *locale_language_dd = nullptr;
static lv_obj_t *locale_timezone_dd = nullptr;
static lv_obj_t *locale_time_format_dd = nullptr;
static lv_obj_t *locale_date_format_dd = nullptr;
static lv_obj_t *locale_keyboard_dd = nullptr;

// System-Popup (Version/Geraet, GitHub-QR, Update-Suche + OTA-Install)
static lv_obj_t *system_info_rows = nullptr;
static lv_obj_t *system_status_label = nullptr;
static lv_obj_t *system_progress_bar = nullptr;
static lv_obj_t *system_check_btn = nullptr;
static lv_obj_t *system_check_btn_label = nullptr;
static lv_obj_t *system_github_btn = nullptr;
static lv_obj_t *system_reboot_btn = nullptr;
static lv_obj_t *system_pair_btn = nullptr;
static lv_obj_t *system_action_row = nullptr;  // Reihe Restart + Pairing
static lv_obj_t *system_qr = nullptr;
static lv_obj_t *system_spacer = nullptr;
static bool system_qr_sized = false;
static bool system_check_running = false;
static bool system_install_running = false;
// Ueberdauern das Popup bewusst: ein bekanntes Check-Ergebnis wird beim
// Wiederoeffnen sofort wieder angezeigt (Button + Statuszeile).
static char system_latest_tag[24] = {};
static bool system_update_available = false;

static const i18n::Strings& tr() {
  return i18n::strings(configManager.getConfig().language);
}

#if LV_USE_QRCODE
static void style_qr_code(lv_obj_t* qr) {
  if (!qr) return;
  lv_obj_set_style_radius(qr, popup_layout::scale(14), 0);
  lv_obj_set_style_clip_corner(qr, true, 0);
  lv_obj_set_style_border_width(qr, 0, 0);
  lv_obj_set_style_bg_color(qr, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(qr, LV_OPA_COVER, 0);
}
#endif

static lv_obj_t *ap_mode_btn = nullptr;
static lv_obj_t *ap_mode_btn_label = nullptr;
static lv_obj_t *wifi_disconnect_btn = nullptr;
// Netzwerkmodus-Schalter (nur auf Ethernet-faehigen Builds sichtbar)
static lv_obj_t *net_mode_btn = nullptr;
static lv_obj_t *net_mode_btn_label = nullptr;
static lv_obj_t *net_mode_hint_label = nullptr;
static lv_obj_t *ip_mode_hint_label = nullptr;
static lv_obj_t *net_mode_row = nullptr;
static lv_obj_t *ethernet_dhcp_btn = nullptr;
static lv_obj_t *ethernet_dhcp_btn_label = nullptr;
static lv_obj_t *ap_confirm_row = nullptr;
static lv_obj_t *ap_confirm_yes_btn = nullptr;
static lv_obj_t *ap_confirm_no_btn = nullptr;
static lv_obj_t *display_section_label = nullptr;
static lv_obj_t *brightness_title_label = nullptr;
static lv_obj_t *screensaver_brightness_title_label = nullptr;
static lv_obj_t *wifi_section_label = nullptr;
static lv_obj_t *ap_yes_label_obj = nullptr;
static lv_obj_t *ap_no_label_obj = nullptr;
static lv_obj_t *sleep_section_label = nullptr;
static lv_timer_t *ap_confirm_timer = nullptr;
// Sperr-Timer nach AP-Umschalten im WLAN-Popup (Schutz vor Doppeltippen)
static lv_timer_t *ap_btn_cooldown_timer = nullptr;
static bool ap_mode_confirm_pending = false;
static bool ap_mode_active = false;
static uint32_t ap_mode_click_block_until = 0;

// Sleep Settings
static lv_obj_t *sleep_slider = nullptr;
static lv_obj_t *sleep_time_label = nullptr;
static lv_obj_t *sleep_label = nullptr;
static lv_obj_t *screensaver_slider = nullptr;
static lv_obj_t *screensaver_time_label = nullptr;
static lv_obj_t *screensaver_label = nullptr;
static lv_obj_t *screensaver_brightness_slider = nullptr;

// Power Status Labels (stubs -> no battery display)
static lv_obj_t *power_status_label = nullptr;
static lv_obj_t *power_level_label = nullptr;
static lv_obj_t *battery_icon_label = nullptr;
static lv_obj_t *battery_percent_label = nullptr;

// Layout constants -> kompakter fuer 720x720, 4x4 Grid
static const int kSettingsColLeftPct = 15;
static const int kSettingsColRightPct = 85;
static const int kSettingsColGap = popup_layout::scale(8);
static const int kSettingsColRowGap = popup_layout::scale(4);
static const int kSettingsBtnHeight = popup_layout::scale(80);
static const int kSettingsButtonWidthPct = 90;
static const int kSettingsSliderLabelWidth = popup_layout::scale(160);
static const int kSettingsSectionTitlePct = 20;
static const int kSettingsSectionContentPct = 50;
static const int kSettingsSectionActionPct = 30;
static const int kSettingsDisplayValueWidth = popup_layout::scale(56);
static const int kSettingsInlineLabelWidth = popup_layout::scale(98);
static const int kSettingsInlineSliderWidth = popup_layout::scale(116);
static const uint8_t kSettingsBrightnessRawMin = 121;
static const uint8_t kSettingsBrightnessRawMax = 255;
static const int kSettingsBrightnessPctMin = 1;
static const int kSettingsBrightnessPctMax = 100;
static const int kSettingsSliderValueWidth = popup_layout::scale(70);
static const int kSettingsSliderHeight = popup_layout::scale(20);
static const int kSettingsSliderKnobSize = popup_layout::scale(42);
static const int kSettingsSliderClickPad = popup_layout::scale(20);
static const uint8_t kSettingsCardColStart = 1;

// Forward declarations
void settings_update_ap_mode(bool running);
void settings_refresh_language();
static void update_settings_tile_summaries();
static void open_settings_popup(SettingsPopupKind kind);
static void wifi_stop_scan_timer();
static void wifi_show_list_view();
static void wifi_update_conn_status_label();
static void build_localization_popup(lv_obj_t* parent);

static void style_settings_button(lv_obj_t *btn, uint32_t base_color) {
  if (!btn) return;
  uint32_t pressed_color = brighten_rgb_color(base_color, 0x10);
  lv_obj_set_style_anim_duration(btn, 0, 0);
  disable_pressed_button_animation(btn);
  lv_obj_set_style_transform_width(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_transform_height(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_translate_x(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_translate_y(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
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

static void format_sleep_popup_value_for_index(char* buf, size_t len, int32_t index) {
  if (sleep_index_is_never(index)) {
    snprintf(buf, len, "%s", tr().sleep_never);
    return;
  }
  char value[16];
  format_sleep_label(value, sizeof(value), sleep_seconds_from_index(index));
  snprintf(buf, len, "%s %s", tr().sleep_after, value);
}

static void update_display_rotate_label() {
  if (!display_rotate_label) return;
  String icon_char = getMdiChar(String("phone-rotate-landscape"));
  lv_label_set_text(display_rotate_label, icon_char.c_str());
  if (display_rotate_sub_label) {
    static char buf[24];
    snprintf(buf, sizeof(buf), "%u%c", static_cast<unsigned>(display_rotation_quarters * 90), 176);
    lv_label_set_text(display_rotate_sub_label, buf);
  }
}

static void sync_display_rotation_state(uint8_t rotation_quarters) {
  display_rotation_quarters = Device::normalizeRotationQuarterTurns(rotation_quarters);
  display_rotated_180 = (display_rotation_quarters == Device::kRotationFlipped);
  display_rotation_mode = display_rotated_180 ? kDisplayRotationFlipped : kDisplayRotationNormal;
  configManager.setRuntimeDisplayRotationQuarters(display_rotation_quarters);
}

static int brightness_pct_from_raw(int raw) {
  if (raw < kSettingsBrightnessRawMin) raw = kSettingsBrightnessRawMin;
  if (raw > kSettingsBrightnessRawMax) raw = kSettingsBrightnessRawMax;
  const int span = kSettingsBrightnessRawMax - kSettingsBrightnessRawMin;
  if (span <= 0) return kSettingsBrightnessPctMax;
  return kSettingsBrightnessPctMin + static_cast<int>((static_cast<long>(raw - kSettingsBrightnessRawMin) * (kSettingsBrightnessPctMax - kSettingsBrightnessPctMin) + (span / 2)) / span);
}

static uint8_t brightness_raw_from_pct(int pct) {
  if (pct < kSettingsBrightnessPctMin) pct = kSettingsBrightnessPctMin;
  if (pct > kSettingsBrightnessPctMax) pct = kSettingsBrightnessPctMax;
  const int span = kSettingsBrightnessRawMax - kSettingsBrightnessRawMin;
  const int pct_span = kSettingsBrightnessPctMax - kSettingsBrightnessPctMin;
  int raw = kSettingsBrightnessRawMin;
  if (pct_span > 0) {
    raw = kSettingsBrightnessRawMin + static_cast<int>((static_cast<long>(pct - kSettingsBrightnessPctMin) * span + (pct_span / 2)) / pct_span);
  }
  if (raw < kSettingsBrightnessRawMin) raw = kSettingsBrightnessRawMin;
  if (raw > kSettingsBrightnessRawMax) raw = kSettingsBrightnessRawMax;
  return static_cast<uint8_t>(raw);
}
static const char* wake_mode_text(uint8_t mode) {
  (void)mode;
  return tr().touch_label;
}

static void update_wake_button(lv_obj_t *main_label, lv_obj_t *sub_label, uint8_t mode) {
  (void)mode;
  if (!main_label) return;
  wake_mode_mains = kWakeModeTouch;
  wake_mode_battery = kWakeModeTouch;
  lv_label_set_text(main_label, wake_mode_text(kWakeModeTouch));
  if (sub_label) {
    lv_label_set_text(sub_label, tr().no_imu_hint);
  }
}

void settings_sync_display_rotation(bool rotated) {
  sync_display_rotation_state(rotated ? Device::kRotationFlipped : Device::kRotationDefault);
  displayManager.setRotation(display_rotation_quarters);
  update_display_rotate_label();
  lv_obj_invalidate(lv_scr_act());
  lv_display_t* disp = lv_display_get_default();
  if (disp) {
    lv_refr_now(disp);
  }
  update_settings_tile_summaries();
}

static void on_display_rotate_clicked(lv_event_t *e) {
  (void)e;
  uint8_t next_rotation = displayManager.getRotation();
  if (Device::supportsQuarterTurnRotation()) {
    next_rotation = (next_rotation + 1) & 0x03;
  } else {
    next_rotation = (next_rotation == Device::kRotationFlipped)
                        ? Device::kRotationDefault
                        : Device::kRotationFlipped;
  }
  displayManager.setRotation(next_rotation);
  sync_display_rotation_state(next_rotation);
  const DeviceConfig& cfg = configManager.getConfig();
  configManager.saveDisplaySettings(
      cfg.display_brightness,
      cfg.auto_sleep_enabled,
      cfg.auto_sleep_seconds,
      cfg.auto_sleep_battery_enabled,
      cfg.auto_sleep_battery_seconds,
      display_rotation_mode,
      display_rotated_180,
      display_rotation_quarters,
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
  int32_t pct = lv_slider_get_value(slider);
  uint8_t raw = brightness_raw_from_pct(pct);

  BoardHAL::setBrightness(raw);

  static char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", (int)pct);
  if (brightness_label) lv_label_set_text(brightness_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    configManager.saveDisplaySettings(
        raw,
        cfg.auto_sleep_enabled,
        cfg.auto_sleep_seconds,
        cfg.auto_sleep_battery_enabled,
        cfg.auto_sleep_battery_seconds,
        display_rotation_mode,
        display_rotated_180,
        display_rotation_quarters,
        wake_mode_mains,
        wake_mode_battery);
    mqttPublishDeviceSettings();
    update_settings_tile_summaries();
  }
}

static void on_screensaver_brightness(lv_event_t *e) {
  lv_obj_t *slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
  const lv_event_code_t code = lv_event_get_code(e);
  int32_t pct = lv_slider_get_value(slider);
  if (pct < kScreensaverBrightnessPctMin) pct = kScreensaverBrightnessPctMin;
  if (pct > kScreensaverBrightnessPctMax) pct = kScreensaverBrightnessPctMax;

  static char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(pct));
  if (screensaver_brightness_value_label) {
    lv_label_set_text(screensaver_brightness_value_label, buf);
  }

  if (code == LV_EVENT_VALUE_CHANGED) {
    // Beim Ziehen kurz die echte Dimmstufe zeigen. Nach dem Loslassen kehrt
    // der Einstellungs-Screen zur normalen Display-Helligkeit zurueck.
    BoardHAL::setBrightness(
        Device::backlightRawFromPercent(static_cast<uint8_t>(pct)));
  } else if (code == LV_EVENT_RELEASED) {
    configManager.saveScreensaverBrightness(static_cast<uint8_t>(pct));
    BoardHAL::setBrightness(configManager.getConfig().display_brightness);
    image_screensaver_brightness_changed();
    mqttPublishDeviceSettings();
  } else if (code == LV_EVENT_PRESS_LOST) {
    // Ein abgebrochener Drag ist keine Einstellung. Die Vorschau darf das
    // normale Dashboard danach nicht dauerhaft gedimmt lassen.
    BoardHAL::setBrightness(configManager.getConfig().display_brightness);
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

static lv_obj_t* create_icon_block(lv_obj_t *parent, const char *icon_name, const char *label_text) {
  lv_obj_t *icon = lv_label_create(parent);
  String icon_char = getMdiChar(String(icon_name));
  lv_label_set_text(icon, icon_char.c_str());
  if (FONT_MDI_ICONS) {
    lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  }
  popup_layout::applyIconScale(icon);
  lv_obj_set_width(icon, LV_PCT(100));
  lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);

  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, label_text);
  lv_obj_set_width(label, LV_PCT(100));
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label, popup_layout::font20(), 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_set_style_margin_top(label, 4, 0);
  return label;
}

static void on_settings_back_clicked(lv_event_t *e) {
  (void)e;
  uiManager.switchToTab(0);
}

static void create_settings_back_button(lv_obj_t *parent) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, 0, 1,
      LV_GRID_ALIGN_STRETCH, 0, 1);

  uint32_t btn_color = 0x2A2A2A;
  style_settings_button(btn, btn_color);
  ui_surface_style::apply_global_tile_border(btn);

  lv_obj_add_event_cb(btn, on_settings_back_clicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *icon = lv_label_create(btn);
  String icon_char = getMdiChar(String("arrow-left"));
  lv_label_set_text(icon, icon_char.c_str());
  if (FONT_MDI_ICONS) {
    lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  }
  popup_layout::applyIconScale(icon);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_center(icon);
}

static void on_sleep_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  int32_t index = lv_slider_get_value(slider);

  static char buf[32];
  format_sleep_popup_value_for_index(buf, sizeof(buf), index);
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
        display_rotation_quarters,
        wake_mode_mains,
        wake_mode_battery);
    mqttPublishDeviceSettings();
    update_settings_tile_summaries();
  }
}

static void on_screensaver_slider(lv_event_t *e) {
  lv_obj_t *slider = (lv_obj_t*)lv_event_get_target(e);
  const lv_event_code_t code = lv_event_get_code(e);
  const int32_t index = lv_slider_get_value(slider);

  static char buf[32];
  format_sleep_popup_value_for_index(buf, sizeof(buf), index);
  if (screensaver_time_label) lv_label_set_text(screensaver_time_label, buf);

  if (code == LV_EVENT_RELEASED) {
    const DeviceConfig& cfg = configManager.getConfig();
    const bool enabled = !sleep_index_is_never(index);
    const uint16_t seconds = enabled
                                 ? sleep_seconds_from_index(index)
                                 : cfg.auto_screensaver_seconds;
    configManager.saveScreensaverTimeout(enabled, seconds);
    update_settings_tile_summaries();
  }
}

// Power Status Update (stub -> no battery on Waveshare)
void settings_update_power_status() {
  // No battery to monitor -> nothing to update
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
  lv_obj_set_style_radius(card, popup_layout::scale(22), 0);
  lv_obj_set_style_pad_hor(card, popup_layout::scale(12), 0);
  lv_obj_set_style_pad_ver(card, popup_layout::scale(10), 0);
  lv_obj_set_style_pad_row(card, popup_layout::scale(4), 0);
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
  lv_obj_set_style_pad_column(row, popup_layout::scale(8), 0);
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
  lv_obj_set_style_pad_bottom(row, popup_layout::scale(2), 0);
  lv_obj_set_style_pad_right(row, popup_layout::scale(12), 0);
  lv_obj_set_style_pad_column(row, popup_layout::scale(8), 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static void style_settings_slider(lv_obj_t *slider) {
  if (!slider) return;
  lv_obj_set_height(slider, kSettingsSliderHeight);
  lv_obj_set_style_width(slider, kSettingsSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_style_height(slider, kSettingsSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, kSettingsSliderClickPad);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(slider, LV_OPA_20, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_border_width(slider, 0, LV_PART_KNOB);
  lv_obj_clear_flag(slider, LV_OBJ_FLAG_SCROLLABLE);
}

// Zeitzonen-Codes und Anzeigenamen kommen aus dem i18n-Katalog
// (i18n::timezone_option + LocaleProfile::timezone_labels), damit Geraet und
// Web-Admin dieselbe Liste zeigen.
static String settings_timezone_options_text;

static uint32_t settings_timezone_index(const char* code) {
  const char* selected = (code && code[0]) ? code : "berlin";
  for (uint32_t i = 0; i < i18n::kTimezoneOptionCount; ++i) {
    if (strcmp(i18n::timezone_option(i).code, selected) == 0) return i;
  }
  return 2;  // berlin
}

static const char* selected_timezone_code(uint32_t index) {
  if (index >= i18n::kTimezoneOptionCount) index = 2;  // berlin
  return i18n::timezone_option(index).code;
}

static void build_timezone_dropdown_options() {
  settings_timezone_options_text = "";
  settings_timezone_options_text.reserve(900);
  const auto& profile = i18n::locale(configManager.getConfig().language);
  for (uint32_t i = 0; i < i18n::kTimezoneOptionCount; ++i) {
    if (i > 0) settings_timezone_options_text += '\n';
    settings_timezone_options_text += profile.timezone_labels[i];
  }
}

static void style_plain_container(lv_obj_t* obj) {
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, 0);
  // border_opa=TRANSP macht die Border nur unsichtbar - LVGL zieht ihre
  // Breite (Theme-Default) trotzdem von lv_obj_get_content_width/height ab
  // (siehe lv_obj_get_style_space_left/right in lv_obj_style.h). Ohne dieses
  // Nullen wird verschachtelter Content (z.B. settings_popup_content ->
  // Spacer) unbemerkt schmaler als sein Elternobjekt.
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_outline_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
}

static void style_popup_textarea(lv_obj_t* ta) {
  lv_obj_set_height(ta, popup_layout::scale(48));
  lv_textarea_set_one_line(ta, true);
  lv_obj_set_style_text_font(ta, popup_layout::font20(), 0);
  lv_obj_set_style_bg_color(ta, lv_color_hex(0x1E1E1E), 0);
  lv_obj_set_style_text_color(ta, lv_color_white(), 0);
  lv_obj_set_style_radius(ta, popup_layout::scale(10), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, 0);
  // Fokus: nur Farbe + Outline (ausserhalb gezeichnet) - die Border-Breite
  // bleibt konstant 1px, sonst waechst das LV_SIZE_CONTENT-Feld beim
  // Fokussieren um 2px und alles darunter springt nach unten.
  lv_obj_set_style_border_color(ta, lv_color_hex(0x26A69A), LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(ta, 3, LV_STATE_FOCUSED);
  lv_obj_set_style_outline_pad(ta, 0, LV_STATE_FOCUSED);
  lv_obj_set_style_outline_color(ta, lv_color_hex(0x26A69A), LV_STATE_FOCUSED);
  lv_obj_set_style_outline_opa(ta, LV_OPA_40, LV_STATE_FOCUSED);
  lv_obj_set_style_border_color(ta, lv_color_white(), LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_width(ta, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_side(ta, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
  lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_PART_CURSOR | LV_STATE_FOCUSED);
}

static void style_popup_dropdown(lv_obj_t* dd) {
  lv_obj_set_height(dd, popup_layout::scale(52));
  lv_obj_set_style_text_font(dd, popup_layout::font20(), LV_PART_MAIN);
  lv_obj_set_style_text_font(dd, &ui_symbols_20, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(dd, lv_color_hex(0x1E1E1E), 0);
  lv_obj_set_style_text_color(dd, lv_color_white(), 0);
  lv_obj_set_style_text_color(dd, lv_color_white(), LV_PART_INDICATOR);
  lv_obj_set_style_radius(dd, popup_layout::scale(10), 0);
  lv_obj_set_style_border_color(dd, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(dd, 1, 0);
  lv_obj_set_style_border_opa(dd, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(dd, popup_layout::scale(12), 0);
  lv_obj_set_style_pad_right(dd, popup_layout::scale(36), 0);
}

static void style_popup_dropdown_list(lv_obj_t* list) {
  if (!list) return;
  lv_obj_set_style_bg_color(list, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_font(list, popup_layout::font20(), LV_PART_MAIN);
  lv_obj_set_style_text_color(list, lv_color_white(), LV_PART_MAIN);
  lv_obj_set_style_radius(list, popup_layout::scale(10), LV_PART_MAIN);
  lv_obj_set_style_border_color(list, lv_color_hex(0x555555), LV_PART_MAIN);
  lv_obj_set_style_border_width(list, 1, LV_PART_MAIN);
  lv_obj_set_style_border_opa(list, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(list, popup_layout::scale(6), LV_PART_MAIN);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x26A69A), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_text_font(list, popup_layout::font20(), LV_PART_SELECTED);
  lv_obj_set_style_text_color(list, lv_color_white(), LV_PART_SELECTED);
  lv_obj_set_style_radius(list, popup_layout::scale(6), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(list, lv_color_hex(0x4A4A4A), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);
}

static lv_obj_t* create_popup_button(lv_obj_t* parent, const char* text, uint32_t color,
                                     lv_event_cb_t cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_height(btn, popup_layout::scale(64));
  style_settings_button(btn, color);
  lv_obj_set_style_border_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(btn, popup_layout::scale(20), 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, popup_layout::font20(), 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_center(label);
  return btn;
}

static lv_obj_t* create_form_area(lv_obj_t* parent) {
  lv_obj_t* form = lv_obj_create(parent);
  lv_obj_set_width(form, LV_PCT(100));
  lv_obj_set_flex_grow(form, 1);
  lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(form, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(form, 0, 0);
  lv_obj_set_style_pad_row(form, popup_layout::scale(8), 0);
  lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(form, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  return form;
}

static lv_obj_t* create_form_row(lv_obj_t* parent) {
  lv_obj_t* row = lv_obj_create(parent);
  style_plain_container(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, popup_layout::scale(76));
  lv_obj_set_style_pad_column(row, popup_layout::scale(10), 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return row;
}

static lv_obj_t* create_display_control_row(lv_obj_t* parent) {
  lv_obj_t* row = create_form_row(parent);
  lv_obj_set_height(row, popup_layout::scale(84));
  lv_obj_set_style_pad_column(row, popup_layout::scale(18), 0);
  return row;
}

// Beschriftung links und Wert rechts einheitlich in Weiss und derselben
// 24er-Groesse - das gedeckte Grau der Beschriftungen wirkte neben den
// weissen Werten wie eine zweite Hierarchie-Ebene (User-Feedback).
static lv_obj_t* create_display_row_label(lv_obj_t* parent, const char* text,
                                          lv_coord_t width,
                                          lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(label, width);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_style_text_font(label, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  return label;
}

static lv_obj_t* create_flex_spacer(lv_obj_t* parent) {
  lv_obj_t* spacer = lv_obj_create(parent);
  style_plain_container(spacer);
  lv_obj_set_width(spacer, 1);
  lv_obj_set_height(spacer, 1);
  lv_obj_set_flex_grow(spacer, 1);
  return spacer;
}

// Tastatur erscheint erst, wenn ein Feld antippt wird (statt permanent unten
// zu stehen), und wechselt fuer reine Zahlen-/IP-Felder auf LVGLs eingebautes
// Ziffernblock-Layout statt der vollen QWERTZ-Tastatur.
static void on_popup_textarea_focused(lv_event_t* e) {
  lv_obj_t* ta = static_cast<lv_obj_t*>(lv_event_get_target(e));
  if (!settings_popup_keyboard || !ta) return;
  const bool numeric = lv_event_get_user_data(e) != nullptr;
  lv_keyboard_set_mode(settings_popup_keyboard,
                       numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
  ui_keyboard_set_target(settings_popup_keyboard, ta, settings_popup_active_ta);
  settings_popup_active_ta = ta;
  lv_obj_clear_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
}

// In der WLAN-Eingabe-Ansicht ist die Tastatur fester Bestandteil des Screens
// (der Verbinden-Button sitzt direkt darueber) - dort darf sie weder ueber die
// Einklapp-Taste noch durch Tippen neben ein Feld verschwinden.
static bool wifi_keyboard_locked() {
  return settings_popup_kind == SettingsPopupKind::Wifi && wifi_entry_view &&
         !lv_obj_has_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);
}

// Tippt der Nutzer ausserhalb eines Feldes (auch Checkbox/Save/X-Button:
// per Default click-focusable), feuert LVGL hier DEFOCUSED, bevor der
// eigentliche Klick verarbeitet wird - blendet die Tastatur also zuverlaessig
// aus, ohne dass jeder Button das separat anstossen muesste.
static void on_popup_textarea_defocused(lv_event_t*) {
  if (wifi_keyboard_locked()) return;
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void reset_popup_refs() {
  settings_popup_card = nullptr;
  settings_popup_title = nullptr;
  settings_popup_content = nullptr;
  settings_popup_keyboard = nullptr;
  settings_popup_active_ta = nullptr;
  settings_popup_close_icon = nullptr;
  settings_popup_kb_spacer = nullptr;

  brightness_label = nullptr;
  screensaver_brightness_value_label = nullptr;
  // WICHTIG: wird in build_display_popup gesetzt - ohne das Nullen hier
  // zeigte der Pointer nach dem Schliessen des Display-Popups ins Leere und
  // der naechste settings_refresh_language()-Aufruf (Sprachwechsel!) schrieb
  // per lv_label_set_text in freigegebenen Speicher -> Guru Meditation
  // (Store access fault in lv_label_revert_dots; Tab5-Crash 2026-07-05).
  brightness_title_label = nullptr;
  screensaver_brightness_title_label = nullptr;
  display_rotate_btn = nullptr;
  display_rotate_label = nullptr;
  display_rotate_text_label = nullptr;
  display_rotate_sub_label = nullptr;
  sleep_slider = nullptr;
  sleep_time_label = nullptr;
  sleep_label = nullptr;
  screensaver_slider = nullptr;
  screensaver_time_label = nullptr;
  screensaver_label = nullptr;
  screensaver_brightness_slider = nullptr;
  ap_mode_btn = nullptr;
  ap_mode_btn_label = nullptr;
  wifi_disconnect_btn = nullptr;
  net_mode_btn = nullptr;
  net_mode_btn_label = nullptr;
  net_mode_hint_label = nullptr;
  ip_mode_hint_label = nullptr;
  net_mode_row = nullptr;
  ethernet_dhcp_btn = nullptr;
  ethernet_dhcp_btn_label = nullptr;

  wifi_ssid_ta = nullptr;
  wifi_pass_ta = nullptr;
  wifi_pass_eye_icon = nullptr;
  wifi_list_view = nullptr;
  wifi_entry_view = nullptr;
  wifi_conn_status_label = nullptr;
  wifi_scan_status_label = nullptr;
  wifi_list_container = nullptr;
  wifi_manual_row = nullptr;
  wifi_manual_gap = nullptr;
  wifi_ap_qr = nullptr;
  wifi_info_box = nullptr;
  wifi_ap_info_rows = nullptr;
  wifi_ap_ssid_val = nullptr;
  wifi_ap_pw_val = nullptr;
  wifi_ap_ip_val = nullptr;
  wifi_list_spacer = nullptr;
  wifi_ap_qr_sized = false;
  wifi_manual_mode = false;

  locale_language_dd = nullptr;
  locale_timezone_dd = nullptr;
  locale_time_format_dd = nullptr;
  locale_date_format_dd = nullptr;
  locale_keyboard_dd = nullptr;

  system_info_rows = nullptr;
  system_status_label = nullptr;
  system_progress_bar = nullptr;
  system_check_btn = nullptr;
  system_check_btn_label = nullptr;
  system_github_btn = nullptr;
  system_reboot_btn = nullptr;
  system_pair_btn = nullptr;
  system_action_row = nullptr;
  system_qr = nullptr;
  system_spacer = nullptr;
  system_qr_sized = false;
  system_check_running = false;
  system_install_running = false;
  // system_latest_tag/system_update_available bleiben absichtlich stehen
}

static void close_settings_popup() {
  wifi_stop_scan_timer();
  if (ap_btn_cooldown_timer) {
    lv_timer_del(ap_btn_cooldown_timer);
    ap_btn_cooldown_timer = nullptr;
  }
  if (networkTransport.isWifiDriverActive()) WiFi.scanDelete();
  // Falls der Screensaver-Helligkeitsregler waehrend einer laufenden
  // Vorschau geschlossen wird, immer zur normalen Helligkeit zurueckkehren.
  if (settings_popup_kind == SettingsPopupKind::Display) {
    powerManager.setDisplayBrightness(
        configManager.getConfig().display_brightness);
  }
  if (settings_popup_overlay) {
    lv_obj_del(settings_popup_overlay);
  }
  settings_popup_overlay = nullptr;
  reset_popup_refs();
}

static void on_settings_popup_close_clicked(lv_event_t*) {
  // In der WLAN-Eingabe-Ansicht ist der Kopf-Button ein Zurueck-Pfeil (Icon
  // wird in wifi_show_entry_view getauscht) und fuehrt zur Liste zurueck -
  // erst von dort schliesst er das Popup wirklich.
  if (settings_popup_kind == SettingsPopupKind::Wifi) {
    const bool entry_visible =
        wifi_entry_view &&
        !lv_obj_has_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);
    if (entry_visible) {
      wifi_show_list_view();
      return;
    }
  }
  close_settings_popup();
}

// WLAN-Liste/Eingabe-Statemachine, portiert aus dem vormals unverdrahteten
// wifi_setup_popup.cpp (Scan/Liste/Verbinden-Logik unveraendert uebernommen),
// aber an die bereits gefixte Tastatur (create_popup_keyboard) gehaengt statt
// eine zweite, eigene Tastatur-Instanz mit eigenen (ungefixten) Raendern zu
// bauen.
static void wifi_show_entry_view(bool manual, const char* ssid, bool open_network);
static void wifi_show_list_view();

static void wifi_stop_scan_timer() {
  if (wifi_scan_timer) {
    lv_timer_del(wifi_scan_timer);
    wifi_scan_timer = nullptr;
  }
}

// "Suche Netzwerke..." erscheint NUR, solange die Liste komplett leer ist
// (allererster Scan ohne bekanntes Netz) - sobald Zeilen stehen, laeuft jeder
// weitere Scan unsichtbar im Hintergrund, statt permanent einen Ladehinweis
// einzublenden. Der Container wird dabei nie vorab geleert (siehe
// wifi_on_scan_timer), damit die Liste nicht kurz leer aufblitzt.
static bool wifi_list_is_empty() {
  return !wifi_list_container || lv_obj_get_child_count(wifi_list_container) == 0;
}

static void wifi_show_scanning_state() {
  if (!wifi_scan_status_label) return;
  if (wifi_list_is_empty()) {
    lv_label_set_text(wifi_scan_status_label, tr().wifi_scan_searching);
    lv_obj_clear_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void wifi_show_scan_finished_state() {
  if (!wifi_scan_status_label) return;
  if (wifi_list_is_empty()) {
    lv_label_set_text(wifi_scan_status_label, tr().wifi_scan_none);
    lv_obj_clear_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);
  }
}

static void on_wifi_manual_clicked(lv_event_t*) {
  wifi_show_entry_view(true, nullptr, false);
}

static void on_wifi_network_row_clicked(lv_event_t* e) {
  const size_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  if (index >= wifi_scan_result_count) return;
  wifi_show_entry_view(false, wifi_scan_results[index].ssid, wifi_scan_results[index].open);
}

static void on_wifi_known_row_clicked(lv_event_t*) {
  wifi_show_entry_view(false, wifi_known_fallback_ssid, false);
}

static lv_obj_t* wifi_create_row(lv_obj_t* parent, const char* name_text, bool show_check,
                                 bool show_lock) {
  lv_obj_t* row = lv_button_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, popup_layout::scale(72));
  style_settings_button(row, 0x3A3A3A);
  lv_obj_set_style_border_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(row, popup_layout::scale(20), 0);
  lv_obj_set_style_pad_hor(row, popup_layout::scale(20), 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, popup_layout::scale(12), 0);

  if (show_check) {
    lv_obj_t* check = lv_label_create(row);
    lv_label_set_text(check, getMdiChar("check").c_str());
    if (FONT_MDI_ICONS) lv_obj_set_style_text_font(check, FONT_MDI_ICONS, 0);
    popup_layout::applyIconScale(check);
    lv_obj_set_style_text_color(check, lv_color_hex(0x26A69A), 0);
  }

  lv_obj_t* name = lv_label_create(row);
  lv_label_set_text(name, name_text);
  lv_obj_set_style_text_font(name, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(name, lv_color_white(), 0);
  lv_obj_set_flex_grow(name, 1);
  lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

  if (show_lock) {
    lv_obj_t* lock = lv_label_create(row);
    lv_label_set_text(lock, getMdiChar("lock").c_str());
    if (FONT_MDI_ICONS) lv_obj_set_style_text_font(lock, FONT_MDI_ICONS, 0);
    popup_layout::applyIconScale(lock);
    lv_obj_set_style_text_color(lock, lv_color_hex(0x888888), 0);
  }
  return row;
}

// Bekanntes/verbundenes Netz wird IMMER angezeigt, auch bevor/ohne dass ein
// Scan es findet (versteckte SSIDs tauchen bei WiFi.scanNetworks() per
// Default gar nicht erst auf) - sonst verschwindet ein gerade manuell
// verbundenes Netz aus der Liste, sobald der naechste Scan durchlaeuft.
static void wifi_populate_list() {
  if (!wifi_list_container) return;
  lv_obj_clean(wifi_list_container);

  const bool sta_connected =
      !ap_mode_active &&
      networkTransport.activeKind() == NetworkTransportKind::Wifi &&
      networkTransport.isWifiConnected();
  const String current_ssid =
      String(configManager.getConfig().wifi_ssid);

  bool current_in_results = false;
  for (size_t i = 0; i < wifi_scan_result_count; ++i) {
    const bool is_current = current_ssid.length() && current_ssid == wifi_scan_results[i].ssid;
    if (is_current) current_in_results = true;
    lv_obj_t* row = wifi_create_row(wifi_list_container, wifi_scan_results[i].ssid,
                                    is_current && sta_connected, !wifi_scan_results[i].open);
    lv_obj_add_event_cb(row, on_wifi_network_row_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
  }

  if (current_ssid.length() && !current_in_results) {
    strncpy(wifi_known_fallback_ssid, current_ssid.c_str(), sizeof(wifi_known_fallback_ssid) - 1);
    wifi_known_fallback_ssid[sizeof(wifi_known_fallback_ssid) - 1] = '\0';
    lv_obj_t* row = wifi_create_row(wifi_list_container, wifi_known_fallback_ssid,
                                    sta_connected, true);
    lv_obj_add_event_cb(row, on_wifi_known_row_clicked, LV_EVENT_CLICKED, nullptr);
  }
}

static void wifi_on_scan_timer(lv_timer_t*) {
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  wifi_stop_scan_timer();

  wifi_scan_result_count = 0;
  for (int16_t i = 0; i < n && wifi_scan_result_count < 24; ++i) {
    String ssid = WiFi.SSID(i);
    if (!ssid.length()) continue;
    const int16_t rssi = static_cast<int16_t>(WiFi.RSSI(i));
    const bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

    bool duplicate = false;
    for (size_t k = 0; k < wifi_scan_result_count; ++k) {
      if (strcmp(wifi_scan_results[k].ssid, ssid.c_str()) == 0) {
        duplicate = true;
        if (rssi > wifi_scan_results[k].rssi) {
          wifi_scan_results[k].rssi = rssi;
          wifi_scan_results[k].open = open;
        }
        break;
      }
    }
    if (duplicate) continue;

    WifiScanEntry& entry = wifi_scan_results[wifi_scan_result_count++];
    strncpy(entry.ssid, ssid.c_str(), sizeof(entry.ssid) - 1);
    entry.ssid[sizeof(entry.ssid) - 1] = '\0';
    entry.rssi = rssi;
    entry.open = open;
  }
  WiFi.scanDelete();

  // Nach Signalstaerke sortieren
  for (size_t i = 1; i < wifi_scan_result_count; ++i) {
    WifiScanEntry key = wifi_scan_results[i];
    size_t j = i;
    while (j > 0 && wifi_scan_results[j - 1].rssi < key.rssi) {
      wifi_scan_results[j] = wifi_scan_results[j - 1];
      --j;
    }
    wifi_scan_results[j] = key;
  }

  wifi_populate_list();
  wifi_show_scan_finished_state();
}

// Alte Ergebnisse bleiben absichtlich stehen (siehe wifi_show_scanning_state) -
// nur der Zaehler wird NICHT vorab genullt, damit die Liste bis zum
// naechsten fertigen Scan nicht kurz leer aufblitzt.
static void wifi_start_scan() {
  wifi_stop_scan_timer();
  // Erst die Liste fuellen (bekanntes Netz erscheint sofort), dann
  // entscheiden, ob der "Suche..."-Hinweis ueberhaupt noetig ist.
  wifi_populate_list();
  // Kein Scan im AP-Modus: WiFi.scanNetworks() schaltet STA dazu und ein
  // noch laufender Scan laesst spaeter WiFi.begin()/den Portal-Betrieb ins
  // Leere laufen (deshalb verband sich das Geraet nach "AP beenden" nicht
  // mehr automatisch).
  if (ap_mode_active) {
    wifi_show_scan_finished_state();
    return;
  }
  // Ethernet and STA WiFi are exclusive. Merely opening the WiFi popup must
  // not restart the hosted P4 WiFi driver behind the transport manager.
  if (!networkTransport.isWifiDriverActive()) {
    wifi_show_scan_finished_state();
    return;
  }
  // Frisch angestossener (Re-)Connect: Scan wuerde WiFi.begin() stoeren
  if (wifi_scan_block_until != 0 &&
      (int32_t)(millis() - wifi_scan_block_until) < 0) {
    wifi_show_scan_finished_state();
    return;
  }
  // scanNetworks() fuehrt intern mehrere ESP-Hosted-RPCs aus
  // (GetMode/GetMode/SetMode/ScanStart). Ist der C6 bereits festgefahren,
  // blockiert das sonst viermal je etwa 5 s. Ein einzelner echter Mode-RPC
  // prueft den Kanal vorab und loest bei dessen Timeout die zentrale
  // Recovery aus.
  if (!networkManager.probeWifiDriverHealth("WLAN-Scan in Settings")) {
    wifi_show_scan_finished_state();
    return;
  }
  wifi_show_scanning_state();
  WiFi.scanDelete();
  const int16_t r = WiFi.scanNetworks(/*async=*/true);
  if (r == WIFI_SCAN_FAILED) {
    wifi_show_scan_finished_state();
    return;
  }
  wifi_scan_timer = lv_timer_create(wifi_on_scan_timer, 500, nullptr);
}

static void wifi_update_conn_status_label() {
  if (!wifi_conn_status_label) return;
  static char buf[96];

  // Aendert sich der Verbindungszustand (z.B. Live-Reconnect nach
  // "Verbinden"), die Zeilen neu aufbauen - sonst fehlt das gruene Haekchen
  // am gerade verbundenen Netz, weil waehrend des Verbindens keine Scans
  // laufen, die die Liste auffrischen wuerden.
  const bool sta_connected =
      !ap_mode_active &&
      networkTransport.activeKind() == NetworkTransportKind::Wifi &&
      networkTransport.isWifiConnected();
  static bool prev_sta_connected = false;
  if (sta_connected != prev_sta_connected) {
    prev_sta_connected = sta_connected;
    if (wifi_list_container) wifi_populate_list();
  }

  // Trennen gibt es nur, solange eine STA-Verbindung besteht (im AP-Modus
  // und offline uebernimmt der AP-Button die volle Reihenbreite).
  if (wifi_disconnect_btn) {
    if (sta_connected) lv_obj_clear_flag(wifi_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(wifi_disconnect_btn, LV_OBJ_FLAG_HIDDEN);
  }

  if (wifi_list_spacer) {
    lv_obj_clear_flag(wifi_list_spacer, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifi_info_box) lv_obj_clear_flag(wifi_info_box, LV_OBJ_FLAG_HIDDEN);

  // Im AP-Modus sind Scan und Netzwerkliste nutzlos (Scans sind waehrend des
  // Portal-Betriebs abgeschaltet, siehe wifi_start_scan) - die Liste weicht
  // der grossen Infobox mit Zugangsdaten + QR-Code.
  const bool ap = ap_mode_active;
  if (wifi_list_container) {
    if (ap) lv_obj_add_flag(wifi_list_container, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(wifi_list_container, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifi_manual_gap) {
    if (ap) lv_obj_add_flag(wifi_manual_gap, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(wifi_manual_gap, LV_OBJ_FLAG_HIDDEN);
  }
  if (wifi_manual_row) {
    if (ap) lv_obj_add_flag(wifi_manual_row, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(wifi_manual_row, LV_OBJ_FLAG_HIDDEN);
  }
  if (ap && wifi_scan_status_label) lv_obj_add_flag(wifi_scan_status_label, LV_OBJ_FLAG_HIDDEN);

  if (ap) {
    // Ueberschrift + Zugangsdaten-Zeilen (SSID/Passwort/IP buendig
    // untereinander) statt des frueheren Fliesstexts mit "(PW: ...)".
    lv_label_set_text(wifi_conn_status_label, tr().wifi_ap_active);
    lv_obj_set_style_text_font(wifi_conn_status_label, popup_layout::font28(), 0);
    lv_obj_set_style_text_color(wifi_conn_status_label, lv_color_hex(0xFFC04D), 0);
    // Direkt nach dem Einschalten liefert softAPIP() noch "0.0.0.0" -
    // dann die AP-Standard-IP anzeigen statt Muell.
    String ap_ip = WiFi.softAPIP().toString();
    const bool ip_valid = ap_ip.length() && ap_ip != "0.0.0.0";
    if (wifi_ap_info_rows) {
      lv_obj_clear_flag(wifi_ap_info_rows, LV_OBJ_FLAG_HIDDEN);
      if (wifi_ap_ssid_val) lv_label_set_text(wifi_ap_ssid_val, webConfigApSsid());
      if (wifi_ap_pw_val) lv_label_set_text(wifi_ap_pw_val, webConfigApPassword());
      if (wifi_ap_ip_val) {
        lv_label_set_text(wifi_ap_ip_val, ip_valid ? ap_ip.c_str() : "192.168.4.1");
      }
    }
#if LV_USE_QRCODE
    if (wifi_ap_qr) {
      if (!wifi_ap_qr_sized) {
        // Einmal pro Popup (der Hauptloop ruft im AP-Betrieb jede Runde hier
        // an): Mit noch verstecktem QR layouten - der flex-grow-Spacer traegt
        // dann genau den Restplatz, den der QR-Code einnehmen darf (abzueglich
        // pad_row der Infobox + etwas Luft).
        if (wifi_list_view) lv_obj_update_layout(wifi_list_view);
        int target = popup_layout::scale(320);
        if (wifi_list_spacer) {
          target = lv_obj_get_height(wifi_list_spacer) -
                   popup_layout::scale(24);
        }
        const int max_w = lv_obj_get_content_width(lv_obj_get_parent(wifi_ap_qr));
        if (target > max_w) target = max_w;
        if (target < popup_layout::scale(240)) {
          target = popup_layout::scale(240);
        }
        lv_qrcode_set_size(wifi_ap_qr, target);
        wifi_ap_qr_sized = true;
      }
      // Handy-Kameras verbinden sich damit direkt mit dem Hotspot
      static char qr_buf[128];
      static char last_qr_buf[128] = {};
      static lv_obj_t* last_qr_obj = nullptr;
      snprintf(qr_buf, sizeof(qr_buf), "WIFI:T:WPA;S:%s;P:%s;;",
               webConfigApSsid(), webConfigApPassword());
      if (last_qr_obj != wifi_ap_qr || strcmp(last_qr_buf, qr_buf) != 0) {
        lv_qrcode_update(wifi_ap_qr, qr_buf, strlen(qr_buf));
        last_qr_obj = wifi_ap_qr;
        strncpy(last_qr_buf, qr_buf, sizeof(last_qr_buf) - 1);
        last_qr_buf[sizeof(last_qr_buf) - 1] = '\0';
      }
      lv_obj_clear_flag(wifi_ap_qr, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    return;
  }

#if LV_USE_QRCODE
  if (wifi_ap_qr) lv_obj_add_flag(wifi_ap_qr, LV_OBJ_FLAG_HIDDEN);
#endif
  if (wifi_ap_info_rows) lv_obj_add_flag(wifi_ap_info_rows, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_text_font(wifi_conn_status_label, popup_layout::font24(), 0);
  if (networkTransport.isConnected()) {
    const String network_name =
        networkTransport.activeKind() == NetworkTransportKind::Wifi
            ? String(configManager.getConfig().wifi_ssid)
            : String(networkTransport.activeName());
    String ip = networkTransport.localIP().toString();
    snprintf(buf, sizeof(buf), "%s: %s (%s)", tr().wifi_connected,
             network_name.length() ? network_name.c_str() : "---", ip.c_str());
    lv_label_set_text(wifi_conn_status_label, buf);
    lv_obj_set_style_text_color(wifi_conn_status_label, lv_color_hex(0x51CF66), 0);
  } else {
    lv_label_set_text(wifi_conn_status_label, tr().wifi_offline);
    lv_obj_set_style_text_color(wifi_conn_status_label, lv_color_hex(0xFF6B6B), 0);
  }
}

// WLAN und Ethernet verwenden denselben IP-Modus und dieselben Adresswerte.
// Der Umschalter aendert nur den gemeinsamen Modus; die Werte bleiben
// gespeichert und koennen vor dem Neustart wieder aktiviert werden.
static bool selected_static_addressing_saved() {
  return configManager.getConfig().wifi_static_enabled;
}

static bool valid_static_ip_value(const char* value) {
  if (!value || !value[0]) return false;
  IPAddress parsed;
  String text(value);
  text.trim();
  return text.length() && parsed.fromString(text);
}

static bool selected_static_addressing_available() {
  const DeviceConfig& cfg = configManager.getConfig();
  return valid_static_ip_value(cfg.wifi_static_ip) &&
         valid_static_ip_value(cfg.wifi_gateway) &&
         valid_static_ip_value(cfg.wifi_subnet);
}

static void update_ethernet_dhcp_button_ui() {
  if (!ethernet_dhcp_btn) return;
  const bool static_enabled = selected_static_addressing_saved();
  const bool boot_static_enabled =
      configManager.bootStaticAddressingEnabled();
  // Im normalen DHCP-Betrieb bleibt die Zeile unsichtbar. Sie dient nur als
  // Rettung, solange eine statische Adresse aktiv ist, und nach dem Umschalten
  // auf DHCP bis zum Neustart einmalig zum Rueckgaengigmachen.
  const bool show_toggle =
      boot_static_enabled &&
      (static_enabled || selected_static_addressing_available());
  if (show_toggle) {
    lv_obj_clear_flag(ethernet_dhcp_btn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ethernet_dhcp_btn, LV_OBJ_FLAG_HIDDEN);
  }
  if (net_mode_row) {
    if (net_mode_btn || show_toggle) {
      lv_obj_clear_flag(net_mode_row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(net_mode_row, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ethernet_dhcp_btn_label) {
    lv_label_set_text(
        ethernet_dhcp_btn_label,
        static_enabled ? tr().ethernet_dhcp_reset
                       : tr().ethernet_static_restore);
  }
  style_settings_button(ethernet_dhcp_btn, 0x424242);
}

static void net_mode_update_ui() {
  const DeviceConfig& cfg = configManager.getConfig();
  const bool eth_saved = cfg.ethernet_enabled;
  if (net_mode_btn_label) {
    lv_label_set_text(net_mode_btn_label,
                      eth_saved ? tr().net_mode_to_wifi : tr().net_mode_to_ethernet);
  }
  if (net_mode_hint_label) {
    if (eth_saved != networkTransport.isEthernetMode()) {
      lv_label_set_text(net_mode_hint_label, tr().net_mode_restart_note);
      lv_obj_clear_flag(net_mode_hint_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(net_mode_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (ip_mode_hint_label) {
    const bool static_enabled = cfg.wifi_static_enabled;
    const bool boot_static_enabled =
        configManager.bootStaticAddressingEnabled();
    if (static_enabled != boot_static_enabled) {
      lv_label_set_text(
          ip_mode_hint_label,
          static_enabled ? tr().ethernet_static_selected
                         : tr().ethernet_dhcp_selected);
      lv_obj_clear_flag(ip_mode_hint_label, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ip_mode_hint_label, LV_OBJ_FLAG_HIDDEN);
    }
  }
  update_ethernet_dhcp_button_ui();
}

static void on_net_mode_clicked(lv_event_t*) {
  const bool to_ethernet = !configManager.getConfig().ethernet_enabled;
  if (!configManager.saveEthernetEnabled(to_ethernet)) return;
  Serial.printf("[Settings] Netzwerkmodus gespeichert: %s (gilt nach Neustart)\n",
                to_ethernet ? "Ethernet" : "WLAN");
  net_mode_update_ui();
}

static void on_ethernet_dhcp_clicked(lv_event_t*) {
  const bool enable_static = !selected_static_addressing_saved();
  if (enable_static && !selected_static_addressing_available()) return;
  if (!configManager.saveStaticAddressingEnabled(enable_static)) return;
  Serial.printf(
      "[Settings] Gemeinsamer IP-Modus gespeichert: %s (gilt nach Neustart)\n",
      enable_static ? "statisch" : "DHCP");
  net_mode_update_ui();
}

// Jeder Aufruf (Erstoeffnen wie auch "Zurueck" aus der Eingabe-Ansicht)
// stoesst einen frischen Scan an - ersetzt den frueheren separaten
// Rescan-Icon-Button, den der Nutzer als unnoetigen Ballast empfand.
static void wifi_show_list_view() {
  if (wifi_list_view) lv_obj_clear_flag(wifi_list_view, LV_OBJ_FLAG_HIDDEN);
  if (wifi_entry_view) lv_obj_add_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
  // Ohne Tastatur braucht die Liste den reservierten Bereich nicht -
  // ausgeblendet nutzt sie die volle Popup-Hoehe.
  if (settings_popup_kb_spacer) lv_obj_add_flag(settings_popup_kb_spacer, LV_OBJ_FLAG_HIDDEN);
  if (settings_popup_close_icon) {
    lv_label_set_text(settings_popup_close_icon, getMdiChar("window-close").c_str());
  }
  settings_popup_active_ta = nullptr;
  wifi_update_conn_status_label();
  wifi_start_scan();
}

static void wifi_show_entry_view(bool manual, const char* ssid, bool open_network) {
  wifi_manual_mode = manual;
  wifi_selected_open = open_network;
  if (ssid) {
    strncpy(wifi_selected_ssid, ssid, sizeof(wifi_selected_ssid) - 1);
    wifi_selected_ssid[sizeof(wifi_selected_ssid) - 1] = '\0';
  } else {
    wifi_selected_ssid[0] = '\0';
  }
  const bool hide_password = !manual && open_network;

  // Einheitliches Layout: das SSID-Feld ist IMMER sichtbar. Aus der Liste
  // kommend ist es vorbefuellt und nicht antippbar (nur bei "Manuell"
  // editierbar) - dezente Farben signalisieren den Nur-Lese-Zustand.
  if (wifi_ssid_ta) {
    lv_textarea_set_text(wifi_ssid_ta, manual ? "" : wifi_selected_ssid);
    if (manual) {
      lv_obj_add_flag(wifi_ssid_ta, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_text_color(wifi_ssid_ta, lv_color_white(), 0);
      lv_obj_set_style_bg_color(wifi_ssid_ta, lv_color_hex(0x1E1E1E), 0);
    } else {
      lv_obj_clear_flag(wifi_ssid_ta, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_set_style_text_color(wifi_ssid_ta, lv_color_hex(0xB8B8B8), 0);
      lv_obj_set_style_bg_color(wifi_ssid_ta, lv_color_hex(0x2A2A2A), 0);
    }
  }

  if (wifi_pass_ta) {
    // Bereits gespeichertes Passwort vorausfuellen, wenn das angetippte Netz
    // das aktuell konfigurierte ist - sonst wirkt das Feld beim erneuten
    // Oeffnen des eigenen Netzes so, als waere gar kein Passwort hinterlegt.
    const DeviceConfig& known_cfg = configManager.getConfig();
    const bool is_known_ssid = !manual && wifi_selected_ssid[0] &&
                              strcmp(wifi_selected_ssid, known_cfg.wifi_ssid) == 0;
    lv_textarea_set_text(wifi_pass_ta, is_known_ssid ? known_cfg.wifi_pass : "");
    lv_textarea_set_password_mode(wifi_pass_ta, true);
    if (wifi_pass_eye_icon) lv_label_set_text(wifi_pass_eye_icon, getMdiChar("eye").c_str());
    lv_obj_t* pass_box = lv_obj_get_parent(wifi_pass_ta);
    if (pass_box) {
      if (hide_password) lv_obj_add_flag(pass_box, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_clear_flag(pass_box, LV_OBJ_FLAG_HIDDEN);
    }
  }

  if (wifi_list_view) lv_obj_add_flag(wifi_list_view, LV_OBJ_FLAG_HIDDEN);
  if (wifi_entry_view) lv_obj_clear_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);

  // Kopf-Button wird zum Zurueck-Pfeil (statt eines eigenen Zurueck-Links im
  // Body) - Klick-Logik siehe on_settings_popup_close_clicked.
  if (settings_popup_close_icon) {
    lv_label_set_text(settings_popup_close_icon, getMdiChar("arrow-left").c_str());
  }

  // Anders als bei jedem anderen Popup-Feld wird die Tastatur hier sofort
  // gezeigt statt erst beim Antippen eines Feldes - dieser Screen dient fast
  // ausschliesslich der Texteingabe. Der Layout-Platzhalter wandert synchron
  // mit, damit der Verbinden-Button nicht unter der Tastatur landet.
  if (settings_popup_keyboard) {
    if (hide_password) {
      lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
      if (settings_popup_kb_spacer) lv_obj_add_flag(settings_popup_kb_spacer, LV_OBJ_FLAG_HIDDEN);
      settings_popup_active_ta = nullptr;
    } else {
      lv_keyboard_set_mode(settings_popup_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
      lv_obj_t* first = manual ? wifi_ssid_ta : wifi_pass_ta;
      lv_obj_t* second = manual ? wifi_pass_ta : wifi_ssid_ta;
      ui_keyboard_set_target(settings_popup_keyboard, first, second);
      settings_popup_active_ta = first;
      if (settings_popup_kb_spacer) lv_obj_clear_flag(settings_popup_kb_spacer, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void on_wifi_pass_eye_clicked(lv_event_t* e) {
  lv_obj_t* icon = static_cast<lv_obj_t*>(lv_event_get_target(e));
  lv_obj_t* ta = icon ? lv_obj_get_parent(icon) : nullptr;
  if (!ta) return;
  const bool currently_masked = lv_textarea_get_password_mode(ta);
  lv_textarea_set_password_mode(ta, !currently_masked);
  lv_label_set_text(icon, getMdiChar(currently_masked ? "eye-off" : "eye").c_str());
}

// Verbindet LIVE statt das Geraet neu zu starten: Zugangsdaten speichern und
// den Reconnect im Hauptloop anstossen (WiFi.disconnect + connectWifi mit den
// frischen Daten). Danach zurueck zur Listen-Ansicht - die Infobox zeigt den
// Verbindungsfortschritt, weil settings_update_wifi_status sie live fuettert.
static void wifi_do_connect() {
  const char* ssid = wifi_manual_mode && wifi_ssid_ta ? lv_textarea_get_text(wifi_ssid_ta)
                                                      : wifi_selected_ssid;
  const char* pass = wifi_pass_ta ? lv_textarea_get_text(wifi_pass_ta) : "";
  if (!ssid || !ssid[0]) return;
  // Gesichertes Netz ohne Passwort: nicht speichern (endet nur in einer
  // toten Verbindung)
  if (!wifi_manual_mode && !wifi_selected_open && (!pass || !pass[0])) return;

  const DeviceConfig& cur = configManager.getConfig();
  const bool creds_changed = strcmp(cur.wifi_ssid, ssid) != 0 ||
                             strcmp(cur.wifi_pass, pass ? pass : "") != 0;

  if (creds_changed) {
    DeviceConfig cfg = configManager.getConfig();
    strncpy(cfg.wifi_ssid, ssid, CONFIG_WIFI_SSID_MAX - 1);
    cfg.wifi_ssid[CONFIG_WIFI_SSID_MAX - 1] = '\0';
    strncpy(cfg.wifi_pass, pass ? pass : "", CONFIG_WIFI_PASS_MAX - 1);
    cfg.wifi_pass[CONFIG_WIFI_PASS_MAX - 1] = '\0';
    // Netzwechsel setzt auf DHCP zurueck, damit eine alte statische IP das
    // Geraet im neuen Netz nicht aussperrt (statische IP gibt's nur noch im
    // Web-Admin).
    cfg.wifi_static_enabled = false;
    cfg.wifi_static_ip[0] = '\0';
    cfg.wifi_gateway[0] = '\0';
    cfg.wifi_subnet[0] = '\0';
    cfg.wifi_dns[0] = '\0';
    if (!configManager.save(cfg)) {
      if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().wifi_save_failed);
      return;
    }
  }

  wifi_stop_scan_timer();
  if (networkTransport.isWifiDriverActive()) WiFi.scanDelete();
  // Waehrend der Verbindungsaufbau laeuft, keine neuen Scans anstossen
  wifi_scan_block_until = millis() + 10000UL;

  if (ap_mode_active) {
    // AP aus - apply_hotspot_mode im Hauptloop verbindet dann mit den
    // (ggf. gerade gespeicherten) Zugangsdaten
    if (g_hotspot_callback) g_hotspot_callback(false);
  } else if (creds_changed || !networkTransport.isWifiConnected()) {
    if (g_wifi_reconnect_callback) g_wifi_reconnect_callback();
  }

  wifi_show_list_view();
}

// Kopf bleibt immer gleich (Icon+"WLAN"+Speichern+X, siehe open_settings_popup) -
// "Speichern" verbindet nur, wenn der Eingabe-Screen offen ist, auf dem
// Listen-Screen tut es nichts.
static void save_wifi_popup() {
  if (wifi_entry_view && !lv_obj_has_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN)) {
    wifi_do_connect();
  }
}

static void save_localization_popup() {
  DeviceConfig cfg = configManager.getConfig();
  const uint32_t language_index = locale_language_dd ? lv_dropdown_get_selected(locale_language_dd) : 0;
  strncpy(cfg.language, i18n::language_code_at(language_index), sizeof(cfg.language) - 1);
  cfg.language[sizeof(cfg.language) - 1] = '\0';

  const uint32_t timezone_index = locale_timezone_dd ? lv_dropdown_get_selected(locale_timezone_dd) : 2;
  strncpy(cfg.timezone, selected_timezone_code(timezone_index), sizeof(cfg.timezone) - 1);
  cfg.timezone[sizeof(cfg.timezone) - 1] = '\0';

  cfg.global_time_format = clock_tile::normalize_time_format(
      locale_time_format_dd ? static_cast<int>(lv_dropdown_get_selected(locale_time_format_dd)) : 0);
  cfg.global_date_format = clock_tile::normalize_date_format(
      locale_date_format_dd ? static_cast<int>(lv_dropdown_get_selected(locale_date_format_dd)) : 0);

  uint8_t keyboard_layout = locale_keyboard_dd
      ? static_cast<uint8_t>(lv_dropdown_get_selected(locale_keyboard_dd)) : 0;
  cfg.keyboard_layout = (keyboard_layout > 2) ? 0 : keyboard_layout;

  if (!configManager.save(cfg)) {
    if (settings_popup_title) lv_label_set_text(settings_popup_title, tr().save_failed);
    return;
  }

  settings_refresh_language();
  uiManager.scheduleNtpSync(0);
  tiles_request_reload_all();
  // Popup bleibt nach dem Speichern offen. Der Inhalt wird neu aufgebaut,
  // damit Zeilen-Labels und Optionstexte eine evtl. geaenderte Sprache
  // sofort zeigen. lv_obj_clean loescht dabei auch den gerade gedrueckten
  // Speichern-Button - dasselbe Muster wie close_settings_popup aus dem
  // X-Klick (LVGL raeumt Events auf geloeschte Ziele selbst ab).
  if (settings_popup_content) {
    locale_language_dd = nullptr;
    locale_timezone_dd = nullptr;
    locale_time_format_dd = nullptr;
    locale_date_format_dd = nullptr;
    locale_keyboard_dd = nullptr;
    lv_obj_clean(settings_popup_content);
    build_localization_popup(settings_popup_content);
  }
}

static void save_settings_popup() {
  switch (settings_popup_kind) {
    case SettingsPopupKind::Wifi:
      save_wifi_popup();
      break;
    case SettingsPopupKind::Localization:
      save_localization_popup();
      break;
    default:
      break;
  }
}

static void on_settings_popup_save_clicked(lv_event_t*) {
  save_settings_popup();
}

static void hide_popup_keyboard() {
  if (settings_popup_active_ta) {
    lv_obj_remove_state(settings_popup_active_ta, LV_STATE_FOCUSED);
    settings_popup_active_ta = nullptr;
  }
  if (settings_popup_keyboard) lv_obj_add_flag(settings_popup_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void on_popup_keyboard_event(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    save_settings_popup();
  } else if (code == LV_EVENT_CANCEL) {
    // Die kleine Tastatur-Taste unten links soll nur die Tastatur einklappen,
    // nicht das ganze Popup schliessen (frueher: close_settings_popup(), was
    // ungesicherte Aenderungen verworfen hat). In der WLAN-Eingabe bleibt die
    // Tastatur grundsaetzlich stehen.
    if (wifi_keyboard_locked()) return;
    hide_popup_keyboard();
  }
}

// Cooldown nach jedem AP-Umschalten: der Button springt sofort auf die neue
// Beschriftung um und sitzt an derselben Stelle - ohne Sperre schaltete ein
// schnelles Doppeltippen den AP direkt wieder ein/aus.
static void on_ap_btn_cooldown_timer(lv_timer_t*) {
  ap_btn_cooldown_timer = nullptr;  // repeat_count=1: LVGL loescht den Timer selbst
  if (ap_mode_btn) {
    lv_obj_set_style_opa(ap_mode_btn, LV_OPA_COVER, 0);
    lv_obj_add_flag(ap_mode_btn, LV_OBJ_FLAG_CLICKABLE);
  }
}

static void on_wifi_disconnect_clicked(lv_event_t*) {
  if (!g_wifi_disconnect_callback) return;
  // Laufenden Scan beenden - das Trennen passiert asynchron im Hauptloop.
  wifi_stop_scan_timer();
  if (networkTransport.isWifiDriverActive()) WiFi.scanDelete();
  g_wifi_disconnect_callback();
  // Kein optimistisches UI-Update: Statuszeile und Button-Sichtbarkeit
  // ziehen ueber wifi_update_conn_status_label nach, sobald die Verbindung
  // tatsaechlich weg ist.
}

static void on_popup_ap_mode_clicked(lv_event_t*) {
  if (ap_btn_cooldown_timer) return;
  if (ap_mode_click_block_until != 0 &&
      (int32_t)(millis() - ap_mode_click_block_until) < 0) {
    return;
  }
  // Laufenden Scan sofort beenden - der eigentliche Moduswechsel passiert
  // asynchron im Hauptloop und darf nicht mit einem Scan kollidieren.
  wifi_stop_scan_timer();
  if (networkTransport.isWifiDriverActive()) WiFi.scanDelete();
  if (g_hotspot_callback) {
    g_hotspot_callback(!ap_mode_active);
  }
  // Kein optimistisches settings_update_ap_mode mehr: Beschriftung/Farbe
  // wechseln erst, wenn der Hauptloop den Modus wirklich umgeschaltet hat
  // (apply_hotspot_mode ruft settings_update_ap_mode). Bis dahin ist der
  // Button gedimmt und inert.
  ap_mode_click_block_until = millis() + 2500;
  if (ap_mode_btn) {
    lv_obj_set_style_opa(ap_mode_btn, LV_OPA_50, 0);
    lv_obj_clear_flag(ap_mode_btn, LV_OBJ_FLAG_CLICKABLE);
  }
  ap_btn_cooldown_timer = lv_timer_create(on_ap_btn_cooldown_timer, 2500, nullptr);
  lv_timer_set_repeat_count(ap_btn_cooldown_timer, 1);
}

static void build_display_popup(lv_obj_t* parent) {
  const DeviceConfig& cfg = configManager.getConfig();
  lv_obj_t* form = create_form_area(parent);
  lv_obj_clear_flag(form, LV_OBJ_FLAG_SCROLLABLE);
  // Fuenf Reihen: mit festem Abstand von oben und ausreichend Luft dazwischen -
  // nicht ganz oben angeklebt (wirkte verloren), aber auch nicht komplett
  // mittig (User-Wunsch).
  lv_obj_set_style_pad_top(form, popup_layout::scale(32), 0);
  lv_obj_set_style_pad_row(form, popup_layout::scale(18), 0);

  // Schmalere Beschriftungs-/Wertspalten als frueher (210/150): der
  // Slider dazwischen bekommt den gewonnenen Platz.
  lv_obj_t* brightness_row = create_display_control_row(form);
  brightness_title_label =
      create_display_row_label(brightness_row, tr().brightness_label,
                               popup_layout::scale(170));

  lv_obj_t* brightness_slider = lv_slider_create(brightness_row);
  style_settings_slider(brightness_slider);
  lv_obj_set_width(brightness_slider, 1);
  lv_obj_set_flex_grow(brightness_slider, 1);
  lv_slider_set_range(brightness_slider, kSettingsBrightnessPctMin, kSettingsBrightnessPctMax);
  const int current_brightness_pct = brightness_pct_from_raw(BoardHAL::getBrightness());
  lv_slider_set_value(brightness_slider, current_brightness_pct, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(brightness_slider, on_brightness, LV_EVENT_RELEASED, nullptr);

  static char bright_buf[16];
  snprintf(bright_buf, sizeof(bright_buf), "%d%%", current_brightness_pct);
  // Gleiche Wertspaltenbreite wie beim Sleep-Slider, damit beide Slider
  // exakt gleich lang sind
  brightness_label =
      create_display_row_label(brightness_row, bright_buf,
                               popup_layout::scale(130),
                               LV_TEXT_ALIGN_RIGHT);

  lv_obj_t* sleep_row = create_display_control_row(form);
  sleep_label = create_display_row_label(
      sleep_row, tr().sleep_label, popup_layout::scale(170));

  sleep_slider = lv_slider_create(sleep_row);
  style_settings_slider(sleep_slider);
  lv_obj_set_width(sleep_slider, 1);
  lv_obj_set_flex_grow(sleep_slider, 1);
  lv_slider_set_range(sleep_slider, 0, sleep_slider_max_index());
  int32_t sleep_index = cfg.auto_sleep_enabled
                            ? sleep_index_from_seconds(cfg.auto_sleep_seconds)
                            : sleep_slider_max_index();
  lv_slider_set_value(sleep_slider, sleep_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(sleep_slider, on_sleep_slider, LV_EVENT_RELEASED, nullptr);

  static char sleep_buf[32];
  format_sleep_popup_value_for_index(sleep_buf, sizeof(sleep_buf), sleep_index);
  sleep_time_label =
      create_display_row_label(sleep_row, sleep_buf,
                               popup_layout::scale(130),
                               LV_TEXT_ALIGN_RIGHT);

  lv_obj_t* screensaver_row = create_display_control_row(form);
  screensaver_label =
      create_display_row_label(screensaver_row, tr().screensaver_label,
                               popup_layout::scale(170));

  screensaver_slider = lv_slider_create(screensaver_row);
  style_settings_slider(screensaver_slider);
  lv_obj_set_width(screensaver_slider, 1);
  lv_obj_set_flex_grow(screensaver_slider, 1);
  lv_slider_set_range(screensaver_slider, 0, sleep_slider_max_index());
  const int32_t screensaver_index = cfg.auto_screensaver_enabled
                                        ? sleep_index_from_seconds(cfg.auto_screensaver_seconds)
                                        : sleep_slider_max_index();
  lv_slider_set_value(screensaver_slider, screensaver_index, LV_ANIM_OFF);
  lv_obj_add_event_cb(screensaver_slider, on_screensaver_slider,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_add_event_cb(screensaver_slider, on_screensaver_slider,
                      LV_EVENT_RELEASED, nullptr);

  static char screensaver_buf[32];
  format_sleep_popup_value_for_index(screensaver_buf, sizeof(screensaver_buf),
                                     screensaver_index);
  screensaver_time_label = create_display_row_label(
      screensaver_row, screensaver_buf, popup_layout::scale(130),
      LV_TEXT_ALIGN_RIGHT);

  lv_obj_t* screensaver_brightness_row = create_display_control_row(form);
  screensaver_brightness_title_label = create_display_row_label(
      screensaver_brightness_row, tr().screensaver_brightness_label,
      popup_layout::scale(170));
  lv_label_set_long_mode(screensaver_brightness_title_label,
                         LV_LABEL_LONG_WRAP);

  screensaver_brightness_slider =
      lv_slider_create(screensaver_brightness_row);
  style_settings_slider(screensaver_brightness_slider);
  lv_obj_set_width(screensaver_brightness_slider, 1);
  lv_obj_set_flex_grow(screensaver_brightness_slider, 1);
  lv_slider_set_range(screensaver_brightness_slider,
                      kScreensaverBrightnessPctMin,
                      kScreensaverBrightnessPctMax);
  lv_slider_set_value(screensaver_brightness_slider,
                      cfg.screensaver_brightness_pct, LV_ANIM_OFF);
  lv_obj_add_event_cb(screensaver_brightness_slider,
                      on_screensaver_brightness, LV_EVENT_VALUE_CHANGED,
                      nullptr);
  lv_obj_add_event_cb(screensaver_brightness_slider,
                      on_screensaver_brightness, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(screensaver_brightness_slider,
                      on_screensaver_brightness, LV_EVENT_PRESS_LOST,
                      nullptr);

  static char screensaver_brightness_buf[16];
  snprintf(screensaver_brightness_buf, sizeof(screensaver_brightness_buf),
           "%u%%",
           static_cast<unsigned>(cfg.screensaver_brightness_pct));
  screensaver_brightness_value_label = create_display_row_label(
      screensaver_brightness_row, screensaver_brightness_buf,
      popup_layout::scale(130), LV_TEXT_ALIGN_RIGHT);

  // Rotation als vollbreiter Button mit Icon + Beschriftung im Button
  // (statt "Rotation"-Label links neben einem Icon-Button)
  display_rotate_btn = create_popup_button(form, "", 0x26A69A, on_display_rotate_clicked);
  lv_obj_set_width(display_rotate_btn, LV_PCT(100));
  lv_obj_set_height(display_rotate_btn, popup_layout::scale(76));
  lv_obj_set_flex_flow(display_rotate_btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(display_rotate_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(display_rotate_btn, popup_layout::scale(14), 0);
  // Das von create_popup_button angelegte Label wird zum Icon (Flex-Layout
  // ueberstimmt dessen lv_obj_center)
  display_rotate_label = lv_obj_get_child(display_rotate_btn, 0);
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(display_rotate_label, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(display_rotate_label);
  display_rotate_text_label = lv_label_create(display_rotate_btn);
  lv_label_set_text(display_rotate_text_label, tr().display_rotate_btn_text);
  lv_obj_set_style_text_font(display_rotate_text_label,
                             popup_layout::font28(), 0);
  lv_obj_set_style_text_color(display_rotate_text_label, lv_color_white(), 0);
  update_display_rotate_label();
}

// Tastatur bleibt naeher am Card-Rand als das restliche Formular: statt der
// normalen 20px Card-Padding nur kKeyboardInset auf allen drei Seiten
// (links/rechts/unten). Dafuer wird sie direkt in settings_popup_card
// (ignore-layout, wie Save-/Close-Button) statt in settings_popup_content
// gehaengt - andernfalls wuerde settings_popup_content (pad=0) das Ausbrechen
// aus seiner eigenen Box abschneiden.
static constexpr int kPopupCardPad = popup_layout::scale(20);
static constexpr int kKeyboardInset = popup_layout::scale(11);
static constexpr int kKeyboardBleed = kPopupCardPad - kKeyboardInset;

// content_parent = settings_popup_content (Breite/Hoehe zur Laufzeit
// gemessen statt aus SCREEN_WIDTH/HEIGHT hochgerechnet - Letzteres passte
// nicht zur tatsaechlichen Kartenbreite und liess rechts keinen Rand).
static void create_popup_keyboard(lv_obj_t* content_parent) {
  // Platzhalter in der normalen Flex-Spalte (wie "form" ein Kind von
  // content_parent): sorgt dafuer, dass "form" wie vorher schrumpft und die
  // unteren Feld-/Button-Reihen nicht von der jetzt frei positionierten
  // Tastatur verdeckt werden.
  lv_obj_t* spacer = lv_obj_create(content_parent);
  style_plain_container(spacer);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(spacer, LV_PCT(100));
  // 48% statt 42%: groessere Tasten (User-Wunsch); die Eingabe-Ansicht
  // (2 Zeilen + Verbinden ~ 280px) passt auch auf den 720ern noch drueber.
  lv_obj_set_height(spacer, LV_PCT(48));
  lv_obj_update_layout(content_parent);
  // Breite von der KARTE statt vom Spacer: der Content ist auf Lesebreite
  // gedeckelt (max_width), die Tastatur soll aber weiterhin die volle
  // Kartenbreite nutzen.
  const int reserved_w = lv_obj_get_content_width(settings_popup_card);
  const int reserved_h = lv_obj_get_height(spacer);
  // Erst NACH der Messung merken/ausblendbar machen - die Tastaturgroesse
  // haengt von der sichtbaren 42%-Hoehe ab.
  settings_popup_kb_spacer = spacer;

  lv_obj_t* kb = ui_keyboard_create(settings_popup_card);
  lv_obj_add_flag(kb, LV_OBJ_FLAG_IGNORE_LAYOUT);
  const int kb_w = reserved_w + (kKeyboardBleed * 2);
  const int kb_h = reserved_h + kKeyboardBleed;
  lv_obj_set_size(kb, kb_w, kb_h);
  // LVGL rechnet bei lv_obj_align IMMER das Padding des Elternobjekts (card)
  // mit ein - auch bei TOP_LEFT (lv_obj_move_to addiert space_left/top des
  // Parents unconditional). BOTTOM_LEFT mit negativem Offset ist also der
  // richtige Weg: card_pad(20) + (-bleed) = Inset links, card_pad(20) - bleed
  // via ph-h+bleed = Inset unten, Breite (siehe reserved_w) sorgt fuer Inset
  // rechts - vorausgesetzt style_plain_container nullt auch border_width
  // (nicht nur border_opa), sonst zaehlt LVGLs unsichtbare Theme-Border
  // zusaetzlich zum Padding und macht reserved_w schmaler als die Card
  // tatsaechlich ist (siehe style_plain_container-Kommentar).
  lv_obj_align(kb, LV_ALIGN_BOTTOM_LEFT, -kKeyboardBleed, kKeyboardBleed);
  lv_obj_add_event_cb(kb, on_popup_keyboard_event, LV_EVENT_ALL, nullptr);

  // Erst sichtbar, sobald ein Feld fokussiert wird (on_popup_textarea_focused) -
  // kein Auto-Fokus eines Feldes beim Oeffnen mehr.
  lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
  settings_popup_keyboard = kb;
}

// Eingabezeile "Label | Textfeld" in EINER Reihe (statt Label ueber dem
// Feld) - kompakter, damit auf dem 4-Zoll-B4 nichts abgeschnitten wird.
// Feldhoehe bewusst LV_SIZE_CONTENT + pad_ver statt fester Pixel: so sitzen
// Text und Cursor konstruktionsbedingt exakt mittig.
// Eine "Beschriftung:  Wert"-Zeile mit fester Beschriftungsspalte, damit die
// Werte aller Zeilen an derselben Kante beginnen (AP-Infobox und System-
// Popup). Rueckgabe ist das Wert-Label; der Aufrufer befuellt es.
static lv_obj_t* create_info_value_row(lv_obj_t* parent, const char* label_text) {
  lv_obj_t* row = lv_obj_create(parent);
  style_plain_container(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);

  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text_fmt(label, "%s:", label_text);
  lv_obj_set_width(label, popup_layout::scale(160));
  lv_obj_set_style_text_font(label, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xC8C8C8), 0);

  lv_obj_t* value = lv_label_create(row);
  lv_label_set_text(value, "-");
  lv_obj_set_style_text_font(value, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(value, lv_color_white(), 0);
  return value;
}

static lv_obj_t* wifi_create_entry_row(lv_obj_t* parent, const char* label_text,
                                       lv_obj_t** ta_out, uint16_t max_len,
                                       bool password) {
  lv_obj_t* row = lv_obj_create(parent);
  style_plain_container(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, popup_layout::scale(14), 0);

  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text_fmt(label, "%s:", label_text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, popup_layout::scale(160));
  lv_obj_set_style_text_font(label, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xC8C8C8), 0);

  lv_obj_t* ta = lv_textarea_create(row);
  style_popup_textarea(ta);
  // flex_grow im ROW-Elternobjekt = restliche Breite (Hauptachse)
  lv_obj_set_flex_grow(ta, 1);
  lv_textarea_set_max_length(ta, max_len);
  lv_textarea_set_password_mode(ta, password);
  lv_textarea_set_placeholder_text(ta, "");
  lv_textarea_set_text(ta, "");
  // PRESSED zusaetzlich zu FOCUSED: FOCUSED haengt an LVGLs interner
  // "last_pressed"-Objektverfolgung und feuert in manchen Abfolgen nicht
  // zuverlaessig - PRESSED reagiert dagegen immer sofort auf den Touch.
  lv_obj_add_event_cb(ta, on_popup_textarea_focused, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(ta, on_popup_textarea_focused, LV_EVENT_FOCUSED, nullptr);
  lv_obj_add_event_cb(ta, on_popup_textarea_defocused, LV_EVENT_DEFOCUSED, nullptr);

  lv_obj_set_height(ta, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(ta, popup_layout::scale(20), 0);
  lv_obj_set_style_pad_left(ta, popup_layout::scale(20), 0);
  lv_obj_set_style_radius(ta, popup_layout::scale(18), 0);
  lv_obj_set_style_text_font(ta, popup_layout::font28(), 0);
  // lv_textarea ist intern scrollbar - ohne das hier zeigt LVGL einen
  // Scrollbalken an der Feldkante.
  lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);

  *ta_out = ta;
  return row;
}

static void build_wifi_popup(lv_obj_t* parent) {
  // ===== Listen-Ansicht: Liste kompakt oben (Manuell direkt darunter),
  // Infobox + AP-Button als Fussbereich unten =====
  wifi_list_view = lv_obj_create(parent);
  style_plain_container(wifi_list_view);
  lv_obj_set_width(wifi_list_view, LV_PCT(100));
  lv_obj_set_flex_grow(wifi_list_view, 1);
  lv_obj_clear_flag(wifi_list_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(wifi_list_view, LV_FLEX_FLOW_COLUMN);
  // Querachse CENTER: zentriert den (nicht mehr vollbreiten) AP-Button,
  // vollbreite Kinder sind davon unbeeindruckt.
  lv_obj_set_flex_align(wifi_list_view, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(wifi_list_view, popup_layout::scale(10), 0);

  wifi_scan_status_label = lv_label_create(wifi_list_view);
  lv_label_set_text(wifi_scan_status_label, tr().wifi_scan_searching);
  lv_obj_set_style_text_font(wifi_scan_status_label,
                             popup_layout::font20(), 0);
  lv_obj_set_style_text_color(wifi_scan_status_label, lv_color_hex(0xA0A0A0), 0);

  // Liste ist nur so hoch wie ihr Inhalt (max. knapp die halbe Ansicht,
  // darueber hinaus scrollt sie) - so rueckt "Manuell" direkt unter die
  // Ergebnisse statt am unteren Rand zu kleben.
  wifi_list_container = lv_obj_create(wifi_list_view);
  style_plain_container(wifi_list_container);
  lv_obj_set_width(wifi_list_container, LV_PCT(100));
  lv_obj_set_height(wifi_list_container, LV_SIZE_CONTENT);
  lv_obj_set_style_max_height(wifi_list_container, LV_PCT(44), 0);
  lv_obj_add_flag(wifi_list_container, LV_OBJ_FLAG_SCROLLABLE);
  // Scrollen per Touch bleibt, nur der Balken verschwindet.
  lv_obj_set_scrollbar_mode(wifi_list_container, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(wifi_list_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(wifi_list_container, popup_layout::scale(8), 0);

  // Ausserhalb des scrollbaren Containers - steht immer fest unter der Liste
  // (statt bei jedem Rescan mit den Ergebnissen neu erzeugt zu werden) und ist
  // durch den Gap optisch abgesetzt.
  wifi_manual_gap = lv_obj_create(wifi_list_view);
  style_plain_container(wifi_manual_gap);
  lv_obj_clear_flag(wifi_manual_gap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(wifi_manual_gap, LV_PCT(100));
  lv_obj_set_height(wifi_manual_gap, popup_layout::scale(6));

  wifi_manual_row = wifi_create_row(wifi_list_view, tr().wifi_manual_entry, false, false);
  lv_obj_add_event_cb(wifi_manual_row, on_wifi_manual_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* manual_chevron = lv_label_create(wifi_manual_row);
  lv_label_set_text(manual_chevron, getMdiChar("chevron-right").c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(manual_chevron, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(manual_chevron);
  lv_obj_set_style_text_color(manual_chevron, lv_color_hex(0x888888), 0);

  // Spacer drueckt den Fussbereich (Infobox + AP-Button) nach unten; im
  // AP-Modus misst wifi_update_conn_status_label an seiner Hoehe, wie gross
  // der QR-Code werden darf.
  wifi_list_spacer = create_flex_spacer(wifi_list_view);

  // Infobox: abgesetzte Karte mit Verbindungsstatus; im AP-Modus zusaetzlich
  // QR-Code zum direkten Verbinden mit dem Hotspot.
  wifi_info_box = lv_obj_create(wifi_list_view);
  style_plain_container(wifi_info_box);
  lv_obj_set_width(wifi_info_box, LV_PCT(100));
  lv_obj_set_height(wifi_info_box, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_color(wifi_info_box, lv_color_hex(0x333333), 0);
  lv_obj_set_style_bg_opa(wifi_info_box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(wifi_info_box, popup_layout::scale(20), 0);
  lv_obj_set_style_pad_all(wifi_info_box, popup_layout::scale(18), 0);
  lv_obj_set_style_pad_row(wifi_info_box, popup_layout::scale(16), 0);
  lv_obj_set_flex_flow(wifi_info_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(wifi_info_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  wifi_conn_status_label = lv_label_create(wifi_info_box);
  lv_obj_set_width(wifi_conn_status_label, LV_PCT(100));
  lv_label_set_long_mode(wifi_conn_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(wifi_conn_status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(wifi_conn_status_label,
                             popup_layout::font24(), 0);

  // Nach einem Wechsel steht der Neustart-Hinweis direkt beim aktuellen
  // Verbindungsstatus in der Infobox statt losgeloest unter den Buttons.
  if (NetworkTransportManager::deviceSupportsEthernet()) {
    net_mode_hint_label = lv_label_create(wifi_info_box);
    lv_obj_set_width(net_mode_hint_label, LV_PCT(100));
    lv_label_set_long_mode(net_mode_hint_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(net_mode_hint_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(net_mode_hint_label,
                               popup_layout::font20(), 0);
    lv_obj_set_style_text_color(net_mode_hint_label, lv_color_hex(0xFFC04D), 0);
    lv_label_set_text(net_mode_hint_label, tr().net_mode_restart_note);
    lv_obj_add_flag(net_mode_hint_label, LV_OBJ_FLAG_HIDDEN);
  }

  // Eigene dritte Statuszeile fuer den IP-Modus. Dadurch koennen ein
  // vorgemerkter Netzwerkwechsel und DHCP/Static gleichzeitig sichtbar sein.
  ip_mode_hint_label = lv_label_create(wifi_info_box);
  lv_obj_set_width(ip_mode_hint_label, LV_PCT(100));
  lv_label_set_long_mode(ip_mode_hint_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(ip_mode_hint_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ip_mode_hint_label, popup_layout::font20(), 0);
  lv_obj_set_style_text_color(
      ip_mode_hint_label, lv_color_hex(0xFFC04D), 0);
  lv_label_set_text(ip_mode_hint_label, tr().ethernet_dhcp_selected);
  lv_obj_add_flag(ip_mode_hint_label, LV_OBJ_FLAG_HIDDEN);

  // Zugangsdaten im AP-Modus: Beschriftungsspalte fest, Werte dahinter -
  // SSID/Passwort/IP stehen dadurch buendig untereinander. Block selbst ist
  // SIZE_CONTENT und wird von der Infobox mittig gesetzt.
  wifi_ap_info_rows = lv_obj_create(wifi_info_box);
  style_plain_container(wifi_ap_info_rows);
  lv_obj_set_size(wifi_ap_info_rows, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(wifi_ap_info_rows, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(wifi_ap_info_rows, popup_layout::scale(8), 0);
  lv_obj_add_flag(wifi_ap_info_rows, LV_OBJ_FLAG_HIDDEN);
  wifi_ap_ssid_val = create_info_value_row(wifi_ap_info_rows, tr().ssid_label);
  wifi_ap_pw_val = create_info_value_row(wifi_ap_info_rows, tr().wifi_password_label);
  wifi_ap_ip_val = create_info_value_row(wifi_ap_info_rows, tr().ip_label);

#if LV_USE_QRCODE
  wifi_ap_qr = lv_qrcode_create(wifi_info_box);
  lv_qrcode_set_size(wifi_ap_qr, popup_layout::scale(320));
  lv_qrcode_set_dark_color(wifi_ap_qr, lv_color_black());
  lv_qrcode_set_light_color(wifi_ap_qr, lv_color_white());
  lv_qrcode_set_quiet_zone(wifi_ap_qr, true);
  style_qr_code(wifi_ap_qr);
  lv_obj_add_flag(wifi_ap_qr, LV_OBJ_FLAG_HIDDEN);
#endif

  // Fussreihe in voller Inhaltsbreite: Trennen (nur bei bestehender STA-
  // Verbindung sichtbar, siehe wifi_update_conn_status_label) + AP-Toggle.
  // Beide mit flex_grow: ist Trennen ausgeblendet, fuellt der AP-Button die
  // Reihe allein (Flex ignoriert versteckte Kinder).
  lv_obj_t* wifi_btn_row = lv_obj_create(wifi_list_view);
  style_plain_container(wifi_btn_row);
  lv_obj_clear_flag(wifi_btn_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(wifi_btn_row, LV_PCT(100));
  lv_obj_set_height(wifi_btn_row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(wifi_btn_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(wifi_btn_row, popup_layout::scale(12), 0);

  wifi_disconnect_btn = create_popup_button(wifi_btn_row, tr().wifi_disconnect_btn,
                                            0xC14444, on_wifi_disconnect_clicked);
  lv_obj_set_flex_grow(wifi_disconnect_btn, 1);
  lv_obj_set_height(wifi_disconnect_btn, popup_layout::scale(76));
  lv_obj_t* disconnect_label = lv_obj_get_child(wifi_disconnect_btn, 0);
  if (disconnect_label) {
    lv_obj_set_style_text_font(disconnect_label,
                               popup_layout::font28(), 0);
  }
  lv_obj_add_flag(wifi_disconnect_btn, LV_OBJ_FLAG_HIDDEN);

  ap_mode_btn = create_popup_button(wifi_btn_row, ap_mode_active ? tr().ap_disable : tr().ap_enable,
                                    ap_mode_active ? 0xC62828 : 0xFF9800,
                                    on_popup_ap_mode_clicked);
  lv_obj_set_flex_grow(ap_mode_btn, 1);
  lv_obj_set_height(ap_mode_btn, popup_layout::scale(76));
  ap_mode_btn_label = lv_obj_get_child(ap_mode_btn, 0);
  if (ap_mode_btn_label) {
    lv_obj_set_style_text_font(ap_mode_btn_label,
                               popup_layout::font28(), 0);
  }

  // Der DHCP-Reset gilt fuer das ausgewaehlte Netzwerkprofil und ist deshalb
  // auch auf reinen WLAN-Geraeten verfuegbar. Der Ethernet-Schalter wird nur
  // auf Builds mit Ethernet-Unterstuetzung daneben eingefuegt.
  net_mode_row = lv_obj_create(wifi_list_view);
  style_plain_container(net_mode_row);
  lv_obj_clear_flag(net_mode_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(net_mode_row, LV_PCT(100));
  lv_obj_set_height(net_mode_row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(net_mode_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(net_mode_row, popup_layout::scale(12), 0);

  if (NetworkTransportManager::deviceSupportsEthernet()) {
    net_mode_btn = create_popup_button(net_mode_row, "", 0x424242,
                                       on_net_mode_clicked);
    lv_obj_set_width(net_mode_btn, 1);
    lv_obj_set_flex_grow(net_mode_btn, 1);
    lv_obj_set_height(net_mode_btn, popup_layout::scale(76));
    net_mode_btn_label = lv_obj_get_child(net_mode_btn, 0);
    if (net_mode_btn_label) {
      lv_obj_set_style_text_font(net_mode_btn_label,
                                 popup_layout::font28(), 0);
    }
  }

  ethernet_dhcp_btn = create_popup_button(
      net_mode_row, "", 0x424242, on_ethernet_dhcp_clicked);
  lv_obj_set_width(ethernet_dhcp_btn, 1);
  lv_obj_set_flex_grow(ethernet_dhcp_btn, 1);
  lv_obj_set_height(ethernet_dhcp_btn, popup_layout::scale(76));
  ethernet_dhcp_btn_label = lv_obj_get_child(ethernet_dhcp_btn, 0);
  if (ethernet_dhcp_btn_label) {
    lv_obj_set_width(ethernet_dhcp_btn_label, LV_PCT(94));
    lv_label_set_long_mode(ethernet_dhcp_btn_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(
        ethernet_dhcp_btn_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(
        ethernet_dhcp_btn_label, popup_layout::font28(), 0);
  }

  net_mode_update_ui();

  // ===== Eingabe-Ansicht: SSID-/Passwort-Zeilen, Verbinden-Button unten =====
  wifi_entry_view = lv_obj_create(parent);
  style_plain_container(wifi_entry_view);
  lv_obj_set_width(wifi_entry_view, LV_PCT(100));
  lv_obj_set_flex_grow(wifi_entry_view, 1);
  lv_obj_clear_flag(wifi_entry_view, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(wifi_entry_view, LV_FLEX_FLOW_COLUMN);
  // Querachse CENTER fuer den kompakten Verbinden-Button; die vollbreiten
  // Eingabezeilen bleiben vollbreit.
  lv_obj_set_flex_align(wifi_entry_view, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  // Etwas enger als die anderen Formulare: mit der 48%-Tastatur muss
  // "2 Zeilen + Verbinden" auch auf den 720er-Displays sicher passen.
  lv_obj_set_style_pad_row(wifi_entry_view, popup_layout::scale(10), 0);
  lv_obj_add_flag(wifi_entry_view, LV_OBJ_FLAG_HIDDEN);

  wifi_create_entry_row(wifi_entry_view, tr().ssid_label, &wifi_ssid_ta,
                        CONFIG_WIFI_SSID_MAX - 1, false);
  wifi_create_entry_row(wifi_entry_view, tr().wifi_password_label, &wifi_pass_ta,
                        CONFIG_WIFI_PASS_MAX - 1, true);
  lv_obj_set_style_pad_right(wifi_pass_ta, popup_layout::scale(64), 0);
  wifi_pass_eye_icon = lv_label_create(wifi_pass_ta);
  lv_label_set_text(wifi_pass_eye_icon, getMdiChar("eye").c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(wifi_pass_eye_icon, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(wifi_pass_eye_icon);
  lv_obj_set_style_text_color(wifi_pass_eye_icon, lv_color_hex(0x888888), 0);
  // FLOATING: sonst zaehlt das Icon in die LV_SIZE_CONTENT-Hoehe des Feldes
  // hinein. lv_obj_align rechnet das Eltern-Padding mit ein (siehe
  // create_popup_keyboard) - der positive Offset kompensiert pad_right(64),
  // damit das Icon wirklich an der Feldkante sitzt (18px Abstand) statt links
  // neben dem Textbereich zu haengen.
  lv_obj_add_flag(wifi_pass_eye_icon, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(wifi_pass_eye_icon, LV_ALIGN_RIGHT_MID,
               popup_layout::scale(64 - 18), 0);
  lv_obj_add_flag(wifi_pass_eye_icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(wifi_pass_eye_icon, on_wifi_pass_eye_clicked, LV_EVENT_CLICKED, nullptr);

  // Kleiner Abstand zwischen Passwortfeld und Verbinden-Button
  lv_obj_t* connect_gap = lv_obj_create(wifi_entry_view);
  style_plain_container(connect_gap);
  lv_obj_clear_flag(connect_gap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(connect_gap, LV_PCT(100), popup_layout::scale(6));

  // Eigener, klar beschrifteter Verbinden-Button statt des generischen
  // Speichern oben rechts. Nutzt denselben Save-Dispatch wie die Tastatur-
  // Enter-Taste (on_settings_popup_save_clicked -> save_wifi_popup).
  // Direkt unter den Feldern (nicht mehr an den unteren Rand gedrueckt),
  // volle Inhaltsbreite wie der AP-Button.
  lv_obj_t* wifi_connect_btn = create_popup_button(wifi_entry_view, tr().wifi_connect_btn, 0x2E7D32,
                                                   on_settings_popup_save_clicked);
  lv_obj_set_width(wifi_connect_btn, LV_PCT(100));
  lv_obj_set_height(wifi_connect_btn, popup_layout::scale(76));
  lv_obj_t* connect_label = lv_obj_get_child(wifi_connect_btn, 0);
  if (connect_label) {
    lv_obj_set_style_text_font(connect_label,
                               popup_layout::font28(), 0);
  }

  // Restplatz zwischen Button und Tastaturbereich
  create_flex_spacer(wifi_entry_view);

  // Tastatur zuerst (legt den Platzhalter an), dann die Listen-Ansicht
  // aktivieren - die blendet den Platzhalter direkt wieder aus.
  create_popup_keyboard(parent);
  wifi_show_list_view();
}

static String format_options_text(bool time_format) {
  String opts;
  opts.reserve(80);
  opts += tr().format_auto_language;
  opts += '\n';
  if (time_format) {
    opts += tr().format_24_hour;
    opts += '\n';
    opts += tr().format_12_hour;
  } else {
    opts += "DD.MM.YYYY\nMM/DD/YYYY\nYYYY/MM/DD";
  }
  return opts;
}

// Aufgeklappte Liste im Stil des geschlossenen Feldes: gleiche 28er-Schrift,
// Eintraege buendig zum Feldtext (pad_left 20 wie das Feld statt 6) und
// clip_corner, damit der blaue Auswahlbalken beim ersten/letzten Eintrag
// nicht ueber den Eckenradius hinausragt.
static void style_locale_dropdown_list(lv_obj_t* list) {
  if (!list) return;
  style_popup_dropdown_list(list);
  lv_obj_set_style_text_font(list, popup_layout::font28(), LV_PART_MAIN);
  lv_obj_set_style_text_font(list, popup_layout::font28(), LV_PART_SELECTED);
  lv_obj_set_style_pad_left(list, popup_layout::scale(20), 0);
  lv_obj_set_style_pad_right(list, popup_layout::scale(10), 0);
  lv_obj_set_style_pad_ver(list, popup_layout::scale(8), 0);
  lv_obj_set_style_clip_corner(list, true, 0);
}

// Liste bei jedem Oeffnen frisch stylen (READY-Event), sonst setzt LVGLs
// Theme sie auf die kompakten Defaults zurueck.
static void on_locale_dropdown_ready(lv_event_t* e) {
  lv_obj_t* dd = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  if (!dd) return;
  style_locale_dropdown_list(lv_dropdown_get_list(dd));
}

// Eine Lokalisierungs-Zeile "Label | Dropdown" - exakt das Muster (Fonts,
// Radius, Padding) der WLAN-Eingabezeilen (wifi_create_entry_row), damit
// beide Seiten gleich aussehen. Zeilenhoehe = Inhaltshoehe: kompakt genug,
// dass alle 5 Zeilen + Speichern-Button auch auf 720p ohne Scrollen passen.
static lv_obj_t* create_locale_dropdown_row(lv_obj_t* form, const char* label_text,
                                            const char* options, uint32_t selected) {
  lv_obj_t* row = lv_obj_create(form);
  style_plain_container(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, popup_layout::scale(14), 0);

  // Breiter als beim WLAN (160): "Datumsformat" braucht Platz, und eine
  // gemeinsame Spaltenbreite haelt alle Dropdowns buendig untereinander.
  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text(label, label_text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(label, popup_layout::scale(210));
  lv_obj_set_style_text_font(label, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);

  lv_obj_t* dd = lv_dropdown_create(row);
  style_popup_dropdown(dd);
  // flex_grow im ROW-Elternobjekt = restliche Breite (Hauptachse)
  lv_obj_set_flex_grow(dd, 1);
  lv_dropdown_set_symbol(dd, LV_SYMBOL_DOWN);
  lv_dropdown_set_options(dd, options);
  lv_dropdown_set_selected(dd, selected);

  // Inhaltshoehe + symmetrisches Padding = Text sitzt exakt mittig
  lv_obj_set_height(dd, LV_SIZE_CONTENT);
  lv_obj_set_style_pad_ver(dd, popup_layout::scale(18), 0);
  lv_obj_set_style_pad_left(dd, popup_layout::scale(20), 0);
  lv_obj_set_style_radius(dd, popup_layout::scale(18), 0);
  lv_obj_set_style_text_font(dd, popup_layout::font28(), LV_PART_MAIN);
  lv_obj_set_style_text_font(dd, &ui_symbols_24, LV_PART_INDICATOR);
  style_locale_dropdown_list(lv_dropdown_get_list(dd));
  lv_obj_add_event_cb(dd, on_locale_dropdown_ready, LV_EVENT_READY, nullptr);
  return dd;
}

static void build_localization_popup(lv_obj_t* parent) {
  const DeviceConfig& cfg = configManager.getConfig();
  lv_obj_t* form = create_form_area(parent);
  // Gleicher Zeilenabstand wie die WLAN-Eingabe-Ansicht
  lv_obj_set_style_pad_row(form, popup_layout::scale(14), 0);

  build_timezone_dropdown_options();
  const String language_options = i18n::build_language_dropdown_options();
  const String time_options = format_options_text(true);
  const String date_options = format_options_text(false);
  const String keyboard_options =
      String(tr().format_auto_language) + "\nDeutsch (QWERTZ)\nEnglish (QWERTY)";

  locale_language_dd = create_locale_dropdown_row(form, tr().language_label,
                                                  language_options.c_str(),
                                                  i18n::language_index(cfg.language));
  locale_timezone_dd = create_locale_dropdown_row(form, tr().timezone_label,
                                                  settings_timezone_options_text.c_str(),
                                                  settings_timezone_index(cfg.timezone));
  locale_time_format_dd = create_locale_dropdown_row(form, tr().time_format_label,
                                                     time_options.c_str(),
                                                     clock_tile::normalize_time_format(cfg.global_time_format));
  locale_date_format_dd = create_locale_dropdown_row(form, tr().date_format_label,
                                                     date_options.c_str(),
                                                     clock_tile::normalize_date_format(cfg.global_date_format));
  locale_keyboard_dd = create_locale_dropdown_row(form, tr().keyboard_layout_label,
                                                  keyboard_options.c_str(),
                                                  cfg.keyboard_layout > 2 ? 0 : cfg.keyboard_layout);

  // Speichern unten in voller Inhaltsbreite wie AP-/Verbinden-Button (das
  // Formular darueber bekommt per flex_grow die Resthoehe)
  lv_obj_t* save_btn = create_popup_button(parent, tr().save, 0x2E7D32,
                                           on_settings_popup_save_clicked);
  lv_obj_set_width(save_btn, LV_PCT(100));
  lv_obj_set_height(save_btn, popup_layout::scale(76));
  lv_obj_t* save_btn_label = lv_obj_get_child(save_btn, 0);
  if (save_btn_label) {
    lv_obj_set_style_text_font(save_btn_label,
                               popup_layout::font28(), 0);
  }
}

// ===== System-Popup: Version/Geraet, GitHub-QR, Update-Suche + OTA-Install =====

static void system_set_buttons_enabled(bool enabled) {
  lv_obj_t* btns[] = {system_check_btn, system_github_btn, system_reboot_btn,
                      system_pair_btn};
  for (lv_obj_t* btn : btns) {
    if (!btn) continue;
    lv_obj_set_style_opa(btn, enabled ? LV_OPA_COVER : LV_OPA_50, 0);
    if (enabled) {
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    }
  }
}

static void system_show_status(const char* text, uint32_t color) {
  if (!system_status_label) return;
  lv_label_set_text(system_status_label, text);
  lv_obj_set_style_text_color(system_status_label, lv_color_hex(color), 0);
  lv_obj_clear_flag(system_status_label, LV_OBJ_FLAG_HIDDEN);
}

static void system_show_update_available_status(bool show_restart_note) {
  if (!system_status_label || !system_latest_tag[0]) return;
  char headline[64];
  snprintf(headline, sizeof(headline), tr().system_update_available_fmt,
           system_latest_tag);
  if (show_restart_note) {
    String text = String(headline) + "\n" + tr().system_update_restart_note;
    system_show_status(text.c_str(), 0x51CF66);
  } else {
    system_show_status(headline, 0x51CF66);
  }
}

// Der gruene Haupt-Button ist zweistufig: erst "Nach Updates suchen", nach
// einem positiven Check "Auf vX.Y.Z aktualisieren".
static void system_update_check_btn_text() {
  if (!system_check_btn_label) return;
  if (system_update_available && system_latest_tag[0]) {
    char buf[48];
    snprintf(buf, sizeof(buf), tr().system_install_btn_fmt, system_latest_tag);
    lv_label_set_text(system_check_btn_label, buf);
  } else {
    lv_label_set_text(system_check_btn_label, tr().system_check_updates_btn);
  }
}

// GitHub-QR ein-/ausblenden; waehrend der QR sichtbar ist, weichen ihm die
// Info-Zeilen, die Statuszeile und die normalen Aktionen. Groesse wird beim
// ersten Einblenden am freien Platz gemessen (gleicher Spacer-Trick wie beim AP-QR).
static void system_show_qr(bool show) {
#if LV_USE_QRCODE
  if (!system_qr) return;
  if (show) {
    if (system_info_rows) lv_obj_add_flag(system_info_rows, LV_OBJ_FLAG_HIDDEN);
    if (system_status_label) lv_obj_add_flag(system_status_label, LV_OBJ_FLAG_HIDDEN);
    if (system_check_btn) lv_obj_add_flag(system_check_btn, LV_OBJ_FLAG_HIDDEN);
    if (system_action_row) lv_obj_add_flag(system_action_row, LV_OBJ_FLAG_HIDDEN);
    if (!system_qr_sized) {
      if (settings_popup_content) lv_obj_update_layout(settings_popup_content);
      int target = popup_layout::scale(280);
      if (system_spacer) {
        target = lv_obj_get_height(system_spacer) -
                 popup_layout::scale(26);
      }
      const int max_w = lv_obj_get_content_width(lv_obj_get_parent(system_qr));
      if (target > max_w) target = max_w;
      if (target < popup_layout::scale(240)) {
        target = popup_layout::scale(240);
      }
      lv_qrcode_set_size(system_qr, target);
      lv_qrcode_update(system_qr, GithubUpdate::kRepoUrl, strlen(GithubUpdate::kRepoUrl));
      system_qr_sized = true;
    }
    lv_obj_clear_flag(system_qr, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(system_qr, LV_OBJ_FLAG_HIDDEN);
    if (system_info_rows) lv_obj_clear_flag(system_info_rows, LV_OBJ_FLAG_HIDDEN);
    if (system_status_label) lv_obj_clear_flag(system_status_label, LV_OBJ_FLAG_HIDDEN);
    if (system_check_btn) lv_obj_clear_flag(system_check_btn, LV_OBJ_FLAG_HIDDEN);
    if (system_action_row) lv_obj_clear_flag(system_action_row, LV_OBJ_FLAG_HIDDEN);
  }
#else
  (void)show;
#endif
}

static void on_system_github_clicked(lv_event_t*) {
  if (system_install_running) return;
#if LV_USE_QRCODE
  if (!system_qr) return;
  system_show_qr(lv_obj_has_flag(system_qr, LV_OBJ_FLAG_HIDDEN));
#endif
}

static void on_system_reboot_clicked(lv_event_t*) {
  if (system_check_running || system_install_running) return;
  if (!g_system_reboot_callback) return;
  system_show_qr(false);
  system_install_running = true;
  system_set_buttons_enabled(false);
  system_show_status(tr().system_restarting, 0xC8C8C8);
  g_system_reboot_callback();
}

static void on_system_check_clicked(lv_event_t*) {
  if (system_check_running || system_install_running) return;
  system_show_qr(false);  // Status/Fortschritt brauchen den Platz

  if (system_update_available && system_latest_tag[0]) {
    // Zweite Stufe: Install im Hauptloop anstossen. Der Sketch legt MQTT/
    // Web-Admin still, laedt das Release-Asset und startet bei Erfolg neu.
    if (!g_fw_install_callback) return;
    system_install_running = true;
    system_set_buttons_enabled(false);
    system_show_status(tr().system_downloading, 0xC8C8C8);
    if (system_progress_bar) {
      lv_bar_set_value(system_progress_bar, 0, LV_ANIM_OFF);
      lv_obj_clear_flag(system_progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
    g_fw_install_callback(system_latest_tag);
    return;
  }

  if (!g_fw_check_callback) return;
  system_check_running = true;
  system_set_buttons_enabled(false);
  system_show_status(tr().system_checking, 0xC8C8C8);
  g_fw_check_callback();
}

// "Pairing": erzwingt einen MQTT-Reconnect - die Post-Connect-Publishes
// (Status/Settings/Snapshot) lassen die HA-Bridge das Geraet neu anlegen.
// Gleicher Effekt wie das manuelle Neuspeichern des MQTT-Hosts, nur ohne
// Umweg ueber den Web-Admin.
static void on_system_pair_clicked(lv_event_t*) {
  if (system_check_running || system_install_running) return;
  if (!g_ha_pair_callback) return;
  system_show_qr(false);
  g_ha_pair_callback();
  system_show_status(tr().system_pair_status, 0x64B5F6);
}

// HomeTiles-Logo als kompiliertes Bild (hometiles_logo_dsc): eine 1:1-
// Rasterisierung von docs/images/logo.svg (144x144 ARGB8888, siehe
// release-helper/gen_logo.py). Eine fruehere Fassung baute das Logo aus
// reinen LVGL-Formen nach, traf aber weder die Kachel/Luecken-Proportionen
// noch die Plus-Form exakt -- hier wird nur noch auf die Zielgroesse skaliert.
static lv_obj_t* create_hometiles_logo_mark(lv_obj_t* parent, int32_t size) {
  lv_obj_t* img = lv_image_create(parent);
  // lv_image erbt vom Theme sonst Standard-Padding/Border (der "Rahmen", der
  // das Icon sichtbar weiter vom Text abrueckte, als die tatsaechlichen
  // Bild-Pixel es tun) -- exakt das Problem, das style_plain_container()
  // ueberall sonst in dieser Datei schon loest.
  style_plain_container(img);
  lv_image_set_src(img, &hometiles_logo_dsc);
  lv_image_set_antialias(img, true);
  const uint32_t zoom = static_cast<uint32_t>(
      (static_cast<int64_t>(size) * 256) / hometiles_logo_dsc.header.w);
  lv_image_set_scale(img, zoom);
  // LV_SIZE_CONTENT (der Default) bemisst sich bei lv_image immer an der
  // unskalierten Quellgroesse (hier 144x144), nicht am sichtbaren skalierten
  // Ergebnis -- das Objekt reservierte dadurch 144px im Layout, obwohl nur
  // ~size Pixel sichtbar sind, macht einen unsichtbaren Rand um das Icon.
  // Groesse deshalb explizit auf die tatsaechliche Zielgroesse setzen.
  lv_obj_set_size(img, size, size);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
  return img;
}

static void build_system_popup(lv_obj_t* parent) {
  lv_obj_t* box = lv_obj_create(parent);
  style_plain_container(box);
  lv_obj_set_width(box, LV_PCT(100));
  lv_obj_set_flex_grow(box, 1);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  // Gleiche Anmutung wie das Display-Formular: fester Abstand von oben
  lv_obj_set_style_pad_top(box, popup_layout::scale(48), 0);
  lv_obj_set_style_pad_row(box, popup_layout::scale(18), 0);

  // Marke oben im Popup: Icon links, rechts daneben Produktname mit der
  // Version klein darunter -- wie ein App-"Ueber"-Screen.
  lv_obj_t* brand = lv_obj_create(box);
  style_plain_container(brand);
  lv_obj_clear_flag(brand, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(brand, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brand, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(brand, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(brand, popup_layout::scale(18), 0);
  create_hometiles_logo_mark(brand, popup_layout::scale(100));

  lv_obj_t* brand_text = lv_obj_create(brand);
  style_plain_container(brand_text);
  lv_obj_clear_flag(brand_text, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(brand_text, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(brand_text, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(brand_text, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(brand_text, popup_layout::scale(2), 0);

  lv_obj_t* brand_title = lv_label_create(brand_text);
  lv_label_set_text(brand_title, "HomeTiles");
  lv_obj_set_style_text_font(brand_title, popup_layout::font40(), 0);
  lv_obj_set_style_text_color(brand_title, lv_color_white(), 0);

  lv_obj_t* version_caption = lv_label_create(brand_text);
  lv_label_set_text(version_caption, FW_VERSION);
  lv_obj_set_style_text_font(version_caption, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(version_caption, lv_color_hex(0xA8A8A8), 0);

  // Geraetename allein, ohne "Device:"-Label -- gleiche Optik wie die
  // Version-Caption darueber (Name wechselt eh nie waehrend das Popup offen ist).
  system_info_rows = lv_label_create(box);
  lv_label_set_text(system_info_rows, Device::displayName());
  lv_obj_set_style_text_font(system_info_rows, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(system_info_rows, lv_color_hex(0xA8A8A8), 0);

#if LV_USE_QRCODE
  system_qr = lv_qrcode_create(box);
  // Etwas mehr Luft zur Marke darueber als der uniforme Zeilenabstand (18) -
  // margin (nicht pad!) schiebt das Element weg, ohne seine eigene Box (den
  // weissen QR-Rahmen) aufzublaehen. QR-Groesse haengt vom Rest-Platz ab
  // (siehe system_show_qr), passt sich also automatisch an und bleibt auch
  // auf 720p-Geraeten im Rahmen.
  lv_obj_set_style_margin_top(system_qr, popup_layout::scale(14), 0);
#if defined(DEVICE_LAYOUT_480X480)
  // Leave enough vertical room for the GitHub action on the compact square
  // layout without changing the QR size on any hardware profile.
  lv_qrcode_set_size(system_qr, popup_layout::scale480(264));
#else
  lv_qrcode_set_size(system_qr, popup_layout::scale(280));
#endif
  lv_qrcode_set_dark_color(system_qr, lv_color_black());
  lv_qrcode_set_light_color(system_qr, lv_color_white());
  lv_qrcode_set_quiet_zone(system_qr, true);
  style_qr_code(system_qr);
  lv_qrcode_update(system_qr, GithubUpdate::kRepoUrl, strlen(GithubUpdate::kRepoUrl));
  system_qr_sized = true;
  lv_obj_add_flag(system_qr, LV_OBJ_FLAG_HIDDEN);
#endif

  // Drueckt Status/Fortschritt+Buttons nach unten; dient beim QR-Einblenden
  // als Platz-Mass.
  system_spacer = create_flex_spacer(box);

  // Status/Fortschritt sitzen direkt ueber dem Update-Button, nicht oben
  // beim Geraet -- das ist die Stelle, an der beim Klick auf "Nach Updates
  // suchen" tatsaechlich etwas passiert.
  system_status_label = lv_label_create(box);
  lv_obj_set_width(system_status_label, LV_PCT(100));
  lv_label_set_long_mode(system_status_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(system_status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(system_status_label, popup_layout::font24(), 0);
  lv_obj_set_style_text_color(system_status_label, lv_color_hex(0xC8C8C8), 0);
  if (system_update_available && system_latest_tag[0]) {
    // Bekanntes Check-Ergebnis samt Neustart-Hinweis beim erneuten Oeffnen
    // weiter anzeigen. Vor dem ersten erfolgreichen Check bleibt die Zeile leer.
    system_show_update_available_status(true);
  } else {
    lv_label_set_text(system_status_label, "");
  }

  system_progress_bar = lv_bar_create(box);
  lv_obj_set_size(system_progress_bar, LV_PCT(100),
                  popup_layout::scale(18));
  lv_obj_set_style_bg_color(system_progress_bar, lv_color_hex(0x1E1E1E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(system_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(system_progress_bar,
                          popup_layout::scale(9), LV_PART_MAIN);
  lv_obj_set_style_bg_color(system_progress_bar, lv_color_hex(0x43A047), LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(system_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(system_progress_bar,
                          popup_layout::scale(9), LV_PART_INDICATOR);
  lv_bar_set_range(system_progress_bar, 0, 100);
  lv_obj_add_flag(system_progress_bar, LV_OBJ_FLAG_HIDDEN);

  // Gruen = positive "Los"-Aktion (wie WLAN-Verbinden und Speichern)
  system_check_btn = create_popup_button(box, "", 0x2E7D32, on_system_check_clicked);
  lv_obj_set_width(system_check_btn, LV_PCT(100));
  lv_obj_set_height(system_check_btn, popup_layout::scale(76));
  system_check_btn_label = lv_obj_get_child(system_check_btn, 0);
  if (system_check_btn_label) {
    lv_obj_set_style_text_font(system_check_btn_label,
                               popup_layout::font28(), 0);
  }
  system_update_check_btn_text();

  // Restart (neutral grau - rot ist fuers Loeschen reserviert) + Pairing
  // (blau, Sync-Aktion) teilen sich eine Reihe unter dem Update-Button.
  system_action_row = lv_obj_create(box);
  style_plain_container(system_action_row);
  lv_obj_clear_flag(system_action_row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(system_action_row, LV_PCT(100));
  lv_obj_set_height(system_action_row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(system_action_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(system_action_row,
                              popup_layout::scale(12), 0);

  // Ohne Icon - der Web-Admin-Neustart-Button hat auch keins.
  system_reboot_btn = create_popup_button(system_action_row, tr().restart_button,
                                          0x424242, on_system_reboot_clicked);
  lv_obj_set_flex_grow(system_reboot_btn, 1);
  lv_obj_set_height(system_reboot_btn, popup_layout::scale(76));
  lv_obj_t* reboot_text = lv_obj_get_child(system_reboot_btn, 0);
  if (reboot_text) {
    lv_obj_set_style_text_font(reboot_text, popup_layout::font28(), 0);
  }

  system_pair_btn = create_popup_button(system_action_row, "", 0x1E88E5,
                                        on_system_pair_clicked);
  lv_obj_set_flex_grow(system_pair_btn, 1);
  lv_obj_set_height(system_pair_btn, popup_layout::scale(76));
  lv_obj_set_flex_flow(system_pair_btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(system_pair_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(system_pair_btn,
                              popup_layout::scale(14), 0);
  lv_obj_t* pair_icon = lv_obj_get_child(system_pair_btn, 0);
  if (pair_icon) {
    lv_label_set_text(pair_icon, getMdiChar("home-assistant").c_str());
    if (FONT_MDI_ICONS) lv_obj_set_style_text_font(pair_icon, FONT_MDI_ICONS, 0);
    popup_layout::applyIconScale(pair_icon);
  }
  lv_obj_t* pair_text = lv_label_create(system_pair_btn);
  lv_label_set_text(pair_text, tr().system_pair_btn);
  lv_label_set_long_mode(pair_text, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(pair_text, popup_layout::font28(), 0);
  lv_obj_set_style_text_color(pair_text, lv_color_white(), 0);

  // GitHub-Button: Icon + Schriftzug im Button (wie der Drehen-Button)
  system_github_btn = create_popup_button(box, "", 0x424242, on_system_github_clicked);
  lv_obj_set_width(system_github_btn, LV_PCT(100));
  lv_obj_set_height(system_github_btn, popup_layout::scale(76));
  lv_obj_set_flex_flow(system_github_btn, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(system_github_btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(system_github_btn,
                              popup_layout::scale(14), 0);
  lv_obj_t* gh_icon = lv_obj_get_child(system_github_btn, 0);
  if (gh_icon) {
    lv_label_set_text(gh_icon, getMdiChar("github").c_str());
    if (FONT_MDI_ICONS) lv_obj_set_style_text_font(gh_icon, FONT_MDI_ICONS, 0);
    popup_layout::applyIconScale(gh_icon);
  }
  lv_obj_t* gh_text = lv_label_create(system_github_btn);
  lv_label_set_text(gh_text, "GitHub");
  lv_obj_set_style_text_font(gh_text, popup_layout::font28(), 0);
  lv_obj_set_style_text_color(gh_text, lv_color_white(), 0);
}

static const char* popup_title_for_kind(SettingsPopupKind kind) {
  switch (kind) {
    case SettingsPopupKind::Display:
      return tr().display_label;
    case SettingsPopupKind::Wifi:
      return tr().wifi_label;
    case SettingsPopupKind::Localization:
      return tr().admin_settings_language;
    case SettingsPopupKind::Firmware:
      return "System";
  }
  return "";
}

static const char* popup_icon_for_kind(SettingsPopupKind kind) {
  switch (kind) {
    case SettingsPopupKind::Display:
      return "monitor";
    case SettingsPopupKind::Wifi:
      return "wifi";
    case SettingsPopupKind::Localization:
      return "translate";
    case SettingsPopupKind::Firmware:
      return "chip";
  }
  return "cog";
}

static void build_popup_content(SettingsPopupKind kind, lv_obj_t* parent) {
  switch (kind) {
    case SettingsPopupKind::Display:
      build_display_popup(parent);
      break;
    case SettingsPopupKind::Wifi:
      build_wifi_popup(parent);
      break;
    case SettingsPopupKind::Localization:
      build_localization_popup(parent);
      break;
    case SettingsPopupKind::Firmware:
      build_system_popup(parent);
      break;
  }
}

static void open_settings_popup(SettingsPopupKind kind) {
  if (settings_popup_overlay) return;
  reset_popup_refs();
  settings_popup_kind = kind;

  settings_popup_overlay = lv_obj_create(lv_screen_active());
  lv_obj_set_size(settings_popup_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(settings_popup_overlay, 0, 0);
  lv_obj_set_style_bg_color(settings_popup_overlay, lv_color_hex(0x0A0A0A), 0);
  lv_obj_set_style_bg_opa(settings_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(settings_popup_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settings_popup_overlay, 0, 0);
  lv_obj_set_style_radius(settings_popup_overlay, 0, 0);
  lv_obj_set_style_pad_all(settings_popup_overlay, Device::kGridPad, 0);
  lv_obj_clear_flag(settings_popup_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(settings_popup_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(settings_popup_overlay, LV_OBJ_FLAG_FLOATING);

  settings_popup_card = lv_obj_create(settings_popup_overlay);
  lv_obj_set_size(settings_popup_card, LV_PCT(100), LV_PCT(100));
  lv_obj_center(settings_popup_card);
  lv_obj_set_style_bg_color(settings_popup_card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_border_opa(settings_popup_card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settings_popup_card, 0, 0);
  ui_surface_style::apply_global_tile_border(settings_popup_card);
  lv_obj_set_style_radius(settings_popup_card, popup_layout::scale(22), 0);
  lv_obj_set_style_clip_corner(settings_popup_card, false, 0);
  lv_obj_set_style_pad_all(settings_popup_card, kPopupCardPad, 0);
  lv_obj_set_style_pad_row(settings_popup_card, popup_layout::scale(8), 0);
  lv_obj_clear_flag(settings_popup_card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(settings_popup_card, LV_FLEX_FLOW_COLUMN);
  // Querachse CENTER: zentriert den (auf Lesebreite begrenzten) Content auf
  // den 1280 Pixel breiten Geraeten; vollbreite Kinder (Header) unbeeindruckt.
  lv_obj_set_flex_align(settings_popup_card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  lv_obj_t* header = lv_obj_create(settings_popup_card);
  lv_obj_set_width(header, LV_PCT(100));
  // Hoehe so gewaehlt, dass LEFT_MID-Kinder (Icon/Titel) exakt auf der
  // Mittelachse der Speichern/X-Buttons landen (beide Buttons zentrieren
  // sich bei content-y=42; siehe deren align-Offsets weiter unten).
  lv_obj_set_height(header, popup_layout::scale(84));
  style_plain_container(header);

  lv_obj_t* header_icon = lv_label_create(header);
  lv_label_set_text(header_icon, getMdiChar(popup_icon_for_kind(kind)).c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(header_icon, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(header_icon);
  lv_obj_set_style_text_color(header_icon, lv_color_white(), 0);
  lv_obj_align(header_icon, LV_ALIGN_LEFT_MID,
               popup_layout::kHeaderIconX, 0);

  // Kein Speichern oben rechts mehr: WLAN hat den Verbinden-Button und
  // Lokalisierung den Speichern-Button jeweils unten im Inhalt.
  settings_popup_title = lv_label_create(header);
  lv_label_set_text(settings_popup_title, popup_title_for_kind(kind));
  lv_label_set_long_mode(settings_popup_title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(settings_popup_title, LV_PCT(62));
  // Header-Titel und Icon folgen denselben kompakten Maßen wie die Tiles.
  lv_obj_set_style_text_font(settings_popup_title,
                              popup_layout::headerTitleFont(), 0);
  lv_obj_set_style_text_color(settings_popup_title, lv_color_white(), 0);
  lv_obj_align(settings_popup_title, LV_ALIGN_LEFT_MID,
               popup_layout::kHeaderTitleX, 0);

  lv_obj_t* close_btn = lv_button_create(settings_popup_card);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_size(close_btn, popup_layout::kCloseButtonSize,
                   popup_layout::kCloseButtonSize);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close_btn, popup_layout::kCloseButtonRadius, 0);
  lv_obj_set_style_pad_all(close_btn, 0, 0);
  lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT,
                popup_layout::kCloseButtonOffsetX,
                popup_layout::kCloseButtonOffsetY);
  lv_obj_set_ext_click_area(close_btn, popup_layout::kCloseButtonClickArea);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(close_btn, on_settings_popup_close_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* close_label = lv_label_create(close_btn);
  lv_obj_set_style_text_font(close_label, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(close_label);
  lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);
  settings_popup_close_icon = close_label;

  settings_popup_content = lv_obj_create(settings_popup_card);
  lv_obj_set_width(settings_popup_content, LV_PCT(100));
  // Lesebreite: auf den 1280 Pixel breiten Geraeten (8-Zoll/Tab5) waren
  // Dropdowns/Felder/Listenzeilen absurd breit. Deckel entspricht der
  // Content-Breite des B4 (720er: 648px -> dort greift der Deckel nie).
  // Nur der Inhalt - Header und Tastatur bleiben volle Kartenbreite.
  lv_obj_set_style_max_width(settings_popup_content,
                             popup_layout::scale(660), 0);
  lv_obj_set_flex_grow(settings_popup_content, 1);
  style_plain_container(settings_popup_content);
  lv_obj_set_style_pad_all(settings_popup_content, 0, 0);
  lv_obj_set_style_pad_row(settings_popup_content,
                           popup_layout::scale(8), 0);
  // Luft zum Header: der X/Zurueck-Button (96px + ext_click_area) ragt unter
  // die Header-Kante - ohne Abstand ueberlappt sein Press-Highlight den
  // Inhalt oben rechts.
  lv_obj_set_style_pad_top(settings_popup_content,
                           popup_layout::scale(14), 0);
  lv_obj_set_flex_flow(settings_popup_content, LV_FLEX_FLOW_COLUMN);
  // Querachse CENTER fuer die schmalen Aktions-Buttons (Speichern etc.);
  // vollbreite Kinder bleiben vollbreit.
  lv_obj_set_flex_align(settings_popup_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
  build_popup_content(settings_popup_kind, settings_popup_content);

  lv_obj_move_foreground(settings_popup_overlay);
  lv_obj_invalidate(settings_popup_overlay);

  if (lv_display_t* disp = lv_display_get_default()) {
    lv_refr_now(disp);
  }
}

static void on_settings_tile_clicked(lv_event_t* e) {
  const uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(e));
  finish_press_before_popup(e);
  open_settings_popup(static_cast<SettingsPopupKind>(static_cast<uint8_t>(raw)));
}

// Kachel im 3x1-Layout: links das Icon+Titel-Feld (eine Zelle), rechts eine
// kurze, lokalisierte Beschreibung dessen, was sich dahinter verbirgt.
static lv_obj_t* create_settings_menu_tile(lv_obj_t* parent, uint8_t col, uint8_t row,
                                           const char* icon_name, const char* title,
                                           lv_obj_t** title_label_out,
                                           lv_obj_t** summary_label_out,
                                           SettingsPopupKind kind) {
  lv_obj_t* tile = lv_button_create(parent);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_grid_cell(tile, LV_GRID_ALIGN_STRETCH, col, 3, LV_GRID_ALIGN_STRETCH, row, 1);
  style_settings_button(tile, 0x2A2A2A);
  lv_obj_set_style_radius(tile, 22, 0);
  lv_obj_set_style_border_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(tile, 0, 0);
  lv_obj_set_style_pad_column(tile, 0, 0);
  ui_surface_style::apply_global_tile_border(tile);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_event_cb(tile, on_settings_tile_clicked, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(
                          static_cast<uintptr_t>(static_cast<uint8_t>(kind))));

  lv_obj_t* face = lv_obj_create(tile);
  style_plain_container(face);
  lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(face, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_width(face, GRID_CELL_W);
  lv_obj_set_height(face, LV_PCT(100));
  lv_obj_set_style_pad_all(face, 0, 0);

  // Icon alleine mittig im linken Feld - Titel wandert nach rechts ueber die
  // Beschreibung (klassischer Listeneintrag: Icon | Titel + Untertext).
  lv_obj_t* icon = lv_label_create(face);
  lv_label_set_text(icon, getMdiChar(icon_name).c_str());
  if (FONT_MDI_ICONS) lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(icon);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_center(icon);

  lv_obj_t* info = lv_obj_create(tile);
  style_plain_container(info);
  lv_obj_clear_flag(info, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(info, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_height(info, LV_PCT(100));
  lv_obj_set_flex_grow(info, 1);
  lv_obj_set_style_pad_right(info, popup_layout::scale(16), 0);
  lv_obj_set_style_pad_ver(info, popup_layout::scale(8), 0);
  lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(info, popup_layout::scale(6), 0);

  lv_obj_t* title_label = lv_label_create(info);
  lv_label_set_text(title_label, title);
  lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_width(title_label, LV_PCT(100));
  lv_obj_set_style_text_font(title_label, popup_layout::font28(), 0);
  lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
  if (title_label_out) *title_label_out = title_label;

  if (summary_label_out) {
    lv_obj_t* summary = lv_label_create(info);
    lv_label_set_text(summary, "");
    lv_label_set_long_mode(summary, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(summary, LV_PCT(100));
    lv_obj_set_style_text_align(summary, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(summary, popup_layout::font20(), 0);
    lv_obj_set_style_text_color(summary, lv_color_hex(0xA8A8A8), 0);
    *summary_label_out = summary;
  }
  return tile;
}

// Kacheln zeigen eine kurze, lokalisierte Beschreibung ihres Inhalts statt
// dynamischer Statuswerte (die stehen in den jeweiligen Popups).
static void update_settings_tile_summaries() {
  if (settings_tile_display_summary) {
    lv_label_set_text(settings_tile_display_summary, tr().settings_tile_desc_display);
  }
  if (settings_tile_wifi_summary) {
    lv_label_set_text(settings_tile_wifi_summary, tr().settings_tile_desc_wifi);
  }
  if (settings_tile_locale_summary) {
    lv_label_set_text(settings_tile_locale_summary, tr().settings_tile_desc_locale);
  }
  if (settings_tile_firmware_summary) {
    static char buf[48];
    snprintf(buf, sizeof(buf), tr().settings_tile_desc_firmware_fmt, FW_VERSION);
    lv_label_set_text(settings_tile_firmware_summary, buf);
  }
}

static void build_grid_track_descriptors(lv_coord_t* dsc, uint8_t count, lv_coord_t cell_size) {
  if (!dsc) return;
  for (uint8_t i = 0; i < count; ++i) {
    dsc[i] = cell_size;
  }
  dsc[count] = LV_GRID_TEMPLATE_LAST;
}

// ========== Public API ==========
void settings_set_wifi_reconnect_callback(wifi_reconnect_callback_t cb) {
  g_wifi_reconnect_callback = cb;
}

void settings_set_fw_check_callback(fw_check_callback_t cb) {
  g_fw_check_callback = cb;
}

void settings_set_fw_install_callback(fw_install_callback_t cb) {
  g_fw_install_callback = cb;
}

void settings_set_system_reboot_callback(system_reboot_callback_t cb) {
  g_system_reboot_callback = cb;
}

void settings_set_wifi_disconnect_callback(wifi_disconnect_callback_t cb) {
  g_wifi_disconnect_callback = cb;
}

void settings_set_ha_pair_callback(ha_pair_callback_t cb) {
  g_ha_pair_callback = cb;
}

// Alle vier Rueckmeldungen laufen auf dem Loop-Task (LVGL-Owner) und
// tolerieren ein inzwischen geschlossenes Popup (Statics sind dann genullt,
// die Helper pruefen selbst).
void settings_fw_check_result(bool ok, const char* latest_tag, bool update_available) {
  system_check_running = false;
  system_update_available = ok && update_available;
  if (ok && latest_tag && latest_tag[0]) {
    snprintf(system_latest_tag, sizeof(system_latest_tag), "%s", latest_tag);
  }
  system_set_buttons_enabled(true);
  system_update_check_btn_text();
  if (!system_status_label) return;
  if (!ok) {
    system_show_status(tr().system_check_failed, 0xFF6B6B);
  } else if (system_update_available) {
    // Der Hinweis bleibt zusammen mit dem bekannten Check-Ergebnis erhalten
    // und wird beim erneuten Oeffnen des System-Popups wieder angezeigt.
    system_show_update_available_status(true);
  } else {
    system_show_status(tr().system_up_to_date, 0x51CF66);
  }
}

void settings_fw_install_progress(size_t written, size_t total) {
  if (!system_progress_bar || total == 0) return;
  lv_bar_set_value(system_progress_bar,
                   static_cast<int32_t>((written * 100ULL) / total), LV_ANIM_OFF);
}

void settings_fw_install_done() {
  system_install_running = false;
  if (system_progress_bar) lv_bar_set_value(system_progress_bar, 100, LV_ANIM_OFF);
  system_show_status(tr().system_installed_restarting, 0x51CF66);
}

void settings_fw_install_failed(const char* error) {
  system_install_running = false;
  system_set_buttons_enabled(true);
  if (system_progress_bar) lv_obj_add_flag(system_progress_bar, LV_OBJ_FLAG_HIDDEN);
  if (!system_status_label) return;
  char buf[96];
  if (error && error[0]) {
    snprintf(buf, sizeof(buf), "%s (%s)", tr().system_install_failed, error);
  } else {
    snprintf(buf, sizeof(buf), "%s", tr().system_install_failed);
  }
  system_show_status(buf, 0xFF6B6B);
}

void build_settings_tab(lv_obj_t *tab, hotspot_callback_t hotspot_cb) {
  g_hotspot_callback = hotspot_cb;
  ap_mode_confirm_pending = false;
  clear_ap_confirm_timer();
  if (settings_popup_overlay) close_settings_popup();

  lv_obj_clean(tab);
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(tab, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
  lv_obj_set_style_border_opa(tab, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(tab, GRID_PAD, 0);

  // 4x4 Grid
  static lv_coord_t col_dsc[GRID_COLS + 1];
  static lv_coord_t row_dsc[GRID_ROWS + 1];
  static bool dsc_ready = false;
  if (!dsc_ready) {
    build_grid_track_descriptors(col_dsc, GRID_COLS, GRID_CELL_W);
    build_grid_track_descriptors(row_dsc, GRID_ROWS, GRID_CELL_H);
    dsc_ready = true;
  }
  lv_obj_set_layout(tab, LV_LAYOUT_GRID);
  lv_obj_set_grid_dsc_array(tab, col_dsc, row_dsc);
  lv_obj_set_style_pad_column(tab, GRID_GAP, 0);
  lv_obj_set_style_pad_row(tab, GRID_GAP, 0);

  const DeviceConfig& cfg = configManager.getConfig();
  display_rotated_180 = cfg.display_rotated_180;
  display_rotation_quarters = Device::normalizeRotationQuarterTurns(cfg.display_rotation_quarters);
  display_rotation_mode = cfg.display_rotation_mode;
  wake_mode_mains = kWakeModeTouch;
  wake_mode_battery = kWakeModeTouch;

  display_section_label = nullptr;
  brightness_title_label = nullptr;
  screensaver_brightness_title_label = nullptr;
  wifi_section_label = nullptr;
  sleep_section_label = nullptr;
  sleep_label = nullptr;
  brightness_label = nullptr;
  screensaver_brightness_value_label = nullptr;
  display_rotate_btn = nullptr;
  display_rotate_label = nullptr;
  display_rotate_sub_label = nullptr;
  sleep_slider = nullptr;
  sleep_time_label = nullptr;
  screensaver_slider = nullptr;
  screensaver_time_label = nullptr;
  screensaver_label = nullptr;
  screensaver_brightness_slider = nullptr;
  ap_mode_btn = nullptr;
  ap_mode_btn_label = nullptr;
  wifi_disconnect_btn = nullptr;
  net_mode_btn = nullptr;
  net_mode_btn_label = nullptr;
  net_mode_hint_label = nullptr;
  ip_mode_hint_label = nullptr;
  ap_confirm_row = nullptr;
  ap_confirm_yes_btn = nullptr;
  ap_confirm_no_btn = nullptr;
  ap_yes_label_obj = nullptr;
  ap_no_label_obj = nullptr;

  create_settings_back_button(tab);

  // Alle vier Kacheln (3 Zellen breit) vertikal untereinander, ab Zeile 0:
  // auf dem 4-Spalten-Grid (B4) direkt rechts neben dem Back-Button, auf
  // breiteren Grids (8-Zoll/Tab5, 7 Spalten) horizontal mittig.
  static constexpr uint8_t kSettingsMenuTileSpanW = 3;
  const uint8_t tile_col = (GRID_COLS <= kSettingsMenuTileSpanW + 1)
                               ? GRID_COLS - kSettingsMenuTileSpanW
                               : (GRID_COLS - kSettingsMenuTileSpanW) / 2;

  create_settings_menu_tile(tab, tile_col, 0, "monitor", tr().display_label,
                            &settings_tile_display_title,
                            &settings_tile_display_summary,
                            SettingsPopupKind::Display);

  create_settings_menu_tile(tab, tile_col, 1, "wifi", tr().wifi_label,
                            &settings_tile_wifi_title,
                            &settings_tile_wifi_summary,
                            SettingsPopupKind::Wifi);

  create_settings_menu_tile(tab, tile_col, 2, "translate", tr().admin_settings_language,
                            &settings_tile_locale_title,
                            &settings_tile_locale_summary,
                            SettingsPopupKind::Localization);

  create_settings_menu_tile(tab, tile_col, 3, "chip", "System",
                            &settings_tile_firmware_title,
                            &settings_tile_firmware_summary,
                            SettingsPopupKind::Firmware);

  mains_wake_btn = nullptr;
  mains_wake_label = nullptr;
  mains_wake_sub_label = nullptr;
  battery_wake_btn = nullptr;
  battery_wake_label = nullptr;
  battery_wake_sub_label = nullptr;
  power_status_label = nullptr;
  power_level_label = nullptr;
  battery_icon_label = nullptr;
  battery_percent_label = nullptr;
  update_settings_tile_summaries();
}

// Die Kacheln zeigen keine Live-Statuswerte mehr (nur Beschreibungen) - die
// Aufrufe aus dem Hauptloop halten aber die Infobox im offenen WLAN-Popup
// aktuell (z.B. wird der Auto-Reconnect nach "AP beenden" sofort sichtbar).
void settings_update_wifi_status(bool, const char*, const char*) {
  wifi_update_conn_status_label();
}

void settings_update_wifi_status_ap(const char*, const char*) {
  wifi_update_conn_status_label();
}

void settings_update_ap_mode(bool running) {
  ap_mode_active = running;
  if (!ap_mode_btn_label && ap_mode_btn) {
    ap_mode_btn_label = lv_obj_get_child(ap_mode_btn, 0);
  }
  if (ap_mode_btn_label) {
    lv_label_set_text(ap_mode_btn_label, running ? tr().ap_disable : tr().ap_enable);
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
  // Statuszeile im WLAN-Popup sofort nachziehen (AP aktiv + IP bzw. wieder
  // Verbunden/Offline) - vorher gab der Toggle dort kein Feedback.
  wifi_update_conn_status_label();
}

void settings_refresh_language() {
  const auto& s = tr();

  if (settings_tile_display_title) lv_label_set_text(settings_tile_display_title, s.display_label);
  if (settings_tile_wifi_title) lv_label_set_text(settings_tile_wifi_title, s.wifi_label);
  if (settings_tile_locale_title) lv_label_set_text(settings_tile_locale_title, s.admin_settings_language);
  if (settings_tile_firmware_title) lv_label_set_text(settings_tile_firmware_title, "System");
  if (settings_popup_title) lv_label_set_text(settings_popup_title, popup_title_for_kind(settings_popup_kind));
  if (display_section_label) lv_label_set_text(display_section_label, s.display_label);
  if (brightness_title_label) lv_label_set_text(brightness_title_label, s.brightness_label);
  if (screensaver_brightness_title_label) {
    lv_label_set_text(screensaver_brightness_title_label,
                      s.screensaver_brightness_label);
  }
  if (display_rotate_text_label) lv_label_set_text(display_rotate_text_label, s.display_rotate_btn_text);
  if (wifi_section_label) lv_label_set_text(wifi_section_label, s.wifi_label);
  if (sleep_section_label) lv_label_set_text(sleep_section_label, s.sleep_label);
  if (sleep_label) lv_label_set_text(sleep_label, s.sleep_label);
  if (screensaver_label) lv_label_set_text(screensaver_label, s.screensaver_label);
  if (ap_yes_label_obj) lv_label_set_text(ap_yes_label_obj, s.yes);
  if (ap_no_label_obj) lv_label_set_text(ap_no_label_obj, s.no);

  update_wake_button(mains_wake_label, mains_wake_sub_label, kWakeModeTouch);
  update_wake_button(battery_wake_label, battery_wake_sub_label, kWakeModeTouch);
  // Aktualisiert auch die Infobox im ggf. offenen WLAN-Popup
  settings_update_ap_mode(ap_mode_active);
  if (ethernet_dhcp_btn) net_mode_update_ui();
  update_settings_tile_summaries();

  // Web-Admin-Speichern kann gleichzeitig Sichtbarkeit, Text und Hoehe der
  // Statuszeilen sowie die beiden unteren Buttons aendern. Das bestehende
  // Flex-Layout dabei nur stueckweise umzubauen hinterliess auf dem Display
  // alte Bildbereiche. Deshalb das offene WLAN-Popup ohne Zwischen-Refresh
  // komplett neu erzeugen; open_settings_popup zeichnet erst den fertigen
  // Zustand.
  if (settings_popup_kind == SettingsPopupKind::Wifi &&
      settings_popup_overlay) {
    close_settings_popup();
    open_settings_popup(SettingsPopupKind::Wifi);
  }

  weather_popup_refresh_language();
}

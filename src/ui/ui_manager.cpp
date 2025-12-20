#include "src/ui/ui_manager.h"

#include "src/ui/tab_tiles_unified.h"
#include "src/ui/tab_settings.h"
#include "src/core/display_manager.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "font_roboto_mono_digits_48.h"
#include "font_roboto_mono_digits_24.h"
#include <WiFi.h>

#include <time.h>

#include <M5Unified.h>



// Globale Instanz

UIManager uiManager;



// Timezone

const char* UIManager::TZ_EUROPE_BERLIN = "CET-1CEST,M3.5.0/02,M10.5.0/03";



// ========== UI aufbauen ==========
void UIManager::buildUI(scene_publish_cb_t scene_cb, hotspot_start_cb_t hotspot_cb) {
  Serial.println("[UI] Baue UI auf...");

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  static const int TABBAR_MARGIN = 22;
  static const int SIDEBAR_WIDTH = 180;

  for (uint8_t i = 0; i < TAB_COUNT; ++i) {
    tab_panels[i] = nullptr;
    tab_buttons[i] = nullptr;
    tab_labels[i] = nullptr;
  }
  active_tab_index = UINT8_MAX;
  nav_container = nullptr;
  tab_content_container = nullptr;

  lv_obj_t *sidebar = lv_obj_create(scr);
  lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(sidebar, 34, 0);
  lv_obj_set_style_border_width(sidebar, 0, 0);
  lv_obj_set_style_shadow_width(sidebar, 0, 0);
  lv_obj_set_style_clip_corner(sidebar, true, 0);
  lv_obj_set_width(sidebar, SIDEBAR_WIDTH);
  lv_obj_set_height(sidebar, 720);
  lv_obj_set_style_pad_top(sidebar, TABBAR_MARGIN, 0);
  lv_obj_set_style_pad_bottom(sidebar, 6, 0);
  lv_obj_set_style_pad_left(sidebar, 6, 0);
  lv_obj_set_style_pad_right(sidebar, 6, 0);
  lv_obj_set_style_pad_gap(sidebar, 16, 0);
  lv_obj_set_pos(sidebar, 0, 0);
  lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

  statusbarInit(sidebar);

  nav_container = lv_obj_create(sidebar);
  lv_obj_set_size(nav_container, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(nav_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(nav_container, 0, 0);
  lv_obj_set_style_pad_left(nav_container, 4, 0);
  lv_obj_set_style_pad_right(nav_container, 4, 0);
  lv_obj_set_style_pad_top(nav_container, 8, 0);  // Mehr Abstand oben, damit Button nicht abgeschnitten wird
  lv_obj_set_style_pad_bottom(nav_container, 4, 0);
  lv_obj_set_style_pad_row(nav_container, 12, 0);
  lv_obj_set_flex_flow(nav_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(nav_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_flex_grow(nav_container, 1);
  lv_obj_clear_flag(nav_container, LV_OBJ_FLAG_SCROLLABLE);

  // Tab-Konfiguration aus tileConfig holen
  for (uint8_t i = 0; i < TAB_COUNT; ++i) {
    lv_obj_t *btn = lv_button_create(nav_container);

    // Alle Tabs (0-3) sind konfigurierbar
    const char* tabName = tileConfig.getTabName(i);
    const char* iconName = tileConfig.getTabIcon(i);

    // Fallback für Tab 3: "Settings" nur wenn BEIDE leer
    if (i == 3 && (!tabName || strlen(tabName) == 0) && (!iconName || strlen(iconName) == 0)) {
      tab_labels[i] = setupTabButton(btn, i, "", "Settings");
    } else {
      tab_labels[i] = setupTabButton(btn, i, iconName, tabName);
    }

    lv_obj_set_flex_grow(btn, 1);
    lv_obj_add_event_cb(btn, nav_button_event_cb, LV_EVENT_CLICKED, this);
    tab_buttons[i] = btn;
    tab_button_overlays[i] = nullptr;  // Nicht verwendet
  }

  tab_content_container = lv_obj_create(scr);
  lv_obj_set_style_bg_color(tab_content_container, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(tab_content_container, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(tab_content_container, 0, 0);
  lv_obj_set_style_pad_all(tab_content_container, 0, 0);
  lv_obj_clear_flag(tab_content_container, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(tab_content_container, 1280 - SIDEBAR_WIDTH, 720);
  lv_obj_set_pos(tab_content_container, SIDEBAR_WIDTH, 0);

  tab_panels[0] = createTabPanel(tab_content_container);
  tab_panels[1] = createTabPanel(tab_content_container);
  tab_panels[2] = createTabPanel(tab_content_container);
  tab_panels[3] = createTabPanel(tab_content_container);

  // SIDEBAR ZUERST RENDERN (vor Tiles!)
  Serial.println("[UI] Rendere Sidebar...");
  lv_timer_handler();          // Sidebar zeichnen (mit Platzhalter --:--)
  delay(100);                  // Sicherstellen dass Sidebar fertig ist
  Serial.println("[UI] Sidebar fertig, lade nun Tiles...");
  // updateStatusbar() wird später in Loop aufgerufen wenn Fonts geladen sind

  build_tiles_tab(tab_panels[0], GridType::TAB0, scene_cb);
  build_tiles_tab(tab_panels[1], GridType::TAB1, scene_cb);
  build_tiles_tab(tab_panels[2], GridType::TAB2, scene_cb);
  build_settings_tab(tab_panels[3], hotspot_cb);
  for (uint8_t i = 0; i < TAB_COUNT; ++i) {
    if (tab_panels[i]) {
      lv_obj_add_flag(tab_panels[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
  switchToTab(0);

  Serial.println("[UI] UI aufgebaut");
}

// ========== Statusbar initialisieren ==========

void UIManager::statusbarInit(lv_obj_t *tab_bar) {
  if (status_container || !tab_bar) return;

  status_container = lv_obj_create(tab_bar);
  lv_obj_set_size(status_container, LV_PCT(100), LV_SIZE_CONTENT);

  lv_obj_set_style_bg_color(status_container, lv_color_hex(0x2A2A2A), 0);

  lv_obj_set_style_bg_opa(status_container, LV_OPA_COVER, 0);

  lv_obj_set_style_border_width(status_container, 0, 0);

  lv_obj_set_style_radius(status_container, 12, 0);

  lv_obj_set_style_pad_all(status_container, 12, 0);

  lv_obj_set_style_pad_row(status_container, 10, 0);  // Abstand zwischen Zeit und Datum
  lv_obj_set_flex_flow(status_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(status_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_clear_flag(status_container, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_clear_flag(status_container, LV_OBJ_FLAG_CLICKABLE);



  status_time_label = lv_label_create(status_container);
  lv_obj_set_width(status_time_label, LV_PCT(100));
  lv_obj_set_style_min_width(status_time_label, 200, 0);
  lv_obj_set_style_max_width(status_time_label, 200, 0);
  lv_obj_set_style_align(status_time_label, LV_ALIGN_CENTER, 0);
  lv_label_set_long_mode(status_time_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(status_time_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(status_time_label, lv_color_white(), 0);

  lv_obj_set_style_text_font(status_time_label, &font_roboto_mono_digits_48, 0);
  // Platzhalter nur mit Zeichen, die im Ziffern-Font vorhanden sind
  lv_label_set_text(status_time_label, "00:00");



  status_date_label = lv_label_create(status_container);

  lv_obj_set_width(status_date_label, LV_PCT(100));

  lv_label_set_long_mode(status_date_label, LV_LABEL_LONG_CLIP);

  lv_obj_set_style_text_align(status_date_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_set_style_text_color(status_date_label, lv_color_hex(0xC8C8C8), 0);

  lv_obj_set_style_text_font(status_date_label, &font_roboto_mono_digits_24, 0);
  // Platzhalter nur mit Zeichen, die im Ziffern-Font vorhanden sind
  lv_label_set_text(status_date_label, "00.00.0000");
}

lv_obj_t* UIManager::setupTabButton(lv_obj_t *btn, uint8_t tab_index, const char *icon_name, const char *tab_name) {
  if (!btn) return nullptr;

  // Button Styling (Normal State: Transparent)
  lv_obj_set_width(btn, LV_PCT(100));
  lv_obj_set_height(btn, 100);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0xE38422), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, 0);

  // PRESSED State: Helle Orange Farbe beim Drücken
  lv_obj_set_style_bg_opa(btn, LV_OPA_30, LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(0xE38422), LV_STATE_PRESSED);

  // Animation deaktivieren
  lv_obj_set_style_transform_width(btn, 0, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(btn, 0, LV_STATE_PRESSED);

  lv_obj_set_style_radius(btn, 24, 0);
  lv_obj_set_style_pad_all(btn, 8, 0);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

  // Flexbox column Layout: Icon oben, Text unten
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(btn, 12, 0);  // Größerer Abstand wie bei Scene-Kacheln

  lv_obj_t *icon_label = nullptr;
  lv_obj_t *text_label = nullptr;

  bool has_icon = (icon_name && strlen(icon_name) > 0);
  bool has_name = (tab_name && strlen(tab_name) > 0);

  // Icon (wenn gesetzt)
  if (has_icon) {
    String iconChar = getMdiChar(String(icon_name));
    if (iconChar.length() > 0 && FONT_MDI_ICONS != nullptr) {
      icon_label = lv_label_create(btn);
      lv_label_set_text(icon_label, iconChar.c_str());
      lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
      lv_obj_set_style_text_font(icon_label, FONT_MDI_ICONS, 0);
    }
  }

  // Name oder Fallback-Nummer
  if (has_name) {
    text_label = lv_label_create(btn);
    lv_label_set_text(text_label, tab_name);
    lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_24, 0);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(text_label, LV_PCT(90));
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
  } else if (!has_icon) {
    // Fallback: Wenn beides leer
    text_label = lv_label_create(btn);
    const char* fallback = (tab_index == 3) ? "Settings" : "";
    if (fallback[0] == '\0') {
      char num_fallback[2];
      snprintf(num_fallback, sizeof(num_fallback), "%d", tab_index + 1);
      lv_label_set_text(text_label, num_fallback);
    } else {
      lv_label_set_text(text_label, fallback);
    }
    lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
  }

  // Return text label (for potential later updates)
  return text_label;
}

lv_obj_t* UIManager::createTabPanel(lv_obj_t *parent) {
  if (!parent) return nullptr;

  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  return panel;
}

void UIManager::switchToTab(uint8_t index) {
  if (index >= TAB_COUNT) return;
  if (active_tab_index == index) return;

  static constexpr size_t kTilesBufferLines = 155;
  static constexpr size_t kSettingsBufferLines = 155;
  if (index == 3) {
    displayManager.setBufferLines(kSettingsBufferLines);
  } else {
    displayManager.setBufferLines(kTilesBufferLines);
  }

  // Alten Tab deaktivieren (falls vorhanden)
  if (active_tab_index != UINT8_MAX && active_tab_index < TAB_COUNT) {
    if (tab_panels[active_tab_index]) {
      lv_obj_add_flag(tab_panels[active_tab_index], LV_OBJ_FLAG_HIDDEN);
    }
    if (tab_buttons[active_tab_index]) {
      lv_obj_set_style_bg_opa(tab_buttons[active_tab_index], LV_OPA_TRANSP, 0);
    }
    // Label bleibt IMMER weiss - nicht aendern!
  }

  if (index <= 2) {
    GridType grid_type = static_cast<GridType>(index);
    if (!tiles_is_loaded(grid_type)) {
      tiles_request_reload(grid_type);
    }
  }

  // Neuen Tab aktivieren
  if (tab_panels[index]) {
    lv_obj_clear_flag(tab_panels[index], LV_OBJ_FLAG_HIDDEN);
  }
  if (tab_buttons[index]) {
    lv_obj_set_style_bg_opa(tab_buttons[index], LV_OPA_COVER, 0);
  }
  // Label bleibt IMMER weiss - nicht aendern!

  active_tab_index = index;
}

void UIManager::nav_button_event_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED) return;
  if (!e) return;

  UIManager *self = static_cast<UIManager*>(lv_event_get_user_data(e));
  if (!self) return;

  lv_obj_t *target = static_cast<lv_obj_t*>(lv_event_get_target(e));

  // Optimierte Schleife: meist sind es nur 4 Tabs, aber trotzdem schnell beenden
  for (uint8_t i = 0; i < TAB_COUNT; ++i) {
    if (self->tab_buttons[i] == target) {
      self->switchToTab(i);
      return;  // Sofort beenden statt break+return
    }
  }
}

// ========== Statusbar aktualisieren ==========
void UIManager::updateStatusbar() {
  if (!status_time_label || !status_date_label) return;

  char buf[48];

  static uint32_t last_rtc_sync_ms = 0;

  bool have_time = false;

  int hour = 0, minute = 0, day = 0, month = 0, year = 0;

  auto is_valid_datetime = [](int y, int m, int d, int h, int min) {
    return (y >= 2023 && y <= 2099) &&
           (m >= 1 && m <= 12) &&
           (d >= 1 && d <= 31) &&
           (h >= 0 && h < 24) &&
           (min >= 0 && min < 60);
  };

  struct tm timeinfo;

  if (getLocalTime(&timeinfo, 0)) {
    hour = timeinfo.tm_hour;
    minute = timeinfo.tm_min;
    day = timeinfo.tm_mday;
    month = timeinfo.tm_mon + 1;
    year = timeinfo.tm_year + 1900;
    have_time = is_valid_datetime(year, month, day, hour, minute);
  } else if (M5.Rtc.isEnabled()) {
    m5::rtc_datetime_t dt;
    if (M5.Rtc.getDateTime(&dt)) {
      hour = dt.time.hours;
      minute = dt.time.minutes;
      day = dt.date.date;
      month = dt.date.month;
      year = dt.date.year;
      // Vermeide kurz falsche RTC-Zeit bevor NTP greift
      have_time = is_valid_datetime(year, month, day, hour, minute);
    }
  }

  // Wenn Zeit aus NTP/System verfuegbar, RTC gelegentlich nachziehen
  if (have_time && M5.Rtc.isEnabled()) {
    uint32_t now_ms = millis();
    if (last_rtc_sync_ms == 0 || (int32_t)(now_ms - last_rtc_sync_ms) > 60000) {
      m5::rtc_datetime_t dt;
      dt.date.year = year;
      dt.date.month = month;
      dt.date.date = day;
      dt.time.hours = hour;
      dt.time.minutes = minute;
      dt.time.seconds = 0;
      M5.Rtc.setDateTime(&dt);
      last_rtc_sync_ms = now_ms;
      Serial.printf("[RTC] NTP->RTC %04d-%02d-%02d %02d:%02d\n", year, month, day, hour, minute);
    }
  }



  if (have_time) {

    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);

  } else {

    // Kein valider Zeitstempel: neutrales Fallback aus Ziffern, damit keine fehlenden Glyphen
    snprintf(buf, sizeof(buf), "00:00");

  }

  lv_label_set_text(status_time_label, buf);



  if (have_time) {

    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", day, month, year);

  } else {

    // Fallback ohne fehlende Glyphen
    snprintf(buf, sizeof(buf), "00.00.0000");

  }

  lv_label_set_text(status_date_label, buf);



  // NTP-Sync triggern wenn keine Zeit aber WiFi verbunden

  if (!have_time && WiFi.status() == WL_CONNECTED) {

    scheduleNtpSync(0);

  }

}



// ========== NTP-Sync ==========

void UIManager::scheduleNtpSync(uint32_t delay_ms) {

  next_ntp_sync_ms = millis() + delay_ms;

}



void UIManager::serviceNtpSync() {

  if (WiFi.status() != WL_CONNECTED) return;



  uint32_t now_ms = millis();

  if ((int32_t)(now_ms - next_ntp_sync_ms) < 0) return;



  configTzTime(TZ_EUROPE_BERLIN, "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");

  tz_configured = true;

  next_ntp_sync_ms = now_ms + 3600000UL; // Stündlich neu syncen

}

// ========== Tab-Button Live-Update ==========
void UIManager::refreshTabButton(uint8_t tab_index) {
  // Alle Tabs 0-3 sind konfigurierbar
  if (tab_index >= 4) return;
  if (!tab_buttons[tab_index]) return;

  lv_obj_t *btn = tab_buttons[tab_index];

  // Alle Children löschen (Icon + Text Label)
  lv_obj_clean(btn);

  // Flexbox column Layout wiederherstellen (wird durch clean gelöscht)
  lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_gap(btn, 12, 0);  // Größerer Abstand wie bei Scene-Kacheln

  // Neue Werte aus tileConfig holen
  const char* tabName = tileConfig.getTabName(tab_index);
  const char* iconName = tileConfig.getTabIcon(tab_index);

  bool has_icon = (iconName && strlen(iconName) > 0);
  bool has_name = (tabName && strlen(tabName) > 0);

  // Icon (wenn gesetzt)
  if (has_icon) {
    String iconChar = getMdiChar(String(iconName));
    if (iconChar.length() > 0 && FONT_MDI_ICONS != nullptr) {
      lv_obj_t *icon_label = lv_label_create(btn);
      lv_label_set_text(icon_label, iconChar.c_str());
      lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
      lv_obj_set_style_text_font(icon_label, FONT_MDI_ICONS, 0);
    }
  }

  // Name oder Fallback-Nummer
  if (has_name) {
    lv_obj_t *text_label = lv_label_create(btn);
    lv_label_set_text(text_label, tabName);
    lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_24, 0);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(text_label, LV_PCT(90));
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);

    // Update tab_labels array für potenzielle spätere Verwendung
    tab_labels[tab_index] = text_label;
  } else if (!has_icon) {
    // Fallback: Wenn beides leer
    lv_obj_t *text_label = lv_label_create(btn);
    const char* fallback = (tab_index == 3) ? "Settings" : "";
    if (fallback[0] == '\0') {
      char num_fallback[2];
      snprintf(num_fallback, sizeof(num_fallback), "%d", tab_index + 1);
      lv_label_set_text(text_label, num_fallback);
    } else {
      lv_label_set_text(text_label, fallback);
    }
    lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(text_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);

    tab_labels[tab_index] = text_label;
  }

  // Display neu zeichnen (wichtig für sofortiges Update!)
  lv_obj_invalidate(btn);

  Serial.printf("[UI] Tab-Button %u aktualisiert: %s (icon: %s)\n", tab_index, tabName, iconName);
}






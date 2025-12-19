#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>
#include <Arduino.h>

// Forward Declarations
typedef void (*scene_publish_cb_t)(const char* scene_name);
typedef void (*hotspot_start_cb_t)();

// UI Manager - Verwaltet die Benutzeroberfläche
class UIManager {
public:
  // UI aufbauen
  void buildUI(scene_publish_cb_t scene_cb, hotspot_start_cb_t hotspot_cb = nullptr);

  // Statusbar
  void updateStatusbar();

  // NTP Sync
  void scheduleNtpSync(uint32_t delay_ms = 0);
  void serviceNtpSync();

  // Tab-Button Live-Update
  void refreshTabButton(uint8_t tab_index);

  // Tab wechseln (public für Navigation-Tiles)
  void switchToTab(uint8_t index);

private:
  static constexpr uint8_t TAB_COUNT = 4;

  // Statusbar-Elemente
  lv_obj_t *status_container = nullptr;
  lv_obj_t *status_time_label = nullptr;
  lv_obj_t *status_date_label = nullptr;

  // Tab-Inhalte + Navigation
  lv_obj_t *tab_panels[TAB_COUNT] = {nullptr};
  lv_obj_t *tab_buttons[TAB_COUNT] = {nullptr};
  lv_obj_t *tab_button_overlays[TAB_COUNT] = {nullptr};  // Für aktiven Zustand
  lv_obj_t *tab_labels[TAB_COUNT] = {nullptr};
  lv_obj_t *tab_content_container = nullptr;
  lv_obj_t *nav_container = nullptr;
  uint8_t active_tab_index = UINT8_MAX;
  // NTP-Sync
  uint32_t next_ntp_sync_ms = 0;
  bool tz_configured = false;
  static const char* TZ_EUROPE_BERLIN;

  // Interne Funktionen
  void statusbarInit(lv_obj_t *tab_bar);
  lv_obj_t* setupTabButton(lv_obj_t *btn, uint8_t tab_index, const char *icon_name, const char *tab_name);
  lv_obj_t* createTabPanel(lv_obj_t *parent);
  static void nav_button_event_cb(lv_event_t *e);
};

// Globale Instanz
extern UIManager uiManager;

#endif // UI_MANAGER_H

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <lvgl.h>

// Display-Konstanten
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

// Display Manager - Verwaltet Display-Hardware und LVGL-Integration
class DisplayManager {
public:
  // Initialisierung
  bool init();

  // Zugriff auf LVGL-Objekte
  lv_display_t* getDisplay() { return disp; }
  lv_indev_t* getInput() { return indev; }

  // Activity Tracking (für Power Manager)
  void resetActivityTimer();
  uint32_t getLastActivityTime() { return last_activity_time; }

  // Touch-Guard nach Wake
  void armWakeTouchGuard();
  void setInputEnabled(bool enable);

private:
  static lv_display_t *disp;
  static lv_indev_t *indev;
  static lv_color_t *buf1;
  static lv_color_t *buf2;
  static uint32_t last_activity_time;

  // LVGL Callbacks
  static void flush_cb(lv_display_t *lv_disp, const lv_area_t *area, uint8_t *px_map);
  static void touch_cb(lv_indev_t* indev_drv, lv_indev_data_t *data);
};

// Globale Instanz
extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <lvgl.h>

#include "src/devices/device.h"

static constexpr int SCREEN_WIDTH = Device::kScreenWidth;
static constexpr int SCREEN_HEIGHT = Device::kScreenHeight;

class DisplayManager {
public:
  bool init();

  lv_display_t* getDisplay() { return disp; }
  lv_indev_t* getInput() { return indev; }

  void resetActivityTimer();
  uint32_t getLastActivityTime() { return last_activity_time; }

  void armWakeTouchGuard();
  void setInputEnabled(bool enable);
  void setRotation(uint8_t rotation);
  void setRotationFlipped(bool flipped);
  bool isRotationFlipped() const;
  uint8_t getRotation() const;
  void debugFlushNext(uint16_t count);
  void setReverseFlush(bool enable);
  void setReverseFlushOnce();
  bool setBufferLines(size_t lines);
  bool setBufferLines(size_t lines, lv_display_render_mode_t render_mode);
  // Temporary large single-buffer mode for full-frame content such as the
  // camera popup. The buffer lives exclusively in PSRAM, so the internal
  // DMA reserve used by WiFi/MQTT remains untouched.
  bool setSinglePsramBufferLines(size_t lines);
  // Nach OTA-Vorbereitung (setBufferLines(8)) zurueck zum schnellen Band:
  // nutzt eine kleinere interne Reserve als beim Boot, weil WLAN/MQTT ihr
  // RAM zur Laufzeit schon besitzen - sonst faellt der Puffer nach jedem
  // fehlgeschlagenen Update bis zum Reboot ins langsame PSRAM zurueck.
  bool restoreBufferLinesAfterOta(size_t lines);
  bool isUsingFastInternalBuffer() const;
  size_t getBufferLines() const;
  size_t getRequestedBufferLines() const;
  lv_display_render_mode_t getRenderMode() const;
  uint32_t getFullScreenFlushSeq() const;

private:
  static lv_display_t* disp;
  static lv_indev_t* indev;
  static lv_color_t* buf1;
  static lv_color_t* buf2;
  static uint32_t last_activity_time;
  static uint8_t rotation;

  static void flush_cb(lv_display_t* lv_disp, const lv_area_t* area, uint8_t* px_map);
  static void touch_cb(lv_indev_t* indev_drv, lv_indev_data_t* data);

  // Allocates LVGL draw buffers, preferring a small fast internal-SRAM band and
  // falling back to the previous PSRAM double buffer when internal RAM is scarce.
  static bool allocDrawBuffers(size_t requested_lines, lv_display_render_mode_t mode);
  static bool allocDrawBuffers(size_t requested_lines, lv_display_render_mode_t mode,
                               size_t internal_reserve_bytes,
                               bool require_fast_internal);
};

extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H

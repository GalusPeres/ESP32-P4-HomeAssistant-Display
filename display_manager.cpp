#include "display_manager.h"
#include "power_manager.h"
#include <M5Unified.h>
#include "esp_heap_caps.h"
#include <Arduino.h>

// Globale Instanz
DisplayManager displayManager;

// Statische Member-Variablen
lv_display_t* DisplayManager::disp = nullptr;
lv_indev_t* DisplayManager::indev = nullptr;
lv_color_t* DisplayManager::buf1 = nullptr;
lv_color_t* DisplayManager::buf2 = nullptr;
uint32_t DisplayManager::last_activity_time = 0;

// ========== Display Flush Callback ==========
// IRAM_ATTR: Diese Funktion wird SEHR oft aufgerufen (jeder Frame!)
// Durch IRAM wird sie aus schnellem internen RAM ausgeführt (keine Cache-Misses)
// -> Deutlich schnellere Display-Updates, besonders beim Scrollen!
void IRAM_ATTR DisplayManager::flush_cb(lv_display_t *lv_disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = (area->x2 - area->x1 + 1);
  const uint32_t h = (area->y2 - area->y1 + 1);
  M5.Display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)px_map);
  lv_display_flush_ready(lv_disp);
}

// ========== Touch Callback ==========
// IRAM_ATTR: Touch wird häufig abgefragt (jede Loop-Iteration)
// Schnellere Touch-Response = besseres Scroll-Gefühl!
void IRAM_ATTR DisplayManager::touch_cb(lv_indev_t* indev_drv, lv_indev_data_t *data) {
  // Wenn im Display-Sleep, erstmal aufwecken
  if (powerManager.isInSleep()) {
    powerManager.wakeFromDisplaySleep();
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  lgfx::touch_point_t tp;
  if (M5.Display.getTouch(&tp)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = tp.x;
    data->point.y = tp.y;

    // Activity-Timer zurücksetzen und Power Manager aufwecken
    last_activity_time = millis();
    powerManager.setHighPerformance(true);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ========== Initialisierung ==========
bool DisplayManager::init() {
  Serial.println("🖥️ Initialisiere Display Manager...");

  // M5Stack Display-Setup
  M5.Display.setRotation(1);  // 1280x720 Querformat
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(150);  // Wird später vom Power Manager gesteuert

  last_activity_time = millis();

  // LVGL initialisieren
  lv_init();

  // Display erstellen
  disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  if (!disp) {
    Serial.println("❌ Display-Erstellung fehlgeschlagen!");
    return false;
  }

  lv_display_set_flush_cb(disp, flush_cb);

  // Farbformat + Anti-Aliasing aus (Performance)
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_antialiasing(disp, false);

  // Grosse DMA-Puffer (>=25 % der Bildschirmhoehe) fuer weniger Flushes
  static constexpr size_t TARGET_LINES   = 240;
  static constexpr size_t FALLBACK_LINES = 160;

  auto release_buffers = []() {
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);
    buf1 = nullptr;
    buf2 = nullptr;
  };

  auto allocate_buffers = [](size_t lines, uint32_t caps) -> bool {
    size_t bytes = SCREEN_WIDTH * lines * sizeof(lv_color_t);
    buf1 = (lv_color_t*)heap_caps_malloc(bytes, caps);
    buf2 = (lv_color_t*)heap_caps_malloc(bytes, caps);
    if (!buf1 || !buf2) {
      if (buf1) heap_caps_free(buf1);
      if (buf2) heap_caps_free(buf2);
      buf1 = buf2 = nullptr;
      return false;
    }
    return true;
  };

  size_t buffer_lines = TARGET_LINES;
  bool using_psram = false;

  if (!allocate_buffers(buffer_lines, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)) {
    release_buffers();
    buffer_lines = FALLBACK_LINES;
    if (!allocate_buffers(buffer_lines, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)) {
      release_buffers();
      buffer_lines = TARGET_LINES;
      if (!allocate_buffers(buffer_lines, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA)) {
        Serial.println("❌ DMA-Buffer-Allokation fehlgeschlagen!");
        return false;
      }
      using_psram = true;
    }
  }

  const size_t buf_bytes = SCREEN_WIDTH * buffer_lines * sizeof(lv_color_t);

  lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  Serial.printf("[OK] DMA-Puffer: 2x %d Bytes (je %d Zeilen, %s)\n",
                buf_bytes, buffer_lines, using_psram ? "PSRAM" : "SRAM");

  // Touch-Input
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_cb);
  lv_indev_set_display(indev, disp);

  Serial.println("[OK] Display Manager initialisiert");
  return true;
}

void DisplayManager::resetActivityTimer() {
  last_activity_time = millis();
}

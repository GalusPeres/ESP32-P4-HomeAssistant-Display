#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include <M5Unified.h>
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <cstring>

// Globale Instanz
DisplayManager displayManager;

// Statische Member-Variablen
lv_display_t* DisplayManager::disp = nullptr;
lv_indev_t* DisplayManager::indev = nullptr;
lv_color_t* DisplayManager::buf1 = nullptr;
lv_color_t* DisplayManager::buf2 = nullptr;
uint32_t DisplayManager::last_activity_time = 0;
static bool g_ignore_touch_until_release = false;
static bool g_input_enabled = true;
static volatile uint16_t g_flush_log_budget = 0;
static size_t g_buffer_lines = 0;
static uint8_t g_bytes_per_pixel = 0;
static lv_display_render_mode_t g_render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
static bool g_reverse_flush = false;
static lv_color_t* g_reverse_buf = nullptr;
static size_t g_reverse_buf_width = 0;
static constexpr size_t kReverseStripeWidth = 16;
static bool g_reverse_flush_once = false;

static bool ensure_reverse_buf() {
  if (g_reverse_buf && g_reverse_buf_width == kReverseStripeWidth) return true;
  if (g_reverse_buf) {
    heap_caps_free(g_reverse_buf);
    g_reverse_buf = nullptr;
    g_reverse_buf_width = 0;
  }
  uint8_t bpp = g_bytes_per_pixel ? g_bytes_per_pixel : 2;
  const size_t bytes = kReverseStripeWidth * SCREEN_HEIGHT * bpp;
  lv_color_t* buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
  if (!buf) {
    buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  }
  if (!buf) return false;
  g_reverse_buf = buf;
  g_reverse_buf_width = kReverseStripeWidth;
  return true;
}

void DisplayManager::debugFlushNext(uint16_t count) {
  g_flush_log_budget = count;
}

void DisplayManager::setReverseFlush(bool enable) {
  if (enable == g_reverse_flush) return;
  if (!enable) {
    if (g_reverse_buf) {
      heap_caps_free(g_reverse_buf);
      g_reverse_buf = nullptr;
    }
    g_reverse_buf_width = 0;
    g_reverse_flush = false;
    g_reverse_flush_once = false;
    return;
  }
  if (!ensure_reverse_buf()) {
    g_reverse_flush = false;
    g_reverse_flush_once = false;
    return;
  }
  g_reverse_flush = true;
  g_reverse_flush_once = false;
}

void DisplayManager::setReverseFlushOnce() {
  if (!ensure_reverse_buf()) return;
  g_reverse_flush = true;
  g_reverse_flush_once = true;
}

bool DisplayManager::setBufferLines(size_t lines) {
  return setBufferLines(lines, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

bool DisplayManager::setBufferLines(size_t lines, lv_display_render_mode_t render_mode) {
  if (!disp || lines == 0) {
    return false;
  }
  if (g_bytes_per_pixel == 0) {
    g_bytes_per_pixel = lv_color_format_get_size(lv_display_get_color_format(disp));
    if (g_bytes_per_pixel == 0) {
      g_bytes_per_pixel = 2;
    }
  }
  if (g_buffer_lines == lines && g_render_mode == render_mode) {
    return true;
  }

  lv_refr_now(disp);

  const size_t bytes = SCREEN_WIDTH * lines * g_bytes_per_pixel;
  if (g_buffer_lines == lines && g_render_mode != render_mode) {
    if (!buf1 || !buf2) return false;
    lv_display_set_buffers(disp, buf1, buf2, bytes, render_mode);
    g_render_mode = render_mode;
    Serial.printf("[Display] Render-Mode umgestellt: %d (Zeilen=%d)\n", (int)render_mode, (int)lines);
    return true;
  }
  lv_color_t* new_buf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  lv_color_t* new_buf2 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  bool using_psram = true;
  if (!new_buf1 || !new_buf2) {
    if (new_buf1) heap_caps_free(new_buf1);
    if (new_buf2) heap_caps_free(new_buf2);
    new_buf1 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    new_buf2 = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    using_psram = false;
  }
  if (!new_buf1 || !new_buf2) {
    if (new_buf1) heap_caps_free(new_buf1);
    if (new_buf2) heap_caps_free(new_buf2);
    return false;
  }

  lv_display_set_buffers(disp, new_buf1, new_buf2, bytes, render_mode);

  if (buf1) heap_caps_free(buf1);
  if (buf2) heap_caps_free(buf2);
  buf1 = new_buf1;
  buf2 = new_buf2;
  g_buffer_lines = lines;
  g_render_mode = render_mode;

  Serial.printf("[Display] DMA-Puffer umgestellt: 2x %d Bytes (je %d Zeilen, %s, %u Bpp)\n",
                bytes, (int)lines, using_psram ? "PSRAM" : "SRAM", g_bytes_per_pixel);
  return true;
}

size_t DisplayManager::getBufferLines() const {
  return g_buffer_lines;
}

lv_display_render_mode_t DisplayManager::getRenderMode() const {
  return g_render_mode;
}

// ========== Display Flush Callback ==========
// IRAM_ATTR: Diese Funktion wird SEHR oft aufgerufen (jeder Frame!)
// Durch IRAM wird sie aus schnellem internen RAM ausgefuehrt (keine Cache-Misses)
// -> Deutlich schnellere Display-Updates, besonders beim Scrollen!
void IRAM_ATTR DisplayManager::flush_cb(lv_display_t *lv_disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = (area->x2 - area->x1 + 1);
  const uint32_t h = (area->y2 - area->y1 + 1);
  if (g_flush_log_budget) {
    Serial.printf("[FLUSH] x=%d..%d y=%d..%d w=%lu h=%lu last=%d\n",
                  area->x1, area->x2, area->y1, area->y2,
                  (unsigned long)w, (unsigned long)h,
                  lv_display_flush_is_last(lv_disp));
    g_flush_log_budget--;
  }
  if (g_reverse_flush && g_reverse_buf && g_reverse_buf_width > 0 &&
      area->x1 == 0 && area->y1 == 0 &&
      w == SCREEN_WIDTH && h == SCREEN_HEIGHT) {
    const uint16_t* src = reinterpret_cast<const uint16_t*>(px_map);
    const uint32_t stripe = (uint32_t)g_reverse_buf_width;
    int32_t x = (int32_t)w;
    while (x > 0) {
      const uint32_t cur_w = (x >= (int32_t)stripe) ? stripe : (uint32_t)x;
      const uint32_t start = (uint32_t)(x - (int32_t)cur_w);
      uint16_t* dst = reinterpret_cast<uint16_t*>(g_reverse_buf);
      for (uint32_t row = 0; row < h; ++row) {
        const uint16_t* src_row = src + row * w + start;
        std::memcpy(dst + row * cur_w, src_row, cur_w * sizeof(uint16_t));
      }
      M5.Display.pushImageDMA((int32_t)start, 0, cur_w, h, dst);
      x -= (int32_t)cur_w;
    }
    if (g_reverse_flush_once) {
      g_reverse_flush = false;
      g_reverse_flush_once = false;
    }
    lv_display_flush_ready(lv_disp);
    return;
  }
  M5.Display.pushImageDMA(area->x1, area->y1, w, h, (uint16_t*)px_map);
  lv_display_flush_ready(lv_disp);
}

// ========== Touch Callback ==========
// IRAM_ATTR: Touch wird haeufig abgefragt (jede Loop-Iteration)
// Schnellere Touch-Response = besseres Scroll-Gefuehl!
void IRAM_ATTR DisplayManager::touch_cb(lv_indev_t* indev_drv, lv_indev_data_t *data) {
  // Wenn im Display-Sleep, erstmal aufwecken
  if (powerManager.isInSleep()) {
    powerManager.wakeFromDisplaySleep();
    g_ignore_touch_until_release = true;  // Erst loslassen, dann wieder reagieren
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  // Eingaben blockieren? (z.B. Sleep aktiv, Touch gesperrt)
  if (!g_input_enabled) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  // Nach Sleep: Erst Touch loslassen, bevor Aktionen erlaubt sind
  if (g_ignore_touch_until_release) {
    lgfx::touch_point_t tmp;
    if (M5.Display.getTouch(&tmp)) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    g_ignore_touch_until_release = false;
  }

  lgfx::touch_point_t tp;
  if (M5.Display.getTouch(&tp)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = tp.x;
    data->point.y = tp.y;

    // Activity-Timer zuruecksetzen und Power Manager aufwecken
    last_activity_time = millis();
    powerManager.setHighPerformance(true);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ========== Initialisierung ==========
bool DisplayManager::init() {
  Serial.println("[Display] Initialisiere Display Manager...");

  // M5Stack Display-Setup
  // 180° Rotation (Landscape inverted)
  M5.Display.setRotation(1);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setBrightness(150);  // Wird spaeter vom Power Manager gesteuert

  last_activity_time = millis();

  // LVGL initialisieren
  lv_init();

  // Display erstellen
  disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  if (!disp) {
    Serial.println("[Display] Display-Erstellung fehlgeschlagen!");
    return false;
  }

  lv_display_set_flush_cb(disp, flush_cb);

  // Farbformat + Anti-Aliasing aus (Performance)
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_antialiasing(disp, false);
  g_bytes_per_pixel = lv_color_format_get_size(lv_display_get_color_format(disp));
  if (g_bytes_per_pixel == 0) {
    g_bytes_per_pixel = 2;
  }

  // Kleinere DMA-Puffer fuer mehr verfuegbaren Heap (wichtig bei vielen Kacheln!)
  static constexpr size_t TARGET_LINES   = 155;
  static constexpr size_t FALLBACK_LINES = 96;

  auto release_buffers = []() {
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);
    buf1 = nullptr;
    buf2 = nullptr;
  };

  auto allocate_buffers = [](size_t lines, uint32_t caps) -> bool {
    size_t bytes = SCREEN_WIDTH * lines * g_bytes_per_pixel;
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
        Serial.println("[Display] DMA-Buffer-Allokation fehlgeschlagen!");
        return false;
      }
      using_psram = true;
    }
  }

  const size_t buf_bytes = SCREEN_WIDTH * buffer_lines * g_bytes_per_pixel;

  lv_display_set_buffers(disp, buf1, buf2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  g_buffer_lines = buffer_lines;
  Serial.printf("[OK] DMA-Puffer: 2x %d Bytes (je %d Zeilen, %s, %u Bpp)\n",
                buf_bytes, buffer_lines, using_psram ? "PSRAM" : "SRAM", g_bytes_per_pixel);

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

void DisplayManager::armWakeTouchGuard() {
  g_ignore_touch_until_release = true;
}

void DisplayManager::setInputEnabled(bool enable) {
  g_input_enabled = enable;
  if (indev) {
    lv_indev_enable(indev, enable);
    if (!enable) {
      lv_indev_reset(indev, nullptr);
    }
  }
}

#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/core/board_hal.h"
#include "src/devices/device_select.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include <Arduino.h>
#include <cstdint>
#include <cstring>

// Globale Instanz
DisplayManager displayManager;

// Statische Member-Variablen
lv_display_t* DisplayManager::disp = nullptr;
lv_indev_t* DisplayManager::indev = nullptr;
lv_color_t* DisplayManager::buf1 = nullptr;
lv_color_t* DisplayManager::buf2 = nullptr;
uint32_t DisplayManager::last_activity_time = 0;
uint8_t DisplayManager::rotation = 0;
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
static constexpr bool kEnableReverseFlushEffect = true;
static constexpr uintptr_t kCacheLineSize = 64;
static bool g_reverse_flush_once = false;
static volatile uint32_t g_fullscreen_flush_seq = 0;
static inline void flush_cache_for_dma(const void* ptr, size_t size) {
  if (!ptr || size == 0) return;
  const uintptr_t start = reinterpret_cast<uintptr_t>(ptr);
  const uintptr_t aligned_start = start & ~(kCacheLineSize - 1);
  const uintptr_t end = start + size;
  const uintptr_t aligned_end = (end + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  if (aligned_end <= aligned_start) return;
  esp_cache_msync(reinterpret_cast<void*>(aligned_start),
                  aligned_end - aligned_start,
                  ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}

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
  if (!kEnableReverseFlushEffect) {
    if (g_reverse_buf) {
      heap_caps_free(g_reverse_buf);
      g_reverse_buf = nullptr;
    }
    g_reverse_buf_width = 0;
    g_reverse_flush = false;
    g_reverse_flush_once = false;
    return;
  }
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
  if (!kEnableReverseFlushEffect) return;
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
  lv_color_t* new_buf1 = (lv_color_t*)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  lv_color_t* new_buf2 = (lv_color_t*)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
  bool using_psram = true;
  if (!new_buf1 || !new_buf2) {
    if (new_buf1) heap_caps_free(new_buf1);
    if (new_buf2) heap_caps_free(new_buf2);
    new_buf1 = (lv_color_t*)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    new_buf2 = (lv_color_t*)heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
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

uint32_t DisplayManager::getFullScreenFlushSeq() const {
  return g_fullscreen_flush_seq;
}

void DisplayManager::setRotation(uint8_t rotation_value) {
  rotation_value = Device::normalizeRotationQuarterTurns(rotation_value);
  if (rotation == rotation_value) return;
  BoardHAL::displaySetRotation(rotation_value);
  rotation = rotation_value;
  lv_display_t* disp_local = lv_display_get_default();
  if (disp_local) {
    lv_obj_invalidate(lv_scr_act());
    lv_refr_now(disp_local);
  }
}

void DisplayManager::setRotationFlipped(bool flipped) {
  setRotation(flipped ? Device::kRotationFlipped : Device::kRotationDefault);
}

bool DisplayManager::isRotationFlipped() const {
  return rotation == Device::kRotationFlipped;
}

uint8_t DisplayManager::getRotation() const {
  return rotation;
}

// ========== Display Flush Callback ==========
// IRAM_ATTR: Diese Funktion wird SEHR oft aufgerufen (jeder Frame!)
// Durch IRAM wird sie aus schnellem internen RAM ausgefuehrt (keine Cache-Misses)
// -> Deutlich schnellere Display-Updates, besonders beim Scrollen!
void IRAM_ATTR DisplayManager::flush_cb(lv_display_t *lv_disp, const lv_area_t *area, uint8_t *px_map) {
  const uint32_t w = (area->x2 - area->x1 + 1);
  const uint32_t h = (area->y2 - area->y1 + 1);
  const size_t row_bytes = (size_t)w * sizeof(uint16_t);
  const size_t stride_bytes = lv_draw_buf_width_to_stride(w, lv_display_get_color_format(lv_disp));
  const bool packed_rows = (stride_bytes == row_bytes);
  const uint32_t area_px = w * h;
  static constexpr uint32_t kMinPixelsForDma = 2048;  // avoid DMA overhead on tiny dirty areas
  static constexpr uint32_t kReverseMinPixels = (SCREEN_WIDTH * SCREEN_HEIGHT) / 8;  // trigger only on large image updates
  if (g_flush_log_budget) {
    Serial.printf("[FLUSH] x=%d..%d y=%d..%d w=%lu h=%lu last=%d\n",
                  area->x1, area->x2, area->y1, area->y2,
                  (unsigned long)w, (unsigned long)h,
                  lv_display_flush_is_last(lv_disp));
    g_flush_log_budget--;
  }
  if (g_reverse_flush && g_reverse_buf && g_reverse_buf_width > 0 &&
      area_px >= kReverseMinPixels) {
    const uint16_t* src = reinterpret_cast<const uint16_t*>(px_map);
    const uint32_t stripe = (uint32_t)g_reverse_buf_width;
    uint32_t x = 0;
    while (x < w) {
      const uint32_t remaining = w - x;
      const uint32_t cur_w = (remaining >= stripe) ? stripe : remaining;
      const uint32_t start = x;
      uint16_t* dst = reinterpret_cast<uint16_t*>(g_reverse_buf);
      for (uint32_t row = 0; row < h; ++row) {
        const uint16_t* src_row = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const uint8_t*>(src) + row * stride_bytes) + start;
        std::memcpy(dst + row * cur_w, src_row, cur_w * sizeof(uint16_t));
      }
      BoardHAL::displayPushPixelsDMA(area->x1 + (int32_t)start, area->y1, cur_w, h, dst);
      x += cur_w;
    }
    if (g_reverse_flush_once) {
      g_reverse_flush = false;
      g_reverse_flush_once = false;
    }
    if (area->x1 == 0 && area->y1 == 0 && w == SCREEN_WIDTH && h == SCREEN_HEIGHT) {
      g_fullscreen_flush_seq++;
    }
    lv_display_flush_ready(lv_disp);
    return;
  }

  // Hybrid flush path:
  // - small/partial areas via CPU pushImage (robust for odd widths and tiny regions)
  // - larger areas via DMA when rows are tightly packed
  const bool use_dma = packed_rows && (area_px >= kMinPixelsForDma);
  if (use_dma) {
    flush_cache_for_dma(px_map, row_bytes * h);
    BoardHAL::displayPushPixelsDMA(area->x1, area->y1, w, h, (uint16_t*)px_map);
    BoardHAL::displayWaitDMA();
  } else {
    if (packed_rows) {
      BoardHAL::displayPushPixels(area->x1, area->y1, w, h, (uint16_t*)px_map);
    } else {
      // LVGL can align each line in px_map; push line-by-line when rows are padded.
      for (uint32_t row = 0; row < h; ++row) {
        const uint16_t* src_row = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const uint8_t*>(px_map) + row * stride_bytes);
        BoardHAL::displayPushPixels(area->x1, area->y1 + (int32_t)row, w, 1, (uint16_t*)src_row);
      }
    }
  }

  if (area->x1 == 0 && area->y1 == 0 && w == SCREEN_WIDTH && h == SCREEN_HEIGHT) {
    g_fullscreen_flush_seq++;
  }
  lv_display_flush_ready(lv_disp);
}

// ========== Touch Callback ==========
// IRAM_ATTR: Touch wird haeufig abgefragt (jede Loop-Iteration)
// Schnellere Touch-Response = besseres Scroll-Gefuehl!
void IRAM_ATTR DisplayManager::touch_cb(lv_indev_t* indev_drv, lv_indev_data_t *data) {
  // Wenn im Display-Sleep, erstmal aufwecken
  if (powerManager.isInSleep()) {
    if (powerManager.isTouchWakeEnabled()) {
      powerManager.wakeFromDisplaySleep();
      g_ignore_touch_until_release = true;  // Erst loslassen, dann wieder reagieren
    }
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
    BoardHAL::TouchPoint tmp;
    if (BoardHAL::getTouch(&tmp)) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    g_ignore_touch_until_release = false;
  }

  BoardHAL::TouchPoint tp;
  if (BoardHAL::getTouch(&tp)) {
    int16_t mapped_x = tp.x;
    int16_t mapped_y = tp.y;
#if !defined(DEVICE_M5STACKS_TAB5)
    switch (rotation & 0x03) {
      case 1:
        mapped_x = tp.y;
        mapped_y = SCREEN_WIDTH - 1 - tp.x;
        break;
      case 2:
        mapped_x = SCREEN_WIDTH - 1 - tp.x;
        mapped_y = SCREEN_HEIGHT - 1 - tp.y;
        break;
      case 3:
        mapped_x = SCREEN_HEIGHT - 1 - tp.y;
        mapped_y = tp.x;
        break;
      default:
        break;
    }
#endif

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = mapped_x;
    data->point.y = mapped_y;

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

  // Waveshare: Display is already initialised by BoardHAL::init().
  // 720×720 square display – no rotation needed by default.
  BoardHAL::displayFillScreen(0x0000);  // black
  BoardHAL::setBrightness(150);  // Wird spaeter vom Power Manager gesteuert
  rotation = Device::kRotationDefault;

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
#if defined(DEVICE_M5STACKS_TAB5)
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
#else
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
#endif
  lv_display_set_antialiasing(disp, false);
  g_bytes_per_pixel = lv_color_format_get_size(lv_display_get_color_format(disp));
  if (g_bytes_per_pixel == 0) {
    g_bytes_per_pixel = 2;
  }

  // Kleinere DMA-Puffer fuer mehr verfuegbaren Heap (wichtig bei vielen Kacheln!)
  static constexpr size_t TARGET_LINES   = SCREEN_HEIGHT / 4;
  static constexpr size_t FALLBACK_LINES = 96;

  auto release_buffers = []() {
    if (buf1) heap_caps_free(buf1);
    if (buf2) heap_caps_free(buf2);
    buf1 = nullptr;
    buf2 = nullptr;
  };

  auto allocate_buffers = [](size_t lines, uint32_t caps) -> bool {
    size_t bytes = SCREEN_WIDTH * lines * g_bytes_per_pixel;
    buf1 = (lv_color_t*)heap_caps_aligned_alloc(64, bytes, caps);
    buf2 = (lv_color_t*)heap_caps_aligned_alloc(64, bytes, caps);
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
  g_render_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
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

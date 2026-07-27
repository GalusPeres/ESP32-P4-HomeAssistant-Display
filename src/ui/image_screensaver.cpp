#include "src/ui/image_screensaver.h"

#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <libs/tjpgd/tjpgd.h>
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include <driver/jpeg_decode.h>
#include <soc/soc_caps.h>
#endif
#include <string.h>

// lvgl.h exportiert lv_image_cache_drop() in 9.5 nicht mehr, die Deklaration
// liegt nur noch im Instanz-Header (siehe tile_renderer.cpp).
#include <misc/cache/instance/lv_image_cache.h>

#include "src/core/dma2d_arbiter.h"
#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/devices/device.h"
#include "src/network/ha_bridge_config.h"
#include "src/tiles/tile_renderer.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/types/clock/clock_format.h"
#include "src/types/clock/renderer.h"
#include "src/types/energy/energy_data.h"
#include "src/ui/screensaver_config.h"
#include "src/ui/tab_tiles_unified.h"
#include "src/ui/ui_surface_style.h"

namespace {

constexpr char kImageDir[] = "/images";
constexpr char kLegacyWallpaperDir[] = "/wallpapers";
constexpr size_t kMaxFileBytes = 8U * 1024U * 1024U;

// Obergrenze fuer den voll dekodierten Zwischenpuffer: 2048x2048 RGB565 sind
// 8 MB PSRAM. Groessere Bilder laufen ueber den TJpgDec-Pfad, der per
// 1/2..1/8-Skalierung unter diese Grenze kommt.
constexpr uint32_t kMaxDecodePixels = 2048U * 2048U;

// Nach den Decode-Puffern muss genug PSRAM fuer den Rest der UI uebrig
// bleiben (Cover-Worker, Popups, LVGL-Zwischenpuffer).
constexpr size_t kPsramReserveBytes = 4U * 1024U * 1024U;
constexpr size_t kPpaBufferAlignment = 64;
constexpr uint16_t kImageRadius =
    static_cast<uint16_t>(tile_layout::scale(26));
// Nach einer Bedienung erst den sichtbaren Tile-/MQTT-Zustand zur Ruhe kommen
// lassen. Sonst kann ein gleichzeitig faelliger Decode/Composite-Durchlauf den
// Loop blockieren, bevor LVGL die Switch-Aenderung auf das Panel geflusht hat.
constexpr uint32_t kInteractionSettleBeforeSlideMs = 1500;

struct ScreensaverState {
  lv_obj_t* overlay = nullptr;
  lv_obj_t* image = nullptr;
  lv_obj_t* clock_box = nullptr;
  lv_obj_t* slot_grid = nullptr;
  lv_timer_t* timer = nullptr;
  int active_wallpaper = -1;
  String active_wallpaper_name;
  uint32_t next_wallpaper_ms = 0;
  uint32_t next_slot_refresh_ms = 0;
  String slot_payloads[TILES_PER_GRID];
  // Einheit separat merken: Der Wert kann schon per retained State da sein,
  // bevor der erste Bridge-Sync die HA-Metadaten (z. B. kWh) liefert. Nur den
  // Payload zu vergleichen wuerde die spaeter eintreffende Einheit dauerhaft
  // verschlucken.
  String slot_units[TILES_PER_GRID];
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  // Fertiger LVGL-Frame fuer ruhige Diawechsel: Wallpaper, Uhr, Kacheln und
  // ein eventuell offenes Popup werden zuerst unsichtbar in PSRAM gerendert
  // und erst danach gemeinsam ueber den geraetespezifischen Vollbildpfad auf
  // das Panel geschrieben (PPA-Rotation bei Tab5/8-Zoll, direkte Kopie beim B4).
  uint8_t* composite_storage = nullptr;
  size_t composite_storage_size = 0;
  lv_draw_buf_t composite_draw_buf{};
  bool composite_draw_buf_ready = false;
#endif
};

ScreensaverState* g_state = nullptr;
// Web-Admin-Saves laufen im loopTask, LVGL-Aenderungen werden aber erst im
// naechsten LVGL-Timer-Tick ausgefuehrt. So bleibt der HTTP-Handler frei von
// direkten Objekt-Lebenszeit-Aenderungen und der sichtbare Overlay-State wird
// niemals geloescht/neu angelegt.
bool g_live_config_refresh_requested = false;
bool g_live_grid_refresh_requested = false;
String g_live_preview_wallpaper;

// Ein-Slot-Cache: das dekodierte Wallpaper bleibt in PSRAM (~2 MB), damit
// das Oeffnen nach dem ersten Mal sofort geht. Ersetzt wird der Slot erst,
// wenn ein anderes Bild (oder eine andere Zielgroesse) gebraucht wird.
// Achtung: gleicher Dateiname mit neuem Inhalt wird bis zum Neustart nicht
// erkannt — der Cache ist nur ueber den Namen gekeyt.
String g_cache_name;
uint16_t g_cache_w = 0;
uint16_t g_cache_h = 0;
uint16_t g_cache_focus_x = 500;
uint16_t g_cache_focus_y = 500;
uint16_t g_cache_zoom = 1000;
lv_image_dsc_t* g_cache_dsc = nullptr;

// Preload: ein paar Sekunden nach dem Kachel-Aufbau wird das Bild einmal
// vordekodiert, damit schon der erste Tap ohne SD-/Decode-Wartezeit oeffnet.
ScreensaverWallpaperConfig g_preload_wallpaper;
lv_timer_t* g_preload_timer = nullptr;
scene_publish_cb_t g_scene_callback = nullptr;

void* alloc_prefer_psram(size_t bytes) {
  if (!bytes) return nullptr;
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  return p;
}

void* alloc_aligned_prefer_psram(size_t bytes) {
  if (!bytes) return nullptr;
  void* p = heap_caps_aligned_alloc(
      kPpaBufferAlignment, bytes,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!p) {
    p = heap_caps_aligned_alloc(
        kPpaBufferAlignment, bytes,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  }
  // Die Vorschau ist optional. Scheitert nur ihre strengere Ausrichtung,
  // muss der bewaehrte LVGL-Pfad trotzdem weiterhin ein Bild bekommen.
  if (!p) p = alloc_prefer_psram(bytes);
  return p;
}

bool psram_budget_ok(size_t needed) {
  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const size_t free_total = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  return largest >= needed && free_total >= needed + kPsramReserveBytes;
}

void free_screensaver_dsc(lv_image_dsc_t*& dsc) {
  if (!dsc) return;
  // LVGL cached Dekodier-Ergebnisse per Pointer-Adresse; ohne Drop kann ein
  // spaeterer malloc auf derselben Adresse das alte Bild treffen (gleiche
  // Falle wie beim Media-Cover, siehe free_media_cover_dsc).
  lv_image_cache_drop(dsc);
  if (dsc->data) {
    free(const_cast<uint8_t*>(dsc->data));
  }
  free(dsc);
  dsc = nullptr;
}

void set_image_src_without_invalidation(lv_obj_t* image, const void* src) {
  if (!image) return;
  lv_display_t* display = lv_obj_get_display(image);
  if (display) lv_display_enable_invalidation(display, false);
  lv_image_set_src(image, src);
  if (display) lv_display_enable_invalidation(display, true);
}

#if defined(CONFIG_IDF_TARGET_ESP32P4)
bool ensure_composite_draw_buf(ScreensaverState* st) {
  if (!st) return false;
  const uint32_t width = Device::kScreenWidth;
  const uint32_t height = Device::kScreenHeight;
  const uint32_t stride = width * sizeof(uint16_t);
  const size_t needed = static_cast<size_t>(stride) * height;
  if (needed > UINT32_MAX) return false;

  if (st->composite_draw_buf_ready && st->composite_storage &&
      st->composite_storage_size >= needed) {
    return true;
  }

  if (st->composite_storage) {
    free(st->composite_storage);
    st->composite_storage = nullptr;
  }
  st->composite_storage_size = 0;
  st->composite_draw_buf_ready = false;

  st->composite_storage = static_cast<uint8_t*>(heap_caps_aligned_alloc(
      kPpaBufferAlignment, needed,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
  if (!st->composite_storage) {
    Serial.printf("[Screensaver] Composite-Puffer fehlt: %u Bytes\n",
                  static_cast<unsigned>(needed));
    return false;
  }

  if (lv_draw_buf_init(&st->composite_draw_buf, width, height,
                       LV_COLOR_FORMAT_RGB565, stride,
                       st->composite_storage,
                       static_cast<uint32_t>(needed)) != LV_RESULT_OK) {
    free(st->composite_storage);
    st->composite_storage = nullptr;
    return false;
  }
  st->composite_storage_size = needed;
  st->composite_draw_buf_ready = true;
  return true;
}

bool present_composited_screensaver_frame(ScreensaverState* st) {
  if (!st || !st->overlay || !ensure_composite_draw_buf(st)) return false;
  lv_display_t* display = lv_obj_get_display(st->overlay);
  lv_obj_t* top_layer = display ? lv_display_get_layer_top(display) : nullptr;
  if (!top_layer) return false;

  const uint32_t snapshot_started = millis();
  // Der komplette Top-Layer ist absichtlich die Quelle. So bleiben Uhr und
  // Kacheln beim Diawechsel sichtbar und werden gemeinsam praesentiert.
  if (lv_snapshot_take_to_draw_buf(top_layer, LV_COLOR_FORMAT_RGB565,
                                   &st->composite_draw_buf) != LV_RESULT_OK) {
    Serial.println("[Screensaver] Composite-Snapshot fehlgeschlagen");
    return false;
  }

  const bool preview_ok = Device::displayTryFullFramePreview(
      0, 0, Device::kScreenWidth, Device::kScreenHeight,
      reinterpret_cast<const uint16_t*>(st->composite_draw_buf.data),
      st->composite_draw_buf.data_size,
      false);  // Snapshot ist natives RGB565, nicht RGB565_SWAPPED.
  // A slideshow frame needs one atomic swap only. Do not leave the persistent
  // camera mirroring state active while the normal UI resumes underneath it.
  Device::displayEndFullFramePreview();
  Serial.printf("[Screensaver] Composite-Preview %s in %u ms\n",
                preview_ok ? "OK" : "übersprungen",
                static_cast<unsigned>(millis() - snapshot_started));
  return preview_ok;
}
#endif

bool is_jpeg(const uint8_t* data, size_t len) {
  return data && len >= 3 && data[0] == 0xFF && data[1] == 0xD8;
}

// --- Datei von SD in einen PSRAM-Puffer lesen (chunked) ---
uint8_t* read_wallpaper_file(const String& file_name, size_t& out_len) {
  out_len = 0;
  if (!Device::sdReady()) {
    Serial.println("[Screensaver] microSD nicht bereit");
    return nullptr;
  }
  String path = String(kImageDir) + "/" + file_name;
  // v0.5.0 uses /images. Existing cards/configurations from older releases
  // continue to work without moving files immediately.
  if (!Device::sdFS().exists(path)) {
    const String legacy_path = String(kLegacyWallpaperDir) + "/" + file_name;
    if (Device::sdFS().exists(legacy_path)) path = legacy_path;
  }
  fs::File f = Device::sdFS().open(path, FILE_READ);
  if (!f) {
    Serial.printf("[Screensaver] open fail: '%s'\n", path.c_str());
    return nullptr;
  }
  const size_t len = f.size();
  if (len < 32 || len > kMaxFileBytes) {
    Serial.printf("[Screensaver] Dateigroesse ungueltig: %u Bytes\n",
                  static_cast<unsigned>(len));
    f.close();
    return nullptr;
  }
  uint8_t* buf = static_cast<uint8_t*>(alloc_prefer_psram(len));
  if (!buf) {
    Serial.println("[Screensaver] Dateipuffer-Alloc fehlgeschlagen");
    f.close();
    return nullptr;
  }
  size_t pos = 0;
  while (pos < len) {
    size_t chunk = len - pos;
    if (chunk > 64U * 1024U) chunk = 64U * 1024U;
    const size_t got = f.read(buf + pos, chunk);
    if (got == 0) break;
    pos += got;
  }
  f.close();
  if (pos != len) {
    Serial.printf("[Screensaver] Kurz gelesen: %u/%u Bytes\n",
                  static_cast<unsigned>(pos), static_cast<unsigned>(len));
    free(buf);
    return nullptr;
  }
  out_len = len;
  return buf;
}

#if defined(CONFIG_IDF_TARGET_ESP32P4) && SOC_JPEG_DECODE_SUPPORTED
// HW-Decode wie beim Media-Cover (tile_renderer.cpp): 16-aligned Dimensionen,
// RGB-Order = big-endian RGB565 = LV_COLOR_FORMAT_RGB565_SWAPPED. Nicht
// passende Bilder fallen auf TJpgDec zurueck. Rueckgabe: malloc-Puffer mit
// out_w*out_h Pixeln (Aufrufer free()t).
uint16_t* hw_decode_jpeg(const uint8_t* data, size_t len,
                         uint16_t& out_w, uint16_t& out_h) {
  if (len > UINT32_MAX) return nullptr;

  jpeg_decode_picture_info_t info{};
  esp_err_t err = jpeg_decoder_get_info(data, static_cast<uint32_t>(len), &info);
  if (err != ESP_OK || info.width == 0 || info.height == 0) return nullptr;
  if ((info.width & 15U) != 0 || (info.height & 15U) != 0) return nullptr;

  const uint32_t pixels = static_cast<uint32_t>(info.width) * info.height;
  if (pixels > kMaxDecodePixels) return nullptr;

  const size_t requested_bytes = static_cast<size_t>(pixels) * sizeof(uint16_t);
  if (!psram_budget_ok(requested_bytes)) {
    Serial.printf("[Screensaver] PSRAM-Budget zu klein fuer %u Bytes\n",
                  static_cast<unsigned>(requested_bytes));
    return nullptr;
  }

  jpeg_decode_memory_alloc_cfg_t mem_cfg{};
  mem_cfg.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER;
  size_t allocated_bytes = 0;
  uint16_t* decoded = static_cast<uint16_t*>(
      jpeg_alloc_decoder_mem(requested_bytes, &mem_cfg, &allocated_bytes));
  if (!decoded || allocated_bytes < requested_bytes) {
    free(decoded);
    Serial.printf("[Screensaver] HW JPEG PSRAM-Puffer fehlt: %u Bytes\n",
                  static_cast<unsigned>(requested_bytes));
    return nullptr;
  }

  // Persistente Engine wie beim Media-Cover: Engine-Auf-/Abbau pro Bild
  // acquired/released den 2D-DMA-Pool, den sich der JPEG-Block mit der PPA
  // teilt — dieses Churn stand auf dem Tab5 im Verdacht, Transaktionen zu
  // verlieren ("PPA VERKLEMMT").
  static jpeg_decoder_handle_t s_engine = nullptr;
  if (!s_engine) {
    jpeg_decode_engine_cfg_t engine_cfg{};
    engine_cfg.intr_priority = 0;
    // Vollbilder brauchen laenger als ein 240er-Cover.
    engine_cfg.timeout_ms = 500;
    err = jpeg_new_decoder_engine(&engine_cfg, &s_engine);
    if (err != ESP_OK || !s_engine) {
      s_engine = nullptr;
      free(decoded);
      Serial.printf("[Screensaver] HW JPEG Engine nicht verfuegbar: %s\n",
                    esp_err_to_name(err));
      return nullptr;
    }
  }

  jpeg_decode_cfg_t decode_cfg{};
  decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
  decode_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB;
  decode_cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
  uint32_t decoded_bytes = 0;
  {
    // Nie parallel zu einer laufenden PPA-Rotation dekodieren (geteilter
    // 2D-DMA-Pool, siehe dma2d_arbiter.h).
    Dma2dArbiterGuard dma2d_guard(2000);
    if (!dma2d_guard.locked()) {
      Serial.println("[Screensaver] 2D-DMA-Arbiter Timeout, decode ungeschuetzt");
    }
    err = jpeg_decoder_process(s_engine, &decode_cfg, data,
                               static_cast<uint32_t>(len),
                               reinterpret_cast<uint8_t*>(decoded),
                               static_cast<uint32_t>(allocated_bytes),
                               &decoded_bytes);
  }
  if (err != ESP_OK || decoded_bytes < requested_bytes) {
    free(decoded);
    Serial.printf("[Screensaver] HW JPEG decode fehlgeschlagen: %s bytes=%u/%u\n",
                  esp_err_to_name(err),
                  static_cast<unsigned>(decoded_bytes),
                  static_cast<unsigned>(requested_bytes));
    // Wie beim Media-Cover: Engine nach Fehlschlag verwerfen statt mit
    // moeglicherweise haengendem Zustand weiterzuarbeiten.
    jpeg_del_decoder_engine(s_engine);
    s_engine = nullptr;
    return nullptr;
  }

  out_w = static_cast<uint16_t>(info.width);
  out_h = static_cast<uint16_t>(info.height);
  return decoded;
}
#endif

// --- TJpgDec-Fallback (nicht-16-aligned oder sehr grosse Bilder) ---
struct SwJpegCtx {
  const uint8_t* data = nullptr;
  size_t len = 0;
  size_t pos = 0;
  uint16_t* pixels = nullptr;
  uint16_t w = 0;
  uint16_t h = 0;
};

size_t sw_jpeg_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  SwJpegCtx* ctx = static_cast<SwJpegCtx*>(jd->device);
  if (!ctx || !ctx->data || ctx->pos >= ctx->len) return 0;
  size_t remain = ctx->len - ctx->pos;
  size_t take = (ndata < remain) ? ndata : remain;
  if (buff && take) {
    memcpy(buff, ctx->data + ctx->pos, take);
  }
  ctx->pos += take;
  return take;
}

int sw_jpeg_output(JDEC* jd, void* bitmap, JRECT* rect) {
  SwJpegCtx* ctx = static_cast<SwJpegCtx*>(jd->device);
  if (!ctx || !ctx->pixels || !bitmap) return 0;

  // Gleiche Byte-Reihenfolge wie beim Media-Cover: Ergebnis ist
  // LV_COLOR_FORMAT_RGB565_SWAPPED.
  const uint8_t* src = static_cast<const uint8_t*>(bitmap);
  const uint16_t rw = rect->right - rect->left + 1;
  for (uint16_t y = rect->top; y <= rect->bottom && y < ctx->h; ++y) {
    for (uint16_t x = rect->left; x <= rect->right && x < ctx->w; ++x) {
      const size_t si = ((y - rect->top) * rw + (x - rect->left)) * 3;
      const uint8_t b = src[si];
      const uint8_t g = src[si + 1];
      const uint8_t r = src[si + 2];
      const uint16_t c = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      ctx->pixels[static_cast<size_t>(y) * ctx->w + x] =
          static_cast<uint16_t>((c >> 8) | (c << 8));
    }
  }
  return 1;
}

uint16_t* sw_decode_jpeg(const uint8_t* data, size_t len,
                         uint16_t& out_w, uint16_t& out_h) {
  uint8_t* work = static_cast<uint8_t*>(heap_caps_malloc(4096, MALLOC_CAP_8BIT));
  if (!work) return nullptr;

  SwJpegCtx ctx{};
  ctx.data = data;
  ctx.len = len;

  JDEC jd;
  JRESULT rc = jd_prepare(&jd, sw_jpeg_input, work, 4096, &ctx);
  if (rc != JDR_OK || jd.width == 0 || jd.height == 0 ||
      jd.width > UINT16_MAX || jd.height > UINT16_MAX) {
    Serial.printf("[Screensaver] SW JPEG prepare fehlgeschlagen: %d\n",
                  static_cast<int>(rc));
    free(work);
    return nullptr;
  }

  // TJpgDec kann beim Dekodieren 1/2, 1/4, 1/8 skalieren — genug, um auch
  // grosse Bilder unter das Pixel-Budget zu bringen.
  uint8_t scale = 0;
  while (scale < 3 &&
         (static_cast<uint32_t>(jd.width >> scale) *
          static_cast<uint32_t>(jd.height >> scale)) > kMaxDecodePixels) {
    ++scale;
  }
  const uint16_t w = static_cast<uint16_t>(jd.width >> scale);
  const uint16_t h = static_cast<uint16_t>(jd.height >> scale);
  const uint32_t pixels = static_cast<uint32_t>(w) * h;
  if (w == 0 || h == 0 || pixels > kMaxDecodePixels) {
    Serial.printf("[Screensaver] JPEG zu gross: %ux%u\n",
                  static_cast<unsigned>(jd.width),
                  static_cast<unsigned>(jd.height));
    free(work);
    return nullptr;
  }

  const size_t bytes = static_cast<size_t>(pixels) * sizeof(uint16_t);
  if (!psram_budget_ok(bytes)) {
    Serial.printf("[Screensaver] PSRAM-Budget zu klein fuer %u Bytes\n",
                  static_cast<unsigned>(bytes));
    free(work);
    return nullptr;
  }
  uint16_t* out = static_cast<uint16_t*>(alloc_prefer_psram(bytes));
  if (!out) {
    free(work);
    return nullptr;
  }
  memset(out, 0, bytes);

  ctx.pixels = out;
  ctx.w = w;
  ctx.h = h;
  rc = jd_decomp(&jd, sw_jpeg_output, scale);
  free(work);
  if (rc != JDR_OK) {
    Serial.printf("[Screensaver] SW JPEG decode fehlgeschlagen: %d\n",
                  static_cast<int>(rc));
    free(out);
    return nullptr;
  }

  out_w = w;
  out_h = h;
  return out;
}

// Anteil eines Pixels innerhalb einer abgerundeten Ecke, mit 4x4
// Supersampling. 16 = voll sichtbar, 0 = komplett Rahmenfarbe.
uint8_t rounded_pixel_coverage(uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h,
                               uint16_t radius) {
  if (radius == 0 || w < radius * 2 || h < radius * 2) return 16;
  const bool at_left = x < radius;
  const bool at_right = x >= w - radius;
  const bool at_top = y < radius;
  const bool at_bottom = y >= h - radius;
  if ((!at_left && !at_right) || (!at_top && !at_bottom)) return 16;

  const uint16_t edge_x = at_left ? x : static_cast<uint16_t>(w - 1 - x);
  const uint16_t edge_y = at_top ? y : static_cast<uint16_t>(h - 1 - y);
  constexpr int32_t kSamples = 4;
  constexpr int32_t kUnitsPerPixel = kSamples * 2;
  const int32_t center = static_cast<int32_t>(radius) * kUnitsPerPixel;
  const int32_t radius_sq = center * center;
  uint8_t covered = 0;
  for (int32_t sample_y = 0; sample_y < kSamples; ++sample_y) {
    const int32_t py = static_cast<int32_t>(edge_y) * kUnitsPerPixel +
                       sample_y * 2 + 1;
    const int32_t dy = center - py;
    for (int32_t sample_x = 0; sample_x < kSamples; ++sample_x) {
      const int32_t px = static_cast<int32_t>(edge_x) * kUnitsPerPixel +
                         sample_x * 2 + 1;
      const int32_t dx = center - px;
      if (dx * dx + dy * dy <= radius_sq) ++covered;
    }
  }
  return covered;
}

uint16_t blend_swapped_rgb565_with_black(uint16_t swapped, uint8_t coverage) {
  if (coverage == 0) return 0;
  if (coverage >= 16) return swapped;
  const uint16_t color =
      static_cast<uint16_t>((swapped >> 8) | (swapped << 8));
  const uint16_t red =
      static_cast<uint16_t>((((color >> 11) & 0x1F) * coverage + 8) / 16);
  const uint16_t green =
      static_cast<uint16_t>((((color >> 5) & 0x3F) * coverage + 8) / 16);
  const uint16_t blue =
      static_cast<uint16_t>(((color & 0x1F) * coverage + 8) / 16);
  const uint16_t blended =
      static_cast<uint16_t>((red << 11) | (green << 5) | blue);
  return static_cast<uint16_t>((blended >> 8) | (blended << 8));
}

// Fertiges Bildschirmbild erzeugen: aussen derselbe schwarze Rahmen wie beim
// normalen Kachel-Grid. Die Bildkante liegt exakt 4 px vor der Kachelkante
// (GRID_PAD - 4), mit bereits eingerechnetem 26-px-Radius: 22 px wie die
// Kacheln plus die 4 px, um die das Bild weiter aussen liegt. PPA und LVGL
// verwenden denselben Vollbildpuffer, sodass beim Oeffnen nichts nachtraeglich
// geclippt oder in mehreren Streifen gezeichnet werden muss.
lv_image_dsc_t* make_cover_dsc(const uint16_t* src, uint16_t src_w,
                               uint16_t src_h, uint16_t target_w,
                               uint16_t target_h, uint16_t focus_x,
                               uint16_t focus_y, uint16_t zoom) {
  if (!src || src_w == 0 || src_h == 0 || target_w == 0 || target_h == 0) {
    return nullptr;
  }
  const uint16_t image_inset = GRID_PAD > 4 ? GRID_PAD - 4 : 0;
  if (target_w <= image_inset * 2 || target_h <= image_inset * 2) {
    return nullptr;
  }
  const uint16_t image_w = target_w - image_inset * 2;
  const uint16_t image_h = target_h - image_inset * 2;

  uint32_t crop_w = src_w;
  uint32_t crop_h =
      static_cast<uint32_t>((static_cast<uint64_t>(crop_w) * image_h) / image_w);
  if (crop_h > src_h || crop_h == 0) {
    crop_h = src_h;
    crop_w =
        static_cast<uint32_t>((static_cast<uint64_t>(crop_h) * image_w) / image_h);
    if (crop_w > src_w) crop_w = src_w;
  }
  if (crop_w == 0 || crop_h == 0) return nullptr;
  if (zoom < 1000) zoom = 1000;
  if (zoom > 3000) zoom = 3000;
  crop_w = (crop_w * 1000U) / zoom;
  crop_h = (crop_h * 1000U) / zoom;
  if (crop_w == 0) crop_w = 1;
  if (crop_h == 0) crop_h = 1;
  if (crop_w > src_w) crop_w = src_w;
  if (crop_h > src_h) crop_h = src_h;
  if (focus_x > 1000) focus_x = 1000;
  if (focus_y > 1000) focus_y = 1000;
  const uint32_t x0 = ((src_w - crop_w) * focus_x) / 1000U;
  const uint32_t y0 = ((src_h - crop_h) * focus_y) / 1000U;

  const size_t bytes = static_cast<size_t>(target_w) * target_h * sizeof(uint16_t);
  // Der fertige Cache darf auf dem 8-Zoll-Geraet direkt als PPA-Eingabe
  // dienen. 64-Byte-Ausrichtung erfuellt zugleich die Cache-/DMA-Anforderung;
  // fuer LVGL und die anderen Targets aendert sich das Datenformat nicht.
  uint16_t* out = static_cast<uint16_t*>(alloc_aligned_prefer_psram(bytes));
  if (!out) return nullptr;
  memset(out, 0, bytes);

  for (uint16_t y = 0; y < image_h; ++y) {
    const uint32_t sy = y0 + (static_cast<uint32_t>(y) * crop_h) / image_h;
    const uint16_t* src_row = src + static_cast<size_t>(sy) * src_w;
    uint16_t* dst_row = out +
        static_cast<size_t>(y + image_inset) * target_w + image_inset;
    for (uint16_t x = 0; x < image_w; ++x) {
      const uint32_t sx = x0 + (static_cast<uint32_t>(x) * crop_w) / image_w;
      const uint8_t coverage =
          rounded_pixel_coverage(x, y, image_w, image_h, kImageRadius);
      dst_row[x] = blend_swapped_rgb565_with_black(src_row[sx], coverage);
    }
  }

  lv_image_dsc_t* dsc = static_cast<lv_image_dsc_t*>(malloc(sizeof(lv_image_dsc_t)));
  if (!dsc) {
    free(out);
    return nullptr;
  }
  memset(dsc, 0, sizeof(*dsc));
  dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc->header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  dsc->header.w = target_w;
  dsc->header.h = target_h;
  dsc->header.stride = target_w * 2;
  dsc->data_size = bytes;
  dsc->data = reinterpret_cast<const uint8_t*>(out);
  return dsc;
}

lv_image_dsc_t* decode_wallpaper_to_size(const String& file_name,
                                         uint16_t target_w, uint16_t target_h,
                                         uint16_t focus_x, uint16_t focus_y,
                                         uint16_t zoom) {
  size_t len = 0;
  uint8_t* file = read_wallpaper_file(file_name, len);
  if (!file) return nullptr;
  if (!is_jpeg(file, len)) {
    Serial.println("[Screensaver] Datei ist kein JPEG");
    free(file);
    return nullptr;
  }

  const uint32_t started_ms = millis();
  uint16_t w = 0;
  uint16_t h = 0;
  uint16_t* pixels = nullptr;
  bool hw = false;
#if defined(CONFIG_IDF_TARGET_ESP32P4) && SOC_JPEG_DECODE_SUPPORTED
  pixels = hw_decode_jpeg(file, len, w, h);
  hw = pixels != nullptr;
#endif
  if (!pixels) {
    pixels = sw_decode_jpeg(file, len, w, h);
  }
  free(file);
  if (!pixels) return nullptr;

  lv_image_dsc_t* dsc = make_cover_dsc(pixels, w, h, target_w, target_h,
                                       focus_x, focus_y, zoom);
  free(pixels);
  if (dsc) {
    Serial.printf("[Screensaver] %s-Decode %ux%u -> %ux%u in %u ms\n",
                  hw ? "HW" : "SW",
                  static_cast<unsigned>(w), static_cast<unsigned>(h),
                  static_cast<unsigned>(target_w), static_cast<unsigned>(target_h),
                  static_cast<unsigned>(millis() - started_ms));
  }
  return dsc;
}

lv_image_dsc_t* get_or_decode_cached(const ScreensaverWallpaperConfig& wallpaper,
                                     uint16_t w, uint16_t h,
                                     lv_obj_t* visible_image = nullptr) {
  const String& name = wallpaper.file_name;
  if (g_cache_dsc && g_cache_name == name && g_cache_w == w && g_cache_h == h &&
      g_cache_focus_x == wallpaper.focus_x &&
      g_cache_focus_y == wallpaper.focus_y && g_cache_zoom == wallpaper.zoom) {
    return g_cache_dsc;
  }
  lv_image_dsc_t* dsc = decode_wallpaper_to_size(
      name, w, h, wallpaper.focus_x, wallpaper.focus_y, wallpaper.zoom);
  if (!dsc) return nullptr;
  // Erst jetzt, unmittelbar vor dem Cache-Tausch, die alte Quelle vom
  // sichtbaren LVGL-Objekt loesen. So bleibt das vorherige Dia waehrend des
  // Decodes stehen, zeigt aber niemals auf bereits freigegebenen Speicher.
  if (visible_image && g_cache_dsc) {
    set_image_src_without_invalidation(visible_image, nullptr);
  }
  // Alten Slot erst nach erfolgreichem Decode ersetzen; Aufrufer stellen
  // sicher, dass kein Overlay den alten Puffer mehr anzeigt.
  free_screensaver_dsc(g_cache_dsc);
  g_cache_dsc = dsc;
  g_cache_name = name;
  g_cache_w = w;
  g_cache_h = h;
  g_cache_focus_x = wallpaper.focus_x;
  g_cache_focus_y = wallpaper.focus_y;
  g_cache_zoom = wallpaper.zoom;
  return dsc;
}


bool is_wallpaper_file(const String& file_name) {
  if (!file_name.length() || file_name.indexOf('/') >= 0 ||
      file_name.indexOf('\\') >= 0 || file_name.indexOf("..") >= 0) {
    return false;
  }
  String lower = file_name;
  lower.toLowerCase();
  return lower.endsWith(".jpg") || lower.endsWith(".jpeg");
}

// is_wallpaper_file prueft nur das Namensmuster, nicht ob die Datei noch auf
// der SD-Karte liegt. Ohne diesen Check wuerde apply_wallpaper() bei einem
// zwischenzeitlich geloeschten, aber weiterhin in der Config gelisteten Bild
// die Fallback-Suche ueberspringen (from_config waere trotzdem true) und
// direkt scheitern, obwohl andere gueltige Bilder auf der Karte liegen.
bool sd_wallpaper_file_exists(const String& file_name) {
  if (!is_wallpaper_file(file_name) || !Device::sdReady()) return false;
  return Device::sdFS().exists(String(kImageDir) + "/" + file_name) ||
         Device::sdFS().exists(String(kLegacyWallpaperDir) + "/" + file_name);
}

bool find_first_sd_wallpaper(ScreensaverWallpaperConfig& out) {
  if (!Device::sdReady()) return false;
  const char* directories[] = {kImageDir, kLegacyWallpaperDir};
  for (const char* directory : directories) {
    fs::File dir = Device::sdFS().open(directory, FILE_READ);
    // Bei der Waveshare-SDMMC-Implementierung kann isDirectory() fuer einen
    // erfolgreich geoeffneten Ordner false liefern. openNextFile() ist hier
    // deshalb (wie im Dateimanager) die verlaessliche Pruefung.
    if (!dir) continue;
    fs::File entry = dir.openNextFile();
    while (entry) {
      String name = entry.name();
      const int slash = max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
      if (slash >= 0) name = name.substring(slash + 1);
      const bool usable = !entry.isDirectory() && is_wallpaper_file(name);
      entry.close();
      if (usable) {
        out = ScreensaverWallpaperConfig{};
        out.file_name = name;
        dir.close();
        return true;
      }
      entry = dir.openNextFile();
    }
    dir.close();
  }
  return false;
}

int first_enabled_wallpaper() {
  const auto& wallpapers = screensaverConfig.get().wallpapers;
  for (size_t i = 0; i < wallpapers.size(); ++i) {
    if (wallpapers[i].enabled && is_wallpaper_file(wallpapers[i].file_name)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int next_enabled_wallpaper(int current) {
  const auto& config = screensaverConfig.get();
  const size_t count = config.wallpapers.size();
  if (count == 0) return -1;
  if (config.shuffle) {
    int choices[kMaxScreensaverWallpapers];
    size_t choice_count = 0;
    for (size_t i = 0; i < count; ++i) {
      if (config.wallpapers[i].enabled &&
          is_wallpaper_file(config.wallpapers[i].file_name) &&
          (static_cast<int>(i) != current || count == 1)) {
        choices[choice_count++] = static_cast<int>(i);
      }
    }
    if (choice_count) return choices[random(choice_count)];
  }
  for (size_t step = 1; step <= count; ++step) {
    const size_t i = (static_cast<size_t>(current < 0 ? 0 : current) + step) % count;
    if (config.wallpapers[i].enabled && is_wallpaper_file(config.wallpapers[i].file_name)) {
      return static_cast<int>(i);
    }
  }
  return first_enabled_wallpaper();
}

void position_global_clock(ScreensaverState* st) {
  if (!st || !st->clock_box) return;
  const ScreensaverConfigData& config = screensaverConfig.get();
  const int x = (static_cast<int>(config.clock_x) * Device::kScreenWidth) / 1000;
  const int y = (static_cast<int>(config.clock_y) * Device::kScreenHeight) / 1000;
  lv_obj_set_pos(st->clock_box, x - lv_obj_get_width(st->clock_box) / 2,
                 y - lv_obj_get_height(st->clock_box) / 2);
}

void on_global_clock_size_changed(lv_event_t* e) {
  ScreensaverState* st =
      static_cast<ScreensaverState*>(lv_event_get_user_data(e));
  if (!st || lv_event_get_target(e) != st->clock_box) return;
  // Beim sehr frühen Öffnen ist die Systemzeit eventuell noch unbekannt.
  // Dann entsteht die Uhr zunächst fast ohne Breite. Sobald NTP die erste
  // Zeit liefert und die Labels wachsen, den gespeicherten Mittelpunkt erneut
  // anwenden, statt den Text von dort nach rechts aus dem Display wachsen zu
  // lassen.
  position_global_clock(st);
}

void rebuild_global_clock(ScreensaverState* st) {
  if (!st || !st->overlay) return;

  // Die Uhr ist ein eigenstaendiger Kindbaum. Synchrones Loeschen innerhalb
  // des LVGL-Timers ist hier sicher und laesst das globale Overlay samt
  // Click-/Popup-Lebenszeit unangetastet.
  if (st->clock_box) {
    lv_obj_delete(st->clock_box);
    st->clock_box = nullptr;
  }

  const ScreensaverConfigData& config = screensaverConfig.get();
  if (!config.show_time && !config.show_date && !config.show_weekday) return;

  ClockWidgetConfig widget_config;
  widget_config.show_time = config.show_time;
  widget_config.show_date = config.show_date;
  widget_config.show_weekday = config.show_weekday;
  widget_config.weekday_language = configManager.getConfig().language;
  widget_config.fill_parent = false;
  widget_config.text_shadow = config.clock_shadow;
  widget_config.time_font_size = config.time_font_size;
  widget_config.date_font_size = config.date_font_size;
  widget_config.time_alignment = config.time_alignment;
  widget_config.date_alignment = config.date_alignment;
  widget_config.time_format = clock_tile::resolve_time_format(
      config.time_format, configManager.getConfig().global_time_format,
      configManager.getConfig().language);
  widget_config.date_format = clock_tile::resolve_date_format(
      config.date_format, configManager.getConfig().global_date_format,
      configManager.getConfig().language);
  st->clock_box = create_clock_widget(st->overlay, widget_config);
  if (!st->clock_box) return;
  lv_obj_add_event_cb(st->clock_box, on_global_clock_size_changed,
                      LV_EVENT_SIZE_CHANGED, st);
  lv_obj_update_layout(st->clock_box);
  position_global_clock(st);
}

bool apply_wallpaper(ScreensaverState* st, int index, bool allow_fallback,
                     bool allow_disabled = false) {
  if (!st || !st->image) return false;
  ScreensaverWallpaperConfig wallpaper;
  bool from_config = false;
  const auto& config = screensaverConfig.get();
  if (config.use_wallpapers && index >= 0 &&
      static_cast<size_t>(index) < config.wallpapers.size()) {
    wallpaper = config.wallpapers[static_cast<size_t>(index)];
    from_config = (wallpaper.enabled || allow_disabled) &&
                  sd_wallpaper_file_exists(wallpaper.file_name);
  }
  if (!from_config && allow_fallback && config.use_wallpapers) {
    if (!find_first_sd_wallpaper(wallpaper)) return false;
    index = -1;
  } else if (!from_config) {
    return false;
  }

  const bool cache_hit = g_cache_dsc && g_cache_name == wallpaper.file_name &&
                         g_cache_w == Device::kScreenWidth &&
                         g_cache_h == Device::kScreenHeight &&
                         g_cache_focus_x == wallpaper.focus_x &&
                         g_cache_focus_y == wallpaper.focus_y &&
                         g_cache_zoom == wallpaper.zoom;
  lv_image_dsc_t* dsc = get_or_decode_cached(
      wallpaper, Device::kScreenWidth, Device::kScreenHeight, st->image);
  if (!dsc) return false;
  // Die Quelle intern sofort umstellen, aber noch keinen bildschirmgrossen
  // LVGL-Redraw ausloesen. Auf den gedrehten P4-Geraeten rendert LVGL daraus
  // zunaechst den unsichtbaren Gesamtframe; nur dieser geht danach an PPA.
  set_image_src_without_invalidation(st->image, dsc);
  lv_obj_remove_flag(st->image, LV_OBJ_FLAG_HIDDEN);
  st->active_wallpaper = index;
  st->active_wallpaper_name = wallpaper.file_name;
  st->next_wallpaper_ms = millis() +
      static_cast<uint32_t>(screensaverConfig.get().duration_seconds) * 1000U;

  // Auf den gedrehten P4-Geraeten darf PPA nicht zuerst nur das Wallpaper auf
  // den sichtbaren Puffer schreiben: dabei waeren Uhr und Kacheln bis zu ihrem
  // anschliessenden LVGL-Redraw wirklich weg und wuerden sichtbar blinken.
  // Stattdessen wird der komplette Top-Layer unsichtbar zusammengesetzt und
  // als fertiger Frame praesentiert. Beim B4 geschieht das ohne Rotation
  // direkt in dessen nativen DSI-Framebuffer.
  const uint32_t started = millis();
  bool preview_ok = false;
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  preview_ok = present_composited_screensaver_frame(st);
#endif
  Serial.printf("[Screensaver] Hardware-Preview %s (%s) in %u ms\n",
                preview_ok ? "OK" : "übersprungen",
                cache_hit ? "cache" : "decode",
                static_cast<unsigned>(millis() - started));
  if (!preview_ok) {
    // Falls der geraetespezifische Vollbildpfad nicht verfuegbar ist, zeichnet
    // LVGL das neue Bild weiterhin sicher wie bisher selbst.
    lv_obj_invalidate(st->image);
  }
  return true;
}

void refresh_slot_values(ScreensaverState* st) {
  if (!st) return;
  const TileGridConfig& grid = screensaverConfig.tileGrid();
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    if (!tile.sensor_entity.length()) continue;
    String payload;
    if (!tiles_get_cached_entity_payload(tile.sensor_entity.c_str(), payload)) {
      payload = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
    }
    if (!payload.length()) continue;
    if (tile.type == TILE_SENSOR || tile.type == TILE_ENERGY) {
      String unit = tile.sensor_unit;
      if (!unit.length()) unit = haBridgeConfig.findSensorUnit(tile.sensor_entity);
      if (!unit.length() && tile.type == TILE_ENERGY) {
        unit = energy_find_cached_unit(tile.sensor_entity);
      }
      if (payload == st->slot_payloads[i] && unit == st->slot_units[i]) continue;
      st->slot_payloads[i] = payload;
      st->slot_units[i] = unit;
      queue_sensor_tile_update(GridType::SCREENSAVER, static_cast<uint8_t>(i),
                               payload.c_str(), unit.length() ? unit.c_str() : nullptr);
    } else if (tile.type == TILE_SWITCH) {
      if (payload == st->slot_payloads[i]) continue;
      st->slot_payloads[i] = payload;
      queue_switch_tile_update(GridType::SCREENSAVER, static_cast<uint8_t>(i),
                               payload.c_str());
    } else if (tile.type == TILE_MEDIA) {
      if (payload == st->slot_payloads[i]) continue;
      st->slot_payloads[i] = payload;
      queue_media_tile_update(GridType::SCREENSAVER, static_cast<uint8_t>(i),
                              payload.c_str());
    }
  }
  process_sensor_update_queue();
  process_switch_update_queue();
  process_media_update_queue();
}

// Globale Schattenoption fuer alle Screensaver-Kacheln. Laeuft ueber die
// bestehenden Karten, damit ein Live-Save keinen Grid-Neubau braucht. Voll
// transparente Kacheln bleiben ohne Schatten - dort wuerde nur ein leerer
// Rahmen um unsichtbare Flaeche schweben.
void apply_slot_tile_shadows(ScreensaverState* st) {
  if (!st || !st->slot_grid) return;
  const bool enabled = screensaverConfig.get().tile_shadow;
  const uint32_t count = lv_obj_get_child_count(st->slot_grid);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* card = lv_obj_get_child(st->slot_grid, i);
    if (!card) continue;
    const bool visible_bg =
        lv_obj_get_style_bg_opa(card, LV_PART_MAIN) != LV_OPA_TRANSP;
    if (enabled && visible_bg) {
      lv_obj_set_style_shadow_width(card, 32, 0);
      lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
      lv_obj_set_style_shadow_opa(card, LV_OPA_60, 0);
      lv_obj_set_style_shadow_spread(card, 3, 0);
    } else {
      lv_obj_set_style_shadow_width(card, 0, 0);
      lv_obj_set_style_shadow_opa(card, LV_OPA_TRANSP, 0);
    }
  }
}

void apply_slot_tile_borders(ScreensaverState* st) {
  if (!st || !st->slot_grid) return;
  const bool enabled = screensaverConfig.get().tile_border;
  const uint32_t count = lv_obj_get_child_count(st->slot_grid);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* card = lv_obj_get_child(st->slot_grid, i);
    if (!card) continue;
    ui_surface_style::apply_tile_border(card, enabled);
  }
}

void rebuild_slot_grid(ScreensaverState* st) {
  if (!st || !st->overlay) return;

  reset_sensor_widgets(GridType::SCREENSAVER);
  reset_switch_widgets(GridType::SCREENSAVER);
  reset_media_widgets(GridType::SCREENSAVER);
  if (st->slot_grid) {
    // Den bildschirmgrossen transparenten Container stehen lassen. Sein
    // Loeschen wuerde die komplette darunterliegende Bildflaeche invalidieren
    // und koennte das Wallpaper wieder ueber den langsamen LVGL-Pfad zeichnen.
    // Beim Clean werden nur die tatsaechlichen alten Kachelflaechen dirty.
    lv_obj_clean(st->slot_grid);
  }
  for (String& payload : st->slot_payloads) payload = String();

  // Exakt dieselben Tracks, Abstaende und Aussenabstaende wie im normalen
  // Kachelsystem. Im vorbereiteten Vollbildpuffer beginnt das Bild bei
  // GRID_PAD - 4 und liegt damit an allen Seiten genau 4 px vor den Kacheln.
  static lv_coord_t col_dsc[GRID_COLS + 1];
  static lv_coord_t row_dsc[GRID_ROWS + 1];
  static bool grid_dsc_ready = false;
  if (!grid_dsc_ready) {
    for (uint8_t i = 0; i < GRID_COLS; ++i) col_dsc[i] = GRID_CELL_W;
    col_dsc[GRID_COLS] = LV_GRID_TEMPLATE_LAST;
    for (uint8_t i = 0; i < GRID_ROWS; ++i) row_dsc[i] = GRID_CELL_H;
    row_dsc[GRID_ROWS] = LV_GRID_TEMPLATE_LAST;
    grid_dsc_ready = true;
  }

  if (!st->slot_grid) {
    st->slot_grid = lv_obj_create(st->overlay);
    lv_obj_set_size(st->slot_grid, Device::kScreenWidth, Device::kScreenHeight);
    lv_obj_set_pos(st->slot_grid, 0, 0);
    lv_obj_set_style_bg_opa(st->slot_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(st->slot_grid, 0, 0);
    lv_obj_set_style_pad_all(st->slot_grid, GRID_PAD, 0);
    lv_obj_set_style_pad_column(st->slot_grid, GRID_GAP, 0);
    lv_obj_set_style_pad_row(st->slot_grid, GRID_GAP, 0);
    lv_obj_set_layout(st->slot_grid, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(st->slot_grid, col_dsc, row_dsc);
    lv_obj_remove_flag(st->slot_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(st->slot_grid, LV_OBJ_FLAG_CLICKABLE);
  }

  const TileGridConfig& tile_grid = screensaverConfig.tileGrid();
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    Tile tile = tile_grid.tiles[i];
    if (tile.type == TILE_EMPTY) continue;
    lv_obj_t* tile_obj = render_tile(st->slot_grid, tile.col, tile.row, tile,
                                     static_cast<uint8_t>(i),
                                     GridType::SCREENSAVER, g_scene_callback);
    if (!tile_obj) continue;
    const lv_opa_t opacity = tile.background_opacity;
    lv_obj_set_style_bg_opa(tile_obj, opacity,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(tile_obj, opacity,
                            LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_remove_flag(tile_obj, LV_OBJ_FLAG_EVENT_BUBBLE);
  }

  apply_slot_tile_shadows(st);
  apply_slot_tile_borders(st);

  // Entspricht der Web-Vorschau: Tiles z=2, frei platzierbare Uhr z=3.
  if (st->clock_box) lv_obj_move_foreground(st->clock_box);
}

int find_config_wallpaper(const String& name, bool enabled_only) {
  if (!name.length()) return -1;
  const auto& wallpapers = screensaverConfig.get().wallpapers;
  for (size_t i = 0; i < wallpapers.size(); ++i) {
    if (wallpapers[i].file_name == name &&
        (!enabled_only || wallpapers[i].enabled)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void clear_live_wallpaper(ScreensaverState* st) {
  if (!st || !st->image) return;
  lv_image_set_src(st->image, nullptr);
  lv_obj_add_flag(st->image, LV_OBJ_FLAG_HIDDEN);
  st->active_wallpaper = -1;
  st->active_wallpaper_name = String();
  st->next_wallpaper_ms = 0;
  // Ein vorheriger PPA-Frame liegt direkt im Panelpuffer. Das komplette
  // Overlay invalidieren, damit LVGL ihn sofort schwarz ueberzeichnet und
  // Uhr/Kacheln im selben Frame wieder darstellt.
  lv_obj_invalidate(st->overlay);
}

void refresh_live_background_and_clock(ScreensaverState* st,
                                       const String& preview_wallpaper) {
  if (!st) return;
  rebuild_global_clock(st);
  apply_slot_tile_shadows(st);
  apply_slot_tile_borders(st);

  const ScreensaverConfigData& config = screensaverConfig.get();
  if (!config.use_wallpapers) {
    clear_live_wallpaper(st);
    return;
  }

  bool allow_disabled = false;
  int desired = -1;
  if (preview_wallpaper.length()) {
    desired = find_config_wallpaper(preview_wallpaper, false);
    allow_disabled = desired >= 0;
  }
  if (desired < 0) {
    desired = find_config_wallpaper(st->active_wallpaper_name, true);
  }
  if (desired < 0) desired = first_enabled_wallpaper();

  if (desired >= 0 && static_cast<size_t>(desired) < config.wallpapers.size()) {
    const ScreensaverWallpaperConfig& wallpaper =
        config.wallpapers[static_cast<size_t>(desired)];
    const bool same_pixels = st->active_wallpaper_name == wallpaper.file_name &&
                             g_cache_dsc && g_cache_name == wallpaper.file_name &&
                             g_cache_w == Device::kScreenWidth &&
                             g_cache_h == Device::kScreenHeight &&
                             g_cache_focus_x == wallpaper.focus_x &&
                             g_cache_focus_y == wallpaper.focus_y &&
                             g_cache_zoom == wallpaper.zoom &&
                             !lv_obj_has_flag(st->image, LV_OBJ_FLAG_HIDDEN);
    if (same_pixels) {
      // Uhr/Diashowdauer duerfen live wechseln, ohne das unveraenderte Bild
      // erneut per PPA ueber den ganzen Bildschirm zu schreiben.
      st->active_wallpaper = desired;
      st->next_wallpaper_ms = millis() +
          static_cast<uint32_t>(config.duration_seconds) * 1000U;
    } else {
      apply_wallpaper(st, desired, false, allow_disabled);
    }
  } else {
    apply_wallpaper(st, -1, true);
  }

  if (st->clock_box) lv_obj_move_foreground(st->clock_box);
}

void global_screensaver_timer_cb(lv_timer_t* timer) {
  ScreensaverState* st = static_cast<ScreensaverState*>(lv_timer_get_user_data(timer));
  if (!st || st != g_state) return;
  if (g_live_config_refresh_requested) {
    g_live_config_refresh_requested = false;
    String preview = g_live_preview_wallpaper;
    g_live_preview_wallpaper = String();
    refresh_live_background_and_clock(st, preview);
    Serial.println("[Screensaver] Bild/Uhr live aktualisiert");
  }
  if (g_live_grid_refresh_requested) {
    g_live_grid_refresh_requested = false;
    rebuild_slot_grid(st);
    refresh_slot_values(st);
    Serial.println("[Screensaver] Kacheln live aktualisiert");
  }
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - st->next_slot_refresh_ms) >= 0) {
    refresh_slot_values(st);
    st->next_slot_refresh_ms = now + 1000U;
  }
  const auto& config = screensaverConfig.get();
  if (config.use_wallpapers && st->active_wallpaper >= 0 &&
      static_cast<int32_t>(now - st->next_wallpaper_ms) >= 0) {
    const uint32_t since_activity =
        static_cast<uint32_t>(now - displayManager.getLastActivityTime());
    if (since_activity < kInteractionSettleBeforeSlideMs) {
      st->next_wallpaper_ms =
          now + (kInteractionSettleBeforeSlideMs - since_activity);
      return;
    }
    const int next = next_enabled_wallpaper(st->active_wallpaper);
    if (next >= 0 && next != st->active_wallpaper) {
      if (!apply_wallpaper(st, next, false)) {
        // Geloeschte/defekte Datei: Rotationszeiger trotzdem weiterdrehen
        // und gedrosselt das uebernaechste Bild versuchen. Das sichtbare
        // Bild bleibt solange stehen.
        st->active_wallpaper = next;
        st->next_wallpaper_ms = now + 3000U;
      }
    } else {
      st->next_wallpaper_ms = now + 1000U;
    }
  }
}

void on_global_screensaver_clicked(lv_event_t* e) {
  if (g_state && lv_event_get_target(e) == g_state->overlay) {
    hide_image_screensaver();
  }
}

void on_global_overlay_delete(lv_event_t* e) {
  ScreensaverState* st = static_cast<ScreensaverState*>(lv_event_get_user_data(e));
  if (!st) return;
  // Das Overlay wird mit lv_obj_delete_async() geloescht. Der zugehoerige
  // Zustand muss deshalb bis zu diesem echten LV_EVENT_DELETE leben; ein
  // frueheres delete fuehrt beim spaeteren Event direkt in freigegebenen
  // Speicher (0xbaad5678). Falls LVGL das Overlay selbst entfernt hat, hier
  // auch den globalen Zustand und die Widget-Referenzen aufraeumen.
  if (g_state == st) {
    reset_sensor_widgets(GridType::SCREENSAVER);
    reset_switch_widgets(GridType::SCREENSAVER);
    reset_media_widgets(GridType::SCREENSAVER);
    g_state = nullptr;
  }
  if (st->timer) lv_timer_delete(st->timer);
#if defined(CONFIG_IDF_TARGET_ESP32P4)
  if (st->composite_storage) {
    free(st->composite_storage);
    st->composite_storage = nullptr;
  }
#endif
  delete st;
}

void global_preload_timer_cb(lv_timer_t*) {
  g_preload_timer = nullptr;
  if (g_state || !g_preload_wallpaper.file_name.length()) return;
  get_or_decode_cached(g_preload_wallpaper,
                       Device::kScreenWidth, Device::kScreenHeight);
}

}  // namespace

void preload_image_screensaver() {
  ScreensaverWallpaperConfig wallpaper;
  const int index = first_enabled_wallpaper();
  if (index >= 0) wallpaper = screensaverConfig.get().wallpapers[index];
  else if (!screensaverConfig.get().use_wallpapers ||
           !find_first_sd_wallpaper(wallpaper)) return;
  g_preload_wallpaper = wallpaper;
  if (g_preload_timer) lv_timer_delete(g_preload_timer);
  g_preload_timer = lv_timer_create(global_preload_timer_cb, 4000, nullptr);
  if (g_preload_timer) lv_timer_set_repeat_count(g_preload_timer, 1);
}

void show_image_screensaver() {
  // Ein bereits sichtbarer Screensaver wird nicht parallel neu aufgebaut.
  // Das alte Overlay wird asynchron geloescht; ein zweiter State waehrend
  // dieses Abbaus waere unnoetig und macht Widget-/Cache-Lebenszeiten schwer
  // vorhersehbar.
  if (g_state) return;
  ScreensaverState* st = new ScreensaverState();
  if (!st) return;

  st->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(st->overlay, Device::kScreenWidth, Device::kScreenHeight);
  lv_obj_set_pos(st->overlay, 0, 0);
  lv_obj_set_style_bg_color(st->overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(st->overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(st->overlay, 0, 0);
  lv_obj_set_style_pad_all(st->overlay, 0, 0);
  lv_obj_remove_flag(st->overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(st->overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(st->overlay, on_global_screensaver_clicked,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(st->overlay, on_global_overlay_delete,
                      LV_EVENT_DELETE, st);

  st->image = lv_image_create(st->overlay);
  lv_obj_set_pos(st->image, 0, 0);
  lv_obj_remove_flag(st->image, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(st->image, LV_OBJ_FLAG_HIDDEN);

  rebuild_global_clock(st);
  rebuild_slot_grid(st);

  g_state = st;
  g_live_config_refresh_requested = false;
  g_live_grid_refresh_requested = false;
  g_live_preview_wallpaper = String();
  // Vor dem ersten PPA-Gesamtframe die schon bekannten Entity-Werte in die
  // frisch erzeugten Widgets einsetzen. Sonst wird der erste sichtbare Frame
  // mit "--" zusammengesetzt und erst spaeter korrigiert.
  refresh_slot_values(st);
  const int wallpaper = first_enabled_wallpaper();
  apply_wallpaper(st, wallpaper, true);
  st->next_slot_refresh_ms = millis() + 1000U;
  st->timer = lv_timer_create(global_screensaver_timer_cb, 1000, st);
}

void hide_image_screensaver() {
  ScreensaverState* st = g_state;
  if (!st) return;
  g_state = nullptr;
  g_live_config_refresh_requested = false;
  g_live_grid_refresh_requested = false;
  g_live_preview_wallpaper = String();
  reset_sensor_widgets(GridType::SCREENSAVER);
  reset_switch_widgets(GridType::SCREENSAVER);
  reset_media_widgets(GridType::SCREENSAVER);
  if (st->timer) {
    lv_timer_delete(st->timer);
    st->timer = nullptr;
  }
  if (st->image) lv_image_set_src(st->image, nullptr);
  if (st->overlay) {
    lv_obj_add_flag(st->overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(st->overlay);
  } else {
    delete st;
  }
}

bool is_image_screensaver_visible() {
  return g_state != nullptr;
}

void service_image_screensaver_auto(uint32_t last_activity_ms) {
  if (g_state) return;
  const auto& config = configManager.getConfig();
  if (!config.auto_screensaver_enabled || config.auto_screensaver_seconds == 0) return;
  if (static_cast<uint32_t>(millis() - last_activity_ms) >=
      static_cast<uint32_t>(config.auto_screensaver_seconds) * 1000U) {
    show_image_screensaver();
  }
}

void image_screensaver_config_changed(const String& preview_wallpaper) {
  // Aus dem HTTP-Handler nur Flags setzen. Der LVGL-Timer aktualisiert das
  // bestehende Overlay synchron im LVGL-Kontext; dadurch gibt es weder einen
  // Overlay-Wechsel noch den alten Async-Delete/UAF-Pfad.
  if (g_state) {
    g_live_preview_wallpaper = preview_wallpaper;
    g_live_config_refresh_requested = true;
    if (g_state->timer) lv_timer_ready(g_state->timer);
    return;
  }
  // Den Bildcache nicht pauschal wegwerfen: Uhrposition, Formate oder
  // Diashowdauer aendern das dekodierte Bild nicht. get_or_decode_cached()
  // prueft Dateiname/Fokus/Zoom selbst und ersetzt nur bei echtem Bedarf.
  preload_image_screensaver();
}

void image_screensaver_tiles_changed() {
  if (!g_state) return;
  g_live_grid_refresh_requested = true;
  if (g_state->timer) lv_timer_ready(g_state->timer);
}

void image_screensaver_set_scene_callback(void (*callback)(const char*)) {
  g_scene_callback = callback;
}

const Tile* image_screensaver_get_slot_tile(uint8_t index) {
  return screensaverConfig.tile(index);
}

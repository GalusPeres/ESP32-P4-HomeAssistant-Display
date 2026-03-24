#include "radar_popup.h"
#include "sensor_popup.h"
#include "light_popup.h"
#include "weather_popup.h"
#include "image_popup.h"
#include "src/core/board_hal.h"
#include "src/core/waveshare_sdmmc.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_renderer_fonts.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <misc/cache/instance/lv_image_cache.h>
#include <misc/cache/instance/lv_image_header_cache.h>
#include <draw/lv_image_dsc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <time.h>
#include <vector>

namespace {

struct RadarPresetDef {
  const char* key;
  const char* label;
  const char* bbox;
};

struct RadarFrameSlot {
  String url;
  String path;
  String label;
  bool loaded = false;
  bool failed = false;
};

struct RadarDownloadDone {
  uint32_t generation;
  uint8_t index;
  bool success;
};

struct RadarWorkerJob {
  uint32_t generation;
  uint8_t index;
  String url;
  String path;
};

static const RadarPresetDef kRadarPresets[] = {

  {"bayern", "Bayern", "47.0,8.9,50.8,14.0"},
  {"germany", "Deutschland", "46.0,5.5,55.5,15.8"},
  {"south", "Sueddeutschland", "46.5,7.0,50.8,14.5"},
  {"custom", "Custom BBOX", ""}
};

static lv_obj_t* g_overlay = nullptr;
static lv_obj_t* g_card = nullptr;
static lv_obj_t* g_icon_label = nullptr;
static lv_obj_t* g_title_label = nullptr;
static lv_obj_t* g_close_btn = nullptr;
static lv_obj_t* g_image_area = nullptr;
static lv_obj_t* g_image = nullptr;
static lv_obj_t* g_status_label = nullptr;
static lv_obj_t* g_time_label = nullptr;
static lv_timer_t* g_anim_timer = nullptr;
static RadarPopupInit g_popup_init;
static std::vector<RadarFrameSlot> g_frames;
static SemaphoreHandle_t g_mutex = nullptr;
static QueueHandle_t g_done_queue = nullptr;
static TaskHandle_t g_worker_task = nullptr;
static bool g_worker_ready = false;
static bool g_popup_visible = false;
static uint32_t g_generation = 0;
static uint32_t g_next_refresh_ms = 0;
static uint8_t g_current_frame = 0;
static uint8_t g_pending_downloads = 0;
static String g_current_src;
static lv_image_dsc_t g_current_png_dsc{};
static uint8_t* g_current_png_data = nullptr;
static lv_obj_t* g_base_image = nullptr;
static lv_image_dsc_t g_base_png_dsc{};
static uint8_t* g_base_png_data = nullptr;
static String g_base_path;
static uint32_t g_last_plan_ms = 0;
static String g_status_message;
static std::vector<RadarWorkerJob> g_job_queue;
static constexpr const char* kRadarCacheDir = "/_radar_cache";
static constexpr uint8_t kDefaultFrameCount = 6;
static constexpr uint8_t kMinFrameCount = 2;
static constexpr uint8_t kMaxFrameCount = 8;
static constexpr uint16_t kDefaultFrameDelayMs = 450;
static constexpr uint16_t kMinFrameDelayMs = 150;
static constexpr uint16_t kMaxFrameDelayMs = 3000;
static constexpr uint16_t kDefaultRefreshSec = 300;
static constexpr uint16_t kMinRefreshSec = 60;
static constexpr uint16_t kMaxRefreshSec = 3600;
static constexpr uint16_t kRadarImageW = 540;
static constexpr uint16_t kRadarImageH = 500;
static constexpr size_t kDoneQueueLen = 16;

static constexpr uint16_t kCardWidth = (GRID_CELL_W * GRID_COLS) + (GRID_GAP * (GRID_COLS - 1));

static constexpr uint16_t kCardHeight = kCardWidth;
static constexpr uint16_t kCardPad = 20;
static constexpr uint16_t kCardRadius = 22;
static constexpr uint16_t kHeaderHeight = 72;
static constexpr uint16_t kCloseButtonSize = 96;
static constexpr uint16_t kCloseIndicatorRadius = 16;
static constexpr uint8_t kBaseMapIndex = 0xFF;
static constexpr uint32_t kRetrySyncMs = 5000UL;
static constexpr uint16_t kWorkerStack = 12 * 1024;

static uint32_t hash_string(const String& value) {

  uint32_t h = 2166136261u;

  for (size_t i = 0; i < value.length(); ++i) {
    h ^= static_cast<uint8_t>(value[i]);
    h *= 16777619u;
  }

  return h;

}

static void set_label_style(lv_obj_t* lbl, lv_color_t color, const lv_font_t* font) {

  if (!lbl) return;
  lv_obj_set_style_text_color(lbl, color, 0);

  if (font) {
    lv_obj_set_style_text_font(lbl, font, 0);
  }

}

static String normalize_preset(String preset) {

  preset.trim();
  preset.toLowerCase();

  for (const auto& entry : kRadarPresets) {

    if (preset == entry.key) return preset;
  }

  return "bayern";

}

static const RadarPresetDef* find_preset(const String& preset_raw) {

  const String preset = normalize_preset(preset_raw);

  for (const auto& entry : kRadarPresets) {

    if (preset == entry.key) return &entry;
  }

  return &kRadarPresets[0];

}

static String preset_label(const String& preset_raw) {

  const RadarPresetDef* preset = find_preset(preset_raw);
  return preset ? String(preset->label) : String("Bayern");

}

static String radar_icon_name(const String& icon_name) {

  String icon = normalizeMdiIconName(icon_name.length() ? icon_name : String("radar"));

  if (!icon.length() || isMdiIconDisabled(icon)) return String("radar");
  return icon;

}

static RadarPopupInit sanitize_init(RadarPopupInit init) {

  init.preset = normalize_preset(init.preset);
  init.bbox.trim();

  if (init.frame_count < kMinFrameCount || init.frame_count > kMaxFrameCount) {
    init.frame_count = kDefaultFrameCount;
  }

  if (init.frame_delay_ms < kMinFrameDelayMs || init.frame_delay_ms > kMaxFrameDelayMs) {
    init.frame_delay_ms = kDefaultFrameDelayMs;
  }

  if (init.refresh_sec < kMinRefreshSec || init.refresh_sec > kMaxRefreshSec) {
    init.refresh_sec = kDefaultRefreshSec;
  }

  if (!init.title.length()) {

    String label = preset_label(init.preset);
    init.title = (init.preset == "custom") ? String("Radar") : (String("Radar ") + label);
  }

  if (!init.icon_name.length()) init.icon_name = "radar";

  if (!init.bg_color) init.bg_color = 0x2A2A2A;
  return init;

}

static bool parse_bbox(const String& raw, float& min_lat, float& min_lon, float& max_lat, float& max_lon) {

  String text = raw;
  text.trim();

  if (!text.length()) return false;
  float values[4] = {0, 0, 0, 0};

  int start = 0;

  for (int i = 0; i < 4; ++i) {

    int comma = text.indexOf(',', start);

    String part = (comma >= 0) ? text.substring(start, comma) : text.substring(start);
    part.trim();

    if (!part.length()) return false;
    char buf[32];
    part.toCharArray(buf, sizeof(buf));
    char* end_ptr = nullptr;
    values[i] = strtof(buf, &end_ptr);

    if (!end_ptr || *end_ptr != '\0') return false;

    if (i < 3 && comma < 0) return false;
    start = comma + 1;
  }

  if (text.indexOf(',', start) >= 0) return false;
  min_lat = values[0];
  min_lon = values[1];
  max_lat = values[2];
  max_lon = values[3];

  if (min_lat >= max_lat || min_lon >= max_lon) return false;

  if (min_lat < -90.0f || max_lat > 90.0f) return false;

  if (min_lon < -180.0f || max_lon > 180.0f) return false;
  return true;

}

static bool resolve_bbox(const RadarPopupInit& init, String& out_bbox, String& error) {

  const String preset = normalize_preset(init.preset);

  if (preset == "custom") {
    float min_lat = 0.0f;
    float min_lon = 0.0f;
    float max_lat = 0.0f;
    float max_lon = 0.0f;

    if (!parse_bbox(init.bbox, min_lat, min_lon, max_lat, max_lon)) {
      error = "Custom BBOX ungueltig";
      return false;

    }

    out_bbox = init.bbox;
    out_bbox.trim();
    return true;

  }

  const RadarPresetDef* preset_def = find_preset(preset);
  out_bbox = preset_def ? String(preset_def->bbox) : String(kRadarPresets[0].bbox);
  return true;

}

static bool ensure_sd_ready() {

  if (SD_MMC.cardType() != CARD_NONE) return true;
  static uint32_t last_attempt_ms = 0;

  uint32_t now = millis();

  if (last_attempt_ms != 0 && (now - last_attempt_ms) < 5000U) return false;
  last_attempt_ms = now;
  return BoardHAL::initSDCard();

}

static bool ensure_cache_dir() {

  if (!ensure_sd_ready()) return false;

  if (SD_MMC.exists(kRadarCacheDir)) return true;
  return SD_MMC.mkdir(kRadarCacheDir);

}

static String format_time_iso_utc(time_t value) {

  struct tm tm_utc {};
  gmtime_r(&value, &tm_utc);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return String(buf);

}

static String format_time_label_local(time_t value) {

  struct tm tm_local {};
  localtime_r(&value, &tm_local);
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M", &tm_local);
  return String(buf);

}

static String build_wms_url(const String& bbox, const String& time_value) {

  String url = "https://maps.dwd.de/geoserver/wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap";
  url += "&LAYERS=dwd:Niederschlagsradar";
  url += "&STYLES=&FORMAT=image/png&TRANSPARENT=TRUE";
  url += "&CRS=EPSG:4326";
  url += "&BBOX=" + bbox;
  url += "&WIDTH=" + String(kRadarImageW);
  url += "&HEIGHT=" + String(kRadarImageH);
  url += "&TIME=" + time_value;
  return url;

}

static String build_basemap_url(const String& bbox) {
  String url = "https://maps.dwd.de/geoserver/wms?SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap";
  url += "&LAYERS=dwd:bluemarble&STYLES=&FORMAT=image/png&TRANSPARENT=FALSE";
  url += "&CRS=EPSG:4326";
  url += "&BBOX=" + bbox;
  url += "&WIDTH=" + String(kRadarImageW);
  url += "&HEIGHT=" + String(kRadarImageH);
  return url;
}

static bool build_frame_plan(const RadarPopupInit& init, std::vector<RadarFrameSlot>& out_frames, String& error) {

  out_frames.clear();

  String bbox;
  Serial.printf("[RadarPopup] Build plan: preset=%s frame_count=%u refresh=%u delay=%u\n",
                init.preset.c_str(),
                static_cast<unsigned>(init.frame_count),
                static_cast<unsigned>(init.refresh_sec),
                static_cast<unsigned>(init.frame_delay_ms));

  if (!resolve_bbox(init, bbox, error)) return false;
  Serial.printf("[RadarPopup] BBOX: %s\n", bbox.c_str());

  const time_t now_utc = time(nullptr);

  if (now_utc < 1700000000) {
    error = "Zeit noch nicht synchron";
    return false;

  }

  uint8_t frame_count = init.frame_count;

  if (frame_count < kMinFrameCount) frame_count = kDefaultFrameCount;

  if (frame_count > kMaxFrameCount) frame_count = kMaxFrameCount;

  const time_t latest_rounded = (now_utc / 300) * 300;

  const String cache_key = bbox + "|" + String(kRadarImageW) + "x" + String(kRadarImageH) + "|" + String(frame_count);

  const uint32_t hash = hash_string(cache_key);

  for (uint8_t i = 0; i < frame_count; ++i) {
    RadarFrameSlot slot;
    char path_buf[80];
    snprintf(path_buf, sizeof(path_buf), "%s/r%08lX_%u.png", kRadarCacheDir,
             static_cast<unsigned long>(hash), static_cast<unsigned int>(i));
    slot.path = String(path_buf);

    const bool newest = (i == frame_count - 1);

    if (newest) {
      slot.url = build_wms_url(bbox, "current");
      slot.label = "Jetzt";
    } else {

      const int rel = (frame_count - 1) - i;

      const time_t frame_time = latest_rounded - static_cast<time_t>(rel * 300);
      slot.url = build_wms_url(bbox, format_time_iso_utc(frame_time));
      slot.label = format_time_label_local(frame_time);
    }

    slot.loaded = SD_MMC.exists(slot.path);
    Serial.printf("[RadarPopup] Frame %u: %s -> %s (cached=%s, label=%s)\n",
                  static_cast<unsigned>(i),
                  slot.url.c_str(),
                  slot.path.c_str(),
                  slot.loaded ? "yes" : "no",
                  slot.label.c_str());
    out_frames.push_back(slot);
  }

  return true;

}

static bool download_url_to_file(const String& url, const String& path, String& error) {

  if (WiFi.status() != WL_CONNECTED) {
    error = "Kein WLAN";
    return false;

  }

  if (!ensure_cache_dir()) {
    error = "SD fehlt";
    return false;

  }

  String temp_path = path + ".tmp";
  SD_MMC.remove(temp_path);
  WiFiClient plain_client;
  WiFiClientSecure secure_client;
  HTTPClient http;

  bool started = false;

  if (url.startsWith("https://")) {
    secure_client.setInsecure();
    started = http.begin(secure_client, url);
  } else {
    started = http.begin(plain_client, url);
  }

  if (!started) {
    error = "HTTP Begin fehlgeschlagen";
    return false;

  }

  http.setConnectTimeout(8000);
  http.setTimeout(15000);
  http.setReuse(false);
  http.useHTTP10(true);
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
  http.addHeader("Accept", "image/png,image/*;q=0.9,*/*;q=0.5");
  http.addHeader("Accept-Language", "de-DE,de;q=0.9,en-US;q=0.8,en;q=0.7");
  http.setAcceptEncoding("identity");
  http.addHeader("Connection", "close");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  const char* headers[] = {"Content-Type", "Content-Encoding"};
  http.collectHeaders(headers, 2);

  int code = http.GET();

  const String content_type = http.header("Content-Type");

  const String content_encoding = http.header("Content-Encoding");
  Serial.printf("[RadarPopup] HTTP GET %s -> %d (type=%s, encoding=%s)\n",
                url.c_str(),
                code,
                content_type.c_str(),
                content_encoding.c_str());

  if (code != HTTP_CODE_OK) {
    error = "HTTP " + String(code);

    String body = http.getString();
    body.trim();

    if (body.length() > 160) body = body.substring(0, 160) + "...";

    if (body.length() > 0) {
      error += " ";
      error += body;
    }

    http.end();
    return false;

  }

  if (content_type.length() > 0 && content_type.indexOf("image/png") < 0) {
    error = "Unerwarteter Content-Type: " + content_type;

    String body = http.getString();
    body.trim();

    if (body.length() > 160) body = body.substring(0, 160) + "...";

    if (body.length() > 0) {
      error += " ";
      error += body;
    }

    http.end();
    return false;

  }

  File out = SD_MMC.open(temp_path, FILE_WRITE);

  if (!out) {
    error = "Datei kann nicht geschrieben werden";
    http.end();
    return false;

  }

  int total_written = http.writeToStream(&out);
  out.flush();
  out.close();
  http.end();

  if (total_written < 0) {
    error = "HTTP Stream Fehler: " + HTTPClient::errorToString(total_written);
    SD_MMC.remove(temp_path);
    return false;

  }

  if (total_written == 0) {
    error = "Leere Antwort";
    SD_MMC.remove(temp_path);
    return false;

  }

  if (SD_MMC.exists(path)) SD_MMC.remove(path);

  if (!SD_MMC.rename(temp_path, path)) {
    error = "Rename fehlgeschlagen";
    SD_MMC.remove(temp_path);
    return false;

  }

  Serial.printf("[RadarPopup] Download OK: %s (%lu bytes)\n", path.c_str(), static_cast<unsigned long>(total_written));
  return true;

}

static void free_current_png_source() {

  if (g_current_png_data) {
    free(g_current_png_data);
    g_current_png_data = nullptr;
  }

  memset(&g_current_png_dsc, 0, sizeof(g_current_png_dsc));
  g_current_src = "";

}

static void free_base_map() {
  if (g_base_png_data) {
    lv_image_cache_drop(&g_base_png_dsc);
    lv_image_header_cache_drop(&g_base_png_dsc);
    free(g_base_png_data);
    g_base_png_data = nullptr;
  }
  memset(&g_base_png_dsc, 0, sizeof(g_base_png_dsc));
}

static bool load_png_file_to_memory(const String& path, lv_image_dsc_t& out_dsc, uint8_t*& out_data, String& error) {

  out_data = nullptr;
  memset(&out_dsc, 0, sizeof(out_dsc));
  File f = SD_MMC.open(path, FILE_READ);

  if (!f) {
    error = "PNG-Datei kann nicht geoeffnet werden";
    return false;

  }

  size_t size = static_cast<size_t>(f.size());

  if (size < 24) {
    error = "PNG-Datei zu klein";
    f.close();
    return false;

  }

  uint8_t* data = static_cast<uint8_t*>(malloc(size));

  if (!data) {
    error = "Kein RAM fuer PNG";
    f.close();
    return false;

  }

  size_t read = f.read(data, size);
  f.close();

  if (read != size) {
    free(data);
    error = "PNG-Datei unvollstaendig gelesen";
    return false;

  }

  static const uint8_t kPngMagic[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

  if (memcmp(data, kPngMagic, sizeof(kPngMagic)) != 0) {
    char dump[3 * 16 + 1] = {0};

    const size_t dump_len = (size < 16) ? size : 16;

    for (size_t i = 0; i < dump_len; ++i) {
      snprintf(dump + (i * 3), sizeof(dump) - (i * 3), "%02X ", data[i]);
    }

    Serial.printf("[RadarPopup] PNG head %s: %s\n", path.c_str(), dump);
    free(data);
    error = "PNG-Signatur ungueltig";
    return false;

  }

  const uint32_t w_be = (static_cast<uint32_t>(data[16]) << 24) | (static_cast<uint32_t>(data[17]) << 16) |
                        (static_cast<uint32_t>(data[18]) << 8) | static_cast<uint32_t>(data[19]);

  const uint32_t h_be = (static_cast<uint32_t>(data[20]) << 24) | (static_cast<uint32_t>(data[21]) << 16) |
                        (static_cast<uint32_t>(data[22]) << 8) | static_cast<uint32_t>(data[23]);
  out_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  out_dsc.header.cf = LV_COLOR_FORMAT_RAW;
  out_dsc.header.flags = 0;
  out_dsc.header.w = static_cast<uint16_t>(w_be);
  out_dsc.header.h = static_cast<uint16_t>(h_be);
  out_dsc.header.stride = 0;
  out_dsc.header.reserved_2 = 0;
  out_dsc.data_size = static_cast<uint32_t>(size);
  out_dsc.data = data;
  out_dsc.reserved = nullptr;
  out_dsc.reserved_2 = nullptr;
  out_data = data;
  return true;

}

static void show_base_map() {
  if (!g_base_image || g_base_path.isEmpty()) return;
  if (!SD_MMC.exists(g_base_path)) return;
  free_base_map();

  String error;
  if (!load_png_file_to_memory(g_base_path, g_base_png_dsc, g_base_png_data, error)) {
    Serial.printf("[RadarPopup] Base map load failed: %s\n", error.c_str());
    return;
  }

  Serial.printf("[RadarPopup] Base map loaded: %u x %u (%lu bytes)\n",
                static_cast<unsigned>(g_base_png_dsc.header.w),
                static_cast<unsigned>(g_base_png_dsc.header.h),
                static_cast<unsigned long>(g_base_png_dsc.data_size));
  lv_obj_set_size(g_base_image, kRadarImageW, kRadarImageH);
  lv_image_set_inner_align(g_base_image, LV_IMAGE_ALIGN_CONTAIN);
  lv_img_set_src(g_base_image, &g_base_png_dsc);
  lv_obj_clear_flag(g_base_image, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(g_base_image);
}

static void ensure_worker();

static void begin_plan_from_init(const RadarPopupInit& init, bool force_refresh);

static void update_status_label();

static void update_anim_timer();

static void show_frame(uint8_t index, bool force_reload = false);

static void push_job(const RadarWorkerJob& job) {

  if (!g_mutex) return;

  if (xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) return;
  g_job_queue.push_back(job);
  xSemaphoreGive(g_mutex);

}

static void clear_job_queue() {

  if (!g_mutex) return;

  if (xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) return;
  g_job_queue.clear();
  xSemaphoreGive(g_mutex);

}

static bool pop_next_job(RadarWorkerJob& job) {

  if (!g_mutex) return false;

  if (xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) return false;

  const bool have_job = !g_job_queue.empty();

  if (have_job) {
    job = g_job_queue.front();
    g_job_queue.erase(g_job_queue.begin());
  }

  xSemaphoreGive(g_mutex);
  return have_job;

}

static void radar_worker_task(void*) {

  for (;;) {
    RadarWorkerJob job;

    if (!pop_next_job(job)) {
      vTaskDelay(pdMS_TO_TICKS(120));
      continue;
    }

    Serial.printf("[RadarPopup] Worker start: frame=%u %s\n", static_cast<unsigned>(job.index), job.url.c_str());

    String error;

    bool success = download_url_to_file(job.url, job.path, error);

    if (!success) {
      Serial.printf("[RadarPopup] Download Fehler: %s -> %s (%s)\n",
                    job.url.c_str(), job.path.c_str(), error.c_str());
    }

    RadarDownloadDone done{job.generation, job.index, success};

    if (g_done_queue) {
      xQueueSend(g_done_queue, &done, portMAX_DELAY);
    }

  }

}

static void ensure_worker() {

  if (!g_mutex) g_mutex = xSemaphoreCreateMutex();

  if (!g_done_queue) g_done_queue = xQueueCreate(kDoneQueueLen, sizeof(RadarDownloadDone));

  if (g_worker_ready || !g_mutex || !g_done_queue) return;
  BaseType_t ok = xTaskCreate(
      radar_worker_task,
      "radar_popup",
      kWorkerStack,
      nullptr,
      1,
      &g_worker_task);

  if (ok == pdPASS) {
    g_worker_ready = true;
  }

}

static void stop_anim_timer() {

  if (!g_anim_timer) return;
  lv_timer_del(g_anim_timer);
  g_anim_timer = nullptr;

}

static int count_loaded_frames() {

  int count = 0;

  for (const auto& slot : g_frames) {

    if (slot.loaded) ++count;
  }

  return count;

}

static int find_first_loaded_frame() {

  for (size_t i = 0; i < g_frames.size(); ++i) {

    if (g_frames[i].loaded) return static_cast<int>(i);
  }

  return -1;

}

static int find_next_loaded_frame(uint8_t current_index) {

  if (g_frames.empty()) return -1;

  const size_t count = g_frames.size();

  for (size_t step = 1; step <= count; ++step) {
    size_t idx = (static_cast<size_t>(current_index) + step) % count;

    if (g_frames[idx].loaded) return static_cast<int>(idx);
  }

  return -1;

}

static bool is_popup_visible() {

  return g_popup_visible && g_card && !lv_obj_has_flag(g_card, LV_OBJ_FLAG_HIDDEN);

}

static void align_header_row(lv_obj_t* card, lv_obj_t* title_label, lv_obj_t* icon_label) {

  if (!card) return;
  lv_obj_update_layout(card);
  lv_coord_t header_center_y = 60 - lv_obj_get_style_pad_top(card, LV_PART_MAIN);

  if (header_center_y < 0) header_center_y = 0;

  if (icon_label) {
    lv_coord_t icon_y = header_center_y - (lv_obj_get_height(icon_label) / 2);

    if (icon_y < 0) icon_y = 0;
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 8, icon_y);
  }

  if (title_label) {
    lv_coord_t title_y = header_center_y - (lv_obj_get_height(title_label) / 2);

    if (title_y < 0) title_y = 0;
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 78, title_y);
  }

}

static void apply_init_to_ui(const RadarPopupInit& init) {

  if (!g_card) return;
  lv_obj_set_style_bg_color(g_card, lv_color_hex(init.bg_color), 0);

  if (g_title_label) {
    lv_label_set_text(g_title_label, init.title.c_str());
  }

  if (g_icon_label) {

    String icon_name = radar_icon_name(init.icon_name);

    if (!icon_name.length() || isMdiIconDisabled(icon_name)) {
      lv_label_set_text(g_icon_label, "");
      lv_obj_add_flag(g_icon_label, LV_OBJ_FLAG_HIDDEN);
    } else {

      String icon_char = getMdiChar(icon_name);

      if (icon_char.length()) {
        lv_label_set_text(g_icon_label, icon_char.c_str());
        lv_obj_clear_flag(g_icon_label, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_label_set_text(g_icon_label, "");
        lv_obj_add_flag(g_icon_label, LV_OBJ_FLAG_HIDDEN);
      }

    }

  }

  align_header_row(g_card, g_title_label, g_icon_label);

}

static void on_overlay_click(lv_event_t* e) {

  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  (void)e;

}

static void on_close_click(lv_event_t* e) {

  lv_event_code_t code = lv_event_get_code(e);

  if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;
  hide_radar_popup();

}

static void on_overlay_delete(lv_event_t* e) {

  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  stop_anim_timer();
  g_overlay = nullptr;
  g_card = nullptr;
  g_icon_label = nullptr;
  g_title_label = nullptr;
  g_close_btn = nullptr;
  g_image_area = nullptr;
  g_image = nullptr;
  g_base_image = nullptr;
  g_status_label = nullptr;
  g_time_label = nullptr;
  g_popup_visible = false;
  free_current_png_source();
  free_base_map();
  g_base_path = "";
  g_frames.clear();

}

static void ensure_popup_ui() {

  if (g_overlay) return;
  g_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(g_overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_overlay, 0, 0);
  lv_obj_remove_flag(g_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE);
  g_card = lv_obj_create(g_overlay);
  lv_obj_set_size(g_card, kCardWidth, kCardHeight);
  lv_obj_center(g_card);
  lv_obj_set_style_bg_color(g_card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(g_card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(g_card, kCardRadius, 0);
  lv_obj_set_style_border_width(g_card, 0, 0);
  lv_obj_set_style_pad_all(g_card, kCardPad, 0);
  lv_obj_set_style_shadow_width(g_card, 28, 0);
  lv_obj_set_style_shadow_color(g_card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(g_card, LV_OPA_40, 0);
  lv_obj_set_style_shadow_spread(g_card, 2, 0);
  lv_obj_remove_flag(g_card, LV_OBJ_FLAG_SCROLLABLE);
  g_title_label = lv_label_create(g_card);
  set_label_style(g_title_label, lv_color_white(), FONT_TITLE);
  lv_label_set_long_mode(g_title_label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(g_title_label, LV_PCT(62));
  lv_obj_align(g_title_label, LV_ALIGN_TOP_LEFT, 78, 10);
  g_icon_label = lv_label_create(g_card);
  set_label_style(g_icon_label, lv_color_white(), FONT_MDI_ICONS);
  lv_label_set_text(g_icon_label, "");
  lv_obj_add_flag(g_icon_label, LV_OBJ_FLAG_HIDDEN);
  g_close_btn = lv_button_create(g_card);
  lv_obj_set_size(g_close_btn, kCloseButtonSize, kCloseButtonSize);
  lv_obj_set_style_bg_opa(g_close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(g_close_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(g_close_btn, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(g_close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(g_close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(g_close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(g_close_btn, kCloseIndicatorRadius, 0);
  lv_obj_set_style_pad_all(g_close_btn, 0, 0);
  lv_obj_align(g_close_btn, LV_ALIGN_TOP_RIGHT, 12, -12);
  lv_obj_set_ext_click_area(g_close_btn, 28);
  lv_obj_add_flag(g_close_btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(g_close_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(g_close_btn, on_close_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g_close_btn, on_close_click, LV_EVENT_RELEASED, nullptr);
  lv_obj_t* close_label = lv_label_create(g_close_btn);
  set_label_style(close_label, lv_color_white(), FONT_MDI_ICONS);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);
  g_image_area = lv_obj_create(g_card);
  lv_obj_remove_style_all(g_image_area);
  lv_obj_set_size(g_image_area, kRadarImageW, kRadarImageH);
  lv_obj_set_style_bg_opa(g_image_area, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_image_area, 0, 0);
  lv_obj_remove_flag(g_image_area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_align(g_image_area, LV_ALIGN_TOP_MID, 0, kHeaderHeight + 10);
  g_base_image = lv_img_create(g_image_area);
  lv_obj_set_size(g_base_image, kRadarImageW, kRadarImageH);
  lv_obj_center(g_base_image);
  lv_obj_add_flag(g_base_image, LV_OBJ_FLAG_HIDDEN);
  g_image = lv_img_create(g_image_area);
  lv_obj_center(g_image);
  lv_obj_add_flag(g_image, LV_OBJ_FLAG_HIDDEN);
  g_time_label = lv_label_create(g_card);
  set_label_style(g_time_label, lv_color_white(), &ui_font_24);
  lv_obj_set_style_text_align(g_time_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(g_time_label, "");
  lv_obj_set_width(g_time_label, LV_PCT(100));
  lv_obj_align_to(g_time_label, g_image_area, LV_ALIGN_OUT_BOTTOM_MID, 0, 14);
  g_status_label = lv_label_create(g_card);
  set_label_style(g_status_label, lv_color_hex(0xC8D4E8), &ui_font_20);
  lv_obj_set_style_text_align(g_status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(g_status_label, "");
  lv_obj_set_width(g_status_label, LV_PCT(92));
  lv_obj_align(g_status_label, LV_ALIGN_BOTTOM_MID, 0, -2);
  lv_obj_move_foreground(g_icon_label);
  lv_obj_move_foreground(g_title_label);
  lv_obj_move_foreground(g_close_btn);
  lv_obj_add_event_cb(g_overlay, on_overlay_click, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(g_overlay, on_overlay_delete, LV_EVENT_DELETE, nullptr);

}

static void show_frame(uint8_t index, bool force_reload) {

  if (!g_image || index >= g_frames.size()) return;
  RadarFrameSlot& slot = g_frames[index];

  if (!slot.loaded) return;

  const String src = slot.path;
  lv_image_dsc_t next_dsc{};

  uint8_t* next_data = nullptr;

  String error;

  if (!load_png_file_to_memory(slot.path, next_dsc, next_data, error)) {
    Serial.printf("[RadarPopup] Decode failed for frame %u: %s (%s)\n",
                  static_cast<unsigned>(index),
                  slot.path.c_str(),
                  error.c_str());
    slot.loaded = false;
    slot.failed = true;
    update_status_label();
    return;

  }

  if (next_dsc.header.cf == LV_COLOR_FORMAT_ARGB8888 && next_dsc.data_size > 200000UL) {
    Serial.printf("[RadarPopup] Guard skip frame %u: oversized RAM image (%lu bytes, cf=%u)\n",
                  static_cast<unsigned>(index),
                  static_cast<unsigned long>(next_dsc.data_size),
                  static_cast<unsigned>(next_dsc.header.cf));
    free(next_data);
    slot.failed = true;
    update_status_label();
    return;

  }

  if (force_reload || g_current_src != src) {
    Serial.printf("[RadarPopup] Show frame %u: %s (%u x %u, %lu bytes, cf=%u)\n",
                  static_cast<unsigned>(index),
                  slot.path.c_str(),
                  static_cast<unsigned>(next_dsc.header.w),
                  static_cast<unsigned>(next_dsc.header.h),
                  static_cast<unsigned long>(next_dsc.data_size),
                  static_cast<unsigned>(next_dsc.header.cf));
    lv_image_cache_drop(&g_current_png_dsc);
    lv_image_header_cache_drop(&g_current_png_dsc);
    lv_img_set_src(g_image, nullptr);
    free_current_png_source();
    g_current_png_dsc = next_dsc;
    g_current_png_data = next_data;
    lv_obj_set_size(g_image, kRadarImageW, kRadarImageH);
    lv_image_set_inner_align(g_image, LV_IMAGE_ALIGN_CONTAIN);
    lv_img_set_src(g_image, &g_current_png_dsc);
    g_current_src = src;
  } else {
    free(next_data);
  }

  g_current_frame = index;
  lv_obj_clear_flag(g_image, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(g_image);

  if (g_time_label) {
    lv_label_set_text(g_time_label, slot.label.c_str());
  }

  update_status_label();

}

static void update_status_label() {

  if (!g_status_label) return;

  String text;

  const int loaded = count_loaded_frames();

  if (g_status_message.length()) {
    text = g_status_message;
  } else if (loaded <= 0) {
    text = (g_pending_downloads > 0) ? String("Radar wird geladen...") : String("Kein Radarbild");
  } else if (g_pending_downloads > 0) {
    text = String("Radar ") + String(loaded) + "/" + String(g_frames.size());
  } else {
    text = String("DWD ") + preset_label(g_popup_init.preset);
  }

  lv_label_set_text(g_status_label, text.c_str());

}

static void radar_anim_cb(lv_timer_t*) {

  if (!is_popup_visible()) return;

  if (count_loaded_frames() < 2) return;

  int next = find_next_loaded_frame(g_current_frame);

  if (next < 0 || static_cast<uint8_t>(next) == g_current_frame) return;
  show_frame(static_cast<uint8_t>(next), false);

}

static void update_anim_timer() {

  const int loaded = count_loaded_frames();

  if (!is_popup_visible() || loaded < 2) {
    stop_anim_timer();
    return;

  }

  if (!g_anim_timer) {
    g_anim_timer = lv_timer_create(radar_anim_cb, g_popup_init.frame_delay_ms, nullptr);
  } else {
    lv_timer_set_period(g_anim_timer, g_popup_init.frame_delay_ms);
    lv_timer_resume(g_anim_timer);
  }

}

static void process_done_queue() {

  if (!g_done_queue) return;
  RadarDownloadDone done{};

  while (xQueueReceive(g_done_queue, &done, 0) == pdTRUE) {

    if (done.generation != g_generation) continue;

    if (done.index == kBaseMapIndex) {
      if (done.success) {
        Serial.printf("[RadarPopup] Base map downloaded\n");
        show_base_map();
      }
      continue;
    }

    if (done.index >= g_frames.size()) continue;
    RadarFrameSlot& slot = g_frames[done.index];
    slot.loaded = done.success && SD_MMC.exists(slot.path);
    Serial.printf("[RadarPopup] Done frame %u: success=%s exists=%s\n",
                  static_cast<unsigned>(done.index),
                  done.success ? "yes" : "no",
                  SD_MMC.exists(slot.path) ? "yes" : "no");
    slot.failed = !slot.loaded;

    if (g_pending_downloads > 0) --g_pending_downloads;

    const String src = slot.path;

    if (slot.loaded) {

      if (g_current_src.isEmpty()) {

        int first = find_first_loaded_frame();

        if (first >= 0) show_frame(static_cast<uint8_t>(first), true);
      } else if (g_current_src == src) {
        show_frame(done.index, true);
      }

    }

  }

  if (g_current_src.isEmpty()) {

    int first = find_first_loaded_frame();

    if (first >= 0) show_frame(static_cast<uint8_t>(first), true);
  }

  update_status_label();
  update_anim_timer();

}

static bool queue_download(uint8_t index, const String& url, const String& path) {

  if (!g_worker_ready) return false;
  RadarWorkerJob job;
  job.generation = g_generation;
  job.index = index;
  job.url = url;
  job.path = path;
  push_job(job);
  return true;

}

static void begin_plan_from_init(const RadarPopupInit& raw_init, bool force_refresh) {

  g_popup_init = sanitize_init(raw_init);
  apply_init_to_ui(g_popup_init);
  g_status_message = "";
  g_current_src = "";
  g_current_frame = 0;
  g_pending_downloads = 0;
  std::vector<RadarFrameSlot> frames;

  String error;

  if (!build_frame_plan(g_popup_init, frames, error)) {
    Serial.printf("[RadarPopup] Build plan failed: %s\n", error.c_str());
    g_frames.clear();
    g_generation++;

    if (g_image) lv_obj_add_flag(g_image, LV_OBJ_FLAG_HIDDEN);

    if (g_time_label) lv_label_set_text(g_time_label, "");
    g_status_message = error;
    g_next_refresh_ms = millis() + kRetrySyncMs;
    clear_job_queue();
    update_status_label();
    update_anim_timer();
    return;

  }

  g_generation++;
  g_frames = frames;

  if (force_refresh) {

    for (auto& frame : g_frames) {
      frame.loaded = false;
      frame.failed = false;
    }

  }

  clear_job_queue();

  int first_loaded = find_first_loaded_frame();

  if (first_loaded >= 0) {
    show_frame(static_cast<uint8_t>(first_loaded), true);
  } else {

    if (g_image) lv_obj_add_flag(g_image, LV_OBJ_FLAG_HIDDEN);

    if (g_time_label) lv_label_set_text(g_time_label, "");
  }

  bool can_download = (WiFi.status() == WL_CONNECTED) && ensure_cache_dir();
  Serial.printf("[RadarPopup] Begin plan: can_download=%s force_refresh=%s frames=%u\n",
                can_download ? "yes" : "no",
                force_refresh ? "yes" : "no",
                static_cast<unsigned>(g_frames.size()));

  if (!can_download && first_loaded < 0) {
    g_status_message = ensure_sd_ready() ? String("Kein WLAN") : String("SD fehlt");
  }

  // Resolve bbox for base map (show cached even without network)
  String bbox;
  String bbox_err;
  if (resolve_bbox(g_popup_init, bbox, bbox_err)) {
    const String bm_key = "basemap|" + bbox + "|" + String(kRadarImageW) + "x" + String(kRadarImageH);
    char bm_buf[80];
    snprintf(bm_buf, sizeof(bm_buf), "%s/bm_%08lX.png", kRadarCacheDir,
             static_cast<unsigned long>(hash_string(bm_key)));
    g_base_path = String(bm_buf);
    if (SD_MMC.exists(g_base_path)) {
      show_base_map();
    }
  }

  if (can_download) {
    ensure_worker();
    // Queue base map download if not cached
    if (g_base_path.length() && !SD_MMC.exists(g_base_path)) {
      String bm_url = build_basemap_url(bbox);
      Serial.printf("[RadarPopup] Queue base map: %s\n", bm_url.c_str());
      queue_download(kBaseMapIndex, bm_url, g_base_path);
    }
    for (uint8_t i = 0; i < g_frames.size(); ++i) {
      if (!force_refresh && g_frames[i].loaded) continue;
      if (queue_download(i, g_frames[i].url, g_frames[i].path)) {
        ++g_pending_downloads;
        Serial.printf("[RadarPopup] Queue frame %u\n", static_cast<unsigned>(i));
      }
    }
  }

  g_last_plan_ms = millis();
  g_next_refresh_ms = millis() + (static_cast<uint32_t>(g_popup_init.refresh_sec) * 1000UL);
  update_status_label();
  update_anim_timer();

}

}  // namespace

void show_radar_popup(const RadarPopupInit& init) {

  Serial.printf("[RadarPopup] Open popup: title=%s preset=%s\n", init.title.c_str(), init.preset.c_str());
  hide_sensor_popup();
  hide_light_popup();
  hide_weather_popup();
  hide_image_popup();
  ensure_popup_ui();

  if (!g_overlay || !g_card) return;
  lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_card, LV_OBJ_FLAG_HIDDEN);
  g_popup_visible = true;

  // Reuse cached frames if still fresh
  bool fresh = (g_last_plan_ms != 0) &&
               ((millis() - g_last_plan_ms) < (g_popup_init.refresh_sec * 1000UL)) &&
               !g_frames.empty() && count_loaded_frames() > 0;

  if (fresh) {
    apply_init_to_ui(g_popup_init);
    if (g_base_path.length() && SD_MMC.exists(g_base_path)) show_base_map();
    int first = find_first_loaded_frame();
    if (first >= 0) show_frame(static_cast<uint8_t>(first), false);
    g_next_refresh_ms = g_last_plan_ms + (static_cast<uint32_t>(g_popup_init.refresh_sec) * 1000UL);
    update_status_label();
    update_anim_timer();
  } else {
    begin_plan_from_init(init, false);
  }

}

void hide_radar_popup() {

  if (!g_overlay || !g_card) return;
  lv_obj_add_flag(g_card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE);

  if (g_image) lv_img_set_src(g_image, nullptr);
  if (g_base_image) lv_img_set_src(g_base_image, nullptr);
  free_current_png_source();
  free_base_map();
  g_popup_visible = false;
  stop_anim_timer();

}

void radar_popup_service() {

  process_done_queue();

  if (!is_popup_visible()) return;

  uint32_t now = millis();

  if (g_next_refresh_ms != 0 && static_cast<int32_t>(now - g_next_refresh_ms) >= 0) {
    begin_plan_from_init(g_popup_init, true);
  }

}


#include "image_popup.h"
#include "sensor_popup.h"
#include "light_popup.h"
#include "src/core/display_manager.h"
#include <SD.h>
#include <M5Unified.h>
#include <vector>
#include <draw/lv_image_decoder.h>
#include <libs/tjpgd/tjpgd.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

// Globaler Context fuer das Image Popup
static lv_obj_t* g_image_popup_overlay = nullptr;
static lv_obj_t* g_image_popup_img = nullptr;
static lv_obj_t* g_image_popup_label = nullptr;
static bool g_image_shown = false;
static lv_image_header_t g_current_header;
static bool g_current_header_valid = false;
static String g_current_image_path;  // Pfad muss persistent sein fuer LVGL!
static const char* kSlideshowToken = "__slideshow__";
static const char* kSlideshowTokenBin = "__slideshow_bin__";
static const char* kSlideshowTokenJpeg = "__slideshow_jpeg__";
static const char* kSlideshowTokenAll = "__slideshow_all__";
static lv_timer_t* g_slideshow_timer = nullptr;
static constexpr uint16_t kDefaultSlideshowSec = 10;
static constexpr uint16_t kMaxSlideshowSec = 3600;
static uint32_t g_slideshow_interval_ms = 1000U * kDefaultSlideshowSec;
static lv_timer_t* g_popup_restore_timer = nullptr;
static std::vector<String> g_slideshow_files;
static size_t g_slideshow_index = 0;
static size_t g_slideshow_prev_lines = 0;
static lv_display_render_mode_t g_slideshow_prev_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
static bool g_slideshow_buffer_override = false;
static constexpr size_t kJpegWorkbufSize = 4096;
static lv_image_dsc_t g_image_ram_dsc;
static uint8_t* g_image_ram_buf = nullptr;
static size_t g_image_ram_buf_size = 0;
static bool g_image_ram_active = false;
static String g_image_ram_source;
static void show_image_popup_error(const char* title, const char* path);
static void apply_slideshow_display_mode(bool enable);
static void schedule_popup_restore();
static void cancel_popup_restore();
static void free_image_ram();

static void close_image_popup(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  hide_image_popup();
}

static void ensure_image_popup_overlay() {
  if (g_image_popup_overlay) return;

  g_image_popup_overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(g_image_popup_overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(g_image_popup_overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_image_popup_overlay, 0, 0);
  lv_obj_set_style_pad_all(g_image_popup_overlay, 0, 0);
  lv_obj_add_event_cb(g_image_popup_overlay, close_image_popup, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  g_image_popup_img = lv_img_create(g_image_popup_overlay);
  lv_image_set_antialias(g_image_popup_img, false);

  g_image_popup_label = lv_label_create(g_image_popup_overlay);
  lv_obj_set_style_text_color(g_image_popup_label, lv_color_white(), 0);
  lv_label_set_long_mode(g_image_popup_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(g_image_popup_label, LV_PCT(90));
  lv_obj_center(g_image_popup_label);
  lv_obj_add_flag(g_image_popup_label, LV_OBJ_FLAG_HIDDEN);
}

enum class SlideshowMode {
  None,
  Bin,
  Jpeg,
  All
};

static SlideshowMode get_slideshow_mode(const String& path) {
  String p = path;
  p.trim();
  if (p.startsWith("/")) p = p.substring(1);
  p.toLowerCase();
  if (p == kSlideshowTokenBin) return SlideshowMode::Bin;
  if (p == kSlideshowTokenJpeg || p == "__slideshow_jpg__") return SlideshowMode::Jpeg;
  if (p == kSlideshowTokenAll) return SlideshowMode::All;
  if (p == kSlideshowToken) return SlideshowMode::Bin; // Legacy
  return SlideshowMode::None;
}

static String normalize_sd_path(String path) {
  path.trim();
  if (!path.startsWith("/")) path = "/" + path;
  return path;
}

static bool is_url_path(const String& path) {
  String p = path;
  p.trim();
  p.toLowerCase();
  return p.startsWith("http://") || p.startsWith("https://");
}

static String strip_url_query(String path) {
  int q = path.indexOf('?');
  if (q >= 0) path = path.substring(0, q);
  return path;
}

static String guess_url_extension(String url, const String& content_type) {
  String u = strip_url_query(url);
  u.toLowerCase();
  if (u.endsWith(".jpg") || u.endsWith(".jpeg")) return ".jpg";
  if (u.endsWith(".png")) return ".png";
  String ct = content_type;
  ct.toLowerCase();
  if (ct.indexOf("image/jpeg") >= 0) return ".jpg";
  if (ct.indexOf("image/jpg") >= 0) return ".jpg";
  if (ct.indexOf("image/pjpeg") >= 0) return ".jpg";
  if (ct.indexOf("image/png") >= 0) return ".png";
  return String();
}

static bool parse_url(const String& input, bool& https, String& host, uint16_t& port, String& path, String& error) {
  String url = input;
  url.trim();
  int scheme_pos = url.indexOf("://");
  if (scheme_pos < 0) {
    error = "URL Schema fehlt";
    return false;
  }
  String scheme = url.substring(0, scheme_pos);
  scheme.toLowerCase();
  https = (scheme == "https");
  if (!(scheme == "http" || scheme == "https")) {
    error = "URL Schema ungueltig";
    return false;
  }

  int host_start = scheme_pos + 3;
  int path_start = url.indexOf('/', host_start);
  String host_port = (path_start >= 0) ? url.substring(host_start, path_start) : url.substring(host_start);
  path = (path_start >= 0) ? url.substring(path_start) : "/";

  int port_pos = host_port.indexOf(':');
  if (port_pos >= 0) {
    host = host_port.substring(0, port_pos);
    String port_str = host_port.substring(port_pos + 1);
    port = static_cast<uint16_t>(port_str.toInt());
    if (port == 0) {
      error = "URL Port ungueltig";
      return false;
    }
  } else {
    host = host_port;
    port = https ? 443 : 80;
  }

  if (host.length() == 0) {
    error = "URL Host fehlt";
    return false;
  }
  return true;
}

static String build_url_base(const String& url) {
  bool https = false;
  String host;
  uint16_t port = 0;
  String path;
  String err;
  if (!parse_url(url, https, host, port, path, err)) return String();
  String base = https ? "https://" : "http://";
  base += host;
  if ((https && port != 443) || (!https && port != 80)) {
    base += ":";
    base += String(port);
  }
  base += "/";
  return base;
}

static bool download_url_to_sd(const String& url, String& out_path, String& error) {
  HTTPClient http;
  bool https = false;
  String host;
  uint16_t port = 0;
  String path;
  if (!parse_url(url, https, host, port, path, error)) return false;
  WiFiClientSecure secure_client;
  if (https) {
    secure_client.setInsecure();
    if (!http.begin(secure_client, host, port, path, true)) {
      error = "HTTP begin fehlgeschlagen";
      return false;
    }
  } else {
    if (!http.begin(host, port, path)) {
      error = "HTTP begin fehlgeschlagen";
      return false;
    }
  }
  http.setReuse(false);
  http.setTimeout(15000);
  http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
  http.addHeader("Accept", "image/avif,image/webp,image/*,*/*;q=0.8");
  http.addHeader("Accept-Language", "de-DE,de;q=0.9,en-US;q=0.8,en;q=0.7");
  String referer = build_url_base(url);
  if (referer.length() > 0) {
    http.addHeader("Referer", referer);
  }
  http.setAcceptEncoding("identity");
  http.addHeader("Connection", "close");
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  const char* headers[] = {"Content-Type"};
  http.collectHeaders(headers, 1);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    if (code <= 0) {
      error = HTTPClient::errorToString(code);
    } else {
      String body = http.getString();
      body.trim();
      if (body.length() > 160) body = body.substring(0, 160) + "...";
      error = String("HTTP Fehler: ") + code;
      if (body.length() > 0) {
        error += " ";
        error += body;
      }
    }
    http.end();
    return false;
  }

  String ext = guess_url_extension(url, http.header("Content-Type"));
  if (ext.length() == 0) {
    error = "Unbekannter Bildtyp";
    http.end();
    return false;
  }

  out_path = "/_url_cache" + ext;
  if (SD.exists(out_path)) {
    SD.remove(out_path);
  }
  File f = SD.open(out_path, FILE_WRITE);
  if (!f) {
    error = "SD Schreibfehler";
    http.end();
    return false;
  }

  WiFiClient* stream = http.getStreamPtr();
  int remaining = http.getSize();
  uint8_t buffer[1024];
  while (http.connected() && (remaining > 0 || remaining == -1)) {
    size_t available = stream->available();
    if (available) {
      int read_len = stream->readBytes(buffer, available > sizeof(buffer) ? sizeof(buffer) : available);
      if (read_len > 0) {
        f.write(buffer, read_len);
        if (remaining > 0) remaining -= read_len;
      }
    } else {
      delay(2);
    }
  }

  f.close();
  http.end();
  if (!SD.exists(out_path)) {
    error = "Download fehlgeschlagen";
    return false;
  }
  return true;
}

static uint32_t normalize_slideshow_interval_ms(uint16_t sec) {
  if (sec == 0) sec = kDefaultSlideshowSec;
  if (sec > kMaxSlideshowSec) sec = kMaxSlideshowSec;
  return static_cast<uint32_t>(sec) * 1000;
}

static void cancel_popup_restore() {
  if (g_popup_restore_timer) {
    lv_timer_del(g_popup_restore_timer);
    g_popup_restore_timer = nullptr;
  }
}

static void popup_restore_cb(lv_timer_t*) {
  cancel_popup_restore();
  apply_slideshow_display_mode(false);
  displayManager.setReverseFlush(false);
}

static void schedule_popup_restore() {
  cancel_popup_restore();
  g_popup_restore_timer = lv_timer_create(popup_restore_cb, 60, nullptr);
}

static void stop_slideshow(bool restore_display) {
  if (g_slideshow_timer) {
    lv_timer_del(g_slideshow_timer);
    g_slideshow_timer = nullptr;
  }
  g_slideshow_files.clear();
  g_slideshow_index = 0;
  if (restore_display) {
    apply_slideshow_display_mode(false);
    displayManager.setReverseFlush(false);
  }
}

static bool ends_with_ignore_case(const String& value, const char* suffix) {
  if (!suffix) return false;
  String v = value;
  v.toLowerCase();
  String s = suffix;
  s.toLowerCase();
  return v.endsWith(s);
}

struct JpegDecodeContext {
  File* file;
  uint16_t src_w;
  uint16_t src_h;
  uint16_t dst_w;
  uint16_t dst_h;
  uint16_t* dst_buf;
  uint16_t* x_start;
  uint16_t* x_end;
  uint16_t* y_start;
  uint16_t* y_end;
};

static size_t tjpgd_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  JpegDecodeContext* ctx = static_cast<JpegDecodeContext*>(jd->device);
  if (!ctx || !ctx->file) return 0;
  if (buff) return ctx->file->read(buff, ndata);
  uint32_t pos = ctx->file->position();
  ctx->file->seek(pos + ndata);
  return ndata;
}

static int tjpgd_output(JDEC* jd, void* bitmap, JRECT* rect) {
  JpegDecodeContext* ctx = static_cast<JpegDecodeContext*>(jd->device);
  if (!ctx || !ctx->dst_buf || !ctx->x_start || !ctx->x_end || !ctx->y_start || !ctx->y_end) return 0;
  if (!bitmap || !rect) return 0;

  const uint16_t rect_w = rect->right - rect->left + 1;
  const uint16_t rect_h = rect->bottom - rect->top + 1;
  const uint8_t* src = static_cast<const uint8_t*>(bitmap);

  for (uint16_t y = 0; y < rect_h; ++y) {
    const uint16_t src_y = rect->top + y;
    if (src_y >= ctx->src_h) continue;
    uint16_t dst_y0 = ctx->y_start[src_y];
    uint16_t dst_y1 = ctx->y_end[src_y];
    if (dst_y0 >= ctx->dst_h) continue;
    if (dst_y1 >= ctx->dst_h) dst_y1 = ctx->dst_h - 1;
    if (dst_y1 < dst_y0) dst_y1 = dst_y0;
    const uint8_t* src_row = src + (static_cast<size_t>(y) * rect_w * 3U);
    for (uint16_t x = 0; x < rect_w; ++x) {
      const uint16_t src_x = rect->left + x;
      if (src_x >= ctx->src_w) continue;
      uint16_t dst_x0 = ctx->x_start[src_x];
      uint16_t dst_x1 = ctx->x_end[src_x];
      if (dst_x0 >= ctx->dst_w) continue;
      if (dst_x1 >= ctx->dst_w) dst_x1 = ctx->dst_w - 1;
      if (dst_x1 < dst_x0) dst_x1 = dst_x0;
      const uint8_t b = src_row[x * 3U + 0];
      const uint8_t g = src_row[x * 3U + 1];
      const uint8_t r = src_row[x * 3U + 2];
      uint16_t rgb565 = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      rgb565 = static_cast<uint16_t>((rgb565 >> 8) | (rgb565 << 8));
      for (uint16_t dy = dst_y0; dy <= dst_y1; ++dy) {
        uint16_t* dst_row = ctx->dst_buf + (static_cast<size_t>(dy) * ctx->dst_w);
        for (uint16_t dx = dst_x0; dx <= dst_x1; ++dx) {
          dst_row[dx] = rgb565;
        }
      }
    }
  }
  return 1;
}

static void free_image_ram() {
  if (g_image_ram_buf) {
    lv_free(g_image_ram_buf);
    g_image_ram_buf = nullptr;
  }
  g_image_ram_buf_size = 0;
  g_image_ram_active = false;
  g_image_ram_source = "";
  memset(&g_image_ram_dsc, 0, sizeof(g_image_ram_dsc));
}

static bool calc_contain_size(uint16_t src_w, uint16_t src_h, uint16_t& dst_w, uint16_t& dst_h) {
  if (src_w == 0 || src_h == 0) return false;
  int32_t scale_x = static_cast<int32_t>(SCREEN_WIDTH) * LV_SCALE_NONE / static_cast<int32_t>(src_w);
  int32_t scale_y = static_cast<int32_t>(SCREEN_HEIGHT) * LV_SCALE_NONE / static_cast<int32_t>(src_h);
  int32_t scale = LV_MIN(scale_x, scale_y);
  if (scale <= 0) scale = LV_SCALE_NONE;
  int32_t w = static_cast<int32_t>(src_w) * scale / LV_SCALE_NONE;
  int32_t h = static_cast<int32_t>(src_h) * scale / LV_SCALE_NONE;
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  dst_w = static_cast<uint16_t>(w);
  dst_h = static_cast<uint16_t>(h);
  return true;
}

static bool decode_jpeg_to_ram(const String& fullPath, lv_image_header_t& header, String& error) {
  free_image_ram();
  File f = SD.open(fullPath, FILE_READ);
  if (!f) {
    error = "JPEG Datei nicht lesbar";
    return false;
  }

  uint8_t* work = static_cast<uint8_t*>(lv_malloc(kJpegWorkbufSize));
  if (!work) {
    f.close();
    error = "Kein RAM fuer JPEG";
    return false;
  }

  JDEC jd;
  JpegDecodeContext ctx{};
  ctx.file = &f;

  JRESULT rc = jd_prepare(&jd, tjpgd_input, work, kJpegWorkbufSize, &ctx);
  if (rc != JDR_OK) {
    lv_free(work);
    f.close();
    error = "JPEG Header Fehler";
    return false;
  }

  ctx.src_w = jd.width;
  ctx.src_h = jd.height;
  if (!calc_contain_size(ctx.src_w, ctx.src_h, ctx.dst_w, ctx.dst_h)) {
    lv_free(work);
    f.close();
    error = "JPEG Groesse ungueltig";
    return false;
  }

  size_t dst_pixels = static_cast<size_t>(ctx.dst_w) * ctx.dst_h;
  g_image_ram_buf_size = dst_pixels * 2U;
  g_image_ram_buf = static_cast<uint8_t*>(lv_malloc(g_image_ram_buf_size));
  if (!g_image_ram_buf) {
    lv_free(work);
    f.close();
    error = "Kein RAM fuer Zielbild";
    return false;
  }
  memset(g_image_ram_buf, 0, g_image_ram_buf_size);
  ctx.dst_buf = reinterpret_cast<uint16_t*>(g_image_ram_buf);

  ctx.x_start = static_cast<uint16_t*>(lv_malloc(sizeof(uint16_t) * ctx.src_w));
  ctx.x_end = static_cast<uint16_t*>(lv_malloc(sizeof(uint16_t) * ctx.src_w));
  ctx.y_start = static_cast<uint16_t*>(lv_malloc(sizeof(uint16_t) * ctx.src_h));
  ctx.y_end = static_cast<uint16_t*>(lv_malloc(sizeof(uint16_t) * ctx.src_h));
  if (!ctx.x_start || !ctx.x_end || !ctx.y_start || !ctx.y_end) {
    if (ctx.x_start) lv_free(ctx.x_start);
    if (ctx.x_end) lv_free(ctx.x_end);
    if (ctx.y_start) lv_free(ctx.y_start);
    if (ctx.y_end) lv_free(ctx.y_end);
    lv_free(work);
    f.close();
    free_image_ram();
    error = "Kein RAM fuer Skalierung";
    return false;
  }

  for (uint16_t x = 0; x < ctx.src_w; ++x) {
    uint32_t start = (static_cast<uint32_t>(x) * ctx.dst_w) / ctx.src_w;
    uint32_t end = (static_cast<uint32_t>(x + 1) * ctx.dst_w) / ctx.src_w;
    if (end == 0) end = 1;
    end -= 1;
    if (end < start) end = start;
    if (start >= ctx.dst_w) start = ctx.dst_w - 1;
    if (end >= ctx.dst_w) end = ctx.dst_w - 1;
    ctx.x_start[x] = static_cast<uint16_t>(start);
    ctx.x_end[x] = static_cast<uint16_t>(end);
  }
  for (uint16_t y = 0; y < ctx.src_h; ++y) {
    uint32_t start = (static_cast<uint32_t>(y) * ctx.dst_h) / ctx.src_h;
    uint32_t end = (static_cast<uint32_t>(y + 1) * ctx.dst_h) / ctx.src_h;
    if (end == 0) end = 1;
    end -= 1;
    if (end < start) end = start;
    if (start >= ctx.dst_h) start = ctx.dst_h - 1;
    if (end >= ctx.dst_h) end = ctx.dst_h - 1;
    ctx.y_start[y] = static_cast<uint16_t>(start);
    ctx.y_end[y] = static_cast<uint16_t>(end);
  }

  rc = jd_decomp(&jd, tjpgd_output, 0);

  lv_free(ctx.x_start);
  lv_free(ctx.x_end);
  lv_free(ctx.y_start);
  lv_free(ctx.y_end);
  lv_free(work);
  f.close();

  if (rc != JDR_OK) {
    free_image_ram();
    error = "JPEG Decode Fehler";
    return false;
  }

  memset(&g_image_ram_dsc, 0, sizeof(g_image_ram_dsc));
  g_image_ram_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  g_image_ram_dsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  g_image_ram_dsc.header.w = ctx.dst_w;
  g_image_ram_dsc.header.h = ctx.dst_h;
  g_image_ram_dsc.header.stride = ctx.dst_w * 2U;
  g_image_ram_dsc.data_size = g_image_ram_buf_size;
  g_image_ram_dsc.data = g_image_ram_buf;

  g_image_ram_active = true;
  g_image_ram_source = fullPath;
  header.w = ctx.dst_w;
  header.h = ctx.dst_h;
  header.stride = ctx.dst_w * 2U;
  header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  return true;
}

static void collect_images(const String& dir, std::vector<String>& out, size_t max_entries, uint8_t depth, bool allow_bin, bool allow_jpeg) {
  if (out.size() >= max_entries) return;
  File root = SD.open(dir);
  if (!root) return;

  File file = root.openNextFile();
  while (file) {
    if (out.size() >= max_entries) break;
    const char* name_c = file.name();
    String name = name_c ? String(name_c) : String();
    if (file.isDirectory()) {
      if (depth > 0 && name.length()) {
        String next = dir == "/" ? "/" + name : dir + "/" + name;
        collect_images(next, out, max_entries, depth - 1, allow_bin, allow_jpeg);
      }
    } else if (name.length() && ends_with_ignore_case(name, ".bin")) {
      if (allow_bin) {
        if (!name.startsWith("/")) {
          name = dir == "/" ? "/" + name : dir + "/" + name;
        }
        out.push_back(name);
      }
    } else if (name.length() && allow_jpeg &&
               (ends_with_ignore_case(name, ".jpg") || ends_with_ignore_case(name, ".jpeg"))) {
      if (!name.startsWith("/")) {
        name = dir == "/" ? "/" + name : dir + "/" + name;
      }
      out.push_back(name);
    }
    file = root.openNextFile();
  }
}

static bool show_image_popup_internal(const String& fullPath, bool allow_error, bool force_reload = false) {
  if (!SD.exists(fullPath)) {
    if (allow_error) show_image_popup_error("Bild nicht gefunden", fullPath.c_str());
    return false;
  }

  const bool is_jpeg = ends_with_ignore_case(fullPath, ".jpg") || ends_with_ignore_case(fullPath, ".jpeg");
  lv_image_header_t header{};
  if (is_jpeg) {
    if (!force_reload && g_image_ram_active && g_image_ram_source == fullPath && g_current_header_valid) {
      header = g_current_header;
    } else {
      String err;
      if (!decode_jpeg_to_ram(fullPath, header, err)) {
        if (allow_error) show_image_popup_error("JPEG Fehler", err.c_str());
        return false;
      }
      g_current_header = header;
      g_current_header_valid = true;
    }
  } else {
    free_image_ram();
    String new_src = "S:" + fullPath;
    if (!force_reload && g_current_image_path == new_src && g_current_header_valid) {
      header = g_current_header;
    } else {
      if (lv_image_decoder_get_info(new_src.c_str(), &header) != LV_RESULT_OK) {
        if (allow_error) show_image_popup_error("Bildformat nicht unterstuetzt", fullPath.c_str());
        return false;
      }
      g_current_header = header;
      g_current_header_valid = true;
    }
  }

  ensure_image_popup_overlay();

  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  if (is_jpeg) {
    g_current_image_path = fullPath;
    lv_img_set_src(g_image_popup_img, &g_image_ram_dsc);
    lv_obj_set_size(g_image_popup_img, header.w, header.h);
    lv_image_set_inner_align(g_image_popup_img, LV_IMAGE_ALIGN_CENTER);
  } else {
    String new_src = "S:" + fullPath;
    if (force_reload || g_current_image_path != new_src) {
      g_current_image_path = new_src;
      lv_img_set_src(g_image_popup_img, g_current_image_path.c_str());
    }
    lv_obj_set_size(g_image_popup_img, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_image_set_inner_align(g_image_popup_img, LV_IMAGE_ALIGN_DEFAULT);
    lv_image_set_inner_align(g_image_popup_img, LV_IMAGE_ALIGN_CONTAIN);
  }
  lv_obj_center(g_image_popup_img);
  lv_obj_clear_flag(g_image_popup_img, LV_OBJ_FLAG_HIDDEN);
  if (g_image_popup_label) lv_obj_add_flag(g_image_popup_label, LV_OBJ_FLAG_HIDDEN);

  g_image_shown = true;
  return true;
}

static void slideshow_tick(lv_timer_t*) {
  if (g_slideshow_files.empty()) {
    stop_slideshow(true);
    return;
  }
  const size_t count = g_slideshow_files.size();
  for (size_t i = 0; i < count; ++i) {
    g_slideshow_index = (g_slideshow_index + 1) % count;
    displayManager.setReverseFlushOnce();
    if (show_image_popup_internal(g_slideshow_files[g_slideshow_index], false)) return;
  }
  show_image_popup_error("Keine Bilder gefunden", "/");
  stop_slideshow(true);
}

static void start_slideshow(uint16_t interval_sec, SlideshowMode mode) {
  cancel_popup_restore();
  stop_slideshow(true);
  apply_slideshow_display_mode(true);
  g_slideshow_interval_ms = normalize_slideshow_interval_ms(interval_sec);
  const bool allow_bin = (mode == SlideshowMode::Bin || mode == SlideshowMode::All);
  const bool allow_jpeg = (mode == SlideshowMode::Jpeg || mode == SlideshowMode::All);
  collect_images("/", g_slideshow_files, 200, 3, allow_bin, allow_jpeg);
  if (g_slideshow_files.empty()) {
    const char* msg = (mode == SlideshowMode::Jpeg)
                        ? "Keine JPEG Bilder gefunden"
                        : (mode == SlideshowMode::Bin)
                            ? "Keine .bin Bilder gefunden"
                            : "Keine Bilder gefunden";
    show_image_popup_error(msg, "/");
    apply_slideshow_display_mode(false);
    return;
  }
  g_slideshow_index = 0;
  displayManager.setReverseFlushOnce();
  if (!show_image_popup_internal(g_slideshow_files[0], true)) {
    slideshow_tick(nullptr);
  }
  g_slideshow_timer = lv_timer_create(slideshow_tick, g_slideshow_interval_ms, nullptr);
}

static void show_image_popup_error(const char* title, const char* path) {
  ensure_image_popup_overlay();

  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_90, 0);
  lv_obj_clear_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  if (g_image_popup_img) {
    lv_obj_add_flag(g_image_popup_img, LV_OBJ_FLAG_HIDDEN);
  }
  if (g_image_popup_label) {
    lv_label_set_text_fmt(g_image_popup_label, "%s:\n%s", title, path);
    lv_obj_clear_flag(g_image_popup_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(g_image_popup_label);
  }

  g_image_shown = true;
}

void show_image_popup(const char* path, uint16_t slideshow_sec) {
  if (!path || strlen(path) == 0) return;

  // Andere Popups verstecken (wie light_popup und sensor_popup)
  hide_sensor_popup();
  hide_light_popup();

  String rawPath = String(path);
  rawPath.trim();
  if (is_url_path(rawPath)) {
    stop_slideshow(true);
    apply_slideshow_display_mode(true);
    displayManager.setReverseFlushOnce();
    String cached;
    String err;
    Serial.printf("[ImagePopup] Lade URL: %s\n", rawPath.c_str());
    if (!download_url_to_sd(rawPath, cached, err)) {
      show_image_popup_error("URL Fehler", err.c_str());
      apply_slideshow_display_mode(false);
      return;
    }
    Serial.printf("[ImagePopup] Zeige URL Cache: %s\n", cached.c_str());
    show_image_popup_internal(cached, true, true);
    return;
  }

  String fullPath = normalize_sd_path(rawPath);
  SlideshowMode mode = get_slideshow_mode(fullPath);
  if (mode != SlideshowMode::None) {
    Serial.println("[ImagePopup] Starte Diashow");
    start_slideshow(slideshow_sec, mode);
    return;
  }

  cancel_popup_restore();
  stop_slideshow(true);
  apply_slideshow_display_mode(true);
  displayManager.setReverseFlushOnce();
  Serial.printf("[ImagePopup] Zeige Bild: %s\n", fullPath.c_str());
  show_image_popup_internal(fullPath, true);
}

void hide_image_popup() {
  if (g_image_popup_overlay) {
    stop_slideshow(false);
    apply_slideshow_display_mode(true);
    displayManager.setReverseFlushOnce();
    lv_obj_add_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);
    g_image_shown = false;
    free_image_ram();
    schedule_popup_restore();
  }
}

void preload_image_popup(const char* path) {
  if (!path || strlen(path) == 0) return;

  String rawPath = String(path);
  rawPath.trim();
  if (is_url_path(rawPath)) return;
  String fullPath = normalize_sd_path(rawPath);
  if (get_slideshow_mode(fullPath) != SlideshowMode::None) return;
  if (!SD.exists(fullPath)) return;

  String src = "S:" + fullPath;
  lv_image_header_t header;
  if (lv_image_decoder_get_info(src.c_str(), &header) == LV_RESULT_OK) {
    g_current_header = header;
    g_current_header_valid = true;
    g_current_image_path = src;
  }
}

static void apply_slideshow_display_mode(bool enable) {
  if (enable) {
    if (g_slideshow_buffer_override) return;
    g_slideshow_prev_lines = displayManager.getBufferLines();
    g_slideshow_prev_mode = displayManager.getRenderMode();
    if (g_slideshow_prev_lines == 0) return;
    if (displayManager.setBufferLines(SCREEN_HEIGHT, g_slideshow_prev_mode)) {
      g_slideshow_buffer_override = true;
    }
  } else {
    if (!g_slideshow_buffer_override) return;
    if (g_slideshow_prev_lines > 0) {
      displayManager.setBufferLines(g_slideshow_prev_lines, g_slideshow_prev_mode);
    }
    g_slideshow_buffer_override = false;
  }
}

 

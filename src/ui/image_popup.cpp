#include "image_popup.h"
#include "sensor_popup.h"
#include "light_popup.h"
#include "src/core/display_manager.h"
#include <SD.h>
#include <M5Unified.h>
#include <vector>
#include <draw/lv_image_decoder.h>

// Globaler Context fuer das Image Popup
static lv_obj_t* g_image_popup_overlay = nullptr;
static lv_obj_t* g_image_popup_img = nullptr;
static lv_obj_t* g_image_popup_label = nullptr;
static bool g_image_shown = false;
static lv_image_header_t g_current_header;
static bool g_current_header_valid = false;
static String g_current_image_path;  // Pfad muss persistent sein fuer LVGL!
static const char* kSlideshowToken = "__slideshow__";
static lv_timer_t* g_slideshow_timer = nullptr;
static lv_timer_t* g_popup_restore_timer = nullptr;
static std::vector<String> g_slideshow_files;
static size_t g_slideshow_index = 0;
static size_t g_slideshow_prev_lines = 0;
static lv_display_render_mode_t g_slideshow_prev_mode = LV_DISPLAY_RENDER_MODE_PARTIAL;
static bool g_slideshow_buffer_override = false;
static void show_image_popup_error(const char* title, const char* path);
static void apply_slideshow_display_mode(bool enable);
static void schedule_popup_restore();
static void cancel_popup_restore();

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

static bool is_slideshow_token(const String& path) {
  String p = path;
  p.trim();
  if (p.startsWith("/")) p = p.substring(1);
  p.toLowerCase();
  return p == kSlideshowToken;
}

static String normalize_sd_path(String path) {
  path.trim();
  if (!path.startsWith("/")) path = "/" + path;
  return path;
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

static void collect_bin_images(const String& dir, std::vector<String>& out, size_t max_entries, uint8_t depth) {
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
        collect_bin_images(next, out, max_entries, depth - 1);
      }
    } else if (name.length() && ends_with_ignore_case(name, ".bin")) {
      if (!name.startsWith("/")) {
        name = dir == "/" ? "/" + name : dir + "/" + name;
      }
      out.push_back(name);
    }
    file = root.openNextFile();
  }
}

static bool show_image_popup_internal(const String& fullPath, bool allow_error) {
  if (!SD.exists(fullPath)) {
    if (allow_error) show_image_popup_error("Bild nicht gefunden", fullPath.c_str());
    return false;
  }

  String new_src = "S:" + fullPath;
  lv_image_header_t header;
  if (g_current_image_path == new_src && g_current_header_valid) {
    header = g_current_header;
  } else {
    if (lv_image_decoder_get_info(new_src.c_str(), &header) != LV_RESULT_OK) {
      if (allow_error) show_image_popup_error("Bildformat nicht unterstuetzt", fullPath.c_str());
      return false;
    }
    g_current_header = header;
    g_current_header_valid = true;
  }

  ensure_image_popup_overlay();

  lv_obj_set_style_bg_opa(g_image_popup_overlay, LV_OPA_COVER, 0);
  lv_obj_clear_flag(g_image_popup_overlay, LV_OBJ_FLAG_HIDDEN);

  if (g_current_image_path != new_src) {
    g_current_image_path = new_src;
    lv_img_set_src(g_image_popup_img, g_current_image_path.c_str());
  }

  lv_obj_set_size(g_image_popup_img, header.w, header.h);
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

static void start_slideshow() {
  cancel_popup_restore();
  stop_slideshow(true);
  apply_slideshow_display_mode(true);
  collect_bin_images("/", g_slideshow_files, 200, 3);
  if (g_slideshow_files.empty()) {
    show_image_popup_error("Keine .bin Bilder gefunden", "/");
    apply_slideshow_display_mode(false);
    return;
  }
  g_slideshow_index = 0;
  displayManager.setReverseFlushOnce();
  if (!show_image_popup_internal(g_slideshow_files[0], true)) {
    slideshow_tick(nullptr);
  }
  g_slideshow_timer = lv_timer_create(slideshow_tick, 10000, nullptr);
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

void show_image_popup(const char* path) {
  if (!path || strlen(path) == 0) return;

  // Andere Popups verstecken (wie light_popup und sensor_popup)
  hide_sensor_popup();
  hide_light_popup();

  String fullPath = normalize_sd_path(path);
  if (is_slideshow_token(fullPath)) {
    Serial.println("[ImagePopup] Starte Diashow");
    start_slideshow();
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
    schedule_popup_restore();
  }
}

void preload_image_popup(const char* path) {
  if (!path || strlen(path) == 0) return;

  String fullPath = normalize_sd_path(path);
  if (is_slideshow_token(fullPath)) return;
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

 

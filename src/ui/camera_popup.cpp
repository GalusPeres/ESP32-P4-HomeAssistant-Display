#include "src/ui/camera_popup.h"

#include <ArduinoJson.h>

#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/core/power_manager.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/ui/climate_popup.h"
#include "src/ui/energy_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/media_popup.h"
#include "src/ui/popup_layout.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/weather_popup.h"
#include "src/video/camera_stream.h"

namespace {

struct CameraPopupContext {
  String entity_id;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* image = nullptr;
  lv_obj_t* placeholder = nullptr;
  lv_obj_t* status = nullptr;
  bool visible = false;
};

CameraPopupContext* g_camera_popup = nullptr;

static void set_status_label(const char* text, bool error) {
  if (!g_camera_popup || !g_camera_popup->status) return;
  lv_label_set_text(g_camera_popup->status, text ? text : "");
  lv_obj_set_style_text_color(
      g_camera_popup->status,
      error ? lv_color_hex(0xFF6B6B) : lv_color_hex(0xD8DEE9), 0);
}

static void close_camera_popup() {
  if (!g_camera_popup || !g_camera_popup->visible) return;
  const String entity_id = g_camera_popup->entity_id;
  g_camera_popup->visible = false;
  camera_stream_stop();
  if (entity_id.length()) {
    mqttPublishCameraCommand(entity_id.c_str(), "close");
  }
  lv_obj_add_flag(g_camera_popup->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_camera_popup->overlay, LV_OBJ_FLAG_CLICKABLE);
}

static void close_event_cb(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_SHORT_CLICKED) {
    close_camera_popup();
  }
}

static CameraPopupContext* create_popup() {
  CameraPopupContext* ctx = new CameraPopupContext();
  ctx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ctx->overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_align(ctx->overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(ctx->overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(ctx->overlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(ctx->overlay, 0, 0);
  lv_obj_set_style_pad_all(ctx->overlay, 0, 0);
  lv_obj_remove_flag(ctx->overlay, LV_OBJ_FLAG_SCROLLABLE);

  ctx->card = lv_obj_create(ctx->overlay);
  lv_obj_set_size(ctx->card, 760, 740);
  lv_obj_align(ctx->card, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(ctx->card, lv_color_hex(0x171717), 0);
  lv_obj_set_style_bg_opa(ctx->card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ctx->card, 24, 0);
  lv_obj_set_style_border_width(ctx->card, 0, 0);
  lv_obj_set_style_shadow_width(ctx->card, 24, 0);
  lv_obj_set_style_shadow_opa(ctx->card, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(ctx->card, 0, 0);
  lv_obj_remove_flag(ctx->card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* header_icon = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(header_icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(header_icon, lv_color_white(), 0);
  lv_obj_align(header_icon, LV_ALIGN_TOP_LEFT, 28, 24);
  lv_obj_add_flag(header_icon, LV_OBJ_FLAG_USER_1);

  lv_obj_t* title = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(title, &ui_font_32, 0);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_width(title, 520);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 92, 28);
  lv_obj_add_flag(title, LV_OBJ_FLAG_USER_2);

  lv_obj_t* close = lv_button_create(ctx->card);
  lv_obj_set_size(close, 56, 56);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -20, 16);
  lv_obj_set_style_radius(close, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(close, lv_color_hex(0x303030), 0);
  lv_obj_set_style_shadow_width(close, 0, 0);
  lv_obj_add_event_cb(close, close_event_cb, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_t* close_icon = lv_label_create(close);
  lv_obj_set_style_text_font(close_icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_icon, lv_color_white(), 0);
  const String close_char = getMdiChar("close");
  lv_label_set_text(close_icon, close_char.c_str());
  lv_obj_center(close_icon);

  lv_obj_t* video = lv_obj_create(ctx->card);
  lv_obj_set_size(video, 640, 480);
  lv_obj_align(video, LV_ALIGN_TOP_MID, 0, 92);
  lv_obj_set_style_bg_color(video, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(video, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(video, 12, 0);
  lv_obj_set_style_clip_corner(video, true, 0);
  lv_obj_set_style_border_width(video, 0, 0);
  lv_obj_set_style_pad_all(video, 0, 0);
  lv_obj_remove_flag(video, LV_OBJ_FLAG_SCROLLABLE);

  ctx->image = lv_image_create(video);
  lv_obj_set_size(ctx->image, 640, 480);
  lv_obj_center(ctx->image);
  lv_obj_add_flag(ctx->image, LV_OBJ_FLAG_HIDDEN);

  ctx->placeholder = lv_label_create(video);
  lv_obj_set_style_text_font(ctx->placeholder, &ui_font_24, 0);
  lv_obj_set_style_text_color(ctx->placeholder, lv_color_hex(0xB7C1D6), 0);
  lv_label_set_text(ctx->placeholder, "Kamerastream wird vorbereitet ...");
  lv_obj_center(ctx->placeholder);

  ctx->status = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->status, &ui_font_20, 0);
  lv_obj_set_style_text_color(ctx->status, lv_color_hex(0xD8DEE9), 0);
  lv_label_set_text(ctx->status, "Bereit");
  lv_obj_align(ctx->status, LV_ALIGN_TOP_MID, 0, 600);

  lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  return ctx;
}

static lv_obj_t* find_flagged_child(lv_obj_t* parent, lv_obj_flag_t flag) {
  const uint32_t count = lv_obj_get_child_count(parent);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t* child = lv_obj_get_child(parent, static_cast<int32_t>(i));
    if (child && lv_obj_has_flag(child, flag)) return child;
  }
  return nullptr;
}

}  // namespace

void show_camera_popup(const CameraPopupInit& init) {
  hide_sensor_popup();
  hide_weather_popup();
  hide_energy_popup();
  hide_light_popup();
  hide_media_popup();
  hide_climate_popup();

  if (!g_camera_popup) g_camera_popup = create_popup();
  if (!g_camera_popup) return;

  g_camera_popup->entity_id = init.entity_id;
  g_camera_popup->visible = true;
  lv_obj_t* icon =
      find_flagged_child(g_camera_popup->card, LV_OBJ_FLAG_USER_1);
  lv_obj_t* title =
      find_flagged_child(g_camera_popup->card, LV_OBJ_FLAG_USER_2);
  String icon_name = normalizeMdiIconName(init.icon_name);
  if (!icon_name.length()) icon_name = "video";
  const String icon_char = getMdiChar(icon_name);
  if (icon) lv_label_set_text(icon, icon_char.c_str());
  if (title) lv_label_set_text(title, init.title.c_str());

  lv_obj_add_flag(g_camera_popup->image, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_camera_popup->placeholder, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(g_camera_popup->placeholder,
                    "Kamerastream wird vorbereitet ...");
  set_status_label("Bridge wird angefragt ...", false);
  lv_obj_clear_flag(g_camera_popup->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_camera_popup->overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_foreground(g_camera_popup->overlay);
  displayManager.resetActivityTimer();
  mqttPublishCameraCommand(init.entity_id.c_str(), "open");
}

void hide_camera_popup() {
  close_camera_popup();
}

void process_camera_popup() {
  if (!g_camera_popup || !g_camera_popup->visible) return;
  if (powerManager.isInSleep()) {
    close_camera_popup();
    return;
  }
  camera_stream_process_ui(g_camera_popup->image,
                           g_camera_popup->placeholder,
                           g_camera_popup->status);
}

void camera_popup_handle_mqtt_status(const char* payload) {
  if (!payload || !*payload || !g_camera_popup ||
      !g_camera_popup->visible) {
    return;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, payload)) {
    set_status_label("Ungueltige Kamera-Antwort", true);
    return;
  }
  const char* entity = doc["entity_id"] | "";
  if (*entity &&
      !g_camera_popup->entity_id.equalsIgnoreCase(String(entity))) {
    return;
  }
  const char* status = doc["status"] | "";
  if (strcmp(status, "ready") == 0) {
    const char* url = doc["url"] | "";
    if (!*url) {
      set_status_label("Bridge lieferte keine Stream-URL", true);
      return;
    }
    set_status_label("Stream wird verbunden ...", false);
    camera_stream_start(url);
    return;
  }
  if (strcmp(status, "error") == 0) {
    const char* error = doc["error"] | "Stream nicht verfuegbar";
    set_status_label(error, true);
    return;
  }
  if (strcmp(status, "stopped") == 0) {
    set_status_label("Stream beendet", false);
  }
}

void camera_popup_set_status(const char* text, bool error) {
  set_status_label(text, error);
  camera_stream_set_external_status(text, error);
}

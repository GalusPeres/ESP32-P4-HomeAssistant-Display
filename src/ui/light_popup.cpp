#include "src/ui/light_popup.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/weather_popup.h"
#include "src/core/config_manager.h"
#include "src/core/display_manager.h"
#include "src/core/i18n.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_config.h"
#include "src/tiles/tile_renderer_shared.h"
#include "esp_heap_caps.h"
#include <math.h>

namespace {

constexpr int kCardMargin = 4;
constexpr int kCardWidth =
    (SCREEN_WIDTH > SCREEN_HEIGHT)
        ? (SCREEN_HEIGHT - (kCardMargin * 2))
        : (SCREEN_WIDTH - (kCardMargin * 2));
constexpr int kCardHeight = SCREEN_HEIGHT - (kCardMargin * 2);
constexpr int kCardPad = 20;
constexpr int kContentPadTop = 94;
constexpr int kHeaderIconOffsetX = 0;
constexpr int kHeaderIconOffsetY = 0;
constexpr int kTopValueHeight = 60;
constexpr int kTopValueBottomPad = 16;
constexpr int kMainPanelHeight = 384;
constexpr int kControlsRowTopPad = 12;
constexpr int kControlsRowHeight = 104;
constexpr int kControlsRowPadX = 12;
constexpr int kControlsRowGap = 12;
constexpr int kControlButtonSize = 92;
constexpr int kVerticalSliderWidth = 160;
constexpr int kVerticalSliderHeight = 340;
constexpr int kVerticalSliderRadius = 32;
constexpr int kVerticalValueLabelPadTop = 0;
constexpr int kTempHandleWidth = kVerticalSliderWidth;
constexpr int kTempHandleRadius = kVerticalSliderRadius;
constexpr int kTempHandleHeight = kTempHandleRadius * 2;
constexpr int kTempHandleDashWidth = 46;
constexpr int kTempHandleDashHeight = 4;
constexpr int kTempWrapWidth = kVerticalSliderWidth;
constexpr int kTempWrapHeight = kVerticalSliderHeight + kTempHandleHeight;
constexpr int kColorFieldWidth = 340;
constexpr int kColorFieldHeight = 340;
constexpr int kColorFieldWrapHeight = 384;
constexpr int kColorFieldCursorSize = 30;
constexpr int kBrightnessDashWidth = kTempHandleDashWidth;
constexpr int kBrightnessDashHeight = kTempHandleDashHeight;

constexpr uint32_t kDefaultColor = 0xFFD54F;
constexpr uint32_t kSwitchOnColor = 0x3B82F6;
constexpr uint32_t kRemoteBlockMs = 3000;
constexpr uint32_t kControlButtonBg = 0x2A2A2A;
constexpr uint32_t kControlButtonActiveBg = 0x3A3A3D;
constexpr uint32_t kControlButtonDisabled = 0x6B6B6B;
constexpr uint32_t kControlBarBg = 0x1F1F22;
constexpr uint32_t kTempWarmColor = 0xFFD27D;
constexpr uint32_t kTempCoolColor = 0xFFFFFF;
constexpr float kPi = 3.14159265358979323846f;

enum class LightPopupMode : uint8_t {
  Brightness,
  Color,
  Temperature,
};

struct LightPopupContext {
  String entity_id;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* top_value_label = nullptr;
  lv_obj_t* main_panel = nullptr;
  lv_obj_t* brightness_panel = nullptr;
  lv_obj_t* color_panel = nullptr;
  lv_obj_t* temperature_panel = nullptr;
  lv_obj_t* controls_row = nullptr;
  lv_obj_t* power_button = nullptr;
  lv_obj_t* power_button_icon = nullptr;
  lv_obj_t* brightness_button = nullptr;
  lv_obj_t* brightness_button_icon = nullptr;
  lv_obj_t* color_button = nullptr;
  lv_obj_t* color_button_icon = nullptr;
  lv_obj_t* temperature_button = nullptr;
  lv_obj_t* temperature_button_icon = nullptr;
  lv_obj_t* color_field_wrap = nullptr;
  lv_obj_t* color_field_frame = nullptr;
  lv_obj_t* color_field_canvas = nullptr;
  lv_obj_t* color_field_cursor = nullptr;
  lv_obj_t* val_slider = nullptr;
  lv_obj_t* val_value = nullptr;
  lv_obj_t* temp_slider_wrap = nullptr;
  lv_obj_t* temp_slider = nullptr;
  lv_obj_t* temp_slider_handle = nullptr;
  lv_obj_t* temp_value_label = nullptr;
  uint16_t hue = 0;
  uint8_t sat = 0;
  uint8_t val = 100;
  uint16_t color_temp_kelvin = 4000;
  uint16_t min_color_temp_kelvin = 2200;
  uint16_t max_color_temp_kelvin = 6500;
  uint8_t last_brightness = 100;
  bool supports_color = false;
  bool supports_brightness = false;
  bool supports_temperature = false;
  bool is_light = true;
  bool is_on = true;
  bool keep_icon_white = false;
  bool has_tile_ref = false;
  uint8_t tile_grid = 0;
  uint8_t tile_index = 0;
  bool user_dragging = false;
  uint32_t last_user_action_ms = 0;
  uint32_t block_remote_until_ms = 0;
  bool suppress_events = false;
  bool color_field_ready = false;
  bool use_color_temperature = false;
  uint8_t* color_field_buf = nullptr;
  uint32_t color_field_stride = 0;
  LightPopupMode mode = LightPopupMode::Color;
};

static LightPopupContext* g_light_popup_ctx = nullptr;

// Forward declarations
static void update_preview(LightPopupContext* ctx);
static void commit_popup_state(LightPopupContext* ctx);
static void on_overlay_click(lv_event_t* e);
static void on_val_slider_draw(lv_event_t* e);
static void on_temp_slider_draw(lv_event_t* e);

static void on_close_click(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->overlay || !ctx->card) return;
  lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

static void on_overlay_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  (void)e;
}

static void mark_user_action(LightPopupContext* ctx) {
  if (!ctx) return;
  uint32_t now = millis();
  ctx->last_user_action_ms = now;
  ctx->block_remote_until_ms = now + kRemoteBlockMs;
}

static bool should_block_remote_update(LightPopupContext* ctx) {
  if (!ctx) return false;
  if (ctx->user_dragging) return true;
  uint32_t now = millis();
  return (now - ctx->last_user_action_ms) < 500;
}

static bool get_init_hs(const LightPopupInit& init, uint16_t& h, uint8_t& s) {
  if (init.has_hs) {
    h = static_cast<uint16_t>(roundf(init.hs_h));
    s = static_cast<uint8_t>(roundf(init.hs_s));
    return true;
  }
  if (init.has_color && init.color) {
    lv_color_hsv_t hsv = lv_color_to_hsv(lv_color_hex(init.color));
    h = hsv.h;
    s = hsv.s;
    return true;
  }
  return false;
}

static bool is_german_language() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  return tr.code && strncmp(tr.code, "de", 2) == 0;
}

static const char* get_off_text() {
  return is_german_language() ? "Aus" : "Off";
}

static const char* get_on_text() {
  return is_german_language() ? "An" : "On";
}

static uint16_t normalize_hue(int value) {
  while (value < 0) value += 360;
  while (value >= 360) value -= 360;
  return static_cast<uint16_t>(value);
}

static uint16_t clamp_color_temp_kelvin(int kelvin) {
  if (kelvin < 2200) return 2200;
  if (kelvin > 6500) return 6500;
  return static_cast<uint16_t>(kelvin);
}

static void normalize_color_temp_range(uint16_t& min_kelvin, uint16_t& max_kelvin) {
  if (min_kelvin < 1500) min_kelvin = 1500;
  if (min_kelvin > 9000) min_kelvin = 9000;
  if (max_kelvin < 1500) max_kelvin = 1500;
  if (max_kelvin > 9000) max_kelvin = 9000;
  if (min_kelvin > max_kelvin) {
    const uint16_t tmp = min_kelvin;
    min_kelvin = max_kelvin;
    max_kelvin = tmp;
  }
}

static uint16_t clamp_color_temp_kelvin_to_range(int kelvin, uint16_t min_kelvin, uint16_t max_kelvin) {
  normalize_color_temp_range(min_kelvin, max_kelvin);
  if (kelvin < static_cast<int>(min_kelvin)) return min_kelvin;
  if (kelvin > static_cast<int>(max_kelvin)) return max_kelvin;
  return static_cast<uint16_t>(kelvin);
}

static float get_color_temp_ratio(uint16_t kelvin, uint16_t min_kelvin, uint16_t max_kelvin) {
  normalize_color_temp_range(min_kelvin, max_kelvin);
  if (max_kelvin <= min_kelvin) return 0.0f;
  const uint16_t clamped = clamp_color_temp_kelvin_to_range(kelvin, min_kelvin, max_kelvin);
  return static_cast<float>(clamped - min_kelvin) /
         static_cast<float>(max_kelvin - min_kelvin);
}

static int slider_value_from_kelvin(uint16_t kelvin, uint16_t min_kelvin, uint16_t max_kelvin) {
  normalize_color_temp_range(min_kelvin, max_kelvin);
  const uint16_t clamped = clamp_color_temp_kelvin_to_range(kelvin, min_kelvin, max_kelvin);
  return static_cast<int>(min_kelvin + max_kelvin - clamped);
}

static uint16_t kelvin_from_slider_value(int value, uint16_t min_kelvin, uint16_t max_kelvin) {
  normalize_color_temp_range(min_kelvin, max_kelvin);
  return clamp_color_temp_kelvin_to_range(min_kelvin + max_kelvin - value, min_kelvin, max_kelvin);
}

static uint8_t clamp_channel(float value) {
  if (value < 0.0f) return 0;
  if (value > 255.0f) return 255;
  return static_cast<uint8_t>(lroundf(value));
}

static uint32_t color_from_hsv(uint16_t h, uint8_t s, uint8_t v) {
  lv_color_t color = lv_color_hsv_to_rgb(h, s, v);
  return lv_color_to_u32(color) & 0xFFFFFF;
}

static lv_color_t color_from_temperature_kelvin(uint16_t kelvin) {
  const float temp = static_cast<float>(clamp_color_temp_kelvin_to_range(kelvin, 1500, 9000)) / 100.0f;

  float red = 255.0f;
  float green = 255.0f;
  float blue = 255.0f;

  if (temp <= 66.0f) {
    red = 255.0f;
    green = 99.4708025861f * logf(temp) - 161.1195681661f;
    if (temp <= 19.0f) {
      blue = 0.0f;
    } else {
      blue = 138.5177312231f * logf(temp - 10.0f) - 305.0447927307f;
    }
  } else {
    red = 329.698727446f * powf(temp - 60.0f, -0.1332047592f);
    green = 288.1221695283f * powf(temp - 60.0f, -0.0755148492f);
    blue = 255.0f;
  }

  return lv_color_make(clamp_channel(red), clamp_channel(green), clamp_channel(blue));
}

static bool is_remote_update_close(LightPopupContext* ctx, const LightPopupInit& init) {
  if (!ctx) return true;
  if (ctx->supports_color) {
    uint16_t h = 0;
    uint8_t s = 0;
    if (get_init_hs(init, h, s)) {
      int dh = abs(static_cast<int>(h) - static_cast<int>(ctx->hue));
      if (dh > 180) dh = 360 - dh;
      int ds = abs(static_cast<int>(s) - static_cast<int>(ctx->sat));
      if (dh > 4 || ds > 4) return false;
    }
  }
  if (ctx->supports_temperature && init.has_color_temp) {
    int dk = abs(static_cast<int>(init.color_temp_kelvin) - static_cast<int>(ctx->color_temp_kelvin));
    if (dk > 120) return false;
  }
  if (ctx->supports_brightness && init.has_brightness) {
    int dv = abs(static_cast<int>(init.brightness_pct) - static_cast<int>(ctx->val));
    if (dv > 3) return false;
  }
  return true;
}

static void set_label_style(lv_obj_t* lbl, lv_color_t color) {
  if (!lbl) return;
  lv_obj_set_style_text_color(lbl, color, 0);
  lv_obj_set_style_text_font(lbl, &ui_font_24, 0);
}

static void update_popup_language(LightPopupContext* ctx) {
  (void)ctx;
}

static void update_value_label(lv_obj_t* label, int value, const char* suffix) {
  if (!label) return;
  char buf[24];
  if (suffix && suffix[0]) {
    snprintf(buf, sizeof(buf), "%d%s", value, suffix);
  } else {
    snprintf(buf, sizeof(buf), "%d", value);
  }
  lv_label_set_text(label, buf);
}

static float get_brightness_visual_ratio(uint8_t value) {
  if (value == 0) return 0.0f;
  if (value >= 100) return 1.0f;
  const float logical = static_cast<float>(value - 1) / 99.0f;
  constexpr float kMinVisibleRatio =
      static_cast<float>(kVerticalSliderRadius * 2) / static_cast<float>(kVerticalSliderHeight);
  return kMinVisibleRatio + (logical * (1.0f - kMinVisibleRatio));
}

static bool ensure_color_field_buffer(LightPopupContext* ctx) {
  if (!ctx) return false;
  if (ctx->color_field_buf) return true;
  ctx->color_field_stride = lv_draw_buf_width_to_stride(kColorFieldWidth, LV_COLOR_FORMAT_ARGB8888);
  const size_t bytes = static_cast<size_t>(ctx->color_field_stride) *
                       static_cast<size_t>(kColorFieldHeight);
  ctx->color_field_buf = static_cast<uint8_t*>(
      heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!ctx->color_field_buf) {
    ctx->color_field_buf = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(64, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  }
  return ctx->color_field_buf != nullptr;
}

static void render_color_field(LightPopupContext* ctx) {
  if (!ctx || !ctx->color_field_canvas) return;
  if (!ensure_color_field_buffer(ctx)) return;

  lv_canvas_set_buffer(ctx->color_field_canvas,
                       ctx->color_field_buf,
                       kColorFieldWidth,
                       kColorFieldHeight,
                       LV_COLOR_FORMAT_ARGB8888);

  const float cx = (kColorFieldWidth - 1) * 0.5f;
  const float cy = (kColorFieldHeight - 1) * 0.5f;
  const float radius = (kColorFieldWidth - 2) * 0.5f;
  const float aa_width = 2.0f;

  for (int y = 0; y < kColorFieldHeight; ++y) {
    lv_color32_t* row = reinterpret_cast<lv_color32_t*>(ctx->color_field_buf + (y * ctx->color_field_stride));
    for (int x = 0; x < kColorFieldWidth; ++x) {
      const float dx = x - cx;
      const float dy = cy - y;
      const float dist = sqrtf((dx * dx) + (dy * dy));
      if (dist > radius) {
        row[x] = lv_color32_make(0, 0, 0, 0);
        continue;
      }
      float angle = atan2f(dy, dx) * 180.0f / kPi;
      if (angle < 0.0f) angle += 360.0f;
      const uint16_t hue = static_cast<uint16_t>(angle);
      const uint8_t sat = static_cast<uint8_t>(lroundf((dist / radius) * 100.0f));
      const lv_color_t color = lv_color_hsv_to_rgb(hue, sat, 100);
      lv_opa_t opa = LV_OPA_COVER;
      if (dist > (radius - aa_width)) {
        const float t = (radius - dist) / aa_width;
        const float clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        opa = static_cast<lv_opa_t>(lroundf(clamped * 255.0f));
      }
      row[x] = lv_color_to_32(color, opa);
    }
  }

  lv_obj_invalidate(ctx->color_field_canvas);
  ctx->color_field_ready = true;
}

static void update_color_field_cursor(LightPopupContext* ctx) {
  if (!ctx || !ctx->color_field_ready || !ctx->color_field_cursor || !ctx->color_field_frame) return;
  uint16_t display_hue = ctx->hue;
  uint8_t display_sat = ctx->sat;
  if (ctx->supports_temperature && ctx->use_color_temperature) {
    const lv_color_hsv_t hsv = lv_color_to_hsv(color_from_temperature_kelvin(ctx->color_temp_kelvin));
    display_hue = normalize_hue(static_cast<int>(hsv.h));
    display_sat = hsv.s;
  }
  const float cx = (kColorFieldWidth - 1) * 0.5f;
  const float cy = (kColorFieldHeight - 1) * 0.5f;
  const float radius = (kColorFieldWidth - 2) * 0.5f;
  const float angle = (static_cast<float>(display_hue) * kPi) / 180.0f;
  const float sat_radius = (static_cast<float>(display_sat) / 100.0f) * radius;
  const int frame_x = lv_obj_get_x(ctx->color_field_frame);
  const int frame_y = lv_obj_get_y(ctx->color_field_frame);
  const int x = frame_x + static_cast<int>(lroundf(cx + cosf(angle) * sat_radius));
  const int y = frame_y + static_cast<int>(lroundf(cy - sinf(angle) * sat_radius));
  lv_obj_set_pos(ctx->color_field_cursor,
                 x - (kColorFieldCursorSize / 2),
                 y - (kColorFieldCursorSize / 2));
  const uint32_t display_rgb = (ctx->supports_temperature && ctx->use_color_temperature)
                                   ? (lv_color_to_u32(color_from_temperature_kelvin(ctx->color_temp_kelvin)) & 0xFFFFFF)
                                   : color_from_hsv(display_hue, display_sat, 100);
  lv_obj_set_style_bg_color(ctx->color_field_cursor, lv_color_hex(display_rgb), 0);
}

static void update_top_value_label(LightPopupContext* ctx) {
  if (!ctx || !ctx->top_value_label) return;
  char buf[24];
  if (!ctx->is_on || (ctx->supports_brightness && ctx->val == 0)) {
    lv_label_set_text(ctx->top_value_label, get_off_text());
    return;
  }
  if (ctx->supports_brightness) {
    snprintf(buf, sizeof(buf), "%u %%", ctx->val);
    lv_label_set_text(ctx->top_value_label, buf);
    return;
  }
  lv_label_set_text(ctx->top_value_label, get_on_text());
}

static void update_temperature_value_label(LightPopupContext* ctx) {
  if (!ctx || !ctx->temp_value_label) return;
  char buf[24];
  snprintf(buf, sizeof(buf), "%u K", static_cast<unsigned>(ctx->color_temp_kelvin));
  lv_label_set_text(ctx->temp_value_label, buf);
}

static void update_temperature_handle(LightPopupContext* ctx) {
  if (!ctx || !ctx->temp_slider || !ctx->temp_slider_handle || !ctx->temp_slider_wrap) return;

  lv_obj_update_layout(ctx->temp_slider_wrap);

  lv_area_t track_area;
  lv_area_t wrap_area;
  lv_obj_get_coords(ctx->temp_slider, &track_area);
  lv_obj_get_coords(ctx->temp_slider_wrap, &wrap_area);

  const int track_height = lv_area_get_height(&track_area);
  if (track_height <= 1) return;

  float ratio = static_cast<float>(ctx->color_temp_kelvin - 2200) /
                static_cast<float>(6500 - 2200);
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;

  const int center_x = track_area.x1 + (lv_area_get_width(&track_area) / 2);
  const int center_y = track_area.y1 + static_cast<int>(lroundf(ratio * (track_height - 1)));
  const int local_x = center_x - wrap_area.x1 - (kTempHandleWidth / 2);
  const int local_y = center_y - wrap_area.y1 - (kTempHandleHeight / 2);

  lv_obj_set_pos(ctx->temp_slider_handle, local_x, local_y);
  lv_obj_move_foreground(ctx->temp_slider_handle);
}

static bool is_mode_available(const LightPopupContext* ctx, LightPopupMode mode) {
  if (!ctx || !ctx->is_light) return false;
  switch (mode) {
    case LightPopupMode::Brightness: return ctx->supports_brightness;
    case LightPopupMode::Color: return ctx->supports_color;
    case LightPopupMode::Temperature: return ctx->supports_temperature;
  }
  return false;
}

static void style_control_button(lv_obj_t* button,
                                 lv_obj_t* icon,
                                 bool active,
                                 bool enabled,
                                 lv_color_t accent = lv_color_white()) {
  if (!button) return;
  lv_obj_set_style_bg_color(button,
                            lv_color_hex(active ? kControlButtonActiveBg : kControlButtonBg),
                            0);
  lv_obj_set_style_bg_opa(button, enabled ? LV_OPA_COVER : LV_OPA_30, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_border_opa(button, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  if (icon) {
    lv_obj_set_style_text_color(
        icon,
        enabled ? lv_color_white() : lv_color_hex(kControlButtonDisabled),
        0);
  }
}

static void apply_mode_visibility(LightPopupContext* ctx) {
  if (!ctx) return;
  const bool show_brightness = ctx->is_light && ctx->mode == LightPopupMode::Brightness && ctx->supports_brightness;
  const bool show_color = ctx->is_light && ctx->mode == LightPopupMode::Color && ctx->supports_color;
  const bool show_temperature = ctx->is_light && ctx->mode == LightPopupMode::Temperature && ctx->supports_temperature;

  if (ctx->brightness_panel) {
    if (show_brightness) lv_obj_clear_flag(ctx->brightness_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->brightness_panel, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->color_panel) {
    if (show_color) lv_obj_clear_flag(ctx->color_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->color_panel, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->temperature_panel) {
    if (show_temperature) lv_obj_clear_flag(ctx->temperature_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->temperature_panel, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->main_panel) {
    if (ctx->is_light) lv_obj_clear_flag(ctx->main_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->main_panel, LV_OBJ_FLAG_HIDDEN);
  }

  const bool show_modes = ctx->is_light;
  if (ctx->brightness_button) {
    if (show_modes) lv_obj_clear_flag(ctx->brightness_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->brightness_button, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->color_button) {
    if (show_modes) lv_obj_clear_flag(ctx->color_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->color_button, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->temperature_button) {
    if (show_modes) lv_obj_clear_flag(ctx->temperature_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->temperature_button, LV_OBJ_FLAG_HIDDEN);
  }

  style_control_button(ctx->brightness_button,
                       ctx->brightness_button_icon,
                       ctx->mode == LightPopupMode::Brightness,
                       ctx->is_light && ctx->supports_brightness);
  style_control_button(ctx->color_button,
                       ctx->color_button_icon,
                       ctx->mode == LightPopupMode::Color,
                       ctx->is_light && ctx->supports_color);
  style_control_button(ctx->temperature_button,
                       ctx->temperature_button_icon,
                       ctx->mode == LightPopupMode::Temperature,
                       ctx->is_light && ctx->supports_temperature);
}

static void set_mode(LightPopupContext* ctx, LightPopupMode mode) {
  if (!ctx || !is_mode_available(ctx, mode)) return;
  ctx->mode = mode;
  apply_mode_visibility(ctx);
  update_preview(ctx);
}

static void publish_light_popup(LightPopupContext* ctx) {
  if (!ctx || !ctx->entity_id.length()) return;
  if (!ctx->is_light) {
    mqttPublishSwitchCommand(ctx->entity_id.c_str(), ctx->is_on ? "on" : "off");
    return;
  }
  if (!ctx->is_on) {
    mqttPublishLightCommand(ctx->entity_id.c_str(), "off", -1, false, 0, -1);
    return;
  }

  const int brightness = ctx->supports_brightness ? ctx->val : -1;
  const bool has_color = ctx->supports_color && !ctx->use_color_temperature;
  const uint32_t rgb = has_color ? color_from_hsv(ctx->hue, ctx->sat, 100) : 0;
  const int color_temp_kelvin =
      (ctx->supports_temperature && ctx->use_color_temperature) ? ctx->color_temp_kelvin : -1;
  mqttPublishLightCommand(ctx->entity_id.c_str(), "on", brightness, has_color, rgb, color_temp_kelvin);
}

static void sync_bound_tile_from_popup(LightPopupContext* ctx) {
  if (!ctx || !ctx->has_tile_ref || ctx->tile_index >= TILES_PER_GRID) return;
  String payload;
  if (!ctx->is_light) {
    payload = ctx->is_on ? "on" : "off";
  } else {
    payload.reserve(120);
    payload = "{\"state\":\"";
    payload += ctx->is_on ? "on" : "off";
    payload += "\"";
    if (ctx->supports_brightness) {
      payload += ",\"brightness_pct\":";
      payload += String(ctx->is_on ? ctx->val : 0);
    }
    if (ctx->supports_temperature && ctx->use_color_temperature) {
      payload += ",\"color_temp_kelvin\":";
      payload += String(ctx->color_temp_kelvin);
    } else if (ctx->supports_color) {
      payload += ",\"hs_color\":[";
      payload += String(ctx->hue);
      payload += ",";
      payload += String(ctx->sat);
      payload += "]";
    }
    payload += "}";
  }
  update_switch_tile_state(static_cast<GridType>(ctx->tile_grid), ctx->tile_index, payload.c_str());
}

static void commit_popup_state(LightPopupContext* ctx) {
  if (!ctx) return;
  sync_bound_tile_from_popup(ctx);
  publish_light_popup(ctx);
}

static void update_preview(LightPopupContext* ctx) {
  if (!ctx) return;
  uint32_t icon_rgb = 0xB0B0B0;
  if (ctx->is_on) {
    if (ctx->supports_temperature && ctx->use_color_temperature) {
      icon_rgb = lv_color_to_u32(color_from_temperature_kelvin(ctx->color_temp_kelvin)) & 0xFFFFFF;
    } else if (ctx->supports_color) {
      icon_rgb = color_from_hsv(ctx->hue, ctx->sat, 100);
    } else {
      icon_rgb = kDefaultColor;
    }
  }

  if (ctx->icon_label) {
    lv_obj_set_style_text_color(
        ctx->icon_label,
        lv_color_hex(ctx->keep_icon_white ? 0xFFFFFF : icon_rgb),
        0);
  }
  if (ctx->power_button) {
    lv_obj_set_style_bg_color(ctx->power_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(ctx->power_button, ctx->is_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ctx->power_button, 0, 0);
    lv_obj_set_style_border_opa(ctx->power_button, LV_OPA_TRANSP, 0);
  }
  if (ctx->power_button_icon) {
    lv_obj_set_style_text_color(ctx->power_button_icon,
                                lv_color_hex(ctx->is_on ? kControlButtonBg : 0xFFFFFF),
                                0);
  }

  // Brightness slider indicator matches lamp color
  if (ctx->val_slider) {
    lv_color_t base_color = lv_color_hex(icon_rgb);
    lv_obj_set_style_bg_color(ctx->val_slider, base_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ctx->val_slider, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ctx->val_slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(ctx->val_slider, 0, LV_PART_MAIN);
    lv_obj_invalidate(ctx->val_slider);
  }

  update_value_label(ctx->val_value, ctx->val, "%");
  update_top_value_label(ctx);
  update_temperature_value_label(ctx);
  if (ctx->temp_slider && ctx->supports_temperature) {
    lv_obj_set_style_bg_color(
        ctx->temp_slider,
        color_from_temperature_kelvin(ctx->min_color_temp_kelvin),
        LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(
        ctx->temp_slider,
        color_from_temperature_kelvin(ctx->max_color_temp_kelvin),
        LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ctx->temp_slider, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_invalidate(ctx->temp_slider);
  }
  update_color_field_cursor(ctx);
}

static void align_header_row(lv_obj_t* card, lv_obj_t* title_label, lv_obj_t* icon_label) {
  if (!card) return;
  lv_obj_update_layout(card);
  lv_coord_t header_center_y = 60 - lv_obj_get_style_pad_top(card, LV_PART_MAIN);
  if (header_center_y < 0) header_center_y = 0;
  if (icon_label) {
    lv_coord_t icon_y = header_center_y - (lv_obj_get_height(icon_label) / 2);
    if (icon_y < 0) icon_y = 0;
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT, 8 + kHeaderIconOffsetX, icon_y + kHeaderIconOffsetY);
  }
  if (title_label) {
    lv_coord_t title_y = header_center_y - (lv_obj_get_height(title_label) / 2);
    if (title_y < 0) title_y = 0;
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 78, title_y);
  }
}

static lv_obj_t* create_color_field_panel(lv_obj_t* parent,
                                          lv_obj_t** frame_out,
                                          lv_obj_t** canvas_out,
                                          lv_obj_t** cursor_out) {
  lv_obj_t* wrap = lv_obj_create(parent);
  lv_obj_set_size(wrap, LV_PCT(100), kColorFieldWrapHeight);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wrap, 0, 0);
  lv_obj_set_style_pad_all(wrap, 0, 0);
  lv_obj_set_layout(wrap, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(wrap, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(wrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* frame = lv_obj_create(wrap);
  lv_obj_set_size(frame, kColorFieldWidth, kColorFieldHeight);
  lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(frame, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(frame, 0, 0);
  lv_obj_set_style_pad_all(frame, 0, 0);
  lv_obj_set_style_clip_corner(frame, false, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(frame, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  lv_obj_t* canvas = lv_canvas_create(frame);
  lv_obj_set_size(canvas, kColorFieldWidth, kColorFieldHeight);
  lv_obj_center(canvas);
  lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* cursor = lv_obj_create(wrap);
  lv_obj_set_size(cursor, kColorFieldCursorSize, kColorFieldCursorSize);
  lv_obj_set_style_radius(cursor, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(cursor, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(cursor, lv_color_white(), 0);
  lv_obj_set_style_border_width(cursor, 3, 0);
  lv_obj_set_style_border_color(cursor, lv_color_white(), 0);
  lv_obj_set_style_outline_width(cursor, 1, 0);
  lv_obj_set_style_outline_color(cursor, lv_color_hex(0x111111), 0);
  lv_obj_set_style_shadow_width(cursor, 12, 0);
  lv_obj_set_style_shadow_opa(cursor, LV_OPA_30, 0);
  lv_obj_set_style_shadow_color(cursor, lv_color_black(), 0);
  lv_obj_add_flag(cursor, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_clear_flag(cursor, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_foreground(cursor);

  if (frame_out) *frame_out = frame;
  if (canvas_out) *canvas_out = canvas;
  if (cursor_out) *cursor_out = cursor;
  return wrap;
}

static lv_obj_t* create_vertical_slider_panel(lv_obj_t* parent,
                                              lv_obj_t** slider_out,
                                              lv_obj_t** value_out,
                                              bool warm_style) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(panel, kVerticalValueLabelPadTop, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t* slider = lv_slider_create(panel);
  lv_obj_set_size(slider, kVerticalSliderWidth, kVerticalSliderHeight);
  lv_slider_set_range(slider, warm_style ? 2200 : 0, warm_style ? 6500 : 100);
  lv_obj_set_style_radius(slider, kVerticalSliderRadius, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, kVerticalSliderRadius, LV_PART_INDICATOR);

  if (warm_style) {
    // Temperature: gradient warm oben zu kalt unten
    lv_obj_set_style_bg_color(slider, lv_color_hex(kTempWarmColor), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(slider, lv_color_hex(kTempCoolColor), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(slider, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
  } else {
    // Brightness: track is tinted, fill is custom-drawn to allow a visible 1% state.
    lv_obj_set_style_bg_color(slider, lv_color_hex(kDefaultColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, LV_PART_KNOB);
  }
  lv_obj_set_style_border_width(slider, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(slider, 0, LV_PART_INDICATOR);
  lv_obj_set_ext_click_area(slider, 24);

  lv_obj_t* value = lv_label_create(panel);
  lv_obj_set_style_text_font(value, &ui_font_24, 0);
  lv_obj_set_style_text_color(value, lv_color_white(), 0);
  lv_label_set_text(value, "");
  lv_obj_add_flag(value, LV_OBJ_FLAG_HIDDEN);

  if (slider_out) *slider_out = slider;
  if (value_out) *value_out = value;
  return panel;
}

static lv_obj_t* create_temperature_panel(lv_obj_t* parent,
                                          lv_obj_t** wrap_out,
                                          lv_obj_t** track_out,
                                          lv_obj_t** handle_out,
                                          lv_obj_t** value_out) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(panel, 0, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(panel, kVerticalValueLabelPadTop, 0);

  lv_obj_t* wrap = lv_obj_create(panel);
  lv_obj_set_size(wrap, kTempWrapWidth, kTempWrapHeight);
  lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(wrap, 0, 0);
  lv_obj_set_style_pad_all(wrap, 0, 0);
  lv_obj_add_flag(wrap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(wrap, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_ext_click_area(wrap, 24);

  lv_obj_t* track = lv_obj_create(wrap);
  lv_obj_set_size(track, kVerticalSliderWidth, kVerticalSliderHeight);
  lv_obj_center(track);
  lv_obj_set_style_radius(track, kVerticalSliderRadius, 0);
  lv_obj_set_style_bg_color(track, lv_color_hex(kTempWarmColor), 0);
  lv_obj_set_style_bg_grad_color(track, lv_color_hex(kTempCoolColor), 0);
  lv_obj_set_style_bg_grad_dir(track, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(track, 0, 0);
  lv_obj_set_style_shadow_width(track, 0, 0);
  lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(track, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t* handle = lv_obj_create(wrap);
  lv_obj_set_size(handle, kTempHandleWidth, kTempHandleHeight);
  lv_obj_set_style_bg_color(handle, lv_color_hex(0xF5F2EE), 0);
  lv_obj_set_style_bg_opa(handle, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(handle, kTempHandleRadius, 0);
  lv_obj_set_style_border_width(handle, 0, 0);
  lv_obj_set_style_outline_width(handle, 0, 0);
  lv_obj_set_style_shadow_width(handle, 18, 0);
  lv_obj_set_style_shadow_color(handle, lv_color_black(), 0);
  lv_obj_set_style_shadow_opa(handle, LV_OPA_20, 0);
  lv_obj_add_flag(handle, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_clear_flag(handle, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(handle, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t* dash = lv_obj_create(handle);
  lv_obj_set_size(dash, kTempHandleDashWidth, kTempHandleDashHeight);
  lv_obj_set_style_radius(dash, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dash, lv_color_hex(0x707684), 0);
  lv_obj_set_style_bg_opa(dash, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dash, 0, 0);
  lv_obj_set_style_shadow_width(dash, 0, 0);
  lv_obj_clear_flag(dash, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(dash, LV_SCROLLBAR_MODE_OFF);
  lv_obj_center(dash);

  lv_obj_t* value = lv_label_create(panel);
  lv_obj_set_style_text_font(value, &ui_font_24, 0);
  lv_obj_set_style_text_color(value, lv_color_white(), 0);
  lv_label_set_text(value, "");

  if (wrap_out) *wrap_out = wrap;
  if (track_out) *track_out = track;
  if (handle_out) *handle_out = handle;
  if (value_out) *value_out = value;
  return panel;
}

static lv_obj_t* create_control_icon_button(lv_obj_t* parent, const char* icon_name, lv_obj_t** icon_out) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, kControlButtonSize, kControlButtonSize);
  lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(btn, lv_color_hex(kControlButtonBg), 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* icon = lv_label_create(btn);
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_label_set_text(icon, getMdiChar(icon_name).c_str());
  lv_obj_center(icon);

  if (icon_out) *icon_out = icon;
  return btn;
}

static void apply_init_to_context(LightPopupContext* ctx, const LightPopupInit& init) {
  if (!ctx) return;
  ctx->suppress_events = true;
  update_popup_language(ctx);
  ctx->entity_id = init.entity_id;
  ctx->supports_color = init.supports_color;
  ctx->supports_brightness = init.supports_brightness || init.supports_color;
  ctx->supports_temperature = init.supports_temperature;
  ctx->is_light = init.is_light;
  ctx->keep_icon_white = init.keep_icon_white;
  ctx->has_tile_ref = init.has_tile_ref;
  ctx->tile_grid = init.tile_grid;
  ctx->tile_index = init.tile_index;
  if (init.has_state) ctx->is_on = init.is_on;

  if (ctx->supports_color) {
    uint16_t h = ctx->hue;
    uint8_t s = ctx->sat;
    if (get_init_hs(init, h, s)) {
      ctx->hue = normalize_hue(static_cast<int>(h));
      ctx->sat = s;
      ctx->use_color_temperature = false;
    }
  } else {
    ctx->hue = 0;
    ctx->sat = 0;
  }

  if (ctx->supports_temperature) {
    ctx->min_color_temp_kelvin = init.min_color_temp_kelvin;
    ctx->max_color_temp_kelvin = init.max_color_temp_kelvin;
    normalize_color_temp_range(ctx->min_color_temp_kelvin, ctx->max_color_temp_kelvin);
    if (init.has_color_temp) {
      ctx->color_temp_kelvin = clamp_color_temp_kelvin_to_range(
          init.color_temp_kelvin, ctx->min_color_temp_kelvin, ctx->max_color_temp_kelvin);
      ctx->use_color_temperature = !init.has_hs && !init.has_color;
    } else {
      ctx->color_temp_kelvin = clamp_color_temp_kelvin_to_range(
        ctx->color_temp_kelvin, ctx->min_color_temp_kelvin, ctx->max_color_temp_kelvin);
    }
  } else {
    ctx->color_temp_kelvin = 4000;
    ctx->min_color_temp_kelvin = 2200;
    ctx->max_color_temp_kelvin = 6500;
    ctx->use_color_temperature = false;
  }
  if (ctx->supports_temperature && !ctx->supports_color) {
    ctx->use_color_temperature = true;
  }

  if (ctx->supports_brightness) {
    if (init.has_brightness) {
      ctx->val = init.brightness_pct;
    } else if (init.has_state && !ctx->is_on) {
      ctx->val = 0;
    }
  } else {
    ctx->val = 0;
  }
  if (ctx->val > 0) ctx->last_brightness = ctx->val;
  if (!ctx->is_on && ctx->supports_brightness) ctx->val = 0;

  if (ctx->title_label) lv_label_set_text(ctx->title_label, init.title.c_str());
  if (ctx->icon_label) {
    String icon_char;
    if (init.icon_name.length() > 0) icon_char = getMdiChar(init.icon_name);
    else if (ctx->is_light) icon_char = getMdiChar("lightbulb");
    else icon_char = getMdiChar("toggle-switch-variant");
    lv_label_set_text(ctx->icon_label, icon_char.c_str());
  }
  align_header_row(ctx->card, ctx->title_label, ctx->icon_label);
  if (ctx->val_slider) lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
  if (ctx->temp_slider) {
    lv_slider_set_range(ctx->temp_slider, ctx->min_color_temp_kelvin, ctx->max_color_temp_kelvin);
    lv_slider_set_value(ctx->temp_slider,
                        slider_value_from_kelvin(ctx->color_temp_kelvin,
                                                 ctx->min_color_temp_kelvin,
                                                 ctx->max_color_temp_kelvin),
                        LV_ANIM_OFF);
  }

  if (!ctx->is_light) {
    ctx->mode = LightPopupMode::Brightness;
  } else if (!is_mode_available(ctx, ctx->mode)) {
    if (ctx->use_color_temperature && ctx->supports_temperature) ctx->mode = LightPopupMode::Temperature;
    else if (ctx->supports_color) ctx->mode = LightPopupMode::Color;
    else if (ctx->supports_brightness) ctx->mode = LightPopupMode::Brightness;
    else if (ctx->supports_temperature) ctx->mode = LightPopupMode::Temperature;
  } else if (ctx->use_color_temperature && ctx->mode == LightPopupMode::Color && ctx->supports_temperature) {
    ctx->mode = LightPopupMode::Temperature;
  }

  apply_mode_visibility(ctx);
  update_preview(ctx);
  ctx->suppress_events = false;
}

static void on_power_button_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events) return;
  mark_user_action(ctx);

  if (ctx->is_on) {
    if (ctx->supports_brightness && ctx->val > 0) {
      ctx->last_brightness = ctx->val;
      ctx->val = 0;
      if (ctx->val_slider) {
        ctx->suppress_events = true;
        lv_slider_set_value(ctx->val_slider, 0, LV_ANIM_OFF);
        ctx->suppress_events = false;
      }
    }
    ctx->is_on = false;
  } else {
    ctx->is_on = true;
    if (ctx->supports_brightness && ctx->val == 0) {
      ctx->val = ctx->last_brightness > 0 ? ctx->last_brightness : 100;
      if (ctx->val_slider) {
        ctx->suppress_events = true;
        lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
        ctx->suppress_events = false;
      }
    }
  }

  update_preview(ctx);
  commit_popup_state(ctx);
}

static void on_mode_brightness_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  set_mode(static_cast<LightPopupContext*>(lv_event_get_user_data(e)), LightPopupMode::Brightness);
}

static void on_mode_color_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  set_mode(static_cast<LightPopupContext*>(lv_event_get_user_data(e)), LightPopupMode::Color);
}

static void on_mode_temperature_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  set_mode(static_cast<LightPopupContext*>(lv_event_get_user_data(e)), LightPopupMode::Temperature);
}

static void on_val_changed(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->val_slider || !ctx->supports_brightness || ctx->suppress_events) return;
  int value = lv_slider_get_value(ctx->val_slider);
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  ctx->val = static_cast<uint8_t>(value);
  ctx->is_on = value > 0;
  if (value > 0) ctx->last_brightness = static_cast<uint8_t>(value);
  mark_user_action(ctx);
  update_preview(ctx);
}

static void on_temp_changed(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* obj = lv_event_get_target_obj(e);
  if (!ctx || !obj || !ctx->supports_temperature || ctx->suppress_events) return;
  ctx->color_temp_kelvin =
      kelvin_from_slider_value(lv_slider_get_value(obj),
                               ctx->min_color_temp_kelvin,
                               ctx->max_color_temp_kelvin);
  ctx->use_color_temperature = true;
  ctx->is_on = true;
  if (ctx->supports_brightness && ctx->val == 0) {
    ctx->val = ctx->last_brightness > 0 ? ctx->last_brightness : 100;
    if (ctx->val_slider) {
      ctx->suppress_events = true;
      lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
      ctx->suppress_events = false;
    }
  }
  mark_user_action(ctx);
  update_preview(ctx);
}

static void on_val_slider_draw(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN_END) return;

  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* obj = lv_event_get_target_obj(e);
  if (!ctx || !obj || !ctx->supports_brightness) return;

  lv_area_t slider_area;
  lv_obj_get_coords(obj, &slider_area);
  const int slider_h = lv_area_get_height(&slider_area);
  if (slider_h <= 1) return;

  const uint8_t logical_value = (ctx->is_on || ctx->val > 0) ? ctx->val : 0;
  const float ratio = get_brightness_visual_ratio(logical_value);
  if (ratio <= 0.0f) return;

  int fill_h = static_cast<int>(lroundf(ratio * slider_h));
  if (fill_h < 1) fill_h = 1;
  lv_area_t fill_area = slider_area;
  fill_area.y1 = slider_area.y2 - fill_h + 1;

  lv_draw_rect_dsc_t fill_dsc;
  lv_draw_rect_dsc_init(&fill_dsc);
  fill_dsc.bg_color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);
  fill_dsc.bg_opa = LV_OPA_COVER;
  fill_dsc.radius = kVerticalSliderRadius;

  lv_layer_t* layer = lv_event_get_layer(e);
  lv_draw_rect(layer, &fill_dsc, &fill_area);

  int dash_center_y = fill_area.y1 + kVerticalSliderRadius;
  const int min_dash_center_y = fill_area.y1 + (kBrightnessDashHeight / 2);
  const int max_dash_center_y = fill_area.y2 - (kBrightnessDashHeight / 2);
  if (dash_center_y < min_dash_center_y) dash_center_y = min_dash_center_y;
  if (dash_center_y > max_dash_center_y) dash_center_y = max_dash_center_y;

  lv_area_t dash_area;
  dash_area.x1 = slider_area.x1 + ((lv_area_get_width(&slider_area) - kBrightnessDashWidth) / 2);
  dash_area.x2 = dash_area.x1 + kBrightnessDashWidth - 1;
  dash_area.y1 = dash_center_y - (kBrightnessDashHeight / 2);
  dash_area.y2 = dash_area.y1 + kBrightnessDashHeight - 1;

  lv_draw_rect_dsc_t dash_dsc;
  lv_draw_rect_dsc_init(&dash_dsc);
  dash_dsc.bg_color = lv_color_hex(0x2A2A2A);
  dash_dsc.bg_opa = LV_OPA_COVER;
  dash_dsc.radius = LV_RADIUS_CIRCLE;
  lv_draw_rect(layer, &dash_dsc, &dash_area);
}

static void on_temp_slider_draw(lv_event_t* e) {
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
    const int32_t extra = (kTempHandleHeight / 2) + 24;
    lv_event_set_ext_draw_size(e, extra);
    return;
  }

  if (code != LV_EVENT_DRAW_MAIN_END) return;

  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  lv_obj_t* obj = lv_event_get_target_obj(e);
  if (!ctx || !obj || !ctx->supports_temperature) return;
  if (!ctx->use_color_temperature) return;

  lv_area_t slider_area;
  lv_obj_get_coords(obj, &slider_area);
  const int slider_h = lv_area_get_height(&slider_area);
  if (slider_h <= 1) return;

  float ratio = get_color_temp_ratio(ctx->color_temp_kelvin,
                                     ctx->min_color_temp_kelvin,
                                     ctx->max_color_temp_kelvin);
  if (ratio < 0.0f) ratio = 0.0f;
  if (ratio > 1.0f) ratio = 1.0f;

  const int half_handle = kTempHandleHeight / 2;
  const int min_center_y = slider_area.y1 + half_handle;
  const int max_center_y = slider_area.y2 - half_handle + 1;
  const int usable_range = max_center_y - min_center_y;
  int center_y = min_center_y;
  if (usable_range > 0) {
    center_y += static_cast<int>(lroundf(ratio * usable_range));
  }
  if (center_y < min_center_y) center_y = min_center_y;
  if (center_y > max_center_y) center_y = max_center_y;

  lv_area_t handle_area;
  handle_area.x1 = slider_area.x1;
  handle_area.x2 = slider_area.x2;
  handle_area.y1 = center_y - half_handle;
  handle_area.y2 = handle_area.y1 + kTempHandleHeight - 1;

  lv_draw_rect_dsc_t handle_dsc;
  lv_draw_rect_dsc_init(&handle_dsc);
  handle_dsc.bg_color = lv_color_hex(0xF5F2EE);
  handle_dsc.bg_opa = LV_OPA_COVER;
  handle_dsc.radius = kTempHandleRadius;
  handle_dsc.shadow_width = 18;
  handle_dsc.shadow_opa = LV_OPA_20;
  handle_dsc.shadow_color = lv_color_black();

  lv_layer_t* layer = lv_event_get_layer(e);
  lv_draw_rect(layer, &handle_dsc, &handle_area);

  lv_area_t dash_area;
  dash_area.x1 = slider_area.x1 + ((lv_area_get_width(&slider_area) - kTempHandleDashWidth) / 2);
  dash_area.x2 = dash_area.x1 + kTempHandleDashWidth - 1;
  dash_area.y1 = center_y - (kTempHandleDashHeight / 2);
  dash_area.y2 = dash_area.y1 + kTempHandleDashHeight - 1;

  lv_draw_rect_dsc_t dash_dsc;
  lv_draw_rect_dsc_init(&dash_dsc);
  dash_dsc.bg_color = lv_color_hex(0x707684);
  dash_dsc.bg_opa = LV_OPA_COVER;
  dash_dsc.radius = LV_RADIUS_CIRCLE;
  lv_draw_rect(layer, &dash_dsc, &dash_area);
}

static void apply_temperature_point(LightPopupContext* ctx, const lv_point_t& point, bool commit) {
  if (!ctx || !ctx->temp_slider || !ctx->supports_temperature) return;

  lv_area_t area;
  lv_obj_get_coords(ctx->temp_slider, &area);
  int y = point.y - area.y1;
  const int track_height = lv_area_get_height(&area);
  if (track_height <= 1) return;

  const int half_handle = kTempHandleHeight / 2;
  const int min_y = half_handle;
  const int max_y = track_height - half_handle - 1;
  if (y < min_y) y = min_y;
  if (y > max_y) y = max_y;

  float ratio = 0.0f;
  const int usable_range = max_y - min_y;
  if (usable_range > 0) {
    ratio = static_cast<float>(y - min_y) / static_cast<float>(usable_range);
  }
  ctx->color_temp_kelvin = clamp_color_temp_kelvin_to_range(
      static_cast<int>(lroundf(ctx->min_color_temp_kelvin +
                               (ratio * (ctx->max_color_temp_kelvin - ctx->min_color_temp_kelvin)))),
      ctx->min_color_temp_kelvin,
      ctx->max_color_temp_kelvin);
  ctx->use_color_temperature = true;
  ctx->is_on = true;
  if (ctx->supports_brightness && ctx->val == 0) {
    ctx->val = ctx->last_brightness > 0 ? ctx->last_brightness : 100;
    if (ctx->val_slider) {
      ctx->suppress_events = true;
      lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
      ctx->suppress_events = false;
    }
  }
  mark_user_action(ctx);
  update_preview(ctx);
  if (commit) commit_popup_state(ctx);
}

static void on_temp_track_event(lv_event_t* e) {
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events || !ctx->supports_temperature) return;
  const lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_PRESSED) {
    ctx->user_dragging = true;
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    ctx->user_dragging = false;
  } else if (code != LV_EVENT_PRESSING) {
    return;
  }

  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;

  lv_point_t point;
  lv_indev_get_point(indev, &point);
  apply_temperature_point(ctx, point, code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST);
}

static void apply_color_field_point(LightPopupContext* ctx,
                                    lv_obj_t* frame,
                                    const lv_point_t& point,
                                    bool commit) {
  if (!ctx || !frame || !ctx->supports_color) return;
  lv_area_t area;
  lv_obj_get_coords(frame, &area);

  int x = point.x - area.x1;
  int y = point.y - area.y1;
  if (x < 0) x = 0;
  if (x >= kColorFieldWidth) x = kColorFieldWidth - 1;
  if (y < 0) y = 0;
  if (y >= kColorFieldHeight) y = kColorFieldHeight - 1;

  const float cx = (kColorFieldWidth - 1) * 0.5f;
  const float cy = (kColorFieldHeight - 1) * 0.5f;
  const float radius = (kColorFieldWidth - 2) * 0.5f;
  float dx = x - cx;
  float dy = cy - y;
  float dist = sqrtf((dx * dx) + (dy * dy));
  if (dist > radius && dist > 0.0f) {
    const float scale = radius / dist;
    dx *= scale;
    dy *= scale;
    dist = radius;
  }

  float angle = atan2f(dy, dx) * 180.0f / kPi;
  if (angle < 0.0f) angle += 360.0f;
  ctx->hue = normalize_hue(static_cast<int>(lroundf(angle)));
  ctx->sat = static_cast<uint8_t>(lroundf((dist / radius) * 100.0f));
  ctx->use_color_temperature = false;

  if (ctx->supports_brightness && !ctx->is_on) {
    ctx->is_on = true;
    if (ctx->val == 0) {
      ctx->val = ctx->last_brightness > 0 ? ctx->last_brightness : 100;
      if (ctx->val_slider) {
        ctx->suppress_events = true;
        lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
        ctx->suppress_events = false;
      }
    }
  }

  mark_user_action(ctx);
  update_preview(ctx);
  if (commit) commit_popup_state(ctx);
}

static void on_color_field_event(lv_event_t* e) {
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events || !ctx->color_field_ready) return;
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) ctx->user_dragging = true;
  else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) ctx->user_dragging = false;
  else if (code != LV_EVENT_PRESSING) return;

  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t point;
  lv_indev_get_point(indev, &point);
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(e));
  apply_color_field_point(ctx, target, point, code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST);
}

static void on_slider_pressed(lv_event_t* e) {
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events) return;
  lv_obj_t* target = lv_event_get_target_obj(e);
  if (target == ctx->temp_slider && ctx->supports_temperature && !ctx->use_color_temperature) {
    lv_indev_t* indev = lv_indev_get_act();
    if (indev) {
      lv_point_t point;
      lv_indev_get_point(indev, &point);
      apply_temperature_point(ctx, point, false);
    } else {
      ctx->use_color_temperature = true;
      update_preview(ctx);
    }
  }
  ctx->user_dragging = true;
  mark_user_action(ctx);
}

static void on_slider_released(lv_event_t* e) {
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events) return;
  ctx->user_dragging = false;
  mark_user_action(ctx);
  commit_popup_state(ctx);
}

static void on_overlay_delete(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx) return;
  if (ctx->color_field_buf) {
    heap_caps_free(ctx->color_field_buf);
    ctx->color_field_buf = nullptr;
  }
  if (g_light_popup_ctx == ctx) g_light_popup_ctx = nullptr;
  delete ctx;
}

}  // namespace

void show_light_popup(const LightPopupInit& init) {
  if (!init.entity_id.length()) return;
  hide_sensor_popup();
  hide_weather_popup();

  if (g_light_popup_ctx && g_light_popup_ctx->overlay && g_light_popup_ctx->card) {
    apply_init_to_context(g_light_popup_ctx, init);
    lv_obj_clear_flag(g_light_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_light_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
    return;
  }

  LightPopupContext* ctx = new LightPopupContext();
  g_light_popup_ctx = ctx;

  lv_obj_t* overlay = lv_obj_create(lv_layer_top());
  ctx->overlay = overlay;
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* card = lv_obj_create(overlay);
  ctx->card = card;
  lv_obj_set_size(card, kCardWidth, kCardHeight);
  lv_obj_center(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x2A2A2A), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, kCardPad, 0);
  lv_obj_set_style_shadow_width(card, 28, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
  lv_obj_set_style_shadow_spread(card, 2, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(card);
  ctx->title_label = title;
  set_label_style(title, lv_color_white());
  lv_obj_set_style_text_font(title, &ui_font_20, 0);
  lv_obj_set_width(title, LV_PCT(62));
  lv_label_set_text(title, init.title.c_str());

  lv_obj_t* icon = lv_label_create(card);
  ctx->icon_label = icon;
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  if (init.icon_name.length() > 0) {
    lv_label_set_text(icon, getMdiChar(init.icon_name).c_str());
  }

  lv_obj_t* close_btn = lv_button_create(card);
  lv_obj_set_size(close_btn, 96, 96);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close_btn, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close_btn, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close_btn, 16, 0);
  lv_obj_set_style_pad_all(close_btn, 0, 0);
  lv_obj_align(close_btn, LV_ALIGN_TOP_RIGHT, 12, -12);
  lv_obj_set_ext_click_area(close_btn, 28);
  lv_obj_add_flag(close_btn, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(close_btn, on_close_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(close_btn, on_close_click, LV_EVENT_RELEASED, ctx);

  lv_obj_t* close_label = lv_label_create(close_btn);
  lv_obj_set_style_text_font(close_label, FONT_MDI_ICONS, 0);
  lv_obj_set_style_text_color(close_label, lv_color_white(), 0);
  lv_label_set_text(close_label, getMdiChar("window-close").c_str());
  lv_obj_center(close_label);

  lv_obj_t* content = lv_obj_create(card);
  lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
  lv_obj_align(content, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_left(content, 0, 0);
  lv_obj_set_style_pad_right(content, 0, 0);
  lv_obj_set_style_pad_top(content, kContentPadTop, 0);
  lv_obj_set_style_pad_bottom(content, 24, 0);
  lv_obj_set_style_pad_row(content, 0, 0);
  lv_obj_set_layout(content, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  ctx->top_value_label = lv_label_create(content);
  lv_obj_set_width(ctx->top_value_label, LV_PCT(100));
  lv_obj_set_height(ctx->top_value_label, kTopValueHeight);
  lv_obj_set_style_text_align(ctx->top_value_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ctx->top_value_label, &ui_font_48, 0);
  lv_obj_set_style_text_color(ctx->top_value_label, lv_color_white(), 0);
  lv_obj_set_style_pad_bottom(ctx->top_value_label, kTopValueBottomPad, 0);
  lv_label_set_text(ctx->top_value_label, "");

  ctx->main_panel = lv_obj_create(content);
  lv_obj_set_size(ctx->main_panel, LV_PCT(100), kMainPanelHeight);
  lv_obj_set_style_bg_opa(ctx->main_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->main_panel, 0, 0);
  lv_obj_set_style_pad_all(ctx->main_panel, 0, 0);
  lv_obj_clear_flag(ctx->main_panel, LV_OBJ_FLAG_SCROLLABLE);

  ctx->brightness_panel = create_vertical_slider_panel(ctx->main_panel, &ctx->val_slider, &ctx->val_value, false);
  ctx->color_panel = create_color_field_panel(ctx->main_panel,
                                              &ctx->color_field_frame,
                                              &ctx->color_field_canvas,
                                              &ctx->color_field_cursor);
  ctx->temperature_panel = create_vertical_slider_panel(ctx->main_panel,
                                                        &ctx->temp_slider,
                                                        &ctx->temp_value_label,
                                                        true);
  render_color_field(ctx);

  ctx->controls_row = lv_obj_create(content);
  lv_obj_set_height(ctx->controls_row, kControlsRowHeight);
  lv_obj_set_width(ctx->controls_row, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(ctx->controls_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(ctx->controls_row, 0, 0);
  lv_obj_set_style_border_width(ctx->controls_row, 0, 0);
  lv_obj_set_style_pad_left(ctx->controls_row, kControlsRowPadX, 0);
  lv_obj_set_style_pad_right(ctx->controls_row, kControlsRowPadX, 0);
  lv_obj_set_style_pad_top(ctx->controls_row, 6, 0);
  lv_obj_set_style_pad_bottom(ctx->controls_row, 6, 0);
  lv_obj_set_style_pad_column(ctx->controls_row, kControlsRowGap, 0);
  lv_obj_set_style_margin_top(ctx->controls_row, kControlsRowTopPad, 0);
  lv_obj_set_layout(ctx->controls_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ctx->controls_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ctx->controls_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(ctx->controls_row, LV_OBJ_FLAG_SCROLLABLE);

  ctx->power_button = create_control_icon_button(ctx->controls_row, "power", &ctx->power_button_icon);
  ctx->brightness_button = create_control_icon_button(ctx->controls_row, "brightness-6", &ctx->brightness_button_icon);
  ctx->color_button = create_control_icon_button(ctx->controls_row, "palette", &ctx->color_button_icon);
  ctx->temperature_button =
      create_control_icon_button(ctx->controls_row, "temperature-kelvin", &ctx->temperature_button_icon);

  lv_obj_move_foreground(icon);
  lv_obj_move_foreground(title);
  lv_obj_move_foreground(close_btn);

  apply_init_to_context(ctx, init);

  lv_obj_add_event_cb(ctx->power_button, on_power_button_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->brightness_button, on_mode_brightness_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->color_button, on_mode_color_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->temperature_button, on_mode_temperature_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_val_changed, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_val_slider_draw, LV_EVENT_DRAW_MAIN_END, ctx);
  lv_obj_add_event_cb(ctx->temp_slider, on_temp_changed, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->temp_slider, on_temp_slider_draw, LV_EVENT_REFR_EXT_DRAW_SIZE, ctx);
  lv_obj_add_event_cb(ctx->temp_slider, on_temp_slider_draw, LV_EVENT_DRAW_MAIN_END, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_slider_pressed, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(ctx->temp_slider, on_slider_pressed, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_slider_released, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(ctx->temp_slider, on_slider_released, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_slider_released, LV_EVENT_PRESS_LOST, ctx);
  lv_obj_add_event_cb(ctx->temp_slider, on_slider_released, LV_EVENT_PRESS_LOST, ctx);
  lv_obj_refresh_ext_draw_size(ctx->temp_slider);
  if (ctx->color_field_frame) {
    lv_obj_add_event_cb(ctx->color_field_frame, on_color_field_event, LV_EVENT_PRESSED, ctx);
    lv_obj_add_event_cb(ctx->color_field_frame, on_color_field_event, LV_EVENT_PRESSING, ctx);
    lv_obj_add_event_cb(ctx->color_field_frame, on_color_field_event, LV_EVENT_RELEASED, ctx);
    lv_obj_add_event_cb(ctx->color_field_frame, on_color_field_event, LV_EVENT_PRESS_LOST, ctx);
  }
  lv_obj_add_event_cb(overlay, on_overlay_click, LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(overlay, on_overlay_delete, LV_EVENT_DELETE, ctx);
}

void update_light_popup(const LightPopupInit& init) {
  if (!g_light_popup_ctx || !g_light_popup_ctx->overlay || !g_light_popup_ctx->card) return;
  if (!g_light_popup_ctx->entity_id.length()) return;
  if (!g_light_popup_ctx->entity_id.equalsIgnoreCase(init.entity_id)) return;
  if (lv_obj_has_flag(g_light_popup_ctx->card, LV_OBJ_FLAG_HIDDEN)) return;
  if (should_block_remote_update(g_light_popup_ctx)) return;
  if (g_light_popup_ctx->block_remote_until_ms != 0 &&
      millis() < g_light_popup_ctx->block_remote_until_ms) {
    return;
  }
  if ((millis() - g_light_popup_ctx->last_user_action_ms) < 4000 &&
      !is_remote_update_close(g_light_popup_ctx, init)) {
    return;
  }
  apply_init_to_context(g_light_popup_ctx, init);
}

void preload_light_popup() {
  if (g_light_popup_ctx && g_light_popup_ctx->overlay && g_light_popup_ctx->card) return;
  LightPopupInit init;
  init.entity_id = "__preload__";
  init.title = "";
  init.icon_name = "lightbulb";
  init.is_light = true;
  init.supports_color = true;
  init.supports_brightness = true;
  init.supports_temperature = true;
  init.has_state = true;
  init.is_on = false;
  show_light_popup(init);
  if (g_light_popup_ctx && g_light_popup_ctx->card && g_light_popup_ctx->overlay) {
    lv_obj_add_flag(g_light_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_light_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  }
}

void hide_light_popup() {
  if (!g_light_popup_ctx || !g_light_popup_ctx->card || !g_light_popup_ctx->overlay) return;
  lv_obj_add_flag(g_light_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_light_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

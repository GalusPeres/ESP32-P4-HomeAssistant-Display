#include "src/ui/light_popup.h"
#include "src/fonts/ui_fonts.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"

namespace {

constexpr int kCardWidth = 760;
constexpr int kCardHeight = 420;
constexpr int kCardPad = 20;
constexpr int kHeaderPadTop = 4;
constexpr int kHeaderIconOffsetX = 4;
constexpr int kHeaderIconOffsetY = -8;
constexpr int kContentPadTop = 85;
constexpr int kPowerStatusGap = 20;
constexpr int kPowerStatusMarginBottom = 35;

constexpr int kLabelWidth = 140;
constexpr int kSliderWidth = 420;
constexpr int kSliderHeight = 18;
constexpr int kSliderKnobSize = 40;
constexpr int kSliderClickPad = 22;
constexpr int kRowHeight = 40;
constexpr int kRowPadX = 28;
constexpr int kRowPadY = 20;
constexpr int kSwitchWidth = 120;
constexpr int kSwitchHeight = 50;

constexpr int kPreviewWidth = 140;
constexpr int kPreviewHeight = 64;

constexpr uint32_t kDefaultColor = 0xFFD54F;
constexpr uint32_t kRemoteBlockMs = 3000;

struct LightPopupContext {
  String entity_id;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* hue_slider = nullptr;
  lv_obj_t* sat_slider = nullptr;
  lv_obj_t* val_slider = nullptr;
  lv_obj_t* hue_value = nullptr;
  lv_obj_t* sat_value = nullptr;
  lv_obj_t* val_value = nullptr;
  lv_obj_t* hue_row = nullptr;
  lv_obj_t* sat_row = nullptr;
  lv_obj_t* val_row = nullptr;
  lv_obj_t* power_row = nullptr;
  lv_obj_t* power_switch = nullptr;
  lv_obj_t* power_status_label = nullptr;
  uint16_t hue = 0;
  uint8_t sat = 0;
  uint8_t val = 100;
  uint8_t last_brightness = 100;  // Remember last brightness before turning off
  bool supports_color = false;
  bool supports_brightness = false;
  bool is_light = true;
  bool is_on = true;
  bool user_dragging = false;
  uint32_t last_user_action_ms = 0;
  uint32_t block_remote_until_ms = 0;
  bool suppress_events = false;
};

static LightPopupContext* g_light_popup_ctx = nullptr;

static void on_overlay_click(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->overlay || !ctx->card) return;
  lv_obj_add_flag(ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
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

static const lv_font_t* get_value_font() {
#if defined(LV_FONT_MONTSERRAT_40) && LV_FONT_MONTSERRAT_40
  return &lv_font_montserrat_40;
#elif defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
  return &lv_font_montserrat_48;
#else
  return LV_FONT_DEFAULT;
#endif
}

static void update_value_label(lv_obj_t* label, int value, const char* suffix) {
  if (!label) return;
  char buf[16];
  if (suffix && suffix[0]) {
    snprintf(buf, sizeof(buf), "%d%s", value, suffix);
  } else {
    snprintf(buf, sizeof(buf), "%d", value);
  }
  lv_label_set_text(label, buf);
}

static uint32_t color_from_hsv(uint16_t h, uint8_t s, uint8_t v) {
  lv_color_t color = lv_color_hsv_to_rgb(h, s, v);
  return lv_color_to_u32(color) & 0xFFFFFF;
}

static void publish_light_popup(LightPopupContext* ctx) {
  if (!ctx || !ctx->entity_id.length()) return;
  if (!ctx->is_light) {
    mqttPublishSwitchCommand(ctx->entity_id.c_str(), ctx->is_on ? "on" : "off");
    return;
  }

  if (!ctx->is_on) {
    mqttPublishLightCommand(ctx->entity_id.c_str(), "off", -1, false, 0);
    return;
  }

  int brightness = ctx->supports_brightness ? ctx->val : -1;
  uint32_t rgb = 0;
  bool has_color = ctx->supports_color;
  if (has_color) {
    rgb = color_from_hsv(ctx->hue, ctx->sat, 100);
  }
  mqttPublishLightCommand(ctx->entity_id.c_str(), "on", brightness, has_color, rgb);
}

static void update_preview(LightPopupContext* ctx) {
  if (!ctx) return;

  // Calculate color for icon and switch
  uint32_t rgb = 0;
  if (!ctx->is_on) {
    rgb = 0xB0B0B0;  // Tile gray when off
  } else {
    // Light is on - mirror tile icon behavior
    if (ctx->supports_color) {
      rgb = color_from_hsv(ctx->hue, ctx->sat, 100);
    } else {
      rgb = kDefaultColor;
    }
  }

  // Update icon color
  if (ctx->icon_label) {
    lv_obj_set_style_text_color(ctx->icon_label, lv_color_hex(rgb), 0);
  }

  // Update switch indicator color (same as icon)
  if (ctx->power_switch) {
    lv_obj_set_style_bg_color(ctx->power_switch, lv_color_hex(rgb), LV_PART_INDICATOR | LV_STATE_CHECKED);
  }

  update_value_label(ctx->hue_value, ctx->hue, "");
  update_value_label(ctx->sat_value, ctx->sat, "%");
  update_value_label(ctx->val_value, ctx->val, "%");
}

static void apply_init_to_context(LightPopupContext* ctx, const LightPopupInit& init) {
  if (!ctx) return;
  ctx->suppress_events = true;
  ctx->entity_id = init.entity_id;
  ctx->supports_color = init.supports_color;
  ctx->supports_brightness = init.supports_brightness || init.supports_color;
  ctx->is_light = init.is_light;
  if (init.has_state) {
    ctx->is_on = init.is_on;
  }

  bool update_color = ctx->supports_color && (init.has_hs || init.has_color);
  if (update_color) {
    if (init.has_hs) {
      ctx->hue = static_cast<uint16_t>(roundf(init.hs_h));
      ctx->sat = static_cast<uint8_t>(roundf(init.hs_s));
    } else {
      uint32_t base_color = init.color ? init.color : kDefaultColor;
      lv_color_hsv_t hsv = lv_color_to_hsv(lv_color_hex(base_color));
      ctx->hue = hsv.h;
      ctx->sat = hsv.s;
    }
  } else if (!ctx->supports_color) {
    ctx->hue = 0;
    ctx->sat = 0;
  }

  bool update_val = false;
  if (ctx->supports_brightness) {
    if (init.has_brightness) {
      ctx->val = init.brightness_pct;
      update_val = true;
    } else if (init.has_state && !ctx->is_on) {
      ctx->val = 0;
      update_val = true;
    }
  } else {
    ctx->val = 0;
    update_val = true;
  }

  if (ctx->val > 0) {
    ctx->last_brightness = ctx->val;
  }

  if (ctx->title_label) {
    lv_label_set_text(ctx->title_label, init.title.c_str());
  }
  if (ctx->icon_label) {
    String icon_char;
    if (init.icon_name.length() > 0) {
      icon_char = getMdiChar(init.icon_name);
    } else if (ctx->is_light) {
      icon_char = getMdiChar("lightbulb");
    } else {
      icon_char = getMdiChar("toggle-switch-variant");
    }
    lv_label_set_text(ctx->icon_label, icon_char.c_str());
  }
  if (ctx->hue_slider && update_color) {
    lv_slider_set_value(ctx->hue_slider, ctx->hue, LV_ANIM_OFF);
  }
  if (ctx->sat_slider && update_color) {
    lv_slider_set_value(ctx->sat_slider, ctx->sat, LV_ANIM_OFF);
  }
  if (ctx->val_slider && update_val) {
    lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
  }

  bool show_color = ctx->supports_color;
  bool show_val = ctx->supports_brightness;
  if (ctx->power_switch) {
    if (ctx->is_on) {
      lv_obj_add_state(ctx->power_switch, LV_STATE_CHECKED);
    } else {
      lv_obj_remove_state(ctx->power_switch, LV_STATE_CHECKED);
    }
  }
  if (ctx->hue_row) {
    if (show_color) lv_obj_clear_flag(ctx->hue_row, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->hue_row, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->sat_row) {
    if (show_color) lv_obj_clear_flag(ctx->sat_row, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->sat_row, LV_OBJ_FLAG_HIDDEN);
  }
  if (ctx->val_row) {
    if (show_val) lv_obj_clear_flag(ctx->val_row, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ctx->val_row, LV_OBJ_FLAG_HIDDEN);
  }
  update_preview(ctx);
  ctx->suppress_events = false;
}

static lv_obj_t* create_centered_power_status(lv_obj_t* parent,
                                               lv_obj_t** switch_out,
                                               lv_obj_t** label_out) {
  // Container für Switch (zentriert)
  lv_obj_t* container = lv_obj_create(parent);
  lv_obj_set_width(container, LV_PCT(100));
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container, 0, 0);
  lv_obj_set_style_pad_all(container, 0, 0);
  lv_obj_set_style_pad_bottom(container, kPowerStatusMarginBottom, 0);
  lv_obj_set_height(container, LV_SIZE_CONTENT);
  lv_obj_set_layout(container, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Switch mit grauem Knob (Farbe wird in update_preview gesetzt)
  lv_obj_t* sw = lv_switch_create(container);
  lv_obj_set_size(sw, kSwitchWidth, kSwitchHeight);
  // Knob in hellgrau für bessere Sichtbarkeit
  lv_obj_set_style_bg_color(sw, lv_color_hex(0x2A2A2A), LV_PART_KNOB);
  *switch_out = sw;

  // Kein Label mehr
  *label_out = nullptr;

  return container;
}

static lv_obj_t* create_slider_row(lv_obj_t* parent,
                                   const char* label_text,
                                   int min_value,
                                   int max_value,
                                   lv_obj_t** slider_out,
                                   lv_obj_t** value_out) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_height(row, kRowHeight);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, kRowPadX, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* label = lv_label_create(row);
  set_label_style(label, lv_color_white());
  lv_label_set_text(label, label_text);
  lv_obj_set_width(label, kLabelWidth);

  lv_obj_t* slider = lv_slider_create(row);
  lv_obj_set_width(slider, kSliderWidth);
  lv_obj_set_height(slider, kSliderHeight);
  lv_slider_set_range(slider, min_value, max_value);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x1E88E5), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0x5A5A5A), LV_PART_MAIN);
  lv_obj_set_style_radius(slider, 12, LV_PART_MAIN);
  lv_obj_set_style_radius(slider, 12, LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_width(slider, kSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_style_height(slider, kSliderKnobSize, LV_PART_KNOB);
  lv_obj_set_ext_click_area(slider, kSliderClickPad);

  lv_obj_t* value = lv_label_create(row);
  set_label_style(value, lv_color_white());

  if (slider_out) *slider_out = slider;
  if (value_out) *value_out = value;
  return row;
}

static lv_obj_t* create_switch_row(lv_obj_t* parent,
                                   const char* label_text,
                                   lv_obj_t** switch_out) {
  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_height(row, kRowHeight);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, kRowPadX, 0);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t* label = lv_label_create(row);
  set_label_style(label, lv_color_white());
  lv_label_set_text(label, label_text);
  lv_obj_set_width(label, kLabelWidth);

  lv_obj_t* sw = lv_switch_create(row);
  lv_obj_set_size(sw, kSwitchWidth, kSwitchHeight);
  lv_obj_set_style_bg_color(sw, lv_color_hex(0xB0B0B0), LV_PART_INDICATOR | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFD54F), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_ext_click_area(sw, 18);

  if (switch_out) *switch_out = sw;
  return row;
}

static void on_power_switch_changed(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->power_switch || ctx->suppress_events) return;
  bool new_state = lv_obj_has_state(ctx->power_switch, LV_STATE_CHECKED);
  mark_user_action(ctx);

  if (ctx->supports_brightness) {
    if (new_state) {
      // Turning ON: Restore last brightness (or 100% if it was 0)
      if (ctx->val == 0) {
        ctx->val = ctx->last_brightness > 0 ? ctx->last_brightness : 100;
        if (ctx->val_slider) {
          lv_slider_set_value(ctx->val_slider, ctx->val, LV_ANIM_OFF);
        }
      }
    } else {
      // Turning OFF: Save current brightness and set to 0
      if (ctx->val > 0) {
        ctx->last_brightness = ctx->val;
      }
      ctx->val = 0;
      if (ctx->val_slider) {
        lv_slider_set_value(ctx->val_slider, 0, LV_ANIM_OFF);
      }
    }
  }

  ctx->is_on = new_state;
  update_preview(ctx);
  publish_light_popup(ctx);
}

static void on_hue_changed(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->hue_slider || !ctx->supports_color || ctx->suppress_events) return;
  int value = lv_slider_get_value(ctx->hue_slider);
  if (value < 0) value = 0;
  if (value > 360) value = 360;
  ctx->hue = static_cast<uint16_t>(value);
  mark_user_action(ctx);
  update_preview(ctx);
  publish_light_popup(ctx);
}

static void on_sat_changed(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->sat_slider || !ctx->supports_color || ctx->suppress_events) return;
  int value = lv_slider_get_value(ctx->sat_slider);
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  ctx->sat = static_cast<uint8_t>(value);
  mark_user_action(ctx);
  update_preview(ctx);
  publish_light_popup(ctx);
}

static void on_val_changed(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || !ctx->val_slider || !ctx->supports_brightness || ctx->suppress_events) return;
  int value = lv_slider_get_value(ctx->val_slider);
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  ctx->val = static_cast<uint8_t>(value);
  mark_user_action(ctx);

  // Sync switch with brightness
  if (ctx->power_switch) {
    if (value == 0 && ctx->is_on) {
      // Brightness to 0 → turn off switch
      ctx->is_on = false;
      lv_obj_remove_state(ctx->power_switch, LV_STATE_CHECKED);
    } else if (value > 0 && !ctx->is_on) {
      // Brightness > 0 → turn on switch
      ctx->is_on = true;
      lv_obj_add_state(ctx->power_switch, LV_STATE_CHECKED);
      ctx->last_brightness = value;
    } else if (value > 0) {
      // Update last brightness while on
      ctx->last_brightness = value;
    }
  }

  update_preview(ctx);
  publish_light_popup(ctx);
}

static void on_slider_pressed(lv_event_t* e) {
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events) return;
  ctx->user_dragging = true;
  mark_user_action(ctx);
}

static void on_slider_released(lv_event_t* e) {
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx || ctx->suppress_events) return;
  ctx->user_dragging = false;
  mark_user_action(ctx);
}

static void on_overlay_delete(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  LightPopupContext* ctx = static_cast<LightPopupContext*>(lv_event_get_user_data(e));
  if (!ctx) return;
  if (g_light_popup_ctx == ctx) {
    g_light_popup_ctx = nullptr;
  }
  delete ctx;
}

}  // namespace

void show_light_popup(const LightPopupInit& init) {
  if (!init.entity_id.length()) return;
  if (g_light_popup_ctx && g_light_popup_ctx->overlay && g_light_popup_ctx->card) {
    apply_init_to_context(g_light_popup_ctx, init);
    lv_obj_clear_flag(g_light_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_light_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(g_light_popup_ctx->overlay);
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

  // Title (left)
  lv_obj_t* title = lv_label_create(card);
  ctx->title_label = title;
  set_label_style(title, lv_color_white());
  lv_label_set_text(title, init.title.c_str());
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, kHeaderPadTop);

  // Icon (right) - colored based on light state
  lv_obj_t* icon = lv_label_create(card);
  ctx->icon_label = icon;
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  lv_obj_align(icon, LV_ALIGN_TOP_RIGHT, kHeaderIconOffsetX, kHeaderIconOffsetY);
  if (init.icon_name.length() > 0) {
    String icon_char = getMdiChar(init.icon_name);
    lv_label_set_text(icon, icon_char.c_str());
  }

  lv_obj_t* content = lv_obj_create(card);
  lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
  lv_obj_align(content, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 0, 0);
  lv_obj_set_style_pad_top(content, kContentPadTop, 0);
  lv_obj_set_layout(content, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(content, kRowPadY, 0);

  ctx->power_row = create_centered_power_status(content, &ctx->power_switch, &ctx->power_status_label);
  ctx->hue_row = create_slider_row(content, "Farbton", 0, 360, &ctx->hue_slider, &ctx->hue_value);
  ctx->sat_row = create_slider_row(content, "Saettigung", 0, 100, &ctx->sat_slider, &ctx->sat_value);
  ctx->val_row = create_slider_row(content, "Helligkeit", 0, 100, &ctx->val_slider, &ctx->val_value);

  apply_init_to_context(ctx, init);

  lv_obj_add_event_cb(ctx->power_switch, on_power_switch_changed, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->hue_slider, on_hue_changed, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->sat_slider, on_sat_changed, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_val_changed, LV_EVENT_VALUE_CHANGED, ctx);
  lv_obj_add_event_cb(ctx->hue_slider, on_slider_pressed, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(ctx->sat_slider, on_slider_pressed, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_slider_pressed, LV_EVENT_PRESSED, ctx);
  lv_obj_add_event_cb(ctx->hue_slider, on_slider_released, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(ctx->sat_slider, on_slider_released, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_slider_released, LV_EVENT_RELEASED, ctx);
  lv_obj_add_event_cb(ctx->hue_slider, on_slider_released, LV_EVENT_PRESS_LOST, ctx);
  lv_obj_add_event_cb(ctx->sat_slider, on_slider_released, LV_EVENT_PRESS_LOST, ctx);
  lv_obj_add_event_cb(ctx->val_slider, on_slider_released, LV_EVENT_PRESS_LOST, ctx);
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
  init.has_state = true;
  init.is_on = false;

  show_light_popup(init);
  if (g_light_popup_ctx && g_light_popup_ctx->card && g_light_popup_ctx->overlay) {
    lv_obj_add_flag(g_light_popup_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_light_popup_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
  }
}

#include "src/ui/cover_popup.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/network/mqtt_handlers.h"
#include "src/tiles/mdi_icons.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/ui/camera_popup.h"
#include "src/ui/climate_popup.h"
#include "src/ui/energy_popup.h"
#include "src/ui/light_popup.h"
#include "src/ui/media_popup.h"
#include "src/ui/popup_layout.h"
#include "src/ui/sensor_popup.h"
#include "src/ui/ui_surface_style.h"
#include "src/ui/weather_popup.h"

#include <cmath>
#include <cstring>

namespace {

enum class CoverPopupMode : uint8_t { Position, Controls };
enum class CoverChannel : uint8_t { None, Position, Tilt };

struct CoverSliderView {
  CoverChannel channel = CoverChannel::None;
  lv_obj_t* column = nullptr;
  lv_obj_t* track = nullptr;
  lv_obj_t* handle = nullptr;
  lv_obj_t* value_label = nullptr;
  bool fill_active = false;
  int16_t fill_center_y = -1;
};

struct CoverPopupContext {
  String entity_id;
  String device_class;
  CoverState state;
  CoverPopupMode mode = CoverPopupMode::Position;
  CoverChannel dragging_channel = CoverChannel::None;
  CoverChannel protected_channel = CoverChannel::None;
  lv_obj_t* overlay = nullptr;
  lv_obj_t* card = nullptr;
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* title_label = nullptr;
  lv_obj_t* top_value_label = nullptr;
  lv_obj_t* main_panel = nullptr;
  lv_obj_t* position_panel = nullptr;
  lv_obj_t* controls_panel = nullptr;
  CoverSliderView position_slider;
  CoverSliderView tilt_slider;
  lv_obj_t* main_open_button = nullptr;
  lv_obj_t* main_open_icon = nullptr;
  lv_obj_t* main_close_button = nullptr;
  lv_obj_t* main_close_icon = nullptr;
  lv_obj_t* tilt_open_button = nullptr;
  lv_obj_t* tilt_open_icon = nullptr;
  lv_obj_t* tilt_close_button = nullptr;
  lv_obj_t* tilt_close_icon = nullptr;
  lv_obj_t* stop_button = nullptr;
  lv_obj_t* stop_icon = nullptr;
  lv_obj_t* mode_row = nullptr;
  lv_obj_t* position_mode_button = nullptr;
  lv_obj_t* position_mode_icon = nullptr;
  lv_obj_t* controls_mode_button = nullptr;
  lv_obj_t* controls_mode_icon = nullptr;
  bool suppress_events = false;
  bool user_dragging = false;
  uint32_t block_remote_until_ms = 0;
  lv_timer_t* remote_apply_timer = nullptr;
  CoverPopupInit pending_remote_init;
  bool has_pending_remote = false;
};

CoverPopupContext* g_ctx = nullptr;

#if defined(DEVICE_LAYOUT_480X480)
constexpr int kContentLiftY = 6;
#else
constexpr int kContentLiftY = 0;
#endif

constexpr int kVerticalSliderWidth = popup_layout::contentScale(160);
constexpr int kVerticalSliderHeight = popup_layout::contentScale(340);
constexpr int kVerticalSliderRadius = popup_layout::contentScale(32);
constexpr int kSliderColumnWidth = popup_layout::contentScale(190);
constexpr int kSliderColumnGap = popup_layout::contentScale(42);
constexpr int kSliderDashWidth = popup_layout::contentScale(46);
constexpr int kSliderDashHeight = popup_layout::contentScale(4);
constexpr int kTiltStripeTopHeight = popup_layout::contentScale(11);
constexpr int kTiltStripeBottomHeight = popup_layout::contentScale(3);
constexpr int kTiltStripePitch = popup_layout::contentScale(18);
constexpr int kModeButtonSize = popup_layout::scale(92);
constexpr int kModeButtonGap = popup_layout::scale(28);
constexpr int kModeButtonTouchPad = popup_layout::scale(10);
constexpr int kActionButtonSize = popup_layout::contentScale(108);
constexpr int kActionButtonGap = popup_layout::contentScale(12);
constexpr uint32_t kRemoteBlockMs = 1500;
constexpr uint32_t kPanelBg = 0x2A2A2A;
constexpr uint32_t kButtonBg = 0x1F1F22;
constexpr uint32_t kAccent = 0xDDA0EE;
constexpr uint32_t kAccentDark = 0x5D4965;
constexpr uint32_t kDisabled = 0x6B6B6B;
constexpr uint32_t kHaCoverActive = 0x926BC7;
constexpr uint32_t kHaCoverInactive = 0x9E9E9E;

bool has_feature(const CoverPopupContext* ctx, uint8_t feature) {
  return ctx && (ctx->state.supported_features & feature) != 0;
}

bool has_position_mode(const CoverPopupContext* ctx) {
  return has_feature(ctx, COVER_FEATURE_SET_POSITION) ||
         has_feature(ctx, COVER_FEATURE_SET_TILT_POSITION);
}

bool has_controls_mode(const CoverPopupContext* ctx) {
  if (!ctx) return false;
  constexpr uint8_t kControlFeatures =
      COVER_FEATURE_OPEN | COVER_FEATURE_CLOSE | COVER_FEATURE_STOP |
      COVER_FEATURE_OPEN_TILT | COVER_FEATURE_CLOSE_TILT |
      COVER_FEATURE_STOP_TILT;
  return (ctx->state.supported_features & kControlFeatures) != 0;
}

bool horizontal_cover(const CoverPopupContext* ctx) {
  if (!ctx) return false;
  return ctx->device_class == "awning" || ctx->device_class == "curtain" ||
         ctx->device_class == "door" || ctx->device_class == "gate";
}

bool cover_icon_is_active(const CoverState& state) {
  return state.valid && state.available &&
         strcmp(state.state, "unknown") != 0 &&
         strcmp(state.state, "unavailable") != 0 &&
         strcmp(state.state, "closed") != 0;
}

void set_hidden(lv_obj_t* obj, bool hidden) {
  if (!obj) return;
  if (hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void set_disabled(lv_obj_t* obj, bool disabled) {
  if (!obj) return;
  if (disabled) {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
  } else {
    lv_obj_clear_state(obj, LV_STATE_DISABLED);
  }
}

void set_icon(lv_obj_t* label, const char* name) {
  if (!label) return;
  lv_label_set_text(label, getMdiChar(name).c_str());
}

void align_header_row(lv_obj_t* card, lv_obj_t* title_label,
                      lv_obj_t* icon_label) {
  if (!card) return;
  lv_obj_update_layout(card);
  lv_coord_t header_center_y =
      popup_layout::kHeaderCenterY -
      lv_obj_get_style_pad_top(card, LV_PART_MAIN);
  if (header_center_y < 0) header_center_y = 0;
  if (icon_label) {
    lv_coord_t icon_y =
        header_center_y - (lv_obj_get_height(icon_label) / 2);
    if (icon_y < 0) icon_y = 0;
    lv_obj_align(icon_label, LV_ALIGN_TOP_LEFT,
                 popup_layout::kHeaderIconX, icon_y);
  }
  if (title_label) {
    lv_coord_t title_y =
        header_center_y - (lv_obj_get_height(title_label) / 2);
    if (title_y < 0) title_y = 0;
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT,
                 popup_layout::kHeaderTitleX, title_y);
  }
}

CoverSliderView* slider_for_channel(CoverPopupContext* ctx,
                                    CoverChannel channel) {
  if (!ctx) return nullptr;
  if (channel == CoverChannel::Position) return &ctx->position_slider;
  if (channel == CoverChannel::Tilt) return &ctx->tilt_slider;
  return nullptr;
}

bool channel_has_value(const CoverPopupContext* ctx, CoverChannel channel) {
  if (!ctx) return false;
  return channel == CoverChannel::Position ? ctx->state.has_position
                                           : ctx->state.has_tilt_position;
}

uint8_t channel_value(const CoverPopupContext* ctx, CoverChannel channel) {
  if (!ctx) return 0;
  return channel == CoverChannel::Position ? ctx->state.position
                                           : ctx->state.tilt_position;
}

void set_channel_value(CoverPopupContext* ctx, CoverChannel channel,
                       uint8_t value) {
  if (!ctx) return;
  if (channel == CoverChannel::Position) {
    ctx->state.has_position = true;
    ctx->state.position = value;
  } else if (channel == CoverChannel::Tilt) {
    ctx->state.has_tilt_position = true;
    ctx->state.tilt_position = value;
  }
}

void update_top_value(CoverPopupContext* ctx) {
  if (!ctx || !ctx->top_value_label) return;
  const String display_state = ctx->state.available
                                   ? String(ctx->state.state)
                                   : String("unavailable");
  String text = i18n::cover_state_label(
      configManager.getConfig().language, display_state);
  if (ctx->state.has_position) {
    text += "  \xC2\xB7  ";
    text += String(ctx->state.position);
    text += "%";
  } else if (ctx->state.has_tilt_position) {
    text += "  \xC2\xB7  ";
    text += String(ctx->state.tilt_position);
    text += "%";
  }
  lv_label_set_text(ctx->top_value_label, text.c_str());
}

int slider_center_y_for_value(uint8_t value) {
  const int min_center_y = kVerticalSliderRadius;
  const int max_center_y = kVerticalSliderHeight - kVerticalSliderRadius;
  const int usable_range = max_center_y - min_center_y;
  if (usable_range <= 0 || value >= 100) return min_center_y;
  if (value == 0) return max_center_y;
  const float progress = static_cast<float>(100 - value) / 100.0f;
  return min_center_y + static_cast<int>(
                            lroundf(progress * static_cast<float>(usable_range)));
}

uint8_t slider_value_from_point(lv_obj_t* track, const lv_point_t& point) {
  if (!track) return 0;
  lv_area_t area;
  lv_obj_get_coords(track, &area);
  const int min_center_y = area.y1 + kVerticalSliderRadius;
  const int max_center_y = area.y2 - kVerticalSliderRadius + 1;
  if (point.y <= min_center_y) return 100;
  if (point.y >= max_center_y) return 0;
  const int usable_range = max_center_y - min_center_y;
  if (usable_range <= 0) return 100;
  const float progress = static_cast<float>(point.y - min_center_y) /
                         static_cast<float>(usable_range);
  int value = 100 - static_cast<int>(lroundf(progress * 100.0f));
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  return static_cast<uint8_t>(value);
}

void update_slider_label(CoverPopupContext* ctx, CoverSliderView& view) {
  if (!ctx || !view.value_label) return;
  const uint8_t label_index =
      view.channel == CoverChannel::Position ? 2 : 3;
  String text = i18n::cover_label(
      configManager.getConfig().language, label_index);
  text += "  \xC2\xB7  ";
  if (channel_has_value(ctx, view.channel)) {
    text += String(channel_value(ctx, view.channel));
    text += "%";
  } else {
    text += "--%";
  }
  lv_label_set_text(view.value_label, text.c_str());
}

void update_position_fill(CoverPopupContext* ctx) {
  if (!ctx || !ctx->position_slider.track) return;
  CoverSliderView& view = ctx->position_slider;
  const bool active = ctx->state.has_position;
  const int center_y = active
                           ? slider_center_y_for_value(ctx->state.position)
                           : -1;
  if (view.fill_active == active && view.fill_center_y == center_y) return;
  view.fill_active = active;
  view.fill_center_y = static_cast<int16_t>(center_y);
  lv_obj_invalidate(view.track);
}

void update_tilt_handle(CoverPopupContext* ctx) {
  if (!ctx || !ctx->tilt_slider.handle) return;
  const uint8_t value = ctx->state.has_tilt_position
                            ? ctx->state.tilt_position
                            : 50;
  const int center_y = slider_center_y_for_value(value);
  lv_obj_set_pos(ctx->tilt_slider.handle, 0,
                 center_y - kVerticalSliderRadius);
}

void update_position_ui(CoverPopupContext* ctx) {
  if (!ctx) return;
  const bool enabled = ctx->state.valid && ctx->state.available;
  const bool show_position =
      has_feature(ctx, COVER_FEATURE_SET_POSITION);
  const bool show_tilt =
      has_feature(ctx, COVER_FEATURE_SET_TILT_POSITION);
  set_hidden(ctx->position_slider.column, !show_position);
  set_hidden(ctx->tilt_slider.column, !show_tilt);
  set_disabled(ctx->position_slider.track, !enabled || !show_position);
  set_disabled(ctx->tilt_slider.track, !enabled || !show_tilt);
  update_slider_label(ctx, ctx->position_slider);
  update_slider_label(ctx, ctx->tilt_slider);
  update_position_fill(ctx);
  update_tilt_handle(ctx);
}

void update_controls_ui(CoverPopupContext* ctx) {
  if (!ctx) return;
  const bool enabled = ctx->state.valid && ctx->state.available;
  const bool assumed = ctx->state.assumed_state;
  const bool show_main_open = has_feature(ctx, COVER_FEATURE_OPEN);
  const bool show_main_close = has_feature(ctx, COVER_FEATURE_CLOSE);
  const bool show_tilt_open = has_feature(ctx, COVER_FEATURE_OPEN_TILT);
  const bool show_tilt_close = has_feature(ctx, COVER_FEATURE_CLOSE_TILT);
  const bool show_stop = has_feature(ctx, COVER_FEATURE_STOP) ||
                         has_feature(ctx, COVER_FEATURE_STOP_TILT);

  set_hidden(ctx->main_open_button, !show_main_open);
  set_hidden(ctx->main_close_button, !show_main_close);
  set_hidden(ctx->tilt_open_button, !show_tilt_open);
  set_hidden(ctx->tilt_close_button, !show_tilt_close);
  set_hidden(ctx->stop_button, !show_stop);

  bool main_open_disabled = !enabled;
  bool main_close_disabled = !enabled;
  bool tilt_open_disabled = !enabled;
  bool tilt_close_disabled = !enabled;
  if (!assumed && ctx->state.has_position) {
    main_open_disabled = main_open_disabled || ctx->state.position >= 100;
    main_close_disabled = main_close_disabled || ctx->state.position == 0;
  }
  if (!assumed && ctx->state.has_tilt_position) {
    tilt_open_disabled =
        tilt_open_disabled || ctx->state.tilt_position >= 100;
    tilt_close_disabled =
        tilt_close_disabled || ctx->state.tilt_position == 0;
  }
  if (!assumed) {
    main_open_disabled =
        main_open_disabled || strcmp(ctx->state.state, "opening") == 0;
    main_close_disabled =
        main_close_disabled || strcmp(ctx->state.state, "closing") == 0 ||
        strcmp(ctx->state.state, "closed") == 0;
  }

  set_disabled(ctx->main_open_button, main_open_disabled);
  set_disabled(ctx->main_close_button, main_close_disabled);
  set_disabled(ctx->tilt_open_button, tilt_open_disabled);
  set_disabled(ctx->tilt_close_button, tilt_close_disabled);
  set_disabled(ctx->stop_button, !enabled);

  if (horizontal_cover(ctx)) {
    set_icon(ctx->main_open_icon, "arrow-expand-horizontal");
    set_icon(ctx->main_close_icon, "arrow-collapse-horizontal");
  } else {
    set_icon(ctx->main_open_icon, "arrow-up");
    set_icon(ctx->main_close_icon, "arrow-down");
  }
  set_icon(ctx->tilt_close_icon, "arrow-down-left");
  set_icon(ctx->tilt_open_icon, "arrow-up-right");
}

void style_mode_button(lv_obj_t* button, lv_obj_t* icon, bool active,
                       bool enabled) {
  if (!button) return;
  const lv_opa_t normal_opa =
      enabled && active ? LV_OPA_20 : LV_OPA_TRANSP;
  lv_obj_set_style_bg_color(button, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(button, normal_opa, 0);
  lv_obj_set_style_bg_color(button, lv_color_white(), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(
      button, enabled ? LV_OPA_20 : LV_OPA_TRANSP, LV_STATE_PRESSED);
  if (icon) {
    lv_obj_set_style_text_color(
        icon, enabled ? lv_color_white() : lv_color_hex(kDisabled), 0);
  }
}

void update_mode_visibility(CoverPopupContext* ctx) {
  if (!ctx) return;
  const bool position_available = has_position_mode(ctx);
  const bool controls_available = has_controls_mode(ctx);
  if (ctx->mode == CoverPopupMode::Position && !position_available) {
    ctx->mode = CoverPopupMode::Controls;
  } else if (ctx->mode == CoverPopupMode::Controls &&
             !controls_available) {
    ctx->mode = CoverPopupMode::Position;
  }
  const bool position_active =
      ctx->mode == CoverPopupMode::Position && position_available;
  const bool controls_active =
      ctx->mode == CoverPopupMode::Controls && controls_available;
  set_hidden(ctx->position_panel, !position_active);
  set_hidden(ctx->controls_panel, !controls_active);
  set_hidden(ctx->position_mode_button, !position_available);
  set_hidden(ctx->controls_mode_button, !controls_available);
  style_mode_button(ctx->position_mode_button, ctx->position_mode_icon,
                    position_active, position_available);
  style_mode_button(ctx->controls_mode_button, ctx->controls_mode_icon,
                    controls_active, controls_available);
}

void refresh_popup(CoverPopupContext* ctx) {
  if (!ctx) return;
  update_top_value(ctx);
  update_position_ui(ctx);
  update_controls_ui(ctx);
  update_mode_visibility(ctx);
  if (ctx->icon_label) {
    lv_obj_set_style_text_color(
        ctx->icon_label,
        lv_color_hex(cover_icon_is_active(ctx->state)
                         ? kHaCoverActive
                         : kHaCoverInactive),
        0);
  }
}

void apply_init(CoverPopupContext* ctx, const CoverPopupInit& init) {
  if (!ctx) return;
  const bool entity_changed =
      !ctx->entity_id.equalsIgnoreCase(init.entity_id);
  ctx->entity_id = init.entity_id;
  ctx->state = init.state;
  ctx->device_class = init.state.device_class;
  ctx->device_class.toLowerCase();
  if (entity_changed) {
    ctx->mode = has_position_mode(ctx) ? CoverPopupMode::Position
                                       : CoverPopupMode::Controls;
  }
  if (ctx->title_label) {
    lv_label_set_text(ctx->title_label, init.title.c_str());
  }
  if (ctx->icon_label) {
    set_hidden(ctx->icon_label, !init.icon_visible);
    if (init.icon_visible) {
      lv_label_set_text(ctx->icon_label,
                        getMdiChar(init.icon_name).c_str());
    }
  }
  align_header_row(ctx->card, ctx->title_label, ctx->icon_label);
  refresh_popup(ctx);
}

bool remote_update_blocked(const CoverPopupContext* ctx) {
  if (!ctx) return false;
  if (ctx->user_dragging) return true;
  return ctx->block_remote_until_ms != 0 &&
         static_cast<int32_t>(millis() - ctx->block_remote_until_ms) < 0;
}

void cancel_deferred_remote_apply(CoverPopupContext* ctx) {
  if (!ctx) return;
  if (ctx->remote_apply_timer) {
    lv_timer_delete(ctx->remote_apply_timer);
    ctx->remote_apply_timer = nullptr;
  }
  ctx->has_pending_remote = false;
}

void remote_apply_timer_cb(lv_timer_t* timer) {
  CoverPopupContext* ctx =
      static_cast<CoverPopupContext*>(lv_timer_get_user_data(timer));
  if (!ctx) {
    lv_timer_delete(timer);
    return;
  }
  if (remote_update_blocked(ctx)) {
    const uint32_t now = millis();
    const uint32_t remaining =
        ctx->user_dragging ? 50 : ctx->block_remote_until_ms - now + 20;
    lv_timer_set_period(timer, remaining);
    lv_timer_reset(timer);
    return;
  }

  ctx->remote_apply_timer = nullptr;
  lv_timer_delete(timer);
  if (!ctx->has_pending_remote) return;
  CoverPopupInit pending = ctx->pending_remote_init;
  ctx->has_pending_remote = false;
  ctx->block_remote_until_ms = 0;
  ctx->protected_channel = CoverChannel::None;
  if (!ctx->card || lv_obj_has_flag(ctx->card, LV_OBJ_FLAG_HIDDEN) ||
      !ctx->entity_id.equalsIgnoreCase(pending.entity_id)) {
    return;
  }
  apply_init(ctx, pending);
}

void defer_remote_apply(CoverPopupContext* ctx,
                        const CoverPopupInit& init) {
  if (!ctx) return;
  ctx->pending_remote_init = init;
  ctx->has_pending_remote = true;
  const uint32_t now = millis();
  const uint32_t delay_ms =
      ctx->user_dragging ? 50 : ctx->block_remote_until_ms - now + 20;
  if (ctx->remote_apply_timer) {
    lv_timer_set_period(ctx->remote_apply_timer, delay_ms);
    lv_timer_reset(ctx->remote_apply_timer);
    return;
  }
  ctx->remote_apply_timer =
      lv_timer_create(remote_apply_timer_cb, delay_ms, ctx);
}

void apply_remote_state_preserving_active_value(
    CoverPopupContext* ctx, const CoverPopupInit& init) {
  if (!ctx) return;
  CoverPopupInit visible = init;
  const CoverChannel channel = ctx->user_dragging
                                   ? ctx->dragging_channel
                                   : ctx->protected_channel;
  if (channel == CoverChannel::Position) {
    visible.state.has_position = ctx->state.has_position;
    visible.state.position = ctx->state.position;
  } else if (channel == CoverChannel::Tilt) {
    visible.state.has_tilt_position = ctx->state.has_tilt_position;
    visible.state.tilt_position = ctx->state.tilt_position;
  }
  apply_init(ctx, visible);
}

void publish_action(CoverPopupContext* ctx, const char* command,
                    int value = -1,
                    CoverChannel channel = CoverChannel::None) {
  if (!ctx || !ctx->state.available || !command) return;
  mqttPublishCoverCommand(ctx->entity_id.c_str(), command, value);
  if (value >= 0 && channel != CoverChannel::None) {
    ctx->protected_channel = channel;
    ctx->block_remote_until_ms = millis() + kRemoteBlockMs;
  } else {
    ctx->protected_channel = CoverChannel::None;
    ctx->block_remote_until_ms = 0;
    cancel_deferred_remote_apply(ctx);
  }
}

void on_action(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  CoverPopupContext* ctx = static_cast<CoverPopupContext*>(
      lv_event_get_user_data(event));
  if (!ctx) return;
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (target == ctx->main_open_button) {
    publish_action(ctx, "open_cover");
  } else if (target == ctx->main_close_button) {
    publish_action(ctx, "close_cover");
  } else if (target == ctx->tilt_open_button) {
    publish_action(ctx, "open_cover_tilt");
  } else if (target == ctx->tilt_close_button) {
    publish_action(ctx, "close_cover_tilt");
  } else if (target == ctx->stop_button) {
    if (has_feature(ctx, COVER_FEATURE_STOP)) {
      publish_action(ctx, "stop_cover");
    }
    if (has_feature(ctx, COVER_FEATURE_STOP_TILT)) {
      publish_action(ctx, "stop_cover_tilt");
    }
  }
}

void apply_slider_point(CoverPopupContext* ctx, CoverChannel channel,
                        const lv_point_t& point) {
  CoverSliderView* view = slider_for_channel(ctx, channel);
  if (!ctx || !view || !view->track) return;
  const uint8_t value = slider_value_from_point(view->track, point);
  set_channel_value(ctx, channel, value);
  update_top_value(ctx);
  update_slider_label(ctx, *view);
  if (channel == CoverChannel::Position) {
    update_position_fill(ctx);
  } else {
    update_tilt_handle(ctx);
  }
}

void commit_slider(CoverPopupContext* ctx, CoverChannel channel) {
  if (!ctx || channel == CoverChannel::None) return;
  const int value = channel_value(ctx, channel);
  publish_action(ctx,
                 channel == CoverChannel::Position
                     ? "set_cover_position"
                     : "set_cover_tilt_position",
                 value, channel);
}

void on_slider_track(lv_event_t* event) {
  CoverPopupContext* ctx = static_cast<CoverPopupContext*>(
      lv_event_get_user_data(event));
  if (!ctx || ctx->suppress_events) return;
  const lv_event_code_t code = lv_event_get_code(event);
  if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
      code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  CoverChannel channel = CoverChannel::None;
  if (target == ctx->position_slider.track) {
    channel = CoverChannel::Position;
  } else if (target == ctx->tilt_slider.track) {
    channel = CoverChannel::Tilt;
  }
  if (channel == CoverChannel::None) return;

  if (code == LV_EVENT_PRESSED) {
    ctx->user_dragging = true;
    ctx->dragging_channel = channel;
  }
  lv_indev_t* indev = lv_indev_get_act();
  if (indev) {
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    apply_slider_point(ctx, channel, point);
  }
  if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
    ctx->user_dragging = false;
    ctx->dragging_channel = CoverChannel::None;
    commit_slider(ctx, channel);
  }
}

void on_position_slider_draw(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
  CoverPopupContext* ctx = static_cast<CoverPopupContext*>(
      lv_event_get_user_data(event));
  if (!ctx || !ctx->position_slider.fill_active ||
      ctx->position_slider.fill_center_y < 0) {
    return;
  }
  lv_layer_t* layer = lv_event_get_layer(event);
  if (!layer) return;
  lv_area_t area;
  lv_obj_get_coords(ctx->position_slider.track, &area);
  const lv_coord_t center_y = static_cast<lv_coord_t>(
      area.y1 + ctx->position_slider.fill_center_y);
  const lv_coord_t top_center_y = static_cast<lv_coord_t>(
      area.y1 + kVerticalSliderRadius);

  lv_draw_rect_dsc_t fill;
  lv_draw_rect_dsc_init(&fill);
  fill.base.layer = layer;
  fill.bg_color = lv_color_hex(kAccent);
  fill.bg_opa = LV_OPA_COVER;
  fill.border_opa = LV_OPA_TRANSP;
  fill.radius = kVerticalSliderRadius;
  lv_area_t top_cap = {
      area.x1,
      static_cast<lv_coord_t>(top_center_y - kVerticalSliderRadius),
      area.x2,
      static_cast<lv_coord_t>(top_center_y + kVerticalSliderRadius - 1),
  };
  lv_draw_rect(layer, &fill, &top_cap);
  lv_area_t moving_cap = {
      area.x1,
      static_cast<lv_coord_t>(center_y - kVerticalSliderRadius),
      area.x2,
      static_cast<lv_coord_t>(center_y + kVerticalSliderRadius - 1),
  };
  lv_draw_rect(layer, &fill, &moving_cap);
  fill.radius = 0;
  lv_area_t middle = {area.x1, top_center_y, area.x2, center_y};
  lv_draw_rect(layer, &fill, &middle);

  lv_draw_rect_dsc_t dash;
  lv_draw_rect_dsc_init(&dash);
  dash.base.layer = layer;
  dash.bg_color = lv_color_hex(kPanelBg);
  dash.bg_opa = LV_OPA_COVER;
  dash.border_opa = LV_OPA_TRANSP;
  dash.radius = LV_RADIUS_CIRCLE;
  const lv_coord_t dash_x1 = static_cast<lv_coord_t>(
      area.x1 + ((lv_area_get_width(&area) - kSliderDashWidth) / 2));
  const lv_coord_t dash_y1 = static_cast<lv_coord_t>(
      center_y - (kSliderDashHeight / 2));
  lv_area_t dash_area = {
      dash_x1,
      dash_y1,
      static_cast<lv_coord_t>(dash_x1 + kSliderDashWidth - 1),
      static_cast<lv_coord_t>(dash_y1 + kSliderDashHeight - 1),
  };
  lv_draw_rect(layer, &dash, &dash_area);
}

int rounded_track_inset(const lv_area_t& area, lv_coord_t y) {
  const int local_y = static_cast<int>(y - area.y1);
  const int height = lv_area_get_height(&area);
  int distance_from_center = 0;
  if (local_y < kVerticalSliderRadius) {
    distance_from_center = kVerticalSliderRadius - local_y;
  } else if (local_y >= height - kVerticalSliderRadius) {
    distance_from_center =
        local_y - (height - kVerticalSliderRadius - 1);
  } else {
    return 0;
  }
  const float radius = static_cast<float>(kVerticalSliderRadius);
  const float distance = static_cast<float>(distance_from_center);
  const float chord = sqrtf(fmaxf(0.0f, radius * radius -
                                            distance * distance));
  return kVerticalSliderRadius - static_cast<int>(floorf(chord));
}

void on_tilt_slider_draw(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;
  lv_layer_t* layer = lv_event_get_layer(event);
  if (!layer) return;

  lv_obj_t* track = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (!track) return;
  lv_area_t area;
  lv_obj_get_coords(track, &area);

  lv_draw_rect_dsc_t stripe;
  lv_draw_rect_dsc_init(&stripe);
  stripe.base.layer = layer;
  stripe.bg_color = lv_color_hex(kAccentDark);
  stripe.bg_opa = LV_OPA_COVER;
  stripe.border_opa = LV_OPA_TRANSP;
  stripe.radius = 0;

  const lv_coord_t first_y =
      static_cast<lv_coord_t>(area.y1 + kTiltStripePitch);
  const int track_span = lv_area_get_height(&area) - 1;
  for (lv_coord_t y = first_y;
       y <= area.y2;
       y = static_cast<lv_coord_t>(y + kTiltStripePitch)) {
    const int local_y = static_cast<int>(y - area.y1);
    const int taper_range =
        kTiltStripeTopHeight - kTiltStripeBottomHeight;
    int stripe_height = kTiltStripeBottomHeight;
    if (track_span > 0 && taper_range > 0) {
      stripe_height +=
          ((track_span - local_y) * taper_range + (track_span / 2)) /
          track_span;
    }
    if (stripe_height < 1) stripe_height = 1;
    const lv_coord_t y2 =
        static_cast<lv_coord_t>(y + stripe_height - 1);
    if (y2 > area.y2) break;
    const int top_inset = rounded_track_inset(area, y);
    const int bottom_inset = rounded_track_inset(area, y2);
    const int inset = top_inset > bottom_inset ? top_inset : bottom_inset;
    lv_area_t stripe_area = {
        static_cast<lv_coord_t>(area.x1 + inset),
        y,
        static_cast<lv_coord_t>(area.x2 - inset),
        y2,
    };
    if (stripe_area.x1 <= stripe_area.x2) {
      lv_draw_rect(layer, &stripe, &stripe_area);
    }
  }
}

void on_mode(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  CoverPopupContext* ctx = static_cast<CoverPopupContext*>(
      lv_event_get_user_data(event));
  if (!ctx) return;
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (target == ctx->position_mode_button && has_position_mode(ctx)) {
    ctx->mode = CoverPopupMode::Position;
  } else if (target == ctx->controls_mode_button &&
             has_controls_mode(ctx)) {
    ctx->mode = CoverPopupMode::Controls;
  }
  update_mode_visibility(ctx);
}

void on_close(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  hide_cover_popup();
}

void on_overlay_click(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  (void)event;
}

void on_delete(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_DELETE) return;
  CoverPopupContext* ctx = static_cast<CoverPopupContext*>(
      lv_event_get_user_data(event));
  cancel_deferred_remote_apply(ctx);
  if (g_ctx == ctx) g_ctx = nullptr;
  delete ctx;
}

lv_obj_t* create_action_button(lv_obj_t* parent, const char* icon_name,
                               lv_obj_t** icon_out) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_size(button, kActionButtonSize, kActionButtonSize);
  lv_obj_set_style_bg_color(button, lv_color_hex(kButtonBg), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(kAccentDark),
                            LV_STATE_PRESSED);
  lv_obj_set_style_radius(button, popup_layout::contentScale(28), 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_opa(button, LV_OPA_30, LV_STATE_DISABLED);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
  disable_pressed_button_animation(button);
  lv_obj_t* icon = lv_label_create(button);
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(icon);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_label_set_text(icon, getMdiChar(icon_name).c_str());
  lv_obj_center(icon);
  if (icon_out) *icon_out = icon;
  return button;
}

lv_obj_t* create_mode_button(lv_obj_t* parent, const char* icon_name,
                             lv_obj_t** icon_out) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_size(button, kModeButtonSize, kModeButtonSize);
  lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(button, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_anim_time(button, 0, 0);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_ext_click_area(button, kModeButtonTouchPad);
  disable_pressed_button_animation(button);
  lv_obj_t* icon = lv_label_create(button);
  lv_obj_set_style_text_font(icon, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(icon);
  lv_obj_set_style_text_color(icon, lv_color_white(), 0);
  lv_label_set_text(icon, getMdiChar(icon_name).c_str());
  lv_obj_center(icon);
  if (icon_out) *icon_out = icon;
  return button;
}

CoverSliderView create_slider_view(lv_obj_t* parent, CoverChannel channel,
                                   bool handled) {
  CoverSliderView view;
  view.channel = channel;
  view.column = lv_obj_create(parent);
  lv_obj_set_size(view.column, kSliderColumnWidth, LV_PCT(100));
  lv_obj_set_style_bg_opa(view.column, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(view.column, 0, 0);
  lv_obj_set_style_pad_all(view.column, 0, 0);
  lv_obj_set_style_pad_row(view.column, popup_layout::scale(8), 0);
  lv_obj_set_layout(view.column, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(view.column, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(view.column, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(view.column, LV_OBJ_FLAG_SCROLLABLE);

  view.track = lv_obj_create(view.column);
  lv_obj_set_size(view.track, kVerticalSliderWidth, kVerticalSliderHeight);
  lv_obj_set_style_radius(view.track, kVerticalSliderRadius, 0);
  lv_obj_set_style_pad_all(view.track, 0, 0);
  lv_obj_set_style_border_width(view.track, 0, 0);
  lv_obj_set_style_clip_corner(view.track, true, 0);
  lv_obj_set_style_opa(view.track, LV_OPA_30, LV_STATE_DISABLED);
  lv_obj_clear_flag(view.track, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(view.track, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(view.track, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_ext_click_area(view.track, popup_layout::scale(18));
  if (handled) {
    lv_obj_set_style_bg_color(view.track, lv_color_hex(kAccentDark), 0);
    lv_obj_set_style_bg_grad_color(view.track, lv_color_hex(kAccent), 0);
    lv_obj_set_style_bg_grad_dir(view.track, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(view.track, LV_OPA_COVER, 0);
    view.handle = lv_obj_create(view.track);
    lv_obj_set_size(view.handle, kVerticalSliderWidth,
                    kVerticalSliderRadius * 2);
    lv_obj_set_style_radius(view.handle, kVerticalSliderRadius, 0);
    lv_obj_set_style_bg_color(view.handle, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(view.handle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view.handle, 0, 0);
    lv_obj_set_style_shadow_width(view.handle,
                                  popup_layout::contentScale(18), 0);
    lv_obj_set_style_shadow_color(view.handle, lv_color_black(), 0);
    lv_obj_set_style_shadow_opa(view.handle, LV_OPA_20, 0);
    lv_obj_add_flag(view.handle, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(view.handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(view.handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* dash = lv_obj_create(view.handle);
    lv_obj_set_size(dash, kSliderDashWidth, kSliderDashHeight);
    lv_obj_set_style_radius(dash, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dash, lv_color_hex(kDisabled), 0);
    lv_obj_set_style_bg_opa(dash, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dash, 0, 0);
    lv_obj_set_style_shadow_width(dash, 0, 0);
    lv_obj_clear_flag(dash, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(dash, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(dash);
  } else {
    lv_obj_set_style_bg_color(view.track, lv_color_hex(kAccent), 0);
    lv_obj_set_style_bg_opa(view.track, LV_OPA_30, 0);
  }

  view.value_label = lv_label_create(view.column);
  lv_obj_set_width(view.value_label, LV_PCT(100));
  lv_obj_set_style_text_align(view.value_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(view.value_label, popup_layout::font20(), 0);
  lv_obj_set_style_text_color(view.value_label, lv_color_white(), 0);
  lv_label_set_text(view.value_label, "");
  return view;
}

}  // namespace

void show_cover_popup(const CoverPopupInit& init) {
  if (!init.entity_id.length()) return;
  hide_light_popup();
  hide_climate_popup();
  hide_sensor_popup();
  hide_weather_popup();
  hide_energy_popup();
  hide_media_popup();
  hide_camera_popup();

  if (g_ctx && g_ctx->overlay && g_ctx->card) {
    if (!g_ctx->entity_id.equalsIgnoreCase(init.entity_id)) {
      cancel_deferred_remote_apply(g_ctx);
      g_ctx->block_remote_until_ms = 0;
      g_ctx->protected_channel = CoverChannel::None;
    }
    apply_init(g_ctx, init);
    lv_obj_clear_flag(g_ctx->card, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(g_ctx->overlay);
    return;
  }

  CoverPopupContext* ctx = new CoverPopupContext();
  g_ctx = ctx;
  ctx->overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(ctx->overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(ctx->overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->overlay, 0, 0);
  lv_obj_remove_flag(ctx->overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ctx->overlay, LV_OBJ_FLAG_CLICKABLE);

  ctx->card = lv_obj_create(ctx->overlay);
  lv_obj_set_size(ctx->card, popup_layout::kCardWidth,
                  popup_layout::kCardHeight);
  lv_obj_center(ctx->card);
  lv_obj_set_style_bg_color(ctx->card, lv_color_hex(kPanelBg), 0);
  lv_obj_set_style_bg_opa(ctx->card, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(ctx->card, popup_layout::kCardRadius, 0);
  lv_obj_set_style_border_width(ctx->card, 0, 0);
  ui_surface_style::apply_global_tile_border(ctx->card);
  lv_obj_set_style_pad_all(ctx->card, popup_layout::kCardPad, 0);
  lv_obj_set_style_shadow_width(ctx->card, popup_layout::scale480(28), 0);
  lv_obj_set_style_shadow_color(ctx->card, lv_color_black(), 0);
  lv_obj_set_style_shadow_opa(ctx->card, LV_OPA_40, 0);
  lv_obj_set_style_shadow_spread(ctx->card, popup_layout::scale480(2), 0);
  lv_obj_remove_flag(ctx->card, LV_OBJ_FLAG_SCROLLABLE);

  ctx->title_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->title_label,
                             popup_layout::headerTitleFont(), 0);
  lv_obj_set_style_text_color(ctx->title_label, lv_color_white(), 0);
  lv_obj_set_width(ctx->title_label, LV_PCT(62));
  lv_label_set_long_mode(ctx->title_label, LV_LABEL_LONG_DOT);

  ctx->icon_label = lv_label_create(ctx->card);
  lv_obj_set_style_text_font(ctx->icon_label, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(ctx->icon_label);

  lv_obj_t* close = lv_button_create(ctx->card);
  lv_obj_set_size(close, popup_layout::kCloseButtonSize,
                  popup_layout::kCloseButtonSize);
  lv_obj_set_style_bg_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_color(close, lv_color_white(), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(close, LV_OPA_20, LV_STATE_PRESSED);
  lv_obj_set_style_border_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_outline_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_shadow_opa(close, LV_OPA_TRANSP, 0);
  lv_obj_set_style_radius(close, popup_layout::kCloseButtonRadius, 0);
  lv_obj_set_style_pad_all(close, 0, 0);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT,
               popup_layout::kCloseButtonOffsetX,
               popup_layout::kCloseButtonOffsetY);
  lv_obj_set_ext_click_area(close, popup_layout::kCloseButtonClickArea);
  lv_obj_add_flag(close, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(close, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(close);
  lv_obj_t* close_icon = lv_label_create(close);
  lv_obj_set_style_text_font(close_icon, FONT_MDI_ICONS, 0);
  popup_layout::applyIconScale(close_icon);
  lv_obj_set_style_text_color(close_icon, lv_color_white(), 0);
  lv_label_set_text(close_icon, getMdiChar("window-close").c_str());
  lv_obj_center(close_icon);
  lv_obj_add_event_cb(close, on_close, LV_EVENT_CLICKED, ctx);

  lv_obj_t* value_box = lv_obj_create(ctx->card);
  lv_obj_remove_style_all(value_box);
  lv_obj_set_size(value_box, LV_PCT(100), popup_layout::kValueHeight);
  lv_obj_align(value_box, LV_ALIGN_TOP_MID, 0,
               popup_layout::kValueY - kContentLiftY);
  lv_obj_set_style_bg_opa(value_box, LV_OPA_TRANSP, 0);
  lv_obj_set_layout(value_box, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(value_box, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(value_box, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(value_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(value_box, LV_OBJ_FLAG_SCROLLABLE);

  ctx->top_value_label = lv_label_create(value_box);
  lv_obj_set_width(ctx->top_value_label, LV_PCT(100));
  lv_obj_set_height(ctx->top_value_label, popup_layout::kValueHeight);
  lv_obj_set_style_text_align(ctx->top_value_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(ctx->top_value_label,
                             popup_layout::font40(), 0);
  lv_obj_set_style_text_color(ctx->top_value_label, lv_color_white(), 0);
  lv_obj_set_style_translate_y(
      ctx->top_value_label, popup_layout::kLargeValueTextOffsetY, 0);

  ctx->main_panel = lv_obj_create(ctx->card);
  lv_obj_set_size(ctx->main_panel, LV_PCT(100), popup_layout::kBodyHeight);
  lv_obj_align(ctx->main_panel, LV_ALIGN_TOP_MID, 0,
               popup_layout::kBodyY - kContentLiftY);
  lv_obj_set_style_bg_opa(ctx->main_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->main_panel, 0, 0);
  lv_obj_set_style_pad_all(ctx->main_panel, 0, 0);
  lv_obj_clear_flag(ctx->main_panel, LV_OBJ_FLAG_SCROLLABLE);

  ctx->position_panel = lv_obj_create(ctx->main_panel);
  lv_obj_set_size(ctx->position_panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(ctx->position_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->position_panel, 0, 0);
  lv_obj_set_style_pad_all(ctx->position_panel, 0, 0);
  lv_obj_set_style_pad_column(ctx->position_panel, kSliderColumnGap, 0);
  lv_obj_set_layout(ctx->position_panel, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ctx->position_panel, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ctx->position_panel, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(ctx->position_panel, LV_OBJ_FLAG_SCROLLABLE);
  ctx->position_slider = create_slider_view(
      ctx->position_panel, CoverChannel::Position, false);
  ctx->tilt_slider = create_slider_view(
      ctx->position_panel, CoverChannel::Tilt, true);

  ctx->controls_panel = lv_obj_create(ctx->main_panel);
  lv_obj_set_size(ctx->controls_panel, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(ctx->controls_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->controls_panel, 0, 0);
  lv_obj_set_style_pad_all(ctx->controls_panel, 0, 0);
  lv_obj_clear_flag(ctx->controls_panel, LV_OBJ_FLAG_SCROLLABLE);
  ctx->main_open_button = create_action_button(
      ctx->controls_panel, "arrow-up", &ctx->main_open_icon);
  ctx->main_close_button = create_action_button(
      ctx->controls_panel, "arrow-down", &ctx->main_close_icon);
  ctx->tilt_close_button = create_action_button(
      ctx->controls_panel, "arrow-down-left", &ctx->tilt_close_icon);
  ctx->tilt_open_button = create_action_button(
      ctx->controls_panel, "arrow-up-right", &ctx->tilt_open_icon);
  ctx->stop_button = create_action_button(
      ctx->controls_panel, "stop", &ctx->stop_icon);
  const int action_step = kActionButtonSize + kActionButtonGap;
  lv_obj_align(ctx->main_open_button, LV_ALIGN_CENTER, 0, -action_step);
  lv_obj_align(ctx->main_close_button, LV_ALIGN_CENTER, 0, action_step);
  lv_obj_align(ctx->tilt_close_button, LV_ALIGN_CENTER, -action_step, 0);
  lv_obj_align(ctx->tilt_open_button, LV_ALIGN_CENTER, action_step, 0);
  lv_obj_align(ctx->stop_button, LV_ALIGN_CENTER, 0, 0);

  ctx->mode_row = lv_obj_create(ctx->card);
  lv_obj_set_size(ctx->mode_row, LV_SIZE_CONTENT,
                  popup_layout::kNavHeight);
  lv_obj_align(ctx->mode_row, LV_ALIGN_BOTTOM_MID, 0,
               -popup_layout::kNavBottomInset);
  lv_obj_set_style_bg_opa(ctx->mode_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ctx->mode_row, 0, 0);
  lv_obj_set_style_pad_all(ctx->mode_row, 0, 0);
  lv_obj_set_style_pad_column(ctx->mode_row, kModeButtonGap, 0);
  lv_obj_set_layout(ctx->mode_row, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ctx->mode_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ctx->mode_row, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(ctx->mode_row, LV_OBJ_FLAG_SCROLLABLE);
  ctx->position_mode_button = create_mode_button(
      ctx->mode_row, "format-list-bulleted", &ctx->position_mode_icon);
  ctx->controls_mode_button = create_mode_button(
      ctx->mode_row, "swap-vertical", &ctx->controls_mode_icon);

  lv_obj_add_event_cb(ctx->main_open_button, on_action,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->main_close_button, on_action,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->tilt_open_button, on_action,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->tilt_close_button, on_action,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->stop_button, on_action, LV_EVENT_CLICKED, ctx);
  for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING,
                               LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST}) {
    lv_obj_add_event_cb(ctx->position_slider.track, on_slider_track,
                        code, ctx);
    lv_obj_add_event_cb(ctx->tilt_slider.track, on_slider_track,
                        code, ctx);
  }
  lv_obj_add_event_cb(ctx->position_slider.track, on_position_slider_draw,
                      LV_EVENT_DRAW_MAIN, ctx);
  lv_obj_add_event_cb(ctx->tilt_slider.track, on_tilt_slider_draw,
                      LV_EVENT_DRAW_MAIN, ctx);
  lv_obj_add_event_cb(ctx->position_mode_button, on_mode,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->controls_mode_button, on_mode,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->overlay, on_overlay_click,
                      LV_EVENT_CLICKED, ctx);
  lv_obj_add_event_cb(ctx->overlay, on_delete, LV_EVENT_DELETE, ctx);

  apply_init(ctx, init);
  lv_obj_move_foreground(ctx->icon_label);
  lv_obj_move_foreground(ctx->title_label);
  lv_obj_move_foreground(close);
  lv_obj_move_foreground(ctx->overlay);
}

void update_cover_popup(const CoverPopupInit& init) {
  if (!g_ctx || !g_ctx->card || !g_ctx->overlay) return;
  if (lv_obj_has_flag(g_ctx->card, LV_OBJ_FLAG_HIDDEN)) return;
  if (!g_ctx->entity_id.equalsIgnoreCase(init.entity_id)) return;
  if (remote_update_blocked(g_ctx)) {
    defer_remote_apply(g_ctx, init);
    apply_remote_state_preserving_active_value(g_ctx, init);
    return;
  }
  cancel_deferred_remote_apply(g_ctx);
  g_ctx->block_remote_until_ms = 0;
  g_ctx->protected_channel = CoverChannel::None;
  apply_init(g_ctx, init);
}

void preload_cover_popup() {
  if (g_ctx && g_ctx->overlay && g_ctx->card) return;
  CoverPopupInit init;
  init.entity_id = "__preload__";
  init.title = "";
  init.icon_name = "blinds";
  init.icon_visible = true;
  init.state.valid = true;
  init.state.available = true;
  init.state.has_position = true;
  init.state.has_tilt_position = true;
  init.state.position = 50;
  init.state.tilt_position = 50;
  init.state.supported_features = 0xFF;
  strlcpy(init.state.state, "unknown", sizeof(init.state.state));
  show_cover_popup(init);
  hide_cover_popup();
}

void hide_cover_popup() {
  if (!g_ctx || !g_ctx->card || !g_ctx->overlay) return;
  g_ctx->user_dragging = false;
  g_ctx->dragging_channel = CoverChannel::None;
  g_ctx->protected_channel = CoverChannel::None;
  g_ctx->block_remote_until_ms = 0;
  cancel_deferred_remote_apply(g_ctx);
  lv_obj_add_flag(g_ctx->card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_ctx->overlay, LV_OBJ_FLAG_CLICKABLE);
}

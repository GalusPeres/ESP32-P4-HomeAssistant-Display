#pragma once

#include <lvgl.h>

#include "src/devices/device_select.h"
#include "src/fonts/ui_fonts.h"

#define FONT_SMALL (&ui_font_16)
#define FONT_TITLE (&ui_font_20)

#if defined(DEVICE_LAYOUT_1024X600)
#define FONT_VALUE (&ui_font_24)
#define FONT_UNIT  (&ui_font_20)
#else
#define FONT_VALUE (&ui_font_28)
#define FONT_UNIT  (&ui_font_24)
#endif

namespace tile_layout {

inline const lv_font_t* header_title_font() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_16;
#else
  return FONT_TITLE;
#endif
}

inline void apply_mdi_icon_scale(lv_obj_t* label) {
  // Each compact layout selects a native-size MDI font at compile time.
  // Keeping this hook as a no-op avoids a runtime transform and preserves
  // natural LVGL label centering.
  (void)label;
}

inline void reset_mdi_icon_scale(lv_obj_t* label) {
  (void)label;
}

constexpr lv_coord_t scale(lv_coord_t value) {
#if defined(DEVICE_LAYOUT_1024X600)
  return static_cast<lv_coord_t>((value * 5 + 3) / 6);
#else
  return value;
#endif
}

constexpr uint16_t scale_u16(uint16_t value) {
#if defined(DEVICE_LAYOUT_1024X600)
  return static_cast<uint16_t>((static_cast<uint32_t>(value) * 5U + 3U) / 6U);
#else
  return value;
#endif
}

constexpr int16_t scale_i16(int16_t value) {
#if defined(DEVICE_LAYOUT_1024X600)
  return static_cast<int16_t>((static_cast<int32_t>(value) * 5 +
                               (value >= 0 ? 3 : -3)) /
                              6);
#else
  return value;
#endif
}

inline const lv_font_t* content_font_20() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_16;
#else
  return &ui_font_20;
#endif
}

inline const lv_font_t* content_font_24() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_20;
#else
  return &ui_font_24;
#endif
}

inline const lv_font_t* content_font_28() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_24;
#else
  return &ui_font_28;
#endif
}

inline const lv_font_t* content_font_32() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_28;
#else
  return &ui_font_32;
#endif
}

inline const lv_font_t* content_font_40() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_32;
#else
  return &ui_font_40;
#endif
}

}  // namespace tile_layout

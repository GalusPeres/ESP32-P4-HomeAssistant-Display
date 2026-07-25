#pragma once

#include "src/core/display_manager.h"
#include "src/fonts/ui_fonts.h"

namespace popup_layout {

constexpr int scale(int value) {
#if defined(DEVICE_LAYOUT_1024X600)
  return (value * 5 + (value >= 0 ? 3 : -3)) / 6;
#else
  return value;
#endif
}

constexpr int contentScale(int value) {
#if defined(DEVICE_LAYOUT_1024X600)
  return (value * 3 + (value >= 0 ? 2 : -2)) / 4;
#else
  return value;
#endif
}

inline const lv_font_t* font20() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_16;
#else
  return &ui_font_20;
#endif
}

inline const lv_font_t* font24() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_20;
#else
  return &ui_font_24;
#endif
}

inline const lv_font_t* font28() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_24;
#else
  return &ui_font_28;
#endif
}

inline const lv_font_t* font32() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_28;
#else
  return &ui_font_32;
#endif
}

inline const lv_font_t* font40() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_32;
#else
  return &ui_font_40;
#endif
}

inline const lv_font_t* font48() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_40;
#else
  return &ui_font_48;
#endif
}

inline const lv_font_t* font56() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_48;
#else
  return &ui_font_56;
#endif
}

inline const lv_font_t* font64() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_56;
#else
  return &ui_font_64;
#endif
}

inline const lv_font_t* font72() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_56;
#else
  return &ui_font_72;
#endif
}

inline const lv_font_t* font80() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_64;
#else
  return &ui_font_80;
#endif
}

inline const lv_font_t* font96() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_80;
#else
  return &ui_font_96;
#endif
}

inline const lv_font_t* headerTitleFont() {
#if defined(DEVICE_LAYOUT_1024X600)
  return &ui_font_16;
#else
  return &ui_font_24;
#endif
}

inline void applyIconScale(lv_obj_t* label) {
  // Compact layouts use native-size MDI fonts, so LVGL can center the label
  // directly without a transform pivot or per-frame resampling.
  (void)label;
}

#if defined(DEVICE_LAYOUT_1024X600)
constexpr int kHeaderCenterY = 42;
constexpr int kHeaderTitleX = 58;
constexpr int kCloseButtonSize = 72;
constexpr int kCloseButtonRadius = 13;
constexpr int kCloseButtonOffsetX = 3;
constexpr int kCloseButtonOffsetY = -3;
constexpr int kCloseButtonClickArea = 7;
#else
constexpr int kHeaderCenterY = 60;
constexpr int kHeaderTitleX = 78;
constexpr int kCloseButtonSize = 96;
constexpr int kCloseButtonRadius = 16;
constexpr int kCloseButtonOffsetX = 6;
constexpr int kCloseButtonOffsetY = -6;
constexpr int kCloseButtonClickArea = 8;
#endif
constexpr int kHeaderIconX = scale(8);

constexpr int kCardMargin = 4;
constexpr int kCardWidth =
    (SCREEN_WIDTH > SCREEN_HEIGHT)
        ? (SCREEN_HEIGHT - (kCardMargin * 2))
        : (SCREEN_WIDTH - (kCardMargin * 2));
constexpr int kCardHeight = SCREEN_HEIGHT - (kCardMargin * 2);
constexpr int kCardPad = scale(20);
constexpr int kContentWidth = kCardWidth - (kCardPad * 2);

#if defined(DEVICE_LAYOUT_1024X600)
constexpr int kExtraHeight = 0;
constexpr int kCompactHeightLift = 0;
#else
constexpr int kDesignCardHeight = 720 - (kCardMargin * 2);
constexpr int kExtraHeight = (kCardHeight > kDesignCardHeight)
                                 ? (kCardHeight - kDesignCardHeight)
                                 : 0;
constexpr int kCompactHeightLift = (kExtraHeight == 0) ? 1 : 0;
#endif

constexpr int kExtraGapBase = kExtraHeight / 2;
constexpr int kExtraGapRemainder = kExtraHeight % 2;

constexpr int extra_gap(int index) {
  return kExtraGapBase + ((index >= 0 && index < kExtraGapRemainder) ? 1 : 0);
}

constexpr int extra_gap_before_value() {
  return 0;
}

constexpr int extra_gap_before_body() {
  return extra_gap(0) / 2;
}

constexpr int kHeaderY = 0;
#if defined(DEVICE_LAYOUT_1024X600)
constexpr int kCompactValueLiftY = 0;
constexpr int kCompactBodyLiftY = 0;
constexpr int kLargeValueTextOffsetY = 0;
constexpr int kHeaderHeight = scale(96);
constexpr int kValueBaseY = scale(98);
constexpr int kValueHeight = scale(74);
constexpr int kBodyBaseY = scale(178);
// Leave a visible buffer above the bottom navigation on the 600 px layout.
constexpr int kBodyHeight = contentScale(414);
constexpr int kNavHeight = scale(92);
constexpr int kNavBottomInset = scale(6);
#else
constexpr int kCompactValueLiftY = kCompactHeightLift * 24;
constexpr int kCompactBodyLiftY = kCompactHeightLift * 28;
constexpr int kLargeValueTextOffsetY = kCompactHeightLift * 6;
constexpr int kHeaderHeight = 96;
constexpr int kValueBaseY = 104 - kCompactValueLiftY;
constexpr int kValueHeight = 74;
constexpr int kBodyBaseY = 186 - kCompactBodyLiftY;
constexpr int kBodyHeight = 408;
constexpr int kNavHeight = 92;
constexpr int kNavBottomInset = 6;
#endif
constexpr int kValueY = kValueBaseY + extra_gap_before_value();
constexpr int kBodyY = kBodyBaseY + extra_gap_before_body();
constexpr int kNavY = kCardHeight - kNavBottomInset - kNavHeight;

}  // namespace popup_layout

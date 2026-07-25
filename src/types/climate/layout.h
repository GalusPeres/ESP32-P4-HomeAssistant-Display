#pragma once

#include "src/devices/device_select.h"

namespace climate_layout {

// One shared mini-grid rectangle for every climate tile size.
#if defined(DEVICE_LAYOUT_480X480)
inline constexpr int kCardPaddingHorizontal = 13;
inline constexpr int kCardPaddingVertical = 16;
inline constexpr int kOuterInset = 4;
inline constexpr int kGap = 7;
// The 15 px outer card radius minus the 4 px inset keeps both arcs concentric.
inline constexpr int kControlRadius = 11;
inline constexpr int kContentTop = 46;
#elif defined(DEVICE_LAYOUT_1024X600)
inline constexpr int kCardPaddingHorizontal = 20;
inline constexpr int kCardPaddingVertical = 24;
inline constexpr int kOuterInset = 5;
inline constexpr int kGap = 8;
// Outer card radius 22 minus the 5 px inset keeps both corner arcs concentric.
inline constexpr int kControlRadius = 17;
inline constexpr int kContentTop = 69;
#else
inline constexpr int kCardPaddingHorizontal = 20;
inline constexpr int kCardPaddingVertical = 24;
inline constexpr int kOuterInset = 6;
inline constexpr int kGap = 10;
inline constexpr int kControlRadius = 16;
inline constexpr int kContentTop = 69;
#endif

inline constexpr int kContentTopInPaddedCard =
    kContentTop - kCardPaddingVertical;

}  // namespace climate_layout

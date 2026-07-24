#pragma once

#include "src/devices/device_select.h"

namespace climate_layout {

// One shared mini-grid rectangle for every climate tile size.
inline constexpr int kCardPaddingHorizontal = 20;
inline constexpr int kCardPaddingVertical = 24;
#if defined(DEVICE_LAYOUT_1024X600)
inline constexpr int kOuterInset = 5;
inline constexpr int kGap = 8;
// Outer card radius 22 minus the 5 px inset keeps both corner arcs concentric.
inline constexpr int kControlRadius = 17;
#else
inline constexpr int kOuterInset = 6;
inline constexpr int kGap = 10;
inline constexpr int kControlRadius = 16;
#endif
inline constexpr int kContentTop = 69;

inline constexpr int kContentTopInPaddedCard =
    kContentTop - kCardPaddingVertical;

}  // namespace climate_layout

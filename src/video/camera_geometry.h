#pragma once

#include <stdint.h>

#include "src/ui/popup_layout.h"

namespace camera_geometry {

constexpr uint16_t evenCeil(uint32_t numerator, uint32_t denominator) {
  const uint32_t rounded = (numerator + denominator - 1U) / denominator;
  return static_cast<uint16_t>((rounded + 1U) & ~1U);
}

// The camera frame always fills the popup width. A 16:9 height is rounded up
// to an even value so FFmpeg can produce yuv420 JPEG frames without padding
// the visible image differently on each display profile.
inline constexpr uint16_t kWidth =
    static_cast<uint16_t>(popup_layout::kContentWidth & ~1);
inline constexpr uint16_t kHeight = evenCeil(
    static_cast<uint32_t>(kWidth) * 9U, 16U);
// 10 FPS is the measured stable operating point on the Waveshare 8": a
// 3227-frame run kept MQTT/WiFi and DMA headroom healthy, while 15 FPS drove
// the shared internal DMA heap below the safety floor after 191 frames.
inline constexpr uint8_t kFps = 10;

// ESP32-P4's JPEG hardware decoder writes in 16-pixel-aligned dimensions.
// LVGL still receives the visible width/height and the aligned row stride.
inline constexpr uint16_t kDecodedWidth = (kWidth + 15U) & ~15U;
inline constexpr uint16_t kDecodedHeight = (kHeight + 15U) & ~15U;

static_assert(kWidth >= 320 && kWidth <= 752,
              "Camera popup width is outside the supported P4 range");
static_assert(kHeight >= 180 && kHeight <= 424,
              "Camera popup height is outside the supported P4 range");

}  // namespace camera_geometry

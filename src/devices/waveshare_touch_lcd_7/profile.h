#pragma once

#include "src/devices/device_types.h"

namespace DeviceWaveshareTouchLCD7Profile {

inline constexpr Device::Profile kProfile{
    "waveshare_touch_lcd_7",
    "Waveshare Touch LCD 7",
    1280,
    720,
    7,
    4,
    16,
    4,
    168,
    166,
    4,
    121,
    Device::RotationStepMode::FlipOnly,
    0,
    2,
    Device::Capabilities{false, false, false, false, true, false},
};

}  // namespace DeviceWaveshareTouchLCD7Profile

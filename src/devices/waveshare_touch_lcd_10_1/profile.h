#pragma once

#include "src/devices/device_types.h"
#include "src/devices/waveshare_touch_lcd_10_1/hardware_io_profile.h"

namespace DeviceWaveshareTouchLCD10Profile {

inline constexpr Device::Profile kProfile{
    "waveshare_touch_lcd_10_1",
    "Waveshare Touch LCD 10.1",
    1280,
    800,
    7,
    5,
    16,
    4,
    168,
    145,
    5,
    121,
    Device::RotationStepMode::FlipOnly,
    0,
    2,
    Device::Capabilities{false, false, false, false, true, false},
    kHardwareIoProfile,
};

}  // namespace DeviceWaveshareTouchLCD10Profile

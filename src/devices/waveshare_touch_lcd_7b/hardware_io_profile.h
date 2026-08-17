#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceWaveshareTouchLCD7BProfile {

// The 7B/7B-C board documentation does not identify any connector GPIOs as
// general-purpose HomeTiles I/O. Keep every panel, touch, backlight and SDMMC
// signal reserved rather than exposing an unverified pin.
inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::kNoHardwareIoProfile;

}  // namespace DeviceWaveshareTouchLCD7BProfile

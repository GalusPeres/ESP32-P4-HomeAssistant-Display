#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceWaveshareS3TouchLCD4B {

// The first hardware bring-up deliberately exposes no configurable GPIOs.
// Every documented pin is currently assigned to the RGB panel, I2C bus,
// backlight, audio, or another onboard peripheral.
inline constexpr Device::HardwareIoProfile kHardwareIoProfile{};

}  // namespace DeviceWaveshareS3TouchLCD4B

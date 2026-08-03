#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceGuitionJC8012P4A1 {

// Verified free signals on the internal FPC3/FPC4 expansion connectors.
// Display, audio, touch, backlight, SD and boot/UART signals stay reserved.
inline constexpr Device::HardwareIoPinOption kHardwareIoPins[] = {
    {2, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 2"},
    {3, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 3"},
    {4, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 4"},
    {5, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 5"},
    {28, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 28"},
    {29, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 29"},
    {30, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 30"},
    {31, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 31"},
    {32, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 32"},
    {33, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 33"},
    {34, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 34"},
    {45, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 45"},
    {46, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 46"},
    {47, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 47"},
    {48, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expansion FPC GPIO 48"},
};

inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::makeHardwareIoProfile(kHardwareIoPins);

}  // namespace DeviceGuitionJC8012P4A1

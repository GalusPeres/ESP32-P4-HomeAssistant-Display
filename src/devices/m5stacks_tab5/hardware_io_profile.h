#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceM5StacksTab5 {

inline constexpr Device::HardwareIoPinOption kHardwareIoPins[] = {
    {0, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO EXT 0"},
    {1, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO EXT 1"},
    {49, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO EXT 49"},
    {50, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO EXT 50"},
    {53, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Port A SDA (GPIO 53)"},
    {54, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Port A SCL (GPIO 54)"},
    {2, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 2"},
    {3, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 3"},
    {4, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 4"},
    {16, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 16"},
    {17, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 17"},
    {45, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 45"},
    {47, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 47"},
    {48, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 48"},
    {51, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 51"},
    {52, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "M5-Bus GPIO 52"},
};

inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::makeHardwareIoProfile(kHardwareIoPins);

}  // namespace DeviceM5StacksTab5

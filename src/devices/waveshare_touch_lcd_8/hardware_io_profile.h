#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceWaveshareTouchLCD8 {

inline constexpr Device::HardwareIoPinOption kHardwareIoPins[] = {
    {2, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 2"},
    {3, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 3"},
    {4, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 4"},
    {5, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 5"},
    {21, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 21"},
    {22, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 22"},
    {28, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 28"},
    {29, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 29"},
    {30, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 30"},
    {31, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 31"},
    {32, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 32"},
    {34, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 34"},
    {46, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 46"},
    {47, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 47"},
    {48, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 48"},
    {49, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 49"},
    {50, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 50"},
    {51, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 51"},
    {52, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 52"},
};

inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::makeHardwareIoProfile(kHardwareIoPins);

}  // namespace DeviceWaveshareTouchLCD8

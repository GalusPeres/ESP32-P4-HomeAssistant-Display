#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceGuitionJC1060P470CV2 {

// Pins exposed by the internal 2x13 "Expand IO" header. Signals already used
// by the panel, audio, touch, camera, SD or ESP-Hosted are deliberately absent.
inline constexpr Device::HardwareIoPinOption kHardwareIoPins[] = {
    {1, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 1"},
    {2, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 2"},
    {3, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 3"},
    {4, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 4"},
    {20, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 20"},
    {32, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 32"},
    {33, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 33"},
    {46, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 46"},
    {47, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "Expand GPIO 47"},
};

inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::makeHardwareIoProfile(kHardwareIoPins);

}  // namespace DeviceGuitionJC1060P470CV2

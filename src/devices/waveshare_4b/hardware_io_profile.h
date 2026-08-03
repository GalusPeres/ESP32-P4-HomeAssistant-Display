#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceWaveshare4B {

inline constexpr Device::HardwareIoPinOption kHardwareIoPins[] = {
    {2, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 2 (P3)"},
    {3, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 3 (P3)"},
    {4, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 4 (P3)"},
    {5, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 5 (P3)"},
    {21, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinTemperature,
     "GPIO 21 (P3)"},
    {32, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinOnboard,
     "Relay 1 (86 Panel, GPIO 32)", "waveshare_86_2ro",
     Device::HardwareIoOutputLogic::ActiveHigh},
    {46, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinOnboard,
     "Relay 2 (86 Panel, GPIO 46)", "waveshare_86_2ro",
     Device::HardwareIoOutputLogic::ActiveHigh},
};

inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::makeHardwareIoProfile(kHardwareIoPins);

}  // namespace DeviceWaveshare4B

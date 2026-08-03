#pragma once

#include "src/devices/hardware_io_profile.h"

namespace DeviceGuitionESP324848S040 {

// GPIO 40 is Relay 1 on both verified relay-equipped variants. GPIO 1/2 are
// intentionally omitted because they are audio signals on other variants.
inline constexpr Device::HardwareIoPinOption kHardwareIoPins[] = {
    {40, Device::kHardwareIoPinSwitch | Device::kHardwareIoPinOnboard,
     "Relay 1 (relay variant, GPIO 40)", "guition_4848s040_relay",
     Device::HardwareIoOutputLogic::ActiveHigh},
};

inline constexpr Device::HardwareIoProfile kHardwareIoProfile =
    Device::makeHardwareIoProfile(kHardwareIoPins);

}  // namespace DeviceGuitionESP324848S040

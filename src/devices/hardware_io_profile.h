#pragma once

#include <Arduino.h>

namespace Device {

enum HardwareIoPinCapability : uint8_t {
  kHardwareIoPinSwitch = 1U << 0,
  kHardwareIoPinTemperature = 1U << 1,
  kHardwareIoPinOnboard = 1U << 2,
};

enum class HardwareIoOutputLogic : uint8_t {
  Configurable = 0,
  ActiveHigh = 1,
  ActiveLow = 2,
};

struct HardwareIoPinOption {
  int8_t gpio;
  uint8_t capabilities;
  const char* label;
  const char* required_variant = nullptr;
  HardwareIoOutputLogic output_logic = HardwareIoOutputLogic::Configurable;
};

struct HardwareIoProfile {
  const HardwareIoPinOption* pin_options = nullptr;
  uint8_t pin_count = 0;
  const char* default_variant = "standard";
};

inline constexpr HardwareIoProfile kNoHardwareIoProfile{};

template <size_t N>
constexpr HardwareIoProfile makeHardwareIoProfile(
    const HardwareIoPinOption (&pin_options)[N],
    const char* default_variant = "standard") {
  static_assert(N <= UINT8_MAX, "Hardware I/O profile has too many pins");
  return HardwareIoProfile{pin_options, static_cast<uint8_t>(N),
                           default_variant};
}

}  // namespace Device

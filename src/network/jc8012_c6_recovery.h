#pragma once

#include <stdint.h>

namespace Jc8012C6Recovery {

enum class Result : uint8_t {
  NoAction,
  RestartRequired,
  Blocked,
};

// Handles the deliberately embedded ESP32-C6 recovery payload used by the
// isolated JC8012P4A1 V1 Issue-30 beta. Other device/build combinations are
// compiled as a no-op.
Result runIfPresent();

// Remains true after an uncertain recovery failure so normal networking and
// configuration mode cannot issue more RPCs to a potentially partial C6.
bool isBlocked();

constexpr uint32_t kCoprocessorRestartWaitMs = 7000;

}  // namespace Jc8012C6Recovery

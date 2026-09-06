#pragma once

#include <stdint.h>
#include <string.h>

namespace hometiles_view {
// Signed subtraction keeps expiry checks valid across the millis() wrap.
inline bool freshDeadline(uint32_t now, uint32_t deadline) {
  const int32_t remaining = static_cast<int32_t>(deadline - now);
  return remaining > 0 && remaining <= 10000;
}

struct CommandGate {
  uint32_t last_sequence = 0;
  bool accept(const char* session, const char* expected, uint32_t sequence,
              uint32_t now, uint32_t deadline) {
    if (!session || !expected || !*expected || strcmp(session, expected) != 0 ||
        sequence <= last_sequence || !freshDeadline(now, deadline)) return false;
    last_sequence = sequence;
    return true;
  }
};
}  // namespace hometiles_view

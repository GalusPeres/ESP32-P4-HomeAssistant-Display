#pragma once

#include <stddef.h>

#include <esp_heap_caps.h>

namespace psram_budget {

// Scale optional feature budgets with the installed PSRAM instead of assuming
// every target has the 32 MB available on the ESP32-P4 boards. The cap keeps
// the established P4 behaviour unchanged, while total/divisor leaves the same
// proportional safety margin on smaller targets such as the 8 MB ESP32-S3.
inline size_t fractionCapped(size_t cap_bytes, size_t divisor = 4U) {
  if (divisor == 0U) return cap_bytes;
  const size_t total =
      heap_caps_get_total_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (total == 0U) return cap_bytes;
  const size_t proportional = total / divisor;
  return proportional < cap_bytes ? proportional : cap_bytes;
}

}  // namespace psram_budget

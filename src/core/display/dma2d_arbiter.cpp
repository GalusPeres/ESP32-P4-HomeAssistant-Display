#include "src/core/display/dma2d_arbiter.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace dma2d_arbiter {
namespace {

SemaphoreHandle_t arbiter_mutex() {
  // Thread-safe local static initialization. The first call occurs during
  // display initialization, before the cover worker task exists.
  static SemaphoreHandle_t handle = xSemaphoreCreateMutex();
  return handle;
}

}  // namespace

bool lock(uint32_t timeout_ms) {
  SemaphoreHandle_t handle = arbiter_mutex();
  if (!handle) {
    return false;
  }
  return xSemaphoreTake(handle, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void unlock() {
  SemaphoreHandle_t handle = arbiter_mutex();
  if (handle) {
    xSemaphoreGive(handle);
  }
}

}  // namespace dma2d_arbiter

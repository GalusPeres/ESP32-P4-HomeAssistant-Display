#include "./lvgl_tick_service.h"

#include <Arduino.h>
#include <lvgl.h>

uint32_t g_lvgl_tick_last_ms = 0;

// Minimum interval between actual service passes. Rendering faster than
// the display can present wastes work: a tight parseEnergySection loop
// (19 entries with less than 1 ms parsing each) spent 811 ms rendering.
// Each previous render already exceeded the frame period, making the
// animation due again. A 15 ms interval supports about 60 FPS. Skipped
// calls lose no elapsed time: only actual service passes advance the
// shared timestamp, and the next service or main-loop tick counts the rest.
static constexpr uint32_t kMinServiceIntervalMs = 15;

void lvglServiceDuringBlockingWork() {
  uint32_t now = millis();
  uint32_t elapsed = now - g_lvgl_tick_last_ms;
  if (elapsed < kMinServiceIntervalMs) {
    yield();  // Give other ready tasks a scheduling opportunity on every call.
    return;
  }
  lv_tick_inc(elapsed);
  g_lvgl_tick_last_ms = now;
  yield();
  lv_timer_handler();
  yield();
}

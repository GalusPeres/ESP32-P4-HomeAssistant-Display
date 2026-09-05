#ifndef LVGL_TICK_SERVICE_H
#define LVGL_TICK_SERVICE_H

#include <cstdint>

// Single source of truth for "when did LVGL's tick last get advanced".
// Shared between the top-of-loop() lv_tick_inc() call in the .ino and
// lvglServiceDuringBlockingWork() below -- both feed the same LVGL tick, so
// there must only be one "last" timestamp or time gets double-counted or
// lost between the two call sites.
extern uint32_t g_lvgl_tick_last_ms;

// Advances LVGL's tick counter with the real elapsed wall-clock time and then
// runs lv_timer_handler(). lv_tick_inc() is normally only called once, at the
// very top of loop() (see the .ino). If a long synchronous operation (flash
// reads, MQTT (un)subscribe, JSON parsing, ...) calls lv_timer_handler()
// internally to keep the UI breathing, LVGL still thinks zero time has passed
// on every one of those nested calls, so periodic timers such as the
// pixel-animation tile never get to run early: the whole blocked duration lands
// as one lump lv_tick_inc() jump once loop() returns to its own top, and the
// timer catches up in a single step. Call this instead of a bare
// lv_timer_handler() anywhere inside such a loop so the animation, and
// everything else timer-driven, keeps advancing smoothly while the surrounding
// work is still in progress.
void lvglServiceDuringBlockingWork();

#endif // LVGL_TICK_SERVICE_H

#pragma once

#include <lvgl.h>

// Reusable on-screen keyboard matching the app: dark-gray letter keys,
// distinct control keys and a green OK key. Used by settings pages with
// text input, including Wi-Fi and MQTT.
lv_obj_t* ui_keyboard_create(lv_obj_t* parent);

// Set the keyboard's target textarea and move focus (cursor and outline)
// to it. prev may be nullptr.
void ui_keyboard_set_target(lv_obj_t* kb, lv_obj_t* ta, lv_obj_t* prev);

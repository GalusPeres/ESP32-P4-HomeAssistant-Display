/*
 * SPDX-FileCopyrightText: 2026 HomeTiles contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void* esp_lcd_touch_handle_t;

esp_lcd_touch_handle_t touch_gsl3680_init();
bool esp_lcd_touch_read_data(esp_lcd_touch_handle_t handle);
bool esp_lcd_touch_get_coordinates(esp_lcd_touch_handle_t handle,
                                   uint16_t* x,
                                   uint16_t* y,
                                   uint16_t* strength,
                                   uint8_t* track_ids,
                                   uint8_t* count,
                                   uint8_t max_points);

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "freertos/portmacro.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef SemaphoreHandle_t esp_h264_mutex_t;
#define ESP_H264_MAX_DELAY                                 portMAX_DELAY
#define esp_h264_mutex_lock(mutex, blocktime)              xSemaphoreTake(mutex, blocktime)
#define esp_h264_mutex_unlock_from_isr(mutex, task_woken)  xSemaphoreGiveFromISR(mutex, task_woken)
#define esp_h264_mutex_unlock(mutex)                       xSemaphoreGive(mutex)
#define esp_h264_mutex_create                              xSemaphoreCreateBinary
#define esp_h264_mutex_delete(mutex)                       vSemaphoreDelete(mutex)
#define esp_h264_port_yield_from_isr()                     portYIELD_FROM_ISR()

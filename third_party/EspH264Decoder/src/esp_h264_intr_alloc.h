/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_intr_alloc.h"

typedef intr_handle_t esp_h264_intr_hd_t;

#define ETS_INTERNAL_H264_INTR_SOURCE                         (0x7E)
#define esp_h264_intr_alloc(flags, handler, arg, ret_handle)  esp_intr_alloc(ETS_INTERNAL_H264_INTR_SOURCE, flags, handler, arg, ret_handle)
#define esp_h264_intr_free(handler)                           esp_intr_free(handler)

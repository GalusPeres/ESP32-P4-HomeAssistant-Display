/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_cache.h"
#include "esp_h264_cache.h"

void esp_h264_cache_check_and_writeback(uint8_t *addr, uint32_t length)
{
    esp_cache_msync(addr, length, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
}

void esp_h264_cache_check_and_invalidate(uint8_t *addr, uint32_t length)
{
    esp_cache_msync(addr, length, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

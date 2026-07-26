/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>

/**
 * @brief  Check the address is in cache or not. If it is in cache, it will be wrote back.
 *
 * @param[in]  addr    The check address
 * @param[in]  length  The length of `addr`
 */
void esp_h264_cache_check_and_writeback(uint8_t *addr, uint32_t length);

/**
 * @brief  Check the address is in cache or not. If it is in cache, it will be invalided.
 *
 * @param[in]  addr    The check address
 * @param[in]  length  The length of `addr`
 */
void esp_h264_cache_check_and_invalidate(uint8_t *addr, uint32_t length);

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_heap_caps.h"

#define ESP_H264_MEM_INTERNAL MALLOC_CAP_INTERNAL
#define ESP_H264_MEM_SPIRAM   MALLOC_CAP_SPIRAM
#define ALIGN_UP(num, align)  ((align) ? (((num) + ((align) - 1)) & ~((align) - 1)) : (num))

/**
 * @brief  Free memory previously allocated
 */
#define esp_h264_free           heap_caps_free

/**
 * @brief  Allocate an aligned chunk of memory which has the given capabilities.
 *
 * @param[in]  alignment  How the pointer received needs to be aligned must be a power of two.
 *                        If the value is less than cache line size, the value will be forced cache line size.
 * @param[in]  n          Number of continuing chunks of memory to allocate
 * @param[in]  size       Size, in bytes, of a chunk of memory to allocate
 * @param[in]  caps       ESP_H264_MEM_INTERNAL or ESP_H264_MEM_SPIRAM
 *
 * @return
 *       - NULL    Failure
 *       - others  A pointer to the memory allocated on success
 */
void *esp_h264_aligned_calloc(uint32_t alignment, uint32_t n, uint32_t size, uint32_t *actual_size, uint32_t caps);

/**
 * @brief  Allocate a chunk of memory as preference in decreasing order. And helper function for calloc a cache aligned data memory buffer
 *
 * @param[in]  n      Number of continuing chunks of memory to allocate
 * @param[in]  size   Size, in bytes, of a chunk of memory to allocate
 * @param[in]  caps1  ESP_H264_MEM_INTERNAL or ESP_H264_MEM_SPIRAM
 * @param[in]  caps2  ESP_H264_MEM_INTERNAL or ESP_H264_MEM_SPIRAM
 *
 * @return
 *       - NULL    Failure
 *       - others  A pointer to the memory allocated on success
 */
void *esp_h264_calloc_prefer(uint32_t n, uint32_t size, uint32_t *actual_size, uint32_t caps1, uint32_t caps2);

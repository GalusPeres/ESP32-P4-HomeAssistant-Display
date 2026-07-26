/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_h264_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  It is a pointer to the H.264 decoding parameters structure
 */
typedef struct esp_h264_dec_param_if *esp_h264_dec_param_handle_t;

/**
 * @brief  The H.264 decoder parameter interface
 */
typedef struct esp_h264_dec_param_if {
    esp_h264_err_t (*get_res)(esp_h264_dec_param_handle_t handle, esp_h264_resolution_t *res); /*<! Get resolution function */
} esp_h264_dec_param_t;

/**
 * @brief  This function retrieves the resolution of the video decoder specified by the handle parameter
 *         The decoder's resolution is stored in the `res`, which is of type `esp_h264_resolution_t`
 *
 * @param[in]   handle   It is a pointer to the H.264 decoding parameters structure
 * @param[out]  out_res  A pointer to the resolution structure where the decoder's resolution will be stored
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments
 *       - ESP_H264_ERR_UNSUPPORTED  Get resolution feature is not supported by the decoder
 */
esp_h264_err_t esp_h264_dec_get_resolution(esp_h264_dec_param_handle_t handle, esp_h264_resolution_t *out_res);

#ifdef __cplusplus
}
#endif

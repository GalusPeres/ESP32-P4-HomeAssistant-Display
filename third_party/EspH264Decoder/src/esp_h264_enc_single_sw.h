/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_h264_enc_single.h"
#include "esp_h264_enc_param.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Handle type for software-based H.264 encoder parameters
 */
typedef esp_h264_enc_param_handle_t esp_h264_enc_param_sw_handle_t;

/**
 * @brief Configuration type for software-based H.264 encoder
 */
typedef esp_h264_enc_cfg_t esp_h264_enc_cfg_sw_t;


/**
 * @brief  This function is used to create a new instance of the `esp_h264_enc_t` data structure,
 *         which represents a single-streams H.264 encoder in software. The encoder is from openh264
 *
 * @note  The group of picture(GOP) will be updated in intra frame
 *
 * @param[in]   cfg      It is a pointer to the `esp_h264_enc_cfg_sw_t` structure, which contains the configuration settings for the encoder
 * @param[out]  out_enc  It is a double pointer to the `esp_h264_enc_t` structure, which will store the created encoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK   Succeeded
 *       - ESP_H264_ERR_ARG  Invalid arguments passed
 *       - ESP_H264_ERR_MEM  Insufficient memory, the `*enc` will be set NULL
 */
esp_h264_err_t esp_h264_enc_sw_new(const esp_h264_enc_cfg_sw_t *cfg, esp_h264_enc_handle_t *out_enc);

/**
 * @brief  This function returns a pointer to the software-encoded parameter structure associated with the given `esp_h264_enc_t` encoder
 *
 * @param[in]   enc        The encoder instance that is from `esp_h264_enc_hw_new`
 * @param[out]  out_param  The parameter set handle
 *
 * @return
 *       - ESP_H264_ERR_OK   Succeeded
 *       - ESP_H264_ERR_ARG  Invalid arguments passed
 */
esp_h264_err_t esp_h264_enc_sw_get_param_hd(esp_h264_enc_handle_t enc, esp_h264_enc_param_sw_handle_t *out_param);

#ifdef __cplusplus
}
#endif

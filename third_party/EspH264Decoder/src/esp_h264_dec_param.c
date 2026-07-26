/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_h264_dec_param.h"
#include "esp_h264_check.h"

static const char *TAG = "H264_DEC.IF.PARAM";

esp_h264_err_t esp_h264_dec_get_resolution(esp_h264_dec_param_handle_t handle, esp_h264_resolution_t *out_res)
{
    ESP_H264_RET_ON_FALSE(handle, ESP_H264_ERR_ARG, TAG, "Invalid h264 parameter");
    ESP_H264_RET_ON_FALSE(out_res, ESP_H264_ERR_ARG, TAG, "The out resolution pointer is NULL");
    ESP_H264_RET_ON_FALSE(handle->get_res, ESP_H264_ERR_UNSUPPORTED, TAG, "`get_res` is not supported yet");
    return handle->get_res(handle, out_res);
}

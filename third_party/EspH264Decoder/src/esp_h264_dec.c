/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_h264_dec.h"
#include "esp_h264_check.h"

static const char *TAG = "H264_DEC.IF";

esp_h264_err_t esp_h264_dec_open(esp_h264_dec_handle_t dec)
{
    ESP_H264_RET_ON_FALSE(dec, ESP_H264_ERR_ARG, TAG, "Invalid h264 handle");
    ESP_H264_RET_ON_FALSE(dec->open, ESP_H264_ERR_UNSUPPORTED, TAG, "Open function is not supported yet");
    return dec->open(dec);
}

esp_h264_err_t esp_h264_dec_process(esp_h264_dec_handle_t dec, esp_h264_dec_in_frame_t *in_frame, esp_h264_dec_out_frame_t *out_frame)
{
    ESP_H264_RET_ON_FALSE(dec && in_frame && out_frame, ESP_H264_ERR_ARG, TAG, "Invalid h264 handle");
    ESP_H264_RET_ON_FALSE(in_frame->raw_data.buffer, ESP_H264_ERR_ARG, TAG, "The buffer pointer of input frame is NULL.");
    ESP_H264_RET_ON_FALSE(dec->process, ESP_H264_ERR_UNSUPPORTED, TAG, "Process function is not supported yet");
    return dec->process(dec, in_frame, out_frame);
}

esp_h264_err_t esp_h264_dec_close(esp_h264_dec_handle_t dec)
{
    ESP_H264_RET_ON_FALSE(dec, ESP_H264_ERR_ARG, TAG, "Invalid h264 handle");
    ESP_H264_RET_ON_FALSE(dec->close, ESP_H264_ERR_UNSUPPORTED, TAG, "Close function is not supported yet");
    return dec->close(dec);
}

esp_h264_err_t esp_h264_dec_del(esp_h264_dec_handle_t dec)
{
    ESP_H264_RET_ON_FALSE(dec, ESP_H264_ERR_ARG, TAG, "Invalid h264 handle");
    ESP_H264_RET_ON_FALSE(dec->del, ESP_H264_ERR_UNSUPPORTED, TAG, "Delete function is not supported yet");
    return dec->del(dec);
}

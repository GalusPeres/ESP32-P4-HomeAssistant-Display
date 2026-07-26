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
 * @brief  H.264 dual streams encoder handle
 */
typedef struct esp_h264_enc_dual_if *esp_h264_enc_dual_handle_t;

/**
 * @brief  H.264 dual streams encoder handle
 */
typedef struct esp_h264_enc_dual_if {
    esp_h264_err_t (*open)(esp_h264_enc_dual_handle_t enc);                                          /*<! The open function */
    esp_h264_err_t (*process)(esp_h264_enc_dual_handle_t enc, esp_h264_enc_in_frame_t *in_frame[2],
                              esp_h264_enc_out_frame_t *out_frame[2]);                               /*<! The process function */
    esp_h264_err_t (*close)(esp_h264_enc_dual_handle_t enc);                                         /*<! The close function */
    esp_h264_err_t (*del)(esp_h264_enc_dual_handle_t enc);                                           /*<! The delete function */
} esp_h264_enc_dual_t;

/**
 * @brief  This function opens an H.264 encoder in dual streams
 *
 * @param[in]  enc  A pointer to the H.264 dual encoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_FAIL         Failed
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Open feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_dual_open(esp_h264_enc_dual_handle_t enc);

/**
 * @brief  This function performs dual encoding of H.264 video frames.
 *         The encoder supports dual channels where each channel can have a different configuration.
 *         It allows for one image encoder per channel and cannot code multiple images consecutively per channel.
 *         For IDR frame, the encoder will automatically add SPS and PPS NALU.
 *
 * @note  The function will return ESP_H264_ERR_TIMEOUT, if `out_frame.raw_data.len` is less than actual encoded data length using hardware encoder.
 *        If the width or height of image is not multi of 16, please do the follow operation.
 *        `width = ((width +15) >> 4 << 4);`
 *        `height = ((height+15) >> 4 << 4);`
 *        `in_frame.raw_data.len = ( width * height + (width * height >> 1));`
 *        `in_frame.raw_data.buffer = heap_caps_aligned_calloc(16, 1, in_frame.raw_data.len, &in_frame.raw_data.len, MALLOC_CAP_DEFAULT);`
 *        The `out_frame.raw_data.buffer` should be allocated by the user for in_frame.raw_data.len bytes to avoid the encoded image size exceeding the `out_frame.raw_data.buffer` size.
 *        If the encoder image size is larger than `out_frame.raw_data.buffer`, it will result in ESP_H264_ERR_MEM.
 *
 * @param[in]      enc        A pointer to the H.264 dual encoder instance
 * @param[in]      in_frame   An array of two pointers to unencoded input frames
 * @param[in/out]  out_frame  An array of two pointers to encoded output frames
 *
 * @return
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_MEM          Insufficient memory
 *       - ESP_H264_ERR_FAIL         Failed
 *       - ESP_H264_ERR_TIMEOUT      Timeout
 *       - ESP_H264_ERR_OVERFLOW     The size of encoder image is greater than `in_frame.raw_data.len`
 *       - ESP_H264_ERR_UNSUPPORTED  Process feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_dual_process(esp_h264_enc_dual_handle_t enc, esp_h264_enc_in_frame_t *in_frame[2], esp_h264_enc_out_frame_t *out_frame[2]);

/**
 * @brief  This function closes the H.264 dual encoder instance specified by `enc`
 *
 * @param[in]  enc  A pointer to the H.264 dual encoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Close feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_dual_close(esp_h264_enc_dual_handle_t enc);

/**
 * @brief  This function is used to delete an H.264 dual encoder
 *
 * @param[in]  enc  A pointer to the H.264 dual encoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed.
 *       - ESP_H264_ERR_UNSUPPORTED  Delete feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_dual_del(esp_h264_enc_dual_handle_t enc);

#ifdef __cplusplus
}
#endif

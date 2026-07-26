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
 * @brief  Decoder configure information
 */
typedef struct {
    esp_h264_raw_format_t pic_type;  /*<! Data format after decoding */
} esp_h264_dec_cfg_t;

/**
 * @brief  H.264 stream decoder handle
 */
typedef struct esp_h264_dec_if *esp_h264_dec_handle_t;

/**
 * @brief  The H.264 stream decoder interface
 */
typedef struct esp_h264_dec_if {
    esp_h264_err_t (*open)(esp_h264_dec_handle_t dec);                                       /*<! The open function */
    esp_h264_err_t (*process)(esp_h264_dec_handle_t dec, esp_h264_dec_in_frame_t *in_frame,
                              esp_h264_dec_out_frame_t *out_frame);                          /*<! The process function */
    esp_h264_err_t (*close)(esp_h264_dec_handle_t dec);                                      /*<! The close function */
    esp_h264_err_t (*del)(esp_h264_dec_handle_t dec);                                        /*<! The delete function */
} esp_h264_dec_t;

/**
 * @brief  This function opens an H.264 decoder in stream
 *
 * @param[in]  dec  A pointer to the H.264 decoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_FAIL         Failed
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Open feature is not supported by the decoder
 */
esp_h264_err_t esp_h264_dec_open(esp_h264_dec_handle_t dec);

/**
 * @brief  This function performs decoding of H.264 video frames.
 *
 * @param[in]      dec        A pointer to the H.264 decoder instance
 * @param[in]      in_frame   A pointer to encoded input frame
 * @param[in/out]  out_frame  A pointer to store decoded output frame.
 *                            The `out_frame->outbuf` will store an image data address after decoding.
 *                            This address will be re-used in `esp_h264_dec_process`.
 *                            Users, ensure to retrieve the data in the address promptly.
 *                            If the NALU is not related to image data like SPS, PPS, etc., `out_frame->out_size` will return 0.
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_MEM          Insufficient memory
 *       - ESP_H264_ERR_FAIL         Failed
 *       - ESP_H264_ERR_UNSUPPORTED  Process feature is not supported by the decoder
 */
esp_h264_err_t esp_h264_dec_process(esp_h264_dec_handle_t dec, esp_h264_dec_in_frame_t *in_frame, esp_h264_dec_out_frame_t *out_frame);

/**
 * @brief  This function closes the H.264 decoder instance specified by `dec`
 *
 * @param[in]  dec  A pointer to the H.264 decoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Close feature is not supported by the decoder
 */
esp_h264_err_t esp_h264_dec_close(esp_h264_dec_handle_t dec);

/**
 * @brief  This function is used to delete an H.264 decoder
 *
 * @param[in]  dec  A pointer to the H.264 decoder instance
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Delete feature is not supported by the decoder
 */
esp_h264_err_t esp_h264_dec_del(esp_h264_dec_handle_t dec);

#ifdef __cplusplus
}
#endif

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
 * @brief  It is a pointer to the H.264 encoding parameters structure
 */
typedef struct esp_h264_enc_param_if *esp_h264_enc_param_handle_t;

/**
 * @brief  The H.264 encoder parameter interface
 */
typedef struct esp_h264_enc_param_if {
    esp_h264_err_t (*get_res)(esp_h264_enc_param_handle_t handle, esp_h264_resolution_t *res);  /*<! Get resolution function */
    esp_h264_err_t (*set_fps)(esp_h264_enc_param_handle_t handle, uint8_t fps);                 /*<! Set frames per second(FPS) function */
    esp_h264_err_t (*get_fps)(esp_h264_enc_param_handle_t handle, uint8_t *fps);                /*<! Get frames per second(FPS) function */
    esp_h264_err_t (*set_gop)(esp_h264_enc_param_handle_t handle, uint8_t gop);                 /*<! Set group of picture(GOP) function */
    esp_h264_err_t (*get_gop)(esp_h264_enc_param_handle_t handle, uint8_t *gop);                /*<! Get group of picture(GOP) function */
    esp_h264_err_t (*set_bitrate)(esp_h264_enc_param_handle_t handle, uint32_t bitrate);        /*<! Set bitrate function */
    esp_h264_err_t (*get_bitrate)(esp_h264_enc_param_handle_t handle, uint32_t *bitrate);       /*<! Get bitrate function */
} esp_h264_enc_param_t;

/**
 * @brief  This function retrieves the resolution of the video encoder specified by the handle parameter
 *         The encoder's resolution is stored in the `res`, which is of type esp_h264_resolution_t
 *
 * @param[in]   handle   It is a pointer to the H.264 encoding parameters structure
 * @param[out]  out_res  A pointer to the resolution structure where the encoder's resolution will be stored
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Get resolution feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_get_resolution(esp_h264_enc_param_handle_t handle, esp_h264_resolution_t *out_res);

/**
 * @brief  This function sets the frames per second (FPS) parameter for the H.264 encoder
 *
 * @note  The higher FPS, the more coherent and realistic the video.
 *        When FPS is greater than 24, the general video seems coherent.
 *        When FPS is greater than 30, the game video seems coherent.
 *        When FPS is greater than 75, increase FPS, the video fluency improvement isn't obvious.
 *        Ensure the `fps` value is within the range of 1 to 255.
 *        This function may return ESP_H264_ERR_ARG if `fps` value is out of range.
 *
 * @param[in]  handle  It is a pointer to the H.264 encoding parameters structure
 * @param[in]  fps     The desired FPS value (a single byte)
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Set FPS feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_set_fps(esp_h264_enc_param_handle_t handle, uint8_t fps);

/**
 * @brief  This function is used to retrieve the frames per second (FPS) from the encoder
 *
 * @param[in]   handle   It is a pointer to the H.264 encoding parameters structure
 * @param[out]  out_fps  Pointer to a variable to store the retrieved FPS value
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Get FPS feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_get_fps(esp_h264_enc_param_handle_t handle, uint8_t *out_fps);

/**
 * @brief  This function sets the group of picture(GOP) parameter for the H.264 encoder
 *
 * @note  GOP is usually set to the number of frames per second output by the encoder,
 *        that is, the GOP, which is generally 25 or 30, but other values can also be set.
 *        Ensure the `gop` value is within the range of 1 to 255.
 *        This function may return ESP_H264_ERR_ARG if `gop` value is out of range.
 *
 * @param[in]  handle  It is a pointer to the H.264 encoding parameters structure.
 * @param[in]  gop     The desired GOP value (a single byte)
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Set GOP feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_set_gop(esp_h264_enc_param_handle_t handle, uint8_t gop);

/**
 * @brief  This function is used to retrieve the group of picture(GOP) from the encoder.
 *
 * @param[in]   handle   It is a pointer to the H.264 encoding parameters structure.
 * @param[out]  out_gop  Pointer to a variable to store the retrieved GOP value
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Get GOP feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_get_gop(esp_h264_enc_param_handle_t handle, uint8_t *out_gop);

/**
 * @brief  This function sets the bitrate parameter for the H.264 encoder
 *
 * @param[in]  handle   It is a pointer to the H.264 encoding parameters structure
 * @param[in]  bitrate  The desired bitrate value
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Set bitrate feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_set_bitrate(esp_h264_enc_param_handle_t handle, uint32_t bitrate);

/**
 * @brief  This function is used to retrieve the bitrate from the encoder
 *
 * @param[in]   handle       It is a pointer to the H.264 encoding parameters structure
 * @param[out]  out_bitrate  Pointer to a variable to store the retrieved bitrate value
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 *       - ESP_H264_ERR_UNSUPPORTED  Get bitrate feature is not supported by the encoder
 */
esp_h264_err_t esp_h264_enc_get_bitrate(esp_h264_enc_param_handle_t handle, uint32_t *out_bitrate);

/**
 * @brief  This function is used to retrieve the bits per pixel from the encoder
 *
 * @param[in]   handle       It is a pointer to the H.264 encoding parameters structure
 * @param[out]  out_bpp      Pointer to a variable to store the retrieved bits per pixel value
 *
 * @return
 *       - ESP_H264_ERR_OK           Succeeded
 *       - ESP_H264_ERR_ARG          Invalid arguments passed
 */
esp_h264_err_t esp_h264_enc_get_bpp(esp_h264_enc_param_handle_t handle, float *out_bpp);

#ifdef __cplusplus
}
#endif

/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_H264_QP_MAX       (51)  /*<! H264 support maximum quantization parameter (QP) */
#define ESP_H264_4CC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/**
 * @brief  The given code snippet defines a structure called `esp_h264_err_t`
 *         It is an enumeration that represents different error codes for H.264 video encoding operations
 */
typedef enum {
    ESP_H264_ERR_OK             = 0,   /*<! Succeeded */
    ESP_H264_ERR_FAIL           = -1,  /*<! Failed */
    ESP_H264_ERR_ARG            = -2,  /*<! Invalid arguments */
    ESP_H264_ERR_MEM            = -3,  /*<! Insufficient memory */
    ESP_H264_ERR_UNSUPPORTED    = -5,  /*<! Un-supported */
    ESP_H264_ERR_TIMEOUT        = -6,  /*<! Timeout */
    ESP_H264_ERR_OVERFLOW       = -7,  /*<! Buffer overflow */
} esp_h264_err_t;

/**
 * @brief  This is an unencoded data format
 *
 * @note
 *        |-------------------------------|--------------|--------------|--------------|
 *        | enum                          |  SW encoder  |  HW encoder  | SW decoder   |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_BGR888       |  un-supported|  supported   | un-supported |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_RGB565_LE    |  un-supported|  supported   | un-supported |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_VUY          |  un-supported|  supported   | un-supported |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_UYVY         |  un-supported|  supported   | un-supported |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_YUYV         |  supported   | un-supported | un-supported |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_I420         |  supported   | un-supported |  supported   |
 *        |-------------------------------|--------------|--------------|--------------|
 *        | ESP_H264_RAW_FMT_O_UYY_E_VYY  | un-supported |  supported   | un-supported |
 *        |-------------------------------|--------------|--------------|--------------|
 */
typedef enum {
    ESP_H264_RAW_FMT_BGR888      = ESP_H264_4CC('B', 'G', 'R', '3'),  /*<! BGR888 */
    ESP_H264_RAW_FMT_RGB565_LE   = ESP_H264_4CC('R', 'G', 'B', 'L'),  /*<! RGB565 little endian */
    ESP_H264_RAW_FMT_VUY         = ESP_H264_4CC('Y', '3', '0', '8'),  /*<! VUY */
    ESP_H264_RAW_FMT_UYVY        = ESP_H264_4CC('U', 'Y', 'V', 'Y'),  /*<! UYVY */
    ESP_H264_RAW_FMT_YUYV        = ESP_H264_4CC('Y', 'U', 'Y', 'V'),  /*<! The storage format is YUV422 packet. The data order is Y U Y V... for per line */
    ESP_H264_RAW_FMT_I420        = ESP_H264_4CC('Y', 'U', '1', '2'),  /*<! The storage format is YUV420 planar（IYUV). The data order is to store all Y first, then all U, and finally all V */
    ESP_H264_RAW_FMT_O_UYY_E_VYY = ESP_H264_4CC('O', 'U', 'E', 'V'),  /*<! The storage format is YUV420 packet, the data order is as follows
                                                                           |-------------|-------------------------------|
                                                                           | line number |         data order            |
                                                                           |-------------|-------------------------------|
                                                                           |   1         |         u y y u y y u y y...  |
                                                                           |-------------|-------------------------------|
                                                                           |   2         |         v y y v y y v y y...  |
                                                                           |-------------|-------------------------------|
                                                                           |   3         |         u y y u y y u y y...  |
                                                                           |-------------|-------------------------------|
                                                                           |   4         |         v y y v y y v y y...  |
                                                                           |-------------|-------------------------------|
                                                                           |   ...       |         ...                   |
                                                                           |-------------|-------------------------------|
 */
} esp_h264_raw_format_t;

/**
 * @brief  Enumerate video frame type
 */
typedef enum {
    ESP_H264_FRAME_TYPE_INVALID = -1,  /*<! Encoder not ready or parameters are invalid */
    ESP_H264_FRAME_TYPE_IDR     = 0,   /*<! Instantaneous decoding refresh (IDR) frame
                                            IDR frames are essentially I-frames and use intra-frame prediction.
                                            IDR frames refresh immediately
                                            So IDR frames assume the random access function, a new IDR frame starts,
                                            can recalculate a new GOP start encoding, the player can always play from an IDR frame,
                                            because after it, no frame references the previous frame.
                                            If there is no IDR frame in a video, the video cannot be accessed randomly. */
    ESP_H264_FRAME_TYPE_I       = 1,   /*<! Intra frame(I frame) type. If output frame type is this,
                                            it means this frame is I-frame except IDR frame. */
    ESP_H264_FRAME_TYPE_P       = 2,   /*<! Predicted frame (P-frame) type */
} esp_h264_frame_type_t;

/**
 * @brief  H.264 Data packet
 */
typedef struct {
    uint8_t *buffer;  /*<! Data buffer */
    uint32_t len;     /*<! It is buffer length in bytes */
} esp_h264_pkt_t;

/**
 * @brief  Picture resolution
 *         In the case of a certain display resolution, the smaller the display, the clearer the picture
 *         On the contrary, when the display size is fixed, the higher the display resolution, the clearer the picture
 */
typedef struct {
    uint16_t width;   /*<! Width of picture
                         |---------|-----------------|------------------|
                         |         | Software encoder| Hardware encoder |
                         |---------|-----------------|------------------|
                         | maximum |    infinite     |       1920       |
                         |---------|-----------------|------------------|
                         | minimum |     16          |        80        |
                         |---------|-----------------|------------------|
                      */
    uint16_t height;  /*<! Height of picture
                         |---------|-----------------|------------------|
                         |         | Software encoder| Hardware encoder |
                         |---------|-----------------|------------------|
                         | maximum |   infinite      |      2032        |
                         |---------|-----------------|------------------|
                         | minimum |     16          |       80         |
                         |---------|-----------------|------------------|
                     */
} esp_h264_resolution_t;

/**
 * @brief  Rate control(RC) parameter
 *         The size of the output stream approaching the target stream is controlled
 *         by using the optimal quantization parameter for each macroblock.
 */
typedef struct {
    uint32_t bitrate;  /*<! Target bitrate desired
                            Under normal circumstances, the compression rate of H.264 can exceed 100 times.
                            Therefore, the recommended value is `width` * `height` * `pixel` / `compression ratio`.
                            For network transmission, a smaller value is recommended when the network environment is poor.*/
    uint8_t  qp_min;   /*<! Minimum range for quantization parameter(QP). The range is [0, 51].
                            Smaller QP values reduce the compression ratio, increasing video quality.*/
    uint8_t  qp_max;   /*<! Maximum range for quantization parameter(QP). The range is [0, 51].
                            Larger QP values increase compression.
                            It must be greater than or equal to `qp_min`.*/
} esp_h264_enc_rc_t;

/**
 * @brief  Encoder configure information
 */
typedef struct {
    esp_h264_raw_format_t pic_type;  /*<! Un-encoding data format */
    uint8_t               gop;       /*<! Period of Intra frame
                                          GOP is usually set to the number of frames per second(FPS) output by the encoder,
                                          that is, the FPS, which is generally 25 or 30, but other values can also be set. */
    uint8_t               fps;       /*<! Maximum input frames per second
                                          The higher the FPS, the more coherent and realistic the video.
                                          24 FPS is standard for videos to appear coherent.
                                          30 FPS is standard for video games to appear coherent.
                                          FPS greater than 75 are generally imperceptible.*/
    esp_h264_resolution_t res;       /*<! Picture resolution */
    esp_h264_enc_rc_t     rc;        /*<! RC parameter */
} esp_h264_enc_cfg_t;

/**
 * @brief  Data stream information after encoding
 */
typedef struct {
    esp_h264_frame_type_t frame_type;  /*<! Frame type */
    esp_h264_pkt_t        raw_data;    /*<! Encoded data stream  */
    uint32_t              length;      /*<! Actual length of encoder data in bytes */
    uint32_t              dts;         /*<! Decoding time stamp(DTS)
                                            The timestamp of the decoder when decoding relative to SCR(system reference time).
                                            It mainly identifies when the bit stream read into memory begins to be sent to the decoder for decoding.*/
    uint32_t              pts;         /*<! Presentation time stamp(PTS). The timestamp relative to the SCR when the frame is displayed.
                                            It mainly measures when the decoded video is displayed.
                                            It is time scale. PTS plus time base is actual time.
                                            Commonly time base in TS stream is {1, 90000}.  So PTS unit is 1/90000 second.
                                            If time base is millisecond, PTS unit is 1 / 1000 second .
                                            If H.264 encode data has only I-frame and P-frame, the DTS is equal to PTS.
 */
} esp_h264_enc_out_frame_t;

/**
 * @brief  Data stream information before encoding
 */
typedef struct {
    esp_h264_pkt_t raw_data;  /*<! Unencoded data stream */
    uint32_t       pts;       /*<! Presentation time stamp(PTS) */
} esp_h264_enc_in_frame_t;

/**
 * @brief  Data stream information after encoding
 */
typedef struct {
    esp_h264_frame_type_t frame_type;  /*<! Frame type */
    uint8_t              *outbuf;      /*<! Decoder data stream  */
    uint32_t              out_size;    /*<! Actual length of decoder data in bytes */
    uint32_t              dts;         /*<! Decoding time stamp(DTS)
                                            The timestamp of the decoder when decoding relative to SCR(system reference time).
                                            It mainly identifies when the bit stream read into memory begins to be sent to the decoder for decoding.*/
    uint32_t              pts;         /*<! Presentation time stamp(PTS).The timestamp relative to the SCR when the frame is displayed.
                                            It mainly measures when the decoded video is displayed.
                                            It is time scale. PTS plus time base is actual time.
                                            Commonly time base in TS stream is {1, 90000}.  So PTS unit is 1/90000 second.
                                            If time base is millisecond, PTS unit is 1 / 1000 second .
                                            If H.264 encode data has only I-frame and P-frame, the DTS is equal to PTS. */
} esp_h264_dec_out_frame_t;

/**
 * @brief  Data stream information before encoding
 *
 * @note  User needs to process the `consume` as follows:
 *
 * @code{c}
 *          esp_h264_dec_in_frame_t in_frame = {.raw_data.buffer = buffer, .raw_data.len = len};
 *          esp_h264_dec_out_frame_t out_frame;
 *          while (raw_data.len)  {
 *            ret = esp_h264_dec_process(dec, &in_frame, &out_frame);
 *            if (ret != ESP_H264_ERR_OK) {
 *              break;
 *            }
 *            in_frame.raw_data.buffer += in_frame.consume;
 *            in_frame.raw_data.len -= in_frame.consume;
 *          }
 * @endcode
 */
typedef struct {
    esp_h264_pkt_t        raw_data;  /*<! Encoded data stream  */
    uint32_t              consume;   /*<! Actual consumed data length in bytes */
    uint32_t              dts;       /*<! Decoding time stamp(DTS)
                                          The timestamp of the decoder when decoding relative to SCR(system reference time).
                                          It mainly identifies when the bit stream read into memory begins to be sent to the decoder for decoding.*/
    uint32_t              pts;       /*<! Presentation time stamp(PTS).The timestamp relative to the SCR when the frame is displayed.
                                          It mainly measures when the decoded video is displayed.
                                          It is time scale. PTS plus time base is actual time.
                                          Commonly time base in TS stream is {1, 90000}.  So PTS unit is 1/90000 second.
                                          If time base is millisecond, PTS unit is 1 / 1000 second.
                                          If H.264 encode data has only I-frame and P-frame, the DTS is equal to PTS.
 */
} esp_h264_dec_in_frame_t;

/**
 * @brief  Get bytes per pixel (BPP) by picture type
 * @param  pic_type  Picture format type
 * @return Bytes per pixel value
 */
#define ESP_H264_GET_BPP_BY_PIC_TYPE(pic_type) \
    ((pic_type) == ESP_H264_RAW_FMT_I420 || (pic_type) == ESP_H264_RAW_FMT_O_UYY_E_VYY ? 1.5f : \
     (pic_type) == ESP_H264_RAW_FMT_RGB565_LE || (pic_type) == ESP_H264_RAW_FMT_UYVY || (pic_type) == ESP_H264_RAW_FMT_YUYV ? 2.0f : \
     3.0f)

#if !defined(CONFIG_ESP_REV_MIN_FULL) || CONFIG_ESP_REV_MIN_FULL < 300

/**
 * @brief  Check if picture type is supported by hardware (HW Version 3)
 * @note   HW Version 3 only supports ESP_H264_RAW_FMT_O_UYY_E_VYY format
 * @param  pic_type  Picture format type to check
 * @return true if supported, false otherwise
 */
#define ESP_H264_HW_IS_SUPPORTED_PIC_TYPE(pic_type) \
    ((pic_type) == ESP_H264_RAW_FMT_O_UYY_E_VYY)

#else

/**
 * @brief  Check if picture type is supported by hardware (HW Version 1/2)
 * @note   HW Version 1/2 supports multiple formats including:
 *         - ESP_H264_RAW_FMT_GREY (Grayscale)
 *         - ESP_H264_RAW_FMT_BGR888 (24-bit RGB)
 *         - ESP_H264_RAW_FMT_RGB565_LE (16-bit RGB Little-Endian)
 *         - ESP_H264_RAW_FMT_VUY (YUV 4:4:4)
 *         - ESP_H264_RAW_FMT_UYVY (YUV 4:2:2)
 *         - ESP_H264_RAW_FMT_O_UYY_E_VYY (Optimized YUV 4:2:0)
 * @param  pic_type  Picture format type to check
 * @return true if supported, false otherwise
 */
#define ESP_H264_HW_IS_SUPPORTED_PIC_TYPE(pic_type) \
    ((pic_type) == ESP_H264_RAW_FMT_BGR888 || \
     (pic_type) == ESP_H264_RAW_FMT_RGB565_LE || \
     (pic_type) == ESP_H264_RAW_FMT_VUY || \
     (pic_type) == ESP_H264_RAW_FMT_UYVY || \
     (pic_type) == ESP_H264_RAW_FMT_O_UYY_E_VYY)

#endif  /* defined(CONFIG_ESP_REV_MIN_FULL) && CONFIG_ESP_REV_MIN_FULL < 300 */

#ifdef __cplusplus
}
#endif

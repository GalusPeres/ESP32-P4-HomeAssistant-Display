/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_H264_VERSION "1.3.6"

/**
 *  Features:
 *     - H264 hardware encoder
 *            - Support baseline profile (max frame size is 36864 macro-block)
 *            - Support a variety of widths in range[80, 1088] and heights in range [80, 2048]
 *            - Support quality first rate control
 *            - Support YUV420 UYVY BGR888 RGB565_LE VUY raw data
 *            - Support dynamic changes about bit rate, frame rate, GOP , QP .ect.
 *            - Support single and dual stream encoder
 *            - Support de-blocking filter, ROI, MV function
 *            - Support SPS and PPS encoding
 *     software encoder
 *            - Support baseline profile (max frame size is 36864 macro-block)
 *            - Support a variety of widths and heights greater than 16
 *            - Support quality first rate control
 *            - Support YUYV and IYUV raw data
 *            - Support dynamic changes about bit rate, frame rate
 *            - Support more than one slice per frame
 *            - Support SPS and PPS encoding
 *     software decoder
 *            - Support baseline profile (max frame size is 36864 macro-block)
 *            - Support a variety of widths and heights
 *            - Support long term reference (LTR) frames
 *            - Support memory management control operation (MMCO)
 *            - Support reference picture list modification
 *            - Support multiple reference frames when specified in sequence parameter set (SPS)
 *            - Support IYUV output
 *  Release Notes:
 *     v1.0.0:
 *     - Add h264 encoder and decoder support
 *     v1.0.1:
 *     - Changed the IDF dependencies from >= 5.3 to >= 4.4
 *     - Fixed the decoder without updating PTS and DTS
 *     v1.2.0:
 *     - Added hardware encoder support for multiple pixel formats(UYVY BGR888 BGR565_BE VUY) on ESP32-P4(>=3.0.0):
 *     - Added hardware version detection macro `ESP_H264_HW_IS_SUPPORTED_PIC_TYPE`
 *     - Added `ESP_H264_GET_BPP_BY_PIC_TYPE` macro to determine the number of bits per pixel for a given picture type
 *     - Added support for ESP32P4 V3 hardware versions
 *     v1.3.0:
 *     - Corrected pixel format name from BGR565_BE to RGB565_LE in documentation comments and public header files
 *     v1.3.5:
 *     - Fixed hardware encoder (P4) encoding failure when flash encryption is enabled and accessing PSRAM
 *     v1.3.6:
 *     - Supported IDF6.0
 *     - Supported ESP32-S31
 *     - Fixed excessive internal buffer allocation in the decoder
 */

/**
 * @brief  Get H.264 version string
 *
 * @return
 *       - ESP_H264_VERSION
 */
const char *esp_h264_get_version(void);

#ifdef __cplusplus
}
#endif

/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "esp_h264_dec.h"
#include "h264bsd_decoder.h"
#include "esp_h264_check.h"
#include "esp_h264_alloc.h"
#include "esp_h264_dec_sw.h"

static const char *TAG = "H264_DEC.SW";

typedef struct esp_h264_dec_sw_handle {
    esp_h264_dec_t        base;
    esp_h264_dec_param_t  param_hd;
    uint32_t              width;
    uint32_t              height;
    h264bsd_hd_t          dec_hd;
    uint32_t              out_len;
} esp_h264_dec_sw_handle_t;

static esp_h264_err_t get_res(esp_h264_dec_param_handle_t param_hd, esp_h264_resolution_t *res)
{
    esp_h264_dec_sw_handle_t *sw_hd = __containerof(param_hd, esp_h264_dec_sw_handle_t, param_hd);
    res->width = sw_hd->width;
    res->height = sw_hd->height;
    return ESP_H264_ERR_OK;
}

static esp_h264_err_t dec_process(esp_h264_dec_handle_t dec, esp_h264_dec_in_frame_t *in_frame, esp_h264_dec_out_frame_t *out_frame)
{
    esp_h264_dec_sw_handle_t *sw_hd = __containerof(dec, esp_h264_dec_sw_handle_t, base);
    uint32_t retCode = 3;
    uint8_t *pic = NULL;
    uint32_t length = in_frame->raw_data.len;

    retCode = h264bsdDecode(sw_hd->dec_hd, in_frame->raw_data.buffer, (u32 *)&length, &pic, (u32 *)&sw_hd->width, (u32 *)&sw_hd->height);
    in_frame->consume = in_frame->raw_data.len - length;
    out_frame->out_size = 0;
    sw_hd->out_len = sw_hd->width * sw_hd->height + (sw_hd->width * sw_hd->height >> 1);
    switch (retCode) {
    case H264BSD_ERROR:
        ESP_H264_LOGE(TAG, "Error in decoding");
        return ESP_H264_ERR_FAIL;
    case H264BSD_PARAM_SET_ERROR:
        ESP_H264_LOGE(TAG, "Serious error in decoding, failed to activate param sets");
        return ESP_H264_ERR_FAIL;
    /* The parsing of not picture data NALU is done, like SPS PPS.*/
    case H264BSD_RDY:
        return ESP_H264_ERR_OK;
    /* The parsing of picture NALU is done for, like I-frame P-frame.*/
    case H264BSD_PIC_RDY:
        out_frame->outbuf = pic;
        out_frame->out_size = sw_hd->out_len;
        out_frame->pts = in_frame->pts;
        out_frame->dts = in_frame->dts;
        return ESP_H264_ERR_OK;
    case H264BSD_MEMALLOC_ERROR:
        ESP_H264_LOGE(TAG, "Memory lack");
        return ESP_H264_ERR_MEM;
    }
    return ESP_H264_ERR_OK;
}

static esp_h264_err_t dec_close(esp_h264_dec_handle_t dec)
{
    return ESP_H264_ERR_OK;
}

static esp_h264_err_t dec_open(esp_h264_dec_handle_t dec)
{
    return ESP_H264_ERR_OK;
}

static esp_h264_err_t dec_del(esp_h264_dec_handle_t dec)
{
    if (dec) {
        esp_h264_dec_sw_handle_t *sw_hd = __containerof(dec, esp_h264_dec_sw_handle_t, base);
        if (sw_hd->dec_hd) {
            h264bsdShutdown(sw_hd->dec_hd);
            h264bsdFree(sw_hd->dec_hd);
            sw_hd->dec_hd = NULL;
        }
        dec_close(dec);
        esp_h264_free(sw_hd);
    }
    return ESP_H264_ERR_OK;
}

esp_h264_err_t esp_h264_dec_sw_new(const esp_h264_dec_cfg_sw_t *cfg, esp_h264_dec_handle_t *out_dec)
{
    /* Parameter check */
    ESP_H264_RET_ON_FALSE(cfg && out_dec, ESP_H264_ERR_ARG, TAG, "Invalid h264 configure and handle parameter");
    ESP_H264_RET_ON_FALSE(cfg->pic_type == ESP_H264_RAW_FMT_I420, ESP_H264_ERR_ARG, TAG, "Un-supported h264 picture type parameter");

    *out_dec = NULL;
    ESP_H264_LOGI(TAG, "tinyh264 version: %s ", esp_tinyh264_get_version());
    /** Create decoder handle */
    uint32_t actual_size;
    esp_h264_dec_sw_handle_t *sw_hd = (esp_h264_dec_sw_handle_t *)esp_h264_calloc_prefer(1, sizeof(esp_h264_dec_sw_handle_t), &actual_size, ESP_H264_MEM_SPIRAM, ESP_H264_MEM_INTERNAL);
    ESP_H264_RET_ON_FALSE(sw_hd != NULL, ESP_H264_ERR_MEM, TAG, "No memory for handle");

    /* Parameter initalization */
    esp_h264_err_t ret = ESP_H264_ERR_OK;
    h264bsd_cfg_t tinyh264_cfg = H264BSD_CFG_DEFAULT();
#if (CONFIG_ESP_H264_DUAL_TASK)
    tinyh264_cfg.dualTaskEnable = 1;
    tinyh264_cfg.dualTaskCore = CONFIG_ESP_H264_DUAL_TASK_CORE;
    tinyh264_cfg.dualTaskPriority = CONFIG_ESP_H264_DUAL_TASK_PRIORITY;
#endif
    sw_hd->dec_hd = h264bsdAlloc(&tinyh264_cfg);
    ESP_H264_GOTO_ON_FALSE(sw_hd->dec_hd != NULL, ret, __dec_exit__, TAG, "No memory for decoder handle");

    /** Encoder handle configure */
    sw_hd->base.open = dec_open;
    sw_hd->base.process = dec_process;
    sw_hd->base.close = dec_close;
    sw_hd->base.del = dec_del;
    sw_hd->param_hd.get_res = get_res;
    *out_dec = &sw_hd->base;
    return ret;
__dec_exit__:
    /** Delete the decoder handle */
    dec_del(&sw_hd->base);
    return ret;
}

esp_h264_err_t esp_h264_dec_sw_get_param_hd(esp_h264_dec_handle_t dec, esp_h264_dec_param_sw_handle_t *out_param)
{
    if (dec && out_param) {
        esp_h264_dec_sw_handle_t *sw_hd = __containerof(dec, esp_h264_dec_sw_handle_t, base);
        *out_param = &sw_hd->param_hd;
        return ESP_H264_ERR_OK;
    }
    return ESP_H264_ERR_ARG;
}

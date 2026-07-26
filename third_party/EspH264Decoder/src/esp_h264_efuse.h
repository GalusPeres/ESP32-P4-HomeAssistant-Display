/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include "esp_efuse.h"
#define ESP_H264_IS_FLASH_ENCRYPTION_ENABLED() esp_efuse_is_flash_encryption_enabled()
#else
#include "esp_flash_encrypt.h"
#define ESP_H264_IS_FLASH_ENCRYPTION_ENABLED() esp_flash_encryption_enabled()
#endif

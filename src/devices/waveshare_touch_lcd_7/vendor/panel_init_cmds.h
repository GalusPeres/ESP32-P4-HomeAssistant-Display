#pragma once

#include <stddef.h>

#include "src/devices/device_select.h"
#include "src/devices/waveshare_touch_lcd_7/vendor/ili9881c/esp_lcd_ili9881c.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const ili9881c_lcd_init_cmd_t kWaveshareIli9881cInit7[];
extern const size_t kWaveshareIli9881cInit7Count;

#ifdef __cplusplus
}
#endif

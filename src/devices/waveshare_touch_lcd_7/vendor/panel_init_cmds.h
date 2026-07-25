#pragma once

#include <stddef.h>

#include "src/devices/device_select.h"
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7)
#include "src/devices/waveshare_touch_lcd_7/vendor/ili9881c/esp_lcd_ili9881c.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7)
extern const ili9881c_lcd_init_cmd_t kWaveshareIli9881cInit7[];
extern const size_t kWaveshareIli9881cInit7Count;
#endif

#ifdef __cplusplus
}
#endif

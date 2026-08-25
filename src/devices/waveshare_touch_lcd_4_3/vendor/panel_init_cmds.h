#pragma once

#include <stddef.h>

#include "src/devices/device_select.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_4_3)
#include "src/devices/waveshare_touch_lcd_4_3/vendor/st7701/esp_lcd_st7701.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const st7701_lcd_init_cmd_t kWaveshareSt7701Init4_3[];
extern const size_t kWaveshareSt7701Init4_3Count;

#ifdef __cplusplus
}
#endif
#endif  // defined(DEVICE_WAVESHARE_TOUCH_LCD_4_3)

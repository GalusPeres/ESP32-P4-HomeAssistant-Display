#pragma once

#include <stddef.h>

#include "src/devices/device_select.h"
#include "src/devices/waveshare_touch_lcd_10_1/vendor/jd9365/esp_lcd_jd9365_10_1.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const jd9365_lcd_init_cmd_t kWaveshareJd9365Init10_1[];
extern const size_t kWaveshareJd9365Init10_1Count;

#ifdef __cplusplus
}
#endif

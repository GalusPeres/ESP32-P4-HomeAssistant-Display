#pragma once

#include "src/devices/device_select.h"

#if defined(DEVICE_M5STACKS_TAB5)
#include "src/devices/m5stacks_tab5/device_m5stacks_tab5.h"
namespace DeviceImpl = DeviceM5StacksTab5;
#elif defined(DEVICE_GUITION_JC8012P4A1)
#include "src/devices/guition_jc8012p4a1/device_guition_jc8012p4a1.h"
namespace DeviceImpl = DeviceGuitionJC8012P4A1;
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
#include "src/devices/waveshare_touch_lcd_8/device_waveshare_touch_lcd_8.h"
namespace DeviceImpl = DeviceWaveshareTouchLCD8;
#elif defined(DEVICE_WAVESHARE_4B)
#include "src/devices/waveshare_4b/device_waveshare_4b.h"
namespace DeviceImpl = DeviceWaveshare4B;
#else
#error "No supported device target selected."
#endif

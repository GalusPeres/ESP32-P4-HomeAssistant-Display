#pragma once

#include "src/devices/device_select.h"

#if defined(DEVICE_M5STACKS_TAB5)
#include "src/devices/m5stacks_tab5/device_m5stacks_tab5.h"
namespace DeviceImpl = DeviceM5StacksTab5;
#elif defined(DEVICE_GUITION_JC8012P4A1)
#include "src/devices/guition_jc8012p4a1/device_guition_jc8012p4a1.h"
namespace DeviceImpl = DeviceGuitionJC8012P4A1;
#elif defined(DEVICE_GUITION_JC1060P470C)
#include "src/devices/guition_jc1060p470c/device_guition_jc1060p470c.h"
namespace DeviceImpl = DeviceGuitionJC1060P470C;
#elif defined(DEVICE_GUITION_ESP32_4848S040)
#include "src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.h"
namespace DeviceImpl = DeviceGuitionESP324848S040;
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_7)
#include "src/devices/waveshare_touch_lcd_7/device_waveshare_touch_lcd_7.h"
namespace DeviceImpl = DeviceWaveshareTouchLCD7;
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
#include "src/devices/waveshare_touch_lcd_8/device_waveshare_touch_lcd_8.h"
namespace DeviceImpl = DeviceWaveshareTouchLCD8;
#elif defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1)
#include "src/devices/waveshare_touch_lcd_10_1/device_waveshare_touch_lcd_10_1.h"
namespace DeviceImpl = DeviceWaveshareTouchLCD10;
#elif defined(DEVICE_LAYOUT_TEST_1024X600)
#include "src/devices/layout_test_1024x600/device_layout_test_1024x600.h"
namespace DeviceImpl = DeviceLayoutTest1024x600;
#elif defined(DEVICE_LAYOUT_TEST_480X480)
#include "src/devices/layout_test_480x480/device_layout_test_480x480.h"
namespace DeviceImpl = DeviceLayoutTest480x480;
#elif defined(DEVICE_WAVESHARE_4B)
#include "src/devices/waveshare_4b/device_waveshare_4b.h"
namespace DeviceImpl = DeviceWaveshare4B;
#else
#error "No supported device target selected."
#endif

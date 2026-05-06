#pragma once

// Central device selection for the shared project.
// Arduino IDE builds usually do not provide per-target build flags.
// For a quick manual switch, uncomment exactly one target below:
//
//#define DEVICE_WAVESHARE_4B
#define DEVICE_WAVESHARE_TOUCH_LCD_8
// #define DEVICE_M5STACKS_TAB5
//
// If nothing is selected, the project defaults to Waveshare 4B.

#if defined(DEVICE_TAB5) && !defined(DEVICE_M5STACKS_TAB5)
#define DEVICE_M5STACKS_TAB5
#endif

#if defined(DEVICE_WAVESHARE_WIFI6_TOUCH_LCD_8) && !defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
#define DEVICE_WAVESHARE_TOUCH_LCD_8
#endif

#if defined(DEVICE_WAVESHARE_4B) && defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
#error "Select only one device target."
#endif

#if defined(DEVICE_WAVESHARE_4B) && defined(DEVICE_M5STACKS_TAB5)
#error "Select only one device target."
#endif

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8) && defined(DEVICE_M5STACKS_TAB5)
#error "Select only one device target."
#endif

#if !defined(DEVICE_WAVESHARE_4B) && !defined(DEVICE_WAVESHARE_TOUCH_LCD_8) && !defined(DEVICE_M5STACKS_TAB5)
#define DEVICE_WAVESHARE_4B
#endif

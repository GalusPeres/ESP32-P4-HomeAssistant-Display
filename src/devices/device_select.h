#pragma once

// Central device selection for the shared project.
// Arduino IDE builds usually do not provide per-target build flags.
// For a quick manual switch, uncomment exactly one target below:
//
#if !defined(HOMETILES_CI_TARGET)
#define DEVICE_WAVESHARE_4B
// #define DEVICE_WAVESHARE_TOUCH_LCD_7
// #define DEVICE_WAVESHARE_TOUCH_LCD_8
// #define DEVICE_WAVESHARE_TOUCH_LCD_10_1
// #define DEVICE_M5STACKS_TAB5
// #define DEVICE_GUITION_JC8012P4A1
#endif
//
// If nothing is selected, the project defaults to Waveshare 4B.

#if defined(DEVICE_TAB5) && !defined(DEVICE_M5STACKS_TAB5)
#define DEVICE_M5STACKS_TAB5
#endif

#if defined(DEVICE_WAVESHARE_WIFI6_TOUCH_LCD_8) && !defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
#define DEVICE_WAVESHARE_TOUCH_LCD_8
#endif

#if (defined(DEVICE_WAVESHARE_4B) + \
     defined(DEVICE_WAVESHARE_TOUCH_LCD_7) + \
     defined(DEVICE_WAVESHARE_TOUCH_LCD_8) + \
     defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1) + \
     defined(DEVICE_M5STACKS_TAB5) + \
     defined(DEVICE_GUITION_JC8012P4A1)) > 1
#error "Select only one device target."
#endif

#if !defined(DEVICE_WAVESHARE_4B) && \
    !defined(DEVICE_WAVESHARE_TOUCH_LCD_7) && \
    !defined(DEVICE_WAVESHARE_TOUCH_LCD_8) && \
    !defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1) && \
    !defined(DEVICE_M5STACKS_TAB5) && \
    !defined(DEVICE_GUITION_JC8012P4A1) && \
    defined(HOMETILES_CI_TARGET)
#error "HOMETILES_CI_TARGET requires one DEVICE_* build flag."
#endif

#if !defined(DEVICE_WAVESHARE_4B) && \
    !defined(DEVICE_WAVESHARE_TOUCH_LCD_7) && \
    !defined(DEVICE_WAVESHARE_TOUCH_LCD_8) && \
    !defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1) && \
    !defined(DEVICE_M5STACKS_TAB5) && \
    !defined(DEVICE_GUITION_JC8012P4A1) && \
    !defined(HOMETILES_CI_TARGET)
#define DEVICE_WAVESHARE_4B
#endif

// The 7", 8" and 10.1" products share the same ESP32-P4-WIFI6-Touch-LCD-X
// base board, I2C/touch, backlight, SDMMC and ESP-Hosted wiring. Only the
// panel controller/timing and logical layout differ.
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7) || \
    defined(DEVICE_WAVESHARE_TOUCH_LCD_8) || \
    defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1)
#define DEVICE_WAVESHARE_TOUCH_LCD_X
#endif

// The 8" and 10.1" panels share the 1280x800 HomeTiles layout.
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8) || \
    defined(DEVICE_WAVESHARE_TOUCH_LCD_10_1)
#define DEVICE_WAVESHARE_TOUCH_LCD_1280X800
#endif

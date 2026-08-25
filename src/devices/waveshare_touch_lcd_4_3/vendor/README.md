# Third-party display and touch code

The ST7701 panel driver and GT911 touch implementation in this directory are
derived from Espressif's Apache-2.0 display and touch components. The matching
license is included as `LICENSE-APACHE-2.0.txt`.

The ST7701 initialization sequence, MIPI-DSI timings, GPIO assignments and
SDMMC configuration are adapted from Waveshare's
`ESP32-P4-WIFI6-Touch-LCD-4.3` BSP version 1.0.1 for the exact board named by
this profile:

- https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4.3.htm
- https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4.3

Modified files are compiled only for `DEVICE_WAVESHARE_TOUCH_LCD_4_3`. The
HomeTiles integration uses the MIPI-DSI ST7701 path, landscape coordinates,
the board's active-low backlight, and the ESP32-P4 pre-v3 clock source tested
on revision v1.3 silicon.

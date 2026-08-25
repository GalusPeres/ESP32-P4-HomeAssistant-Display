# Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3

HomeTiles profile for the exact 4.3-inch Waveshare board.

Hardware contract:

- native 480 x 800 ST7701 MIPI-DSI panel, rendered as 800 x 480 landscape;
- two DSI lanes at 500 Mbps and a 30 MHz DPI clock;
- GT911 touch on SDA GPIO7 and SCL GPIO8, polled without INT or RST;
- active-low backlight PWM on GPIO26 and active-low panel reset on GPIO27;
- SDMMC slot 0 on GPIO39-44 with LDO channel 4;
- ESP32-C6 networking through the existing ESP-Hosted transport.

The panel table and timings come from Waveshare BSP 1.0.1. The tested hardware
reports ESP32-P4 revision v1.3 and must use the pre-v3 build profile. Display,
touch, Wi-Fi, Web Admin, tile persistence, MQTT, Home Assistant discovery and a
live Weather tile are confirmed on physical hardware. Complete microSD and OTA
validation remains pending.

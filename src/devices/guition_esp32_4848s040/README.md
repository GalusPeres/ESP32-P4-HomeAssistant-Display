# GUITION ESP32-4848S040

Experimental HomeTiles target for the GUITION/Jingcai
`ESP32-4848S040C_I` family:

- ESP32-S3, 16 MB flash and 8 MB octal PSRAM
- 480 x 480 ST7701 RGB panel
- GT911 capacitive touch
- PWM backlight on GPIO 38
- microSD card in SPI mode

The pin mapping and panel configuration were cross-checked against:

- [GUITION's product documentation](https://www.guition.com/esp32-display-module/4-inch-esp32s3-display-module)
- [Espressif's Apache-2.0 `ESP32_Display_Panel` board definition](https://github.com/esp-arduino-libs/ESP32_Display_Panel)
- [Arduino_GFX's BSD-licensed `ESP32_4848S040_86BOX_GUITION` example](https://github.com/moononournation/Arduino_GFX)
- [public hardware reports for the same board family](https://github.com/arendst/Tasmota/discussions/20527)

HomeTiles uses Arduino_GFX's included ST7701 type-9 initialization table.
No code from the PolyForm Noncommercial-licensed EspControl repository is
included here.

The initial hardware-test profile uses a conservative 10 MHz RGB pixel clock
and a ten-line bounce buffer to reduce RGB drift while WiFi and flash are busy.

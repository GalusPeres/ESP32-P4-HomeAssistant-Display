# Waveshare ESP32-S3-Touch-LCD-4 Rev 4.0

This profile is for the exact touch-equipped **Rev 4.0** board with a
CH32V003 helper MCU. Revisions 1.0–3.0 have different hardware and are not
supported. The Waveshare ESP32-S3-Touch-LCD-4B and ESP32-P4 LCD-4B use their
own profiles and firmware images.

## Profile and build

- Profile and release key: `waveshare_s3_touch_lcd_4`
- Device define: `DEVICE_WAVESHARE_S3_TOUCH_LCD_4`
- ESP32-S3, 16MB flash, 8MB octal PSRAM
- 480×480 ST7701 RGB display and GT911 capacitive touch
- Shared control I2C bus: SDA GPIO15, SCL GPIO7
- CH32V003 helper at `0x24` for display/touch sequencing and backlight PWM
- Internal LittleFS for runtime files; shared repository `partitions.csv`

Use the exact Arduino IDE settings in
[BOARD_SETTINGS.md](../../../BOARD_SETTINGS.md). The local build profile is
`waveshare_s3_touch_lcd_4`.

Factory and OTA images are integrated into the build and release pipeline
for the next release. They are **not included in v0.6.9**:

- First flash: `hometiles_<version>_waveshare_s3_touch_lcd_4_factory.bin`
- OTA update: `hometiles_<version>_waveshare_s3_touch_lcd_4.bin`

## Hardware validation and limitations

Contributor [@verrat73](https://github.com/verrat73) reported successful
display output, backlight dimming, capacitive touch, Wi-Fi, Web Admin, MQTT,
HomeTiles Bridge integration, and Web OTA upload/verification on physical
Rev 4.0 hardware in [PR #35](https://github.com/GalusPeres/HomeTiles/pull/35).
This is the contributor's test of the submitted profile; compilation does
not establish hardware validation of subsequent integration changes.

Camera tiles are unavailable on this ESP32-S3 profile. The board physically
has a microSD slot, but the HomeTiles driver does not implement SD access.
The microSD file manager, screenshot export to SD, and SD image slideshows
are therefore unavailable on this profile.

## Primary hardware references

- [Waveshare product wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4)
- [Official board examples and V4.0 hardware references](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-4)
- [Official Waveshare board support package](https://github.com/waveshareteam/Waveshare-ESP32-components/tree/master/bsp/esp32_s3_touch_lcd_4)

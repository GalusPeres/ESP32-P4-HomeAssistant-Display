# Board Settings

This file documents the working Arduino IDE board settings for the device profiles used in this project.

Note:
- The remaining values should be set exactly as listed below.
- This repo now contains a shared sketch-root `partitions.csv`.
- Arduino ESP32 uses that file automatically for both boards during build.
- The shared layout keeps both OTA app slots below `16MB`.

## M5Stacks Tab5

Used for:
- `src/devices/m5stacks_tab5`

Important:
- Leave `Partition Scheme` on the normal Tab5 default.
- The actual partition layout still comes from the shared repo `partitions.csv`.

Arduino IDE:
- Board: `M5Tab5`
- USB CDC On Boot: `Enabled`
- CPU Frequency: `360MHz`
- Core Debug Level: `None`
- USB DFU On Boot: `Disabled`
- Erase All Flash Before Sketch Upload: `Disabled`
- Flash Frequency: `80MHz`
- Flash Mode: `QIO`
- Flash Size: `16MB (128Mb)`
- JTAG Adapter: `Disabled`
- USB Firmware MSC On Boot: `Disabled`
- Partition Scheme: `Default (2 x 6.5 MB app, 3.6 MB SPIFFS)`
- PSRAM: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- Upload Speed: `921600`
- USB Mode: `Hardware CDC and JTAG`

## Waveshare B4 / ESP32-P4-86-Panel-ETH-2RO

Used for:
- `src/devices/waveshare_4b`

Important:
- The ESP32-P4-86-Panel-ETH-2RO uses this same firmware profile. Its native
  Ethernet connection is supported; its two onboard relays are not exposed by
  HomeTiles yet.
- Leave `Partition Scheme` on the normal 32MB B4 setting.
- The actual partition layout still comes from the shared repo `partitions.csv`.
- This avoids switching partition files between boards.

Arduino IDE:
- Board: `ESP32P4 Dev Module`
- USB CDC On Boot: `Disabled`
- CPU Frequency: `360MHz`
- Core Debug Level: `None`
- USB DFU On Boot: `Disabled`
- Erase All Flash Before Sketch Upload: `Disabled`
- Flash Frequency: `80MHz`
- Flash Mode: `QIO`
- Flash Size: `32MB (256Mb)`
- JTAG Adapter: `Disabled`
- USB Firmware MSC On Boot: `Disabled`
- Partition Scheme: `32M Flash (13MB APP/6.75MB SPIFFS)`
- PSRAM: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- Upload Speed: `921600`
- USB Mode: `USB-OTG (TinyUSB)`

## Waveshare Touch LCD 8

Used for:
- `src/devices/waveshare_touch_lcd_8`

Important:
- Leave `Partition Scheme` on the normal 32MB ESP32-P4 setting.
- The actual partition layout still comes from the shared repo `partitions.csv`.
- The `Chip Variant` must be set to `Before v3.00` for this hardware.

Arduino IDE:
- Board: `ESP32P4 Dev Module`
- USB CDC On Boot: `Disabled`
- Chip Variant: `Before v3.00`
- Core Debug Level: `None`
- USB DFU On Boot: `Disabled`
- Erase All Flash Before Sketch Upload: `Disabled`
- Flash Frequency: `80MHz`
- Flash Mode: `QIO`
- Flash Size: `32MB (256Mb)`
- JTAG Adapter: `Disabled`
- USB Firmware MSC On Boot: `Disabled`
- Partition Scheme: `32M Flash (13MB APP/6.75MB SPIFFS)`
- PSRAM: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- Upload Speed: `921600`
- USB Mode: `USB-OTG (TinyUSB)`

## Guition JC8012P4A1

Used for:
- `src/devices/guition_jc8012p4a1`

Important:
- The module has `16MB` flash and `32MB` PSRAM.
- The native panel is `800x1280`; HomeTiles renders `1280x800` and rotates it.
- Backlight is active-high on GPIO 23 using 20kHz, 10-bit LEDC PWM.
- Touch uses SDA 7, SCL 8 and reset 22. The target tries 400kHz first and the
  official Guition 100kHz setting as a compatibility fallback.
- The microSD slot uses native 4-bit SDMMC slot 0 (`CLK 43`, `CMD 44`,
  `D0-D3 39-42`) and P4 LDO channel 4.
- Use the repository's `partitions.csv`; HomeTiles needs two 6.5MB OTA slots.
- Do not use `16M Flash (3MB APP/9.9MB FATFS)`: the application is larger
  than its 3MB app slot.
- The `Chip Variant` must be set to `Before v3.00` for the tested hardware.

Arduino IDE:
- Board: `ESP32P4 Dev Module`
- USB CDC On Boot: `Disabled`
- Chip Variant: `Before v3.00`
- Core Debug Level: `None`
- USB DFU On Boot: `Disabled`
- Erase All Flash Before Sketch Upload: `Disabled`
- Flash Frequency: `80MHz`
- Flash Mode: `QIO`
- Flash Size: `16MB (128Mb)`
- JTAG Adapter: `Disabled`
- USB Firmware MSC On Boot: `Disabled`
- Partition Scheme: `Custom`
- PSRAM: `Enabled`
- Upload Mode: `UART0 / Hardware CDC`
- Upload Speed: `921600`
- USB Mode: `USB-OTG (TinyUSB)`

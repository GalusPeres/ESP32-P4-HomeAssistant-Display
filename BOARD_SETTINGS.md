# Board Settings

This file documents the working Arduino IDE board settings for the two device profiles used in this project.

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

## Waveshare B4

Used for:
- `src/devices/waveshare_4b`

Important:
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

# ESP32-P4 Home Assistant Display

ESP32-P4 touchscreen firmware for Home Assistant dashboards with a fully tile-configurable web interface.

The project currently supports multiple ESP32-P4 display devices and is built around MQTT, LVGL, on-device web configuration, and a microSD-based runtime setup.

## Overview

This firmware is designed to turn supported ESP32-P4 touch displays into configurable Home Assistant panels.

Everything visible on the dashboard is tile-based and can be configured through the built-in web interface:
- add, remove, move, and resize tiles
- configure tile content and behavior
- create folders and navigation structures
- manage WiFi, MQTT, and language settings without changing code

## Supported Devices

- M5Stacks Tab5
- Waveshare B4

Device-specific Arduino IDE settings are documented in [BOARD_SETTINGS.md](BOARD_SETTINGS.md).

## Requirements

- Home Assistant
- MQTT broker
- Home Assistant bridge/integration: [ha-tab5-mqtt-bridge](https://github.com/GalusPeres/ha-tab5-mqtt-bridge)
- microSD card required, formatted as FAT32

## Features

- Fully tile-configurable dashboard via the built-in web admin panel
- MQTT-based Home Assistant communication
- Access Point based first-time setup
- Device-local WiFi and MQTT configuration
- Multi-language UI/admin support
- Tile types currently include:
  - clock
  - counter
  - empty
  - key
  - navigate
  - scene
  - sensor
  - switch
  - text
  - weather

## Installation

### Option 1: Prebuilt Binaries

If release binaries are available, download the `.bin` file matching your device:
- `...m5stacks-tab5.bin`
- `...waveshare-b4.bin`

Flash the correct binary for your hardware using your usual ESP32-P4 flashing workflow.

### Option 2: Build From Source

1. Open [ESP32_P4_HomeAssistant_Display.ino](ESP32_P4_HomeAssistant_Display.ino) in the Arduino IDE.
2. Select the target device in [src/devices/device_select.h](src/devices/device_select.h).
3. Apply the correct board settings from [BOARD_SETTINGS.md](BOARD_SETTINGS.md).
4. Build and flash the firmware.

## First Setup

1. Insert a microSD card formatted as FAT32.
2. Flash the firmware.
3. Boot the device.
4. Open the device's temporary WiFi Access Point if needed.
5. Configure WiFi and MQTT in the built-in web interface.
6. Open the web admin panel through the device IP address.
7. Configure your tiles, folders, and layout.

## Home Assistant Integration

This firmware expects the Home Assistant side to be provided by the MQTT bridge/integration:

- [ha-tab5-mqtt-bridge](https://github.com/GalusPeres/ha-tab5-mqtt-bridge)

That integration is responsible for the Home Assistant-side MQTT communication and entity bridge.

## Repository Structure

- `src/` firmware source code
- `electron-app/` optional desktop companion tooling
- `mdi-extractor/` icon tooling
- `simconnect-bridge/` additional companion tooling
- `BOARD_SETTINGS.md` documented Arduino IDE board settings

## Notes

- OTA is not implemented yet.
- The microSD card is part of the expected runtime setup and should not be treated as optional.
- Board selection and board settings must match the target device.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

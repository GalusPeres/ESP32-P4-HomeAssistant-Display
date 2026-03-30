# ESP32-P4 Home Assistant Display

Tile-based ESP32-P4 firmware for Home Assistant dashboards with a fully configurable web interface.

The project currently supports multiple ESP32-P4 display devices and combines:
- touch-first dashboard UI
- MQTT-based Home Assistant integration
- on-device web configuration
- microSD-backed runtime storage

## Overview

This firmware turns supported ESP32-P4 touch displays into configurable Home Assistant control panels.

Everything visible on the dashboard is tile-based and managed from the built-in web interface:
- add, remove, move, and resize tiles
- configure tile content and behavior
- create folders and navigation structures
- manage WiFi, MQTT, language, and time zone settings without changing code

## Supported Devices

- [M5Stacks Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4)
- [Waveshare B4](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm)

Device-specific Arduino IDE settings are documented in [BOARD_SETTINGS.md](BOARD_SETTINGS.md).

## Screenshots

The screenshots below were captured on the Waveshare B4. They are meant as example views of the UI; the same firmware and web admin panel also run on the M5Stacks Tab5.

### Sensor Popup Examples

These three screenshots show example sensor popups with larger values, chart history, and the time labels below the graph.

<table>
  <tr>
    <td align="center"><img src="docs/images/b4-sensor-popup-kitchen.png" alt="Kitchen sensor popup" width="240"></td>
    <td align="center"><img src="docs/images/b4-sensor-popup-water.png" alt="Water sensor popup" width="240"></td>
    <td align="center"><img src="docs/images/b4-sensor-popup-battery.png" alt="Battery sensor popup" width="240"></td>
  </tr>
  <tr>
    <td align="center">Kitchen sensor popup</td>
    <td align="center">Water temperature popup</td>
    <td align="center">Battery / energy popup</td>
  </tr>
</table>

### Other UI Examples

These examples show the normal dashboard, a folder view, the on-device settings screen, and other popup types used in daily operation.

<table>
  <tr>
    <td align="center"><img src="docs/images/b4-home.png" alt="Waveshare B4 home dashboard" width="240"></td>
    <td align="center"><img src="docs/images/b4-folder-lights.png" alt="Waveshare B4 folder view" width="240"></td>
    <td align="center"><img src="docs/images/b4-settings.png" alt="Waveshare B4 settings screen" width="240"></td>
  </tr>
  <tr>
    <td align="center">Home dashboard</td>
    <td align="center">Folder view</td>
    <td align="center">On-device settings</td>
  </tr>
</table>

<table>
  <tr>
    <td align="center"><img src="docs/images/b4-weather-popup.png" alt="Weather popup" width="240"></td>
    <td align="center"><img src="docs/images/b4-light-popup-desk.png" alt="Light popup" width="240"></td>
  </tr>
  <tr>
    <td align="center">Weather popup</td>
    <td align="center">Light control popup</td>
  </tr>
</table>

### Web Admin

The web admin panel is used to configure tiles, folders, WiFi, MQTT, localization, screenshots, and the overall layout directly from the browser.

<table>
  <tr>
    <td align="center"><img src="docs/images/web-admin.png" alt="Web admin interface" width="900"></td>
  </tr>
  <tr>
    <td align="center">Built-in web admin interface</td>
  </tr>
</table>

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
- English and German UI/admin support
- Local screenshot export to microSD from the web interface
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

If release binaries are available, download the files matching your device:

- `...m5stacks-tab5-factory.bin`
- `...m5stacks-tab5-update.bin`
- `...waveshare-b4-factory.bin`
- `...waveshare-b4-update.bin`

Use:
- `factory.bin` for a clean first flash
- `update.bin` for updating an existing device

When using the ESP Flash Download Tool:
- flash `factory.bin` at `0x00000`
- flash `update.bin` at `0x10000`
- a manual reset after flashing may be required

### Option 2: Build From Source

1. Open [ESP32_P4_HomeAssistant_Display.ino](ESP32_P4_HomeAssistant_Display.ino) in the Arduino IDE.
2. Select the target device in [src/devices/device_select.h](src/devices/device_select.h).
3. Apply the correct board settings from [BOARD_SETTINGS.md](BOARD_SETTINGS.md).
4. Build and flash the firmware.

## First Setup

1. Insert a microSD card formatted as FAT32.
2. Flash the firmware.
3. Boot the device.
4. Open the temporary device WiFi Access Point if needed.
5. Configure WiFi and MQTT in the built-in web interface.
6. Open the web admin panel through the device IP address.
7. Configure your tiles, folders, and layout.

## Home Assistant Integration

This firmware expects the Home Assistant side to be provided by the MQTT bridge/integration:

- [ha-tab5-mqtt-bridge](https://github.com/GalusPeres/ha-tab5-mqtt-bridge)

That integration handles the Home Assistant-side MQTT communication and entity bridge.

## Repository Structure

- `src/` firmware source code
- `docs/images/` screenshots and documentation images
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

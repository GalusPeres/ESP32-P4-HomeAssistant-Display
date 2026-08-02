# Plan for Additional Display Support

This is an internal implementation and testing plan. New devices remain
experimental until they have been tested on real hardware.

## Order

1. Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1
2. Waveshare ESP32-P4-WIFI6-Touch-LCD-7
3. GUITION 10-inch ESP32-P4 device
4. GUITION 7-inch ESP32-P4 device

## Important distinction for the Waveshare X series

The Waveshare 7, 8 and 10.1 use the same X-series base board. This allows
board-level code such as power, I2C, SD card, ESP32-C6 networking and parts of
the touch integration to be shared internally.

The displays are **not all the same**:

- Waveshare 8 and 10.1: native 800 x 1280, JD9365 controller, but different
  vendor initialization data.
- Waveshare 7: native 720 x 1280 and a separate ILI9881C display controller.

Therefore every size gets its own build target, device key, OTA file and panel
configuration. There will be no universal Waveshare-X firmware file.

## Low-risk implementation approach

### 1. Waveshare 10.1

- Keep the currently working Waveshare 8 target unchanged.
- Add a separate 10.1 target with its own panel initialization table and
  firmware metadata.
- Reuse the proven 1280 x 800 HomeTiles UI profile.
- Initially share only code that is demonstrably identical.
- After both devices are confirmed working, optionally move duplicated base
  board functions into a small common X-series module.

### 2. Waveshare 7

- Add a separate ILI9881C panel driver and panel initialization data.
- Use a logical landscape resolution of 1280 x 720.
- Reuse the existing 1280 x 720 UI geometry from the Tab5, but do not reuse its
  hardware driver.
- Keep a separate firmware file and OTA device key.

### 3. GUITION devices

- First collect the exact model number, schematic, vendor example, display
  controller, touch controller, pin assignments and flash/PSRAM configuration.
- Create a separate GUITION hardware family; do not mix its hardware driver
  with the Waveshare driver.
- Share HomeTiles UI and services where possible, while keeping separate builds
  for the 10-inch and 7-inch panels until hardware tests prove otherwise.

## Required release targets

Each supported device needs:

- its own compile define and device profile;
- its own firmware/OTA device key;
- its own factory and OTA binaries;
- a CI build-matrix entry;
- correct Web Admin device naming and update-file validation;
- a supported-device documentation entry only after successful testing.

## Community hardware testing

Because not every device can be purchased, initial builds are marked
**experimental** and provided directly to a tester who owns the exact model.
The tester should verify:

- first boot, backlight, colors, resolution and orientation;
- touch accuracy at all four corners and in the center;
- brightness, sleep and wake;
- Wi-Fi/ESP32-C6 operation and reconnects;
- SD card access;
- restart, factory flash and OTA update;
- main tiles, popups and screensaver;
- stability during an extended test.

Only after these checks pass is the device added to normal releases as
officially supported.


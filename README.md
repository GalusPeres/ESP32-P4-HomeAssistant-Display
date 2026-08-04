<div align="center">

# <img src="docs/images/favicon.svg" width="42" alt="" align="top"> HomeTiles

**Tile-based ESP32-P4 firmware for Home Assistant dashboards<br>with an experimental ESP32-S3 image and a fully configurable web interface.**

<a href="https://galusperes.github.io/HomeTiles/#demo"><strong>Live demo</strong></a>
&nbsp;·&nbsp;
<a href="https://galusperes.github.io/HomeTiles/"><strong>Documentation</strong></a>
&nbsp;·&nbsp;
<a href="https://github.com/GalusPeres/HomeTiles/releases/latest"><strong>Latest release</strong></a>

<br>

<img src="docs/images/hometiles-supported-devices.png" alt="HomeTiles running on the M5Stack Tab5, Waveshare 8 inch display, and Waveshare 4B" width="92%">

</div>

The project supports multiple ESP32-P4 touch displays plus an experimental
ESP32-S3 image and combines:

- touch-first, tile-based dashboard UI
- MQTT-based Home Assistant integration
- on-device settings: WiFi setup, display, language, firmware updates
- firmware updates directly on the device (GitHub releases) or via the web interface
- full dashboard configuration through the built-in web admin panel

## Requirements

- Home Assistant
- MQTT broker
- The current Home Assistant bridge/integration:
  [HomeTiles Bridge](https://github.com/GalusPeres/HomeTiles-Bridge)

Camera tiles require **HomeTiles Bridge v0.6.28 or newer**. Other tile types do
not depend on the camera protocol.
Local Hardware I/O works directly on the panel; **HomeTiles Bridge v0.6.32 or
newer** is required to expose those assignments in Home Assistant.

New to this? The [Home Assistant Setup Guide](docs/home-assistant-setup.md) walks through
the whole chain: MQTT broker, bridge integration, and connecting the display.

## Documentation

**Full documentation: https://galusperes.github.io/HomeTiles/**

- [Flashing the Firmware](docs/flashing.md) — first installation over USB, factory images
- [Home Assistant Setup Guide](docs/home-assistant-setup.md) — MQTT broker, bridge integration, first connection
- [Bridge Integration](docs/bridge.md) — installation, panel settings, entity configuration
- [Web Admin Panel](docs/web-admin.md) — creating tiles, drag & drop, folders, import/export
- [Local Hardware I/O](docs/hardware-io.md) — GPIO switches, onboard relays, DS18B20 sensors
- [Screensaver](docs/screensaver.md) — microSD images, slideshow, clock, and overlay tiles
- [On-Device UI](docs/device-ui.md) — popups and on-device settings, with screenshots
- [Tile Types](docs/tiles.md) — every tile type and what it needs
- [Firmware Updates](docs/updating.md) — on-device updater, web OTA, factory flash
- [FAQ & Troubleshooting](docs/faq.md) — common questions and known quirks
- [BOARD_SETTINGS.md](BOARD_SETTINGS.md) — Arduino IDE build settings per device

## Highlights Of v0.6.4

- Added configurable local Hardware I/O for Switch outputs, the two onboard
  relays of the ESP32-P4-86-Panel-ETH-2RO, and DS18B20 temperature inputs.
- Added a separate screensaver brightness setting with live preview and correct
  restoration of normal display brightness.
- Kept folders, Weather popups, tile icons, and the web admin responsive with
  bounded caches, navigation preloading, and generated gzip assets.
- Fixed brightness, color, and Kelvin dragging so the 500 ms command limiter
  preserves the newest value and both slider endpoints.
- Hardened ESP-Hosted RPC/SDIO recovery and added persistent diagnostics for the
  remaining upstream P4/C6 WiFi-driver wedge.
- Added a flash-safe RGB restart path for the experimental Guition
  ESP32-4848S040 ESP32-S3 image.

Local hardware entities in Home Assistant require
[HomeTiles Bridge v0.6.32 or newer](https://github.com/GalusPeres/HomeTiles-Bridge/releases/tag/v0.6.32).
Camera support and the ESP32-S3 target remain experimental.

See the [v0.6.4 release notes](docs/releases/v0.6.4.md) for details.

<details>
<summary>Highlights of v0.6.2</summary>

- Added official factory and OTA binaries for the 10.1-inch
  **Guition JC8012P4A1C_I_W_Y**.
- Integrated its display, brightness control, GSL3680 touch, ESP-Hosted WiFi,
  microSD storage and dual-partition OTA.
- Fixed the touch direction and verified MQTT, Home Assistant Bridge data and
  automatic discovery on real hardware.
- Fixed Bridge setup for newly added devices.

Many thanks to [@brmo](https://github.com/brmo) for the original working display
code, real-device testing and detailed feedback.

See the [v0.6.2 release notes](docs/releases/v0.6.2.md) for details.

</details>

<details>
<summary>Highlights of v0.6.0</summary>

- New fully configurable **Climate mini-tile system**: arrange current temperature,
  humidity, heating/cooling targets, target humidity, and mode freely inside a
  Climate tile.
- Climate tiles resize into a consistent slot grid in the web admin, with direct
  mini-tile selection, drag & drop, live resize previews, and matching layouts on
  all three displays.
- The new touch-first Climate popup supports the controls exposed by each Home
  Assistant entity, including temperature ranges, humidity, HVAC mode, presets,
  fan mode, and swing controls.
- Climate defaults and automatic content are capability-aware, so unsupported
  controls stay hidden.
- German and English Climate labels now come from the shared localization layer
  and remain consistent between the web preview and the device UI.
- Network transport and ESP-Hosted SDIO handling were hardened for more reliable
  WiFi/Ethernet operation and safer recovery.

See the [v0.6.0 release notes](docs/releases/v0.6.0.md) for details.

</details>

<details>
<summary>Highlights of the v0.5.x releases</summary>

- Failed on-device GitHub OTA downloads leave a diagnostic report, restart safely,
  and retry from a fresh boot instead of leaving WiFi and the UI stuck.
- The System popup explains that an update may restart the device twice and reminds
  the user to verify the installed version afterwards.
- Added dedicated [OTA troubleshooting](docs/updating.md#troubleshooting-esp32-p4c6-github-downloads)
  for the ESP32-P4/ESP32-C6 HTTPS path and the reliable manual Web Admin fallback.

- Correct controls for CCT-only lights: brightness and color temperature remain
  available while the unsupported color palette stays hidden
- CCT-only light tiles now reflect the selected warm-to-cool color temperature
  instead of always using the generic yellow light color
- Energy and history requests no longer get stuck behind throttled MQTT
  subscriptions on ESP32-P4 devices
- Energy data requests now retry in a controlled way when a response is delayed
  or a queue operation cannot be completed

See the [v0.5.3 release notes](docs/releases/v0.5.3.md) for details.

</details>

<details>
<summary>Highlights of the v0.5.0 and v0.5.1 releases</summary>

- Optional subtle tile borders across the complete normal dashboard, controlled globally under **Settings → Display**
- Cleaner web-admin checkboxes and clearer **Slideshow / Images** terminology in the screensaver editor
- New configurable screensaver with JPEG slideshows from a microSD card, shuffle, global duration, zoom, and focus controls
- Dedicated live screensaver editor in the web admin: freely move and resize the clock and place regular tiles in the bottom two rows
- Optional tile shadows, subtle borders, opacity controls, clock alignment, larger UI fonts, and a consistent Inter-based interface
- Automatic screensaver timeout in the on-device Display settings; tapping any Clock tile still opens it immediately
- Faster hardware-assisted image preparation on all supported ESP32-P4 displays, with stable overlays while slides change
- Web admin improvements including live entity refresh, multi-file uploads, persistent tabs/selections, and screensaver-aware import/export
- Energy tiles now show aggregated Energy Dashboard values in the web preview as well as on the device

See the [screensaver setup guide](docs/screensaver.md) for the required `/images` folder and all controls.

</details>

### Screensaver

<p align="center">
  <img src="docs/images/8in-screensaver.png" alt="HomeTiles screensaver on the Waveshare 8 inch display" width="48%"> <img src="docs/images/web-admin-screensaver.png" alt="Screensaver editor in the HomeTiles web admin" width="48%">
</p>

<details>
<summary>Highlights of the v0.4.x releases</summary>

- Redesigned web admin panel: live tile grid preview, pinned tile settings panel, smoother drag & drop, and per-folder selection memory
- Firmware updates from the browser: the web admin can now run the GitHub update check itself, in addition to the manual file upload
- New on-device WiFi **Disconnect** button (keeps the saved credentials) and a **Pairing** button that re-announces the device to Home Assistant without touching any settings
- Consistent button colors across the device, web admin, and captive portal: green for go-actions, red only for deleting
- Anti-aliased UI rendering — no more jagged edges on switches, sliders, and popup corners
- More reliable on-device GitHub updates: the installer now downloads the image in one pass, fixing a crash that could occur mid-update
- Screenshot export now uses the hardware JPEG encoder
- Built-in crash diagnostics: after a crash the device writes a crash log and keeps a core dump, both downloadable from the web admin — see [FAQ](https://galusperes.github.io/HomeTiles/faq/#the-display-crashed-or-restarted-by-itself) for how to report a crash

</details>

<details>
<summary>Highlights of the v0.3.x releases</summary>

- New in v0.3.1: automatic device pairing — a freshly connected device (no MQTT credentials configured on it yet) announces itself on the network, shows up as a "discovered device" card in Home Assistant, and the bridge pushes your existing MQTT broker's credentials to it automatically once confirmed. No manual host/user/password entry required on the device itself.
- Fixed in v0.3.3: the display could wake itself up out of sleep — without being touched — whenever a background data update arrived. It now only wakes on an actual touch, and tiles stay up to date in the background the whole time it's asleep, so there's no lag when you do wake it.
- Fixed in v0.3.2: saving a tile (or importing a tile layout) into a folder that had never been saved before — most notably the Home screen right after a first-time setup or full factory reset — always failed. Fresh installs work correctly now.
- Project rebranded to **HomeTiles** (formerly ESP32-P4-HomeAssistant-Display) — existing devices keep updating automatically across the rename
- New boot splash screen: logo, firmware version, and device name shown briefly on startup before the dashboard loads
- Polished branding across the on-device System popup and the web admin panel
- More reliable tile storage: tile grids and the folder index are written atomically, avoiding partial/corrupted saves
- Smoother MQTT behavior under load, with traffic throttled during heavy rendering/DMA activity

</details>

<details>
<summary>Highlights of the v0.2.x releases</summary>

- All three supported devices are now covered by every release
- Firmware updates directly from the device: Settings → System checks GitHub for new releases and installs them over the air
- Reworked on-device settings: WiFi network scan with on-screen keyboard, Access Point mode with QR code, display/brightness/sleep options, language and time settings, restart button
- Major rendering performance improvements on the M5Stack Tab5 and the Waveshare 8" display (hardware-accelerated rotation, faster draw paths)
- General UI polish across tiles and popups

</details>

## Overview

This firmware turns supported ESP32-P4 touch displays into configurable Home Assistant control panels.

Everything visible on the dashboard is tile-based and managed from the built-in web interface:
- add, remove, move, and resize tiles
- drag and drop tiles between positions directly in the web interface
- configure tile content and behavior
- create folders and navigation structures
- manage WiFi, MQTT, language, and time zone settings without changing code

## Device Support

### Hardware-confirmed

| Device | Status |
| --- | --- |
| [M5Stack Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4) | Supported |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm) | Supported |
| [Waveshare ESP32-P4-86-Panel-ETH-2RO](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B) | Supported, including native Ethernet; uses the 4B firmware |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-8](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | Supported |
| [Guition JC8012P4A1C_I_W_Y](https://www.guition.com/esp32p4-display-module/hmi-display-panel) | Supported |

### Experimental / community-testing builds

These binaries are included on the release page so owners can test them. A
successful compile does not mean that display, touch, brightness, storage,
networking and OTA have been confirmed on the physical device.

| Exact device | Test status |
| --- | --- |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-7](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | Experimental; feedback in [issue #7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | Experimental; feedback in [issue #7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| [Guition JC1060P470C_I_W_Y](https://www.guition.com/esp32p4-display-module/7-inch-esp32p4-display-module) | Experimental; only the `_I_W_Y` variant, see [issue #8](https://github.com/GalusPeres/HomeTiles/issues/8) |
| [Guition ESP32-4848S040C_I](https://www.guition.com/esp32-display-module/4-inch-esp32s3-display-module) | Experimental ESP32-S3 target; initial hardware testing is positive, while long-duration and OTA coverage are still pending in [issue #9](https://github.com/GalusPeres/HomeTiles/issues/9). Camera tiles are not available on this target. |

Device-specific Arduino IDE settings are documented in [BOARD_SETTINGS.md](BOARD_SETTINGS.md).

## Screenshots

Captured on the Waveshare 8". All targets share the HomeTiles UI and web admin,
with a hardware-specific firmware image for each display profile.

### On The Device

Home dashboard, folder view, and the settings menu:

<p align="center">
  <img src="docs/images/8in-home-new.png" alt="Home dashboard" width="32%"> <img src="docs/images/8in-folder-lighting.png" alt="Folder view with light tiles and scenes" width="32%"> <img src="docs/images/8in-settings.png" alt="On-device settings menu" width="32%">
</p>

Configurable Climate mini-tiles for current values and heating/cooling targets:

<p align="center">
  <img src="docs/images/8in-climate.png" alt="Configurable Climate mini-tile layouts on the Waveshare 8 inch display" width="75%">
</p>

### Popups

Light control — brightness, color, and color temperature:

<p align="center">
  <img src="docs/images/8in-light-brightness.png" alt="Light popup brightness view" width="32%"> <img src="docs/images/8in-light-color.png" alt="Light popup color wheel" width="32%"> <img src="docs/images/8in-light-temperature.png" alt="Light popup color temperature view" width="32%">
</p>

Climate control adapts to each entity — from full HVAC mode, range, humidity,
preset, fan, and swing controls to a minimal heat-only interface:

<p align="center">
  <img src="docs/images/8in-climate-popup-1.png" alt="Full Climate popup with HVAC modes, temperature range, humidity, preset, fan, and swing controls" width="49%"> <img src="docs/images/8in-climate-popup-2.png" alt="Capability-aware heat-only Climate popup" width="49%">
</p>

Energy statistics (day and week) and sensor history:

<p align="center">
  <img src="docs/images/8in-energy-24h.png" alt="Energy popup day view" width="32%"> <img src="docs/images/8in-energy-7d.png" alt="Energy popup week view" width="32%"> <img src="docs/images/8in-sensor-popup-7d.png" alt="Sensor history popup" width="32%">
</p>

Weather forecast, media player, and the system popup with the built-in updater:

<p align="center">
  <img src="docs/images/8in-weather-popup.png" alt="Weather popup" width="32%"> <img src="docs/images/8in-media-popup.png" alt="Media player popup" width="32%"> <img src="docs/images/8in-system-popup.png" alt="System popup with update check and pairing" width="32%">
</p>

### Web Admin Panel

The dashboard is built entirely in the browser — click a tile to edit it, drag & drop to move it, every change saves automatically:

<p>
  <img src="docs/images/web-admin-home.png" alt="Web admin panel with tile grid and tile settings" width="100%">
</p>

WiFi, MQTT, and localization settings without touching code:

<p>
  <img src="docs/images/web-admin-settings.png" alt="Web admin settings tab" width="100%">
</p>

More screenshots and how everything works: [Web Admin Panel](docs/web-admin.md) and [On-Device UI](docs/device-ui.md).

## Features

- Firmware updates directly on the device (checks GitHub releases, installs over the air)
- OTA firmware upload from the built-in web admin panel
- Fully tile-configurable dashboard via the built-in web admin panel
- Local GPIO switches, supported onboard relays, and DS18B20 sensors configured
  through the Hardware tab
- Drag-and-drop tile layout editing in the web admin panel
- Configurable Climate mini-tile layouts with capability-aware controls
- MQTT-based Home Assistant communication
- On-device WiFi setup: network scan with on-screen keyboard, or Access Point mode with QR code
- On-device settings for display brightness, sleep, orientation, language, time zone, and time format
- English and German UI/admin support, Cyrillic tile-title glyphs, and 12h/24h time formats
- Home Assistant energy statistics tile with day and week popup charts
- Media player tile with cover art and playback controls
- microSD file manager in the web admin (upload, download, rename, delete, folders)
- Runtime storage on internal LittleFS; microSD is optional
- Screenshot export to microSD from the web interface
- Tile types currently include: sensor, energy, weather, scene, switch, climate,
  camera, media, folder, clock, text, animation, and empty — see
  [Tile Types](docs/tiles.md)

## Installation

### Option 1: Prebuilt Binaries

Download the files matching your device from the [latest release](https://github.com/GalusPeres/HomeTiles/releases/latest):

| Device | Status | First flash | OTA update file |
| --- | --- | --- | --- |
| M5Stack Tab5 | Supported | `..._m5stacks_tab5_factory.bin` | `..._m5stacks_tab5.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-4B / 86 Panel | Supported | `..._waveshare_4b_factory.bin` | `..._waveshare_4b.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-8 | Supported | `..._waveshare_touch_lcd_8_factory.bin` | `..._waveshare_touch_lcd_8.bin` |
| Guition JC8012P4A1C_I_W_Y | Supported | `..._guition_jc8012p4a1_factory.bin` | `..._guition_jc8012p4a1.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7 | Experimental | `..._waveshare_touch_lcd_7_factory.bin` | `..._waveshare_touch_lcd_7.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 | Experimental | `..._waveshare_touch_lcd_10_1_factory.bin` | `..._waveshare_touch_lcd_10_1.bin` |
| Guition JC1060P470C_I_W_Y | Experimental | `..._guition_jc1060p470c_factory.bin` | `..._guition_jc1060p470c.bin` |
| Guition ESP32-4848S040C_I | Experimental | `..._guition_esp32_4848s040_factory.bin` | `..._guition_esp32_4848s040.bin` |

Use:
- `factory.bin` for a clean first flash (ESP Flash Download Tool at address `0x00000`)
- the plain `.bin` for OTA updates of an existing device (web admin upload)

A manual reset after flashing may be required.

### Option 2: Update From The Device

Devices already running a recent firmware version can update themselves:
open `Settings` → `System` → check for updates. The device finds the
latest GitHub release and installs it directly.

### Option 3: Build From Source

1. Open [HomeTiles.ino](HomeTiles.ino) in the Arduino IDE.
2. Select the target device in [src/devices/device_select.h](src/devices/device_select.h).
3. Apply the correct board settings from [BOARD_SETTINGS.md](BOARD_SETTINGS.md).
4. Build and flash the firmware.

## First Setup

1. Flash the firmware and boot the device.
2. Open `Settings` → `WiFi` on the device. Either:
   - pick your network from the scan list and enter the password with the on-screen keyboard, or
   - enable Access Point mode: connect to the device hotspot (password `12345678`, QR code shown on screen) and enter your WiFi credentials in the captive portal.
3. After saving, the device restarts and connects to your WiFi network.
4. The device IP address is shown in the on-device WiFi settings.
5. Open the web admin panel through that IP address.
6. Install the [HomeTiles Bridge](https://github.com/GalusPeres/HomeTiles-Bridge) integration in Home Assistant, if you haven't already.
7. As long as the device has no MQTT credentials configured on it yet, it announces itself on the network automatically. A "discovered device" card appears under Settings → Devices & Services in Home Assistant — confirm it, and the bridge pushes your existing MQTT broker's credentials to the device for you, no typing required.
   - Alternatively, enter MQTT host/user/password by hand in the device's web admin panel — see the [Home Assistant Setup Guide](docs/home-assistant-setup.md) for the full walkthrough either way.
8. Configure your tiles, folders, and layout.

Optional:
- Insert a FAT32-formatted microSD card if you want to use the file manager or screenshot export from the web interface.

## Home Assistant Integration

This firmware expects the Home Assistant side to be provided by the MQTT bridge/integration:

- [HomeTiles Bridge](https://github.com/GalusPeres/HomeTiles-Bridge)

That integration handles the Home Assistant-side MQTT communication and entity bridge.
For Energy tiles, Home Assistant energy statistics, live icon updates, and popup history, use the current bridge release.
Camera tiles specifically require HomeTiles Bridge v0.6.28 or newer.

Step-by-step instructions (broker, integration, display): [Home Assistant Setup Guide](docs/home-assistant-setup.md)

## Repository Structure

- `src/` firmware source code
- `docs/images/` screenshots and documentation images
- `mdi-extractor/` icon tooling
- `BOARD_SETTINGS.md` documented Arduino IDE board settings

## Known Issues

- M5Stack Tab5: Access Point mode is currently only reliable with a battery installed. Without a battery, keep brightness at the lowest available level; otherwise the device can crash. (Since v0.2.9 the firmware automatically caps the backlight around AP start and WiFi reconnects to prevent brownouts.)
- Waveshare 4B / 8": the display can briefly flash blue whenever the firmware writes to internal flash (saving tile edits, OTA installs). This is a cosmetic MIPI-DSI underrun — the panel framebuffer lives in PSRAM, and flash writes stall PSRAM access. The precompiled Arduino core does not enable `CONFIG_SPIRAM_XIP_FROM_PSRAM`, which would fix this; it cannot be enabled from the sketch.
- Camera tiles are experimental and available only on ESP32-P4 targets. The
  Bridge transcodes video in Home Assistant, so CPU use increases with source
  resolution, requested frame rate and the number of simultaneously open panels.
- The Guition ESP32-4848S040 ESP32-S3 image remains experimental. Community
  testing confirms smooth operation with the flash-safe display fix, but boot,
  OTA, storage, and long-duration behavior still need broader validation.

## Notes

- A microSD card is not required for normal operation; it is only used for the web file manager and screenshot export.
- Board selection and board settings must match the target device.
## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

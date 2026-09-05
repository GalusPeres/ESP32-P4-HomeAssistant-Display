<div align="center">

# <img src="docs/images/favicon.svg" width="42" alt="" align="top"> HomeTiles

**Tile-based ESP32-P4 and ESP32-S3 firmware for Home Assistant dashboards<br>with a fully configurable web interface.**

<a href="https://galusperes.github.io/HomeTiles/#demo"><strong>Live demo</strong></a>
&nbsp;·&nbsp;
<a href="https://galusperes.github.io/HomeTiles/"><strong>Documentation</strong></a>
&nbsp;·&nbsp;
<a href="https://github.com/GalusPeres/HomeTiles/releases/latest"><strong>Latest release</strong></a>

<p>
  <a href="https://buymeacoffee.com/galusperes">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" width="217" height="60">
  </a>
</p>

<img src="docs/images/hometiles-supported-devices.png" alt="HomeTiles running on the M5Stack Tab5, Waveshare 8 inch display, and Waveshare 4B" width="92%">

</div>

The project supports multiple ESP32-P4 and ESP32-S3 touch displays and combines:

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
Cover tiles require **HomeTiles Bridge v0.6.35 or newer**.

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

## Highlights Of v0.6.9

- Added dedicated Binary Sensor tiles with localized device-class states,
  state-aware icons, availability handling, and matching live Web Admin
  previews and autosave.
- Added 24-hour and 7-day categorical timelines with date-grouped Activity for
  Binary Sensors and text-valued Sensor entities; numeric Sensors keep their
  existing chart.
- Added release and browser-installer support for the Waveshare ESP32-P4 4.3-inch.
- Fixed a Waveshare 8-inch PPA timeout caused by thin Activity-list redraws and
  corrected Waveshare 4B display, screensaver, and Home Assistant brightness
  scaling to the visible `1–100 %` range.
- Improved ESP32-S3 update/render work and Web Admin accessibility and safety.

[HomeTiles Bridge v0.6.40](https://github.com/GalusPeres/HomeTiles-Bridge/releases/tag/v0.6.40)
is the recommended companion release and is required for the new Binary Sensor
and categorical Sensor history. Camera support remains experimental and is
available only on ESP32-P4 targets.

See the [v0.6.9 release notes](docs/releases/v0.6.9.md) for details.

<details>
<summary>Highlights of v0.6.8</summary>

- Stabilized Camera presentation across native ESP32-P4 DSI profiles with
  protected double buffering and persistent diagnostics before a safe restart.
- Fixed Waveshare 7B/7B-C startup and published separate pre-v3 and exact-v3.1
  release images.
- Corrected the Waveshare 10.1-inch and Guition JC1060 V2 defaults.

See the [v0.6.8 release notes](docs/releases/v0.6.8.md) for details.

</details>

<details>
<summary>Highlights of v0.6.4</summary>

- Added configurable local hardware I/O for switches, relays and DS18B20
  sensors, including matching Home Assistant entities through the Bridge.
- Improved folder navigation, Weather popups, tile restoration and Web Admin
  loading during longer sessions.
- Made Light brightness, color and Kelvin sliders reliably deliver their newest
  value while remaining rate-limited.
- Improved ESP-Hosted RPC/SDIO recovery and diagnostics.

See the [v0.6.4 release notes](docs/releases/v0.6.4.md) for details.

</details>

<details>
<summary>Highlights of v0.6.3</summary>

- Added the experimental Camera tile and popup for ESP32-P4 displays.
- Improved Light brightness, color and color-temperature interaction.
- Added modern Cyrillic glyphs to the common tile-title fonts.
- Strengthened ESP-Hosted receive handling and camera buffer cleanup.

See the [v0.6.3 release notes](docs/releases/v0.6.3.md) for details.

</details>

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
<summary>Highlights of v0.6.1</summary>

- Unified DHCP and static-IP settings across WiFi, native Ethernet and USB
  Ethernet.
- Added safe on-device DHCP recovery and undo actions.
- Changed Network and MQTT Web Admin saves to update in the background without
  disruptive full-page reloads.
- Polished network settings and Light popup spacing.

See the [v0.6.1 release notes](docs/releases/v0.6.1.md) for details.

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
| [Guition JC8012P4A1C_I_W_Y](https://www.guition.com/esp32p4-display-module/hmi-display-panel) | Supported V1 panel; no `V2` suffix on the rear material number |
| [Guition ESP32-4848S040C_I](https://www.guition.com/esp32-display-module/4-inch-esp32s3-display-module) | Supported ESP32-S3 target; Camera tiles are unavailable |
| [Waveshare ESP32-S3-Touch-LCD-4 Rev 4.0](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4) | Community hardware test reported in [PR #35](https://github.com/GalusPeres/HomeTiles/pull/35); first release pending; Camera tiles and SD access are unavailable in this profile |

For the Waveshare S3 LCD-4, contributor @verrat73 reported working display,
backlight dimming, touch, Wi-Fi, Web Admin, MQTT, HomeTiles Bridge and Web OTA.
Support covers **Rev 4.0 only**, with the CH32V003 helper at `0x24`; revisions
1.0–3.0 are not supported. The similarly named LCD-4B uses a separate profile.
The board has a microSD slot, but HomeTiles uses internal LittleFS on this
profile. See the [device notes](src/devices/waveshare_s3_touch_lcd_4/README.md).

### Hardware validation notes

These binaries are included on the release page so owners can test them. A
successful compile does not mean that display, touch, brightness, storage,
networking and OTA have been confirmed on the physical device.

| Exact device | Test status |
| --- | --- |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4.3.htm) | ESP32-P4 v1.3 hardware confirms display, touch, Wi-Fi, Web Admin, tile persistence, MQTT, Home Assistant discovery and Weather; microSD and OTA validation remain pending |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-7](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | Physical display, touch, storage, networking and OTA confirmation requested in [issue #7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7B.htm) | Separate 1024×600 EK79007 images for pre-v3 revisions 1–199 and exact v3.1; both still need physical confirmation after the startup fix, and the experimental v3.1 path has no exact-hardware validation, in [issue #7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | Display, touch, Wi-Fi, MQTT and OTA were reported working; corrected default orientation, brightness floor, SD and Camera behavior still need release confirmation in [issue #7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| Guition JC8012P4A1 V2 (`SKU:10153002-V2`) | Separate V2 panel image with hardware-tested JD9365 table/timing; full release/OTA validation pending in [issue #18](https://github.com/GalusPeres/HomeTiles/issues/18) |
| [Guition JC1060P470C_I_W_Y V1](https://www.guition.com/esp32p4-display-module/7-inch-esp32p4-display-module) | Only the `_I_W_Y` variant; SD-card behavior still requires physical confirmation in [issue #8](https://github.com/GalusPeres/HomeTiles/issues/8) |
| Guition JC1060P470C V2 / New Panel | Basic operation was reported working; v0.6.8 corrects the reported default orientation and exact LCD-reset/GPIO mapping, with full release/OTA confirmation tracked in [issue #27](https://github.com/GalusPeres/HomeTiles/issues/27) |
| [Waveshare ESP32-S3-Touch-LCD-4B](https://www.waveshare.com/esp32-s3-touch-lcd-4b.htm) | ESP32-S3 profile without microSD or Camera tiles; physical validation requested in [issue #26](https://github.com/GalusPeres/HomeTiles/issues/26) |

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

Energy statistics, numeric Sensor charts, and categorical Sensor history:

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
  through the I/O tab
- Drag-and-drop tile layout editing in the web admin panel
- Configurable Climate mini-tile layouts with capability-aware controls
- MQTT-based Home Assistant communication
- On-device WiFi setup: network scan with on-screen keyboard, or Access Point mode with QR code
- On-device settings for display brightness, sleep, orientation, language, time zone, and time format
- English and German UI/admin support, Cyrillic tile-title glyphs, and 12h/24h time formats
- Home Assistant energy statistics tile with day and week popup charts
- Home Assistant Binary Sensor tiles with localized states, dynamic icons, and
  24-hour/7-day Activity history
- Home Assistant Cover tiles with position, tilt and feature-aware controls
- Media player tile with cover art and playback controls
- microSD file manager in the web admin (upload, download, rename, delete, folders)
- Runtime storage on internal LittleFS; microSD is optional
- Screenshot export to microSD from the web interface
- Tile types currently include: sensor, binary sensor, energy, weather, scene, switch, cover,
  climate, camera, media, folder, clock, text, animation, and empty — see
  [Tile Types](docs/tiles.md)

## Installation

### Option 1: Prebuilt Binaries

Download the files matching your device from the [latest release](https://github.com/GalusPeres/HomeTiles/releases/latest):

| Device | Status | First flash | OTA update file |
| --- | --- | --- | --- |
| M5Stack Tab5 | Supported | `..._m5stacks_tab5_factory.bin` | `..._m5stacks_tab5.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-4B / 86 Panel | Supported | `..._waveshare_4b_factory.bin` | `..._waveshare_4b.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 | Core hardware validated; microSD/OTA pending | `..._waveshare_touch_lcd_4_3_factory.bin` | `..._waveshare_touch_lcd_4_3.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-8 | Supported | `..._waveshare_touch_lcd_8_factory.bin` | `..._waveshare_touch_lcd_8.bin` |
| Guition JC8012P4A1C_I_W_Y V1 (no V2 sticker) | Supported | `..._guition_jc8012p4a1_factory.bin` | `..._guition_jc8012p4a1.bin` |
| Guition JC8012P4A1 V2 (`SKU:10153002-V2`) | Hardware validation pending | `..._guition_jc8012p4a1_v2_factory.bin` | `..._guition_jc8012p4a1_v2.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7 | Hardware validation pending | `..._waveshare_touch_lcd_7_factory.bin` | `..._waveshare_touch_lcd_7.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C, ESP32-P4 before v3.0 (revisions 1–199) | Hardware validation pending | `..._waveshare_touch_lcd_7b_factory.bin` | `..._waveshare_touch_lcd_7b.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C, exact ESP32-P4 v3.1 | Experimental; exact hardware unverified | `..._waveshare_touch_lcd_7b_rev3_1_factory.bin` | `..._waveshare_touch_lcd_7b_rev3_1.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 | Hardware validation pending | `..._waveshare_touch_lcd_10_1_factory.bin` | `..._waveshare_touch_lcd_10_1.bin` |
| Guition JC1060P470C_I_W_Y V1 | SD-card validation pending | `..._guition_jc1060p470c_factory.bin` | `..._guition_jc1060p470c.bin` |
| Guition JC1060P470C V2 / New Panel | Hardware validation pending | `..._guition_jc1060p470c_v2_factory.bin` | `..._guition_jc1060p470c_v2.bin` |
| Guition ESP32-4848S040C_I | Supported | `..._guition_esp32_4848s040_factory.bin` | `..._guition_esp32_4848s040.bin` |
| Waveshare ESP32-S3-Touch-LCD-4 Rev 4.0 | Community-tested; first release pending | `..._waveshare_s3_touch_lcd_4_factory.bin` | `..._waveshare_s3_touch_lcd_4.bin` |
| Waveshare ESP32-S3-Touch-LCD-4B | Hardware validation pending; no microSD | `..._waveshare_s3_touch_lcd_4b_factory.bin` | `..._waveshare_s3_touch_lcd_4b.bin` |

Waveshare ESP32-S3-Touch-LCD-4 Rev 4.0 images are scheduled for the next
release and are **not included in v0.6.9**. Until then, build the dedicated
`waveshare_s3_touch_lcd_4` profile from source.

Use:
- `factory.bin` for a clean first flash (ESP Flash Download Tool at address `0x00000`)
- the plain `.bin` for OTA updates of an existing device (web admin upload)

For Waveshare 7B/7B-C, select either **ESP32-P4 before v3.0 (revisions 1–199)**
or **exact ESP32-P4 v3.1 (experimental)** in the
[browser installer](docs/installer.md). That selection determines the firmware
file. The browser installer, Web Admin upload, and on-device updater enforce the
exact revision contract; they never turn the v3.1 image into an all-revision
image. ESP32-P4 v3.2 or newer is unsupported with Arduino-ESP32 3.3.7 / ESP-IDF
5.5.2 and must not be flashed. Direct `esptool` flashing is unsafe for that case:
the Arduino `v3.00 or newer` build can carry a broader ESP image-header range of
301–399, bypassing the narrower HomeTiles v3.1-only check.

The other current ESP32-P4 profiles use vendor-listed P4NRW32/pre-v3 modules.
Their HomeTiles release images are explicitly guarded to revisions 1–199 and
are not generic all-revision P4 firmware.

A manual reset after flashing may be required.

### Option 2: Update From The Device

Devices already running a recent firmware version can update themselves:
open `Settings` → `System` → check for updates. The device finds the
latest GitHub release and installs it directly.

### Option 3: Build From Source

1. Install Node.js/npm on the build computer. From the repository root, prepare
   the locked host tools and generated web assets, then run the host tests:

   ```text
   npm ci --ignore-scripts
   node tools/generate-web-assets.mjs
   node tools/run-tests.mjs
   ```

2. Open [HomeTiles.ino](HomeTiles.ino) in the Arduino IDE.
3. Select the target device in [src/devices/device_select.h](src/devices/device_select.h).
4. Apply the correct board settings from [BOARD_SETTINGS.md](BOARD_SETTINGS.md).
5. Build and flash the firmware.

Node.js/npm is needed only on the build computer. The firmware serves its web
assets directly; devices and browsers need no npm installation. See
[CONTRIBUTING.md](CONTRIBUTING.md) for the local build helper and validation.

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
For Binary Sensor tiles, categorical Sensor history, Energy statistics, live
icon updates, and popup history, use HomeTiles Bridge v0.6.40 or newer.
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
- The Guition ESP32-4848S040, Waveshare ESP32-S3-Touch-LCD-4 Rev 4.0, and
  Waveshare ESP32-S3-Touch-LCD-4B targets do not support Camera tiles; use the
  exact S3 factory or OTA image listed above.

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md) for the device and tile extension
workflows, generated-source commands, tests and firmware size checks.
[ARCHITECTURE.md](ARCHITECTURE.md) maps subsystem ownership, runtime data flow
and the next maintenance priorities. Browser source lives in `src/web/admin/`
and beside each tile type; `src/web/assets/admin.js` remains a readable assembly.
Pinned host tooling formats and validates a smaller delivery copy before gzip,
without adding a browser loader or runtime dependency. Build/release/installer
identities come from `tools/device-profiles.json`, while exact hardware remains
in its own driver.

## Notes

- A microSD card is not required for normal operation; it is only used for the web file manager and screenshot export.
- Board selection and board settings must match the target device.

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE).

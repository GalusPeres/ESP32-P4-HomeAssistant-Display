---
title: ESP32 Touch Dashboard for Home Assistant
description: Open-source firmware for configurable Home Assistant touch dashboards on ESP32-P4 and ESP32-S3 displays, with built-in web admin and MQTT integration.
---

# HomeTiles

Tile-based firmware that turns ESP32-P4 and ESP32-S3 touch displays into Home
Assistant control panels — configured entirely in the browser, updated over the
air, connected via MQTT.

<p align="center">
  <a href="https://buymeacoffee.com/galusperes">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me a Coffee" width="217" height="60">
  </a>
</p>

<p align="center">
  <img src="images/8in-home-new.png" alt="HomeTiles dashboard on the Waveshare 8 inch display" width="48%">
  <img src="images/8in-screensaver.png" alt="HomeTiles screensaver with clock and sensor tiles" width="48%">
</p>

## Demo

<video class="ht-demo" controls playsinline preload="metadata" poster="images/hometiles-demo-poster.jpg" aria-label="HomeTiles device demo">
  <source src="videos/hometiles-demo.mp4" type="video/mp4">
</video>

## New Here? Four Steps

<div class="ht-steps" markdown>

1.  **Flashing the Firmware**

    Install or update HomeTiles directly here in the browser. Select the exact
    device, then choose Update to keep its settings or First install / factory
    reset for a clean installation.

    [Flashing the Firmware :octicons-arrow-right-24:](installer.md)

2.  **Connect everything**

    Set up the MQTT broker, install the bridge integration, and pair the
    display with Home Assistant.

    [Home Assistant Setup :octicons-arrow-right-24:](home-assistant-setup.md)

3.  **Build your dashboard**

    Open the display's admin panel in your browser: click a cell, pick a tile
    type, done. Drag & drop, folders, everything saves automatically.

    [Web Admin Panel :octicons-arrow-right-24:](web-admin.md)

4.  **Use the display**

    Control lights with a color wheel, check sensor history, energy statistics,
    weather, and media — all in touch popups on the device.

    [On-Device UI :octicons-arrow-right-24:](device-ui.md)

</div>

Looking for something specific? [Tile Types](tiles.md) ·
[Local Hardware I/O](hardware-io.md) · [Screensaver](screensaver.md) ·
[Firmware Updates](updating.md) ·
[FAQ & Troubleshooting](faq.md) ·
[GitHub](https://github.com/GalusPeres/HomeTiles)

## New In v0.6.9

HomeTiles v0.6.9 adds dedicated Binary Sensor tiles with localized states,
state-aware icons, live Web Admin previews, and 24-hour/7-day Activity history.
Text-valued Sensor entities use the same categorical timeline while numeric
Sensors retain their chart.

The release adds Waveshare ESP32-P4 4.3-inch support, fixes a thin-redraw PPA
timeout on the Waveshare 8-inch, and maps Waveshare 4B display, screensaver, and
Home Assistant brightness to the complete visible `1–100 %` range. **HomeTiles
Bridge v0.6.40** is recommended for the new history features. Camera support
remains experimental and ESP32-P4-only.

[Read the v0.6.9 release notes :octicons-arrow-right-24:](releases/v0.6.9.md)

## Device Support

![HomeTiles running on three ESP32-P4 displays](images/hometiles-supported-devices.png){ width="100%" .ht-hero }

### Hardware-confirmed

| Device | Display | Status |
| --- | --- | --- |
| [M5Stack Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4) | 5" 1280×720 | Supported |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm) | 4" 720×720 | Supported |
| [Waveshare ESP32-P4-86-Panel-ETH-2RO](https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-4B) | 4" 720×720 | Supported, native Ethernet; uses the 4B firmware |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-8](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | 8" 1280×800 | Supported |
| [Guition JC8012P4A1C_I_W_Y](https://www.guition.com/esp32p4-display-module/hmi-display-panel) | 10.1" 1280×800 | Supported V1 panel; no `V2` suffix |
| [Guition ESP32-4848S040C_I](https://www.guition.com/esp32-display-module/4-inch-esp32s3-display-module) | 4" 480×480 | Supported ESP32-S3 target; no Camera tiles |
| [Waveshare ESP32-S3-Touch-LCD-4 Rev 4.0](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4) | 4" 480×480 | Community hardware test reported in [PR #35](https://github.com/GalusPeres/HomeTiles/pull/35); first release pending; Camera tiles and SD access unavailable in this profile |

### Hardware validation notes

| Exact device | Display | Test status |
| --- | --- | --- |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4.3.htm) | 4.3" 800×480 landscape | ESP32-P4 v1.3 hardware confirms display, touch, Wi-Fi, Web Admin, tile persistence, MQTT, Home Assistant discovery and Weather; microSD and OTA remain pending |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-7](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | 7" 1280×720 | [Testing requested in #7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7B.htm) | 7" 1024×600 | Explicit pre-v3 revisions 1–199 and experimental exact-v3.1 EK79007 images; exact-v3.1 hardware is unverified and testing is requested in [#7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | 10.1" 1280×800 | Core display/touch/network/OTA reported working; corrected defaults, SD and Camera checks remain in [#7](https://github.com/GalusPeres/HomeTiles/issues/7) |
| Guition JC8012P4A1 V2 (`SKU:10153002-V2`) | 10.1" 1280×800 | Separate V2 image; [release/OTA testing tracked in #18](https://github.com/GalusPeres/HomeTiles/issues/18) |
| [Guition JC1060P470C_I_W_Y V1](https://www.guition.com/esp32p4-display-module/7-inch-esp32p4-display-module) | 7" 1024×600 | `_I_W_Y` only; [testing requested in #8](https://github.com/GalusPeres/HomeTiles/issues/8) |
| Guition JC1060P470C V2 / New Panel | 7" 1024×600 | Basic operation reported working; corrected orientation and full release/OTA validation tracked in [issue #27](https://github.com/GalusPeres/HomeTiles/issues/27) |
| [Waveshare ESP32-S3-Touch-LCD-4B](https://www.waveshare.com/esp32-s3-touch-lcd-4b.htm) | 4" 480×480 | Separate ESP32-S3 profile without microSD; validation tracked in [issue #26](https://github.com/GalusPeres/HomeTiles/issues/26) |

The release pipeline now covers 15 explicit installer/release profiles for
fourteen physical device profiles, producing 15 builds and 30 firmware files
for the next release. Published v0.6.9 still contains 14 profiles and 28 files;
the browser installer mirrors only assets present in that release.

The new Waveshare S3 LCD-4 profile covers **Rev 4.0 only**, with the CH32V003
helper at `0x24`. Revisions 1.0–3.0 are unsupported, and the S3 LCD-4B uses a
separate image. The board has a microSD slot, but HomeTiles uses LittleFS on
this profile. See [flashing and availability](flashing.md).

The Waveshare 7B/7B-C has separate pre-v3 revisions 1–199 and exact-v3.1
entries. Other current P4 profiles use vendor-listed
P4NRW32/pre-v3 modules and are also guarded to revisions 1–199. HomeTiles'
browser, Web Admin, and OTA paths enforce these ranges; v3.2 or newer is not
supported. The notes above identify profiles whose complete hardware checklist
still needs confirmation.

## How It Works

<div class="ht-flow">
  <span class="ht-node">Display</span>
  <span class="ht-link">←&thinsp;MQTT&thinsp;→</span>
  <span class="ht-node">MQTT Broker</span>
  <span class="ht-link">←&thinsp;MQTT&thinsp;→</span>
  <span class="ht-node">Bridge Integration<small>Home Assistant</small></span>
</div>

The display never talks to Home Assistant directly. The
[bridge integration](bridge.md) pushes entity states, icons, weather, history,
and energy data over MQTT — and executes the commands the display sends back.
Firmware and bridge are MIT-licensed and developed together.

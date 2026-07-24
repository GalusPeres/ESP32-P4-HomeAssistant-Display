---
title: ESP32-P4 Touch Dashboard for Home Assistant
description: Open-source firmware for configurable Home Assistant touch dashboards on ESP32-P4 displays, with a built-in web admin and MQTT integration.
---

# HomeTiles

Tile-based firmware that turns ESP32-P4 touch displays into Home Assistant control
panels — configured entirely in the browser, updated over the air, connected via MQTT.

<p align="center">
  <img src="images/8in-home.png" alt="HomeTiles dashboard on the Waveshare 8 inch display" width="48%">
  <img src="images/8in-screensaver.png" alt="HomeTiles screensaver with clock and sensor tiles" width="48%">
</p>

## Demo

<video class="ht-demo" controls playsinline preload="metadata" poster="images/hometiles-demo-poster.jpg" aria-label="HomeTiles device demo">
  <source src="videos/hometiles-demo.mp4" type="video/mp4">
</video>

## New Here? Four Steps

<div class="ht-steps" markdown>

1.  **Flash the firmware**

    Download the prebuilt binaries for your device and flash them once over
    USB — every update after that installs over the air.

    [Flashing the Firmware :octicons-arrow-right-24:](flashing.md)

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
[Screensaver](screensaver.md) · [Firmware Updates](updating.md) ·
[FAQ & Troubleshooting](faq.md) ·
[GitHub](https://github.com/GalusPeres/HomeTiles)

## New In v0.6.1

Network configuration is now consistent across WiFi and Ethernet. Both
transports share the same DHCP/static-IP profile, and static addressing is
supported by WiFi, native Ethernet, and USB Ethernet.

The on-device network popup provides a DHCP recovery action only when the device
actually started with a static address. Web Admin saves no longer reload the
complete page, and an open network popup refreshes cleanly without stale display
artifacts. Navigation controls in the light popup now have more even spacing.

[Read the v0.6.1 release notes :octicons-arrow-right-24:](releases/v0.6.1.md)

## Supported Devices

![HomeTiles running on all three supported ESP32-P4 displays](images/hometiles-supported-devices.png){ width="100%" .ht-hero }

| Device | Display |
| --- | --- |
| [M5Stack Tab5](https://shop.m5stack.com/products/m5stack-tab5-iot-development-kit-esp32-p4) | 5" 1280×720 |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-4B](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-4b.htm) | 4" 720×720 |
| [Waveshare ESP32-P4-WIFI6-Touch-LCD-8](https://www.waveshare.com/esp32-p4-wifi6-touch-lcd-7-8-10.1.htm) | 8" 1280×800 |

The same firmware runs on all devices; every release ships prebuilt binaries for each.

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

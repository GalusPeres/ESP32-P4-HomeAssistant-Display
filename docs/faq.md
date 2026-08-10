# FAQ & Troubleshooting

## Tiles show no data

Work through this checklist:

1. Is the entity selected in the bridge integration options? (**Settings → Devices &
   Services → HomeTiles Bridge → Configure → Entity Configuration**)
2. Are the display's MQTT settings correct (host, port, credentials)? Check the web admin.
3. Do the base topic and HA prefix match between the display and the integration entry?
4. Is the panel listed as a device under the bridge integration? If not, tap
   **Settings → System → Pairing** on the display — it re-announces itself.

More details in the [Home Assistant Setup Guide](home-assistant-setup.md).

## The Camera tile asks for a newer Bridge or never shows video

Camera tiles require HomeTiles firmware v0.6.3 and HomeTiles Bridge v0.6.28 or
newer. Update the bridge through HACS, restart Home Assistant, and verify the
installed integration version before testing again.

Then check:

1. The camera is selected under **HomeTiles Bridge → Configure → Entity
   Configuration**.
2. The display can reach the Home Assistant host on TCP ports `8124`–`8131`;
   these are local camera data ports, not MQTT ports.
3. The camera itself works in Home Assistant.
4. The Home Assistant log does not show an FFmpeg or camera-source error.

Direct video sources can reach up to 24 FPS. Snapshot-only cameras update at
the source's actual refresh rate. Transcoding uses Home Assistant host CPU and
each simultaneously open panel creates its own experimental stream session.
Camera tiles are not available on the ESP32-S3 Guition ESP32-4848S040C_I.

## A local Hardware entity is missing in Home Assistant

First verify that the assignment was saved in the panel's web-admin **I/O**
tab. It should already be selectable by a tile on that same panel; this local
path does not need Home Assistant.

Home Assistant discovery requires HomeTiles Bridge v0.6.32 or newer and an MQTT
connection. After updating the Bridge, restart Home Assistant once, save the
I/O page again or use **Settings → System → Pairing**, and check the entity
list of that panel device. A second identical panel is still a separate device;
Home Assistant may add its normal `_2` suffix to a colliding visible entity ID.

## The ESP32-S3 screen briefly goes black while saving

This is expected on the Guition ESP32-4848S040 build. The stock
Arduino SDK cannot feed the RGB panel safely from PSRAM while internal flash is
being written. HomeTiles briefly blanks the backlight, writes the data, restarts
RGB DMA on VSYNC, and then restores the image. This avoids the permanently
shifted picture seen by early testers.

If the image does not recover immediately, report the repeatable failure in
[issue #9](https://github.com/GalusPeres/HomeTiles/issues/9) with the serial log.

## The display is missing in Home Assistant / I deleted it there

Tap **Settings → System → Pairing** on the display: it reconnects MQTT and republishes
its discovery data, and the device reappears under the bridge integration. If you
deleted the device in Home Assistant, do that first — Home Assistant ignores discovery
from device IDs it still knows.

## The display briefly flashes blue when saving or updating (Waveshare)

Cosmetic and harmless. The display panel is refreshed continuously from PSRAM; while the
firmware writes to internal flash (saving tile edits, installing updates), that refresh
briefly stalls and the panel shows a solid color. The underlying fix is an ESP-IDF build
option (`CONFIG_SPIRAM_XIP_FROM_PSRAM`) that the precompiled Arduino core does not enable,
so it currently cannot be fixed from this project's code.

## The Tab5 dims itself when enabling AP mode or after a reboot

Intentional. Full backlight plus a WiFi radio burst can trip the Tab5's brownout detector
(hard reset). Since v0.2.9 the firmware caps the backlight around these moments and
restores your configured brightness automatically once WiFi is connected.

## Ghost images / shadows of previous content (Waveshare)

Temporary LCD image retention, typical for these panels: static high-contrast content
(white text on dark tiles) leaves faint ghosts, most visible on grey surfaces and on a
cold panel. It fades as the display warms up and is not permanent burn-in. A shorter
display sleep timeout reduces it.

## The display crashed or restarted by itself

The firmware records crash diagnostics automatically: after a crash, the next boot
appends the reset reason and a summary (crashed task, program counter, registers) to a
crash log, and the full crash state is kept in flash as a core dump. Please report it —
these files are exactly what's needed to find and fix the bug:

1. Open the web admin panel and go to **Screenshot & Diagnostics**.
2. Click **Download crash log** to get `crashlog.txt`.
3. If a stored core dump is shown, download it too and **pack the `.bin` into a
   `.zip`** — GitHub does not accept raw `.bin` attachments.
4. [Open an issue](https://github.com/GalusPeres/HomeTiles/issues) describing what the
   display was doing when it crashed (which firmware version, which popup or action, how
   often it happens), and attach both files by dragging them into the issue text box.

!!! note "What's in a core dump?"
    A snapshot of the firmware's working memory at the moment of the crash. It can
    contain things currently on screen or in memory — tile titles, entity names, sensor
    values. If you'd rather not share that, attach only the crash log; it already
    narrows down the crash location.

## The screen goes black during a web admin OTA upload

Intentional — the display is suspended during the transfer to free memory. The device
restarts when the installation finishes. The on-device updater (Settings → System) keeps
the screen on and shows a progress bar instead.

## The update check fails even though WiFi works

The HTTPS connection to GitHub needs a contiguous block of free working memory. After a
long uptime the memory can be too fragmented for it, and the check fails with "check
failed" while everything else keeps working. **Restarting the display fixes it** — use
the restart button in the on-device System popup or in the web admin, then run the
check again right after boot.

The firmware frees memory automatically before the check. If it still fails on a
current version, please [report it](#the-display-crashed-or-restarted-by-itself).

## WiFi or MQTT stops responding and the panel restarts

ESP32-P4 WiFi uses ESP-Hosted communication with the panel's ESP32-C6. A rare
upstream RPC/SDIO driver wedge can leave the cached WiFi link looking connected
after MQTT has already stopped. It has also occurred without OTA and without an
open Camera popup, so Camera activity in the report is diagnostic context, not
proof of the cause.

v0.6.4 adds response routing, more SDIO recovery paths, persistent counters, and
a safe restart which resets both chips if the driver no longer responds. This is
a mitigation and diagnostic improvement, not a claim that the upstream fault is
fully fixed. After a restart, open **Settings → System → Download crash log** in
the web admin and include the exact device, firmware version, uptime, and report
when opening a GitHub issue.

## The GitHub update download fails or the device restarts during it

ESP32-P4 displays use a separate ESP32-C6 WiFi coprocessor through ESP-Hosted/SDIO.
After a long uptime, the large GitHub HTTPS/TLS download can occasionally stop with
`connection lost` or an `esp-aes` allocation error. The exact interaction is still
under investigation.

Since v0.5.6 the device records the failure, safely restarts, and retries the
update from a fresh boot. The display may therefore still restart instead of
remaining permanently offline. When it has settled, open **Settings → System**
and check whether the new version is shown. The separate WiFi/MQTT section above
explains the additional v0.6.4 diagnostics.

If the automatic retry still fails, the user must download the matching plain OTA
`.bin` from the [release page](https://github.com/GalusPeres/HomeTiles/releases) and
upload it manually in the Web Admin Firmware section. Do not use the `_factory.bin`.
This manual path avoids the large outbound GitHub TLS stream on the display. See
[Firmware Updates](updating.md#troubleshooting-esp32-p4c6-github-downloads) for the
complete steps.

## The update check says "up to date" but I expected an update

The device compares its own version (shown in Settings → System) against the latest
GitHub release tag. If a release was just published, wait a moment and check again. If an
install fails repeatedly, download the OTA file and use the web admin upload instead —
see [Firmware Updates](updating.md).

## MQTT disconnects during updates

Intentional. Update installs temporarily disconnect MQTT and stop the web admin to free
memory. Everything reconnects automatically afterwards; Home Assistant data resyncs on
reconnect.

## AP mode basics

- Password: `12345678` (shown with a QR code on the display)
- The access point switches itself off after 10 minutes without a saved configuration
- While AP mode is active, MQTT and the web admin are unavailable

## What happens if Home Assistant or the MQTT broker is offline?

The display keeps running and retries the broker every few seconds in the background;
the UI stays fully responsive. Once the broker is reachable again, it reconnects and
resyncs all entity states automatically.

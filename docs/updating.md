# Firmware Updates

There are four ways to get firmware onto a device. For normal operation you only ever
need the first one.

## 1. On-Device Updater (recommended)

Open **Settings → System** on the display and tap **Check for updates**. The device
looks up the latest [GitHub release](https://github.com/GalusPeres/HomeTiles/releases/latest),
and if a newer version exists, offers to install it. The download and installation run
directly on the device with a progress bar; afterwards it restarts into the new version.

![System popup with the update check](images/8in-system-popup.png){ width="65%" }

Notes:
- The device briefly disconnects from MQTT and stops the web admin during the install —
  both come back automatically.
- Since v0.5.6, a failed GitHub download is recorded and the device safely restarts before
  retrying it from a fresh boot. The update can therefore cause more than one restart.
  After the device has settled, open **Settings → System** again and verify the installed
  firmware version.
- If an install still fails, nothing is lost: the unchanged active firmware remains
  bootable. Use the manual Web Admin upload described below.
- If the update **check** itself fails even though WiFi works, restart the display and
  check again — see the [FAQ](faq.md#the-update-check-fails-even-though-wifi-works).

### Troubleshooting ESP32-P4/C6 GitHub downloads

On supported ESP32-P4 displays, WiFi is provided by a separate ESP32-C6 coprocessor
connected through ESP-Hosted/SDIO. After a long uptime, a large outbound GitHub
HTTPS/TLS download can occasionally fail with messages such as `connection lost` or
`esp-aes: Failed to allocate memory`. The exact underlying ESP-Hosted/TLS interaction is
still under investigation; this is not caused by selecting the wrong OTA partition.

The v0.5.6 updater handles this failure safely by restarting and retrying from a fresh
boot. If the automatic retry does not complete the update, the user must perform the
manual Web Admin upload:

1. Open the target version on the
   [GitHub releases page](https://github.com/GalusPeres/HomeTiles/releases).
2. Download the plain OTA `.bin` matching the device from the table below. Do **not**
   use the `_factory.bin` file for this.
3. Open the display's Web Admin at `http://<display-ip>/`.
4. In the Firmware section, select the downloaded file and start the manual upload.
5. Leave the device powered on. A black screen during this upload is intentional; the
   device restarts automatically after a successful installation.

The manual upload uses the browser to download the GitHub file and sends it to the
display over the local network. The display therefore does not have to maintain the
large outbound GitHub HTTPS/TLS stream through the ESP32-C6, which is why this method
can work even when the on-device GitHub download does not.

## 2. Web Admin OTA Upload

Open the [web admin panel](web-admin.md) (`http://<display-ip>/`), go to the Firmware
section, and either run the same GitHub update check from the browser or upload the
update binary manually. During a manual upload the screen turns off — this is
intentional (it frees memory for the transfer) and the device restarts when done.

![Firmware section in the web admin](images/web-admin-firmware.png)

Use the asset matching your device from the release page:

| Device | Status | OTA update file |
| --- | --- | --- |
| M5Stack Tab5 | Supported | `hometiles_<version>_m5stacks_tab5.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-4B / 86 Panel | Supported | `hometiles_<version>_waveshare_4b.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 | Core hardware validated; microSD/OTA pending | `hometiles_<version>_waveshare_touch_lcd_4_3.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-8 | Supported | `hometiles_<version>_waveshare_touch_lcd_8.bin` |
| Guition JC8012P4A1C_I_W_Y V1 (no V2 sticker) | Supported | `hometiles_<version>_guition_jc8012p4a1.bin` |
| Guition JC8012P4A1 V2 (`SKU:10153002-V2`) | Hardware validation pending | `hometiles_<version>_guition_jc8012p4a1_v2.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7 | Hardware validation pending | `hometiles_<version>_waveshare_touch_lcd_7.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C, ESP32-P4 before v3.0 (revisions 1–199) | Hardware validation pending; not the older 7-inch profile | `hometiles_<version>_waveshare_touch_lcd_7b.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C, exact ESP32-P4 v3.1 | Experimental; exact hardware unverified | `hometiles_<version>_waveshare_touch_lcd_7b_rev3_1.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 | Hardware validation pending | `hometiles_<version>_waveshare_touch_lcd_10_1.bin` |
| Guition JC1060P470C_I_W_Y V1 | SD-card validation pending; exact suffix required | `hometiles_<version>_guition_jc1060p470c.bin` |
| Guition JC1060P470C V2 / New Panel | Hardware validation pending; use only for the marked V2 revision | `hometiles_<version>_guition_jc1060p470c_v2.bin` |
| Guition ESP32-4848S040C_I | Supported | `hometiles_<version>_guition_esp32_4848s040.bin` |
| Waveshare ESP32-S3-Touch-LCD-4B | Hardware validation pending; no microSD interface | `hometiles_<version>_waveshare_s3_touch_lcd_4b.bin` |

Older devices still running v0.2.9 or earlier look for the previous
`esp32-p4-homeassistant-display-<version>-<device>-update.bin` naming; the on-device
updater falls back to it automatically if a release doesn't have the current-named asset.

The Waveshare 7B/7B-C on-device updater enforces the running chip revision and
requests either the pre-v3 asset for revisions 1–199 or the experimental asset
for exact v3.1 (301). In the browser installer, the user selects one of those
two explicit entries, and that selection determines the asset. Browser updates
and manual Web Admin uploads also reject a mismatched revision contract before
writing. The other current P4 profiles use vendor-listed P4NRW32/pre-v3 modules
and are likewise guarded to revisions 1–199.

ESP32-P4 v3.2 or newer is unsupported with Arduino-ESP32 3.3.7 / ESP-IDF 5.5.2
and must not be flashed. The HomeTiles browser, Web Admin, and on-device OTA
paths enforce that restriction. Direct `esptool` flashing can bypass it because
the underlying ESP image header for the Arduino `v3.00 or newer` choice can span
revisions 301–399; see the [manual flashing warning](flashing.md).

## 3. Browser Installer over USB

Use the [Browser Firmware Installer](installer.md) when normal OTA is unavailable
or when you want a guided USB update without installing a desktop flashing tool.
Select the exact device, choose **Update — keep settings**, and connect its USB
serial port in a current desktop Chrome or Edge browser.

Before writing, the installer checks the ESP32-P4/ESP32-S3 family, flash size,
current HomeTiles partition table, firmware device ID, file size, and SHA-256.
It reads the redundant ESP-IDF OTA selection data, writes the regular OTA image
only to the inactive application slot, and verifies the completed image before
committing a new redundant OTA selection entry. NVS and LittleFS are not
written, so Wi-Fi, MQTT, tiles, and other local settings remain intact.

Keep power, USB, and the browser connected until verification is complete. If
the transfer is interrupted while the inactive slot is being written, the
previously selected application remains untouched and bootable. Restart the
display to continue using the previous version, or reconnect and retry Update.

Do not use this Update mode on a device with an unknown or modified partition
layout. The installer rejects a mismatch before flashing; make a backup and use
Factory only if a complete reset is intentional.

## 4. Factory Flash (first installation / full reset)

For a brand-new device or a full reset, flash the `-factory.bin` image over USB — it's
a complete flash image that wipes and reinstalls everything: bootloader, app, and the
stored WiFi/MQTT/tile configuration.

The full walkthrough (tools, files, first boot, re-pairing after a reset) is on the
[Flashing the Firmware](flashing.md) page. The
[Browser Firmware Installer](installer.md) also offers this mode, but requires a
separate confirmation because it erases the complete flash before writing the
full `_factory.bin` at `0x0`.

## Building From Source

1. Open `HomeTiles.ino` in the Arduino IDE.
2. Select the target device in `src/devices/device_select.h`.
3. Apply the board settings from [BOARD_SETTINGS.md](https://github.com/GalusPeres/HomeTiles/blob/main/BOARD_SETTINGS.md).
4. Build and flash.

The firmware version comes from `version.txt`. The on-device updater compares this
version against the latest release tag, and expects release assets to follow the naming
scheme shown above.

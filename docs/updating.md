# Firmware Updates

There are three ways to get firmware onto a device. For normal operation you only ever
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

| Device | OTA update file |
| --- | --- |
| M5Stack Tab5 | `hometiles_<version>_m5stacks_tab5.bin` |
| Waveshare 4B | `hometiles_<version>_waveshare_4b.bin` |
| Waveshare 8" | `hometiles_<version>_waveshare_touch_lcd_8.bin` |

Older devices still running v0.2.9 or earlier look for the previous
`esp32-p4-homeassistant-display-<version>-<device>-update.bin` naming; the on-device
updater falls back to it automatically if a release doesn't have the current-named asset.

## 3. Factory Flash (first installation / full reset)

For a brand-new device or a full reset, flash the `-factory.bin` image over USB — it's
a complete flash image that wipes and reinstalls everything: bootloader, app, and the
stored WiFi/MQTT/tile configuration.

The full walkthrough (tools, files, first boot, re-pairing after a reset) is on the
[Flashing the Firmware](flashing.md) page.

## Building From Source

1. Open `HomeTiles.ino` in the Arduino IDE.
2. Select the target device in `src/devices/device_select.h`.
3. Apply the board settings from [BOARD_SETTINGS.md](https://github.com/GalusPeres/HomeTiles/blob/master/BOARD_SETTINGS.md).
4. Build and flash.

The firmware version comes from `version.txt`. The on-device updater compares this
version against the latest release tag, and expects release assets to follow the naming
scheme shown above.

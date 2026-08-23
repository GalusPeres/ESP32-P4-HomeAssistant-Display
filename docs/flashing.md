# Flashing the Firmware

A brand-new device gets the firmware exactly once over USB — every update afterwards
happens [over the air](updating.md). Flashing takes about five minutes.

!!! tip "Recommended: flash directly from the documentation"
    Open the [Browser Firmware Installer](installer.md), select the exact device,
    choose **Factory reset**, confirm the data wipe, and connect the USB
    serial port. It uses the latest release image for the selected entry. A
    current desktop Chrome or Edge browser and an HTTPS connection are required.

The steps below are the manual alternative for users who prefer Espressif's
desktop tool or command-line esptool.

## What You Need

- Your display and a USB data cable
- The **factory image** for your device (next section)
- Espressif's free **Flash Download Tool** (Windows):
  [download from Espressif](https://www.espressif.com/en/support/download/other-tools) —
  or `esptool` on Linux/macOS

## Step 1: Download The Factory Image

Grab the file matching your device from the
[latest release](https://github.com/GalusPeres/HomeTiles/releases/latest):

| Device | Status | File |
| --- | --- | --- |
| M5Stack Tab5 | Supported | `hometiles_<version>_m5stacks_tab5_factory.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-4B / 86 Panel | Supported | `hometiles_<version>_waveshare_4b_factory.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-8 | Supported | `hometiles_<version>_waveshare_touch_lcd_8_factory.bin` |
| Guition JC8012P4A1C_I_W_Y V1 (no V2 sticker) | Supported | `hometiles_<version>_guition_jc8012p4a1_factory.bin` |
| Guition JC8012P4A1 V2 (`SKU:10153002-V2`) | Hardware validation pending | `hometiles_<version>_guition_jc8012p4a1_v2_factory.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7 | Hardware validation pending | `hometiles_<version>_waveshare_touch_lcd_7_factory.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C, ESP32-P4 before v3.0 (revisions 1–199) | Hardware validation pending; not the older 7-inch profile | `hometiles_<version>_waveshare_touch_lcd_7b_factory.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-7B / 7B-C, exact ESP32-P4 v3.1 | Experimental; exact hardware unverified | `hometiles_<version>_waveshare_touch_lcd_7b_rev3_1_factory.bin` |
| Waveshare ESP32-P4-WIFI6-Touch-LCD-10.1 | Hardware validation pending | `hometiles_<version>_waveshare_touch_lcd_10_1_factory.bin` |
| Guition JC1060P470C_I_W_Y V1 | SD-card validation pending; exact suffix required | `hometiles_<version>_guition_jc1060p470c_factory.bin` |
| Guition JC1060P470C V2 / New Panel | Hardware validation pending; use only for the marked V2 revision | `hometiles_<version>_guition_jc1060p470c_v2_factory.bin` |
| Guition ESP32-4848S040C_I | Supported ESP32-S3 target | `hometiles_<version>_guition_esp32_4848s040_factory.bin` |
| Waveshare ESP32-S3-Touch-LCD-4B | Hardware validation pending; no microSD interface | `hometiles_<version>_waveshare_s3_touch_lcd_4b_factory.bin` |

!!! warning "Exact hardware images"
    Some targets have not yet completed the full physical-device checklist.
    Use only the image matching the exact model and report results in
    [open hardware issues](https://github.com/GalusPeres/HomeTiles/issues),
    including [JC8012 V2 issue #18](https://github.com/GalusPeres/HomeTiles/issues/18).
    The normal ESP32-P4 profiles use vendor-listed P4NRW32/pre-v3 modules and
    are explicitly guarded to revisions 1–199; they are not all-revision P4
    images.

!!! danger "JC8012 V1 and V2 images are not interchangeable"
    Check the rear material-number sticker before flashing. A unit marked `V2`
    requires `guition_jc8012p4a1_v2`; the original image is only for the V1
    panel. Using the wrong revision can leave the backlight on with a black or
    grey-banded display.

!!! danger "Other similarly named panels also use separate images"
    `guition_jc1060p470c_v2` is only for the marked JC1060 V2 / New Panel;
    the original `_I_W_Y` panel uses `guition_jc1060p470c`. Likewise, the
    1024×600 Waveshare 7B/7B-C uses a silicon-specific 7B image, while the
    older 1280×720 Waveshare 7-inch panel uses `waveshare_touch_lcd_7`.

!!! danger "Waveshare 7B silicon revision"
    HomeTiles provides one 7B image for pre-v3 revisions 1–199 and a separate,
    experimental image for exact revision v3.1 (301). In the
    [browser installer](installer.md), explicitly select the matching entry;
    that selection determines the image. Browser installation, Web Admin
    upload, and on-device OTA enforce the revision and reject a mismatch.

    ESP32-P4 v3.2 or newer is unsupported with Arduino-ESP32 3.3.7 / ESP-IDF
    5.5.2 and must not be flashed. Run `esptool --chip esp32p4 chip-id` before a
    manual download: use the base filename only for revisions 1–199 and
    `_rev3_1` only when it reports exact v3.1. Direct `esptool` flashing to v3.2+
    is unsafe because the Arduino `v3.00 or newer` build's underlying ESP image
    header can still allow revisions 301–399, bypassing HomeTiles' exact-v3.1
    guard.

!!! note "`factory.bin` vs. plain `.bin`"
    The **factory** image is a complete flash image — bootloader, firmware, and empty
    configuration in one file. It is only for the first flash (or a full reset).
    The plain `.bin` of the same name is the small OTA update file used later by the
    on-device updater and the web admin. Never write that plain image manually at
    `0x0`; the [Browser Firmware Installer](installer.md) can use it safely by
    verifying the partition table, writing only the inactive application slot,
    and changing the OTA boot selection only after verification succeeds.

## Step 2: Flash

Connect the device to your PC via USB, then in the **Flash Download Tool**:

1. Select ChipType **ESP32-P4** for every P4 target. Select **ESP32-S3** only
   for the Guition ESP32-4848S040C_I or Waveshare
   ESP32-S3-Touch-LCD-4B. Use WorkMode **Develop** and LoadMode **UART** → OK.
2. In the first file row: select the `factory.bin`, set the address to `0x0`,
   and tick the row's checkbox.
3. **COM**: pick the device's serial port (if none appears, try another cable/port).
   Leave **BAUD** at `115200`.
4. Click **START** and wait for *FINISH*.
5. Unplug/replug or press the reset button — some boards don't restart on their own.

??? info "Linux / macOS: esptool instead"
    ```
    esptool --chip <esp32p4-or-esp32s3> --port <PORT> write_flash 0x0 hometiles_<version>_<device>_factory.bin
    ```
    Replace the chip placeholder with `esp32p4`, or with `esp32s3` only for the
    Guition ESP32-4848S040C_I or Waveshare ESP32-S3-Touch-LCD-4B. Replace
    `<PORT>` with the serial port (for example `/dev/ttyACM0`).

## Step 3: First Boot

The device shows the boot splash (logo + firmware version) and starts with an empty
dashboard. It is not on your network yet — open **Settings → WiFi** on the device
and continue with the [Home Assistant Setup](home-assistant-setup.md), which walks
through WiFi, MQTT, and pairing.

## Resetting An Existing Device

Flashing the factory image again wipes **everything**: WiFi, MQTT, and all tiles —
back up first via [Import/Export](web-admin.md#import-export) if you want to keep
the layout.

!!! warning "Re-pairing after a reset"
    Delete the device's old entry in Home Assistant *before* expecting a new
    "discovered device" card. The device ID is derived from the MAC address, which
    survives the flash — Home Assistant won't re-discover an ID it already knows.

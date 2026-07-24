# Flashing the Firmware

A brand-new device gets the firmware exactly once over USB — every update afterwards
happens [over the air](updating.md). Flashing takes about five minutes.

## What You Need

- Your display and a USB data cable
- The **factory image** for your device (next section)
- Espressif's free **Flash Download Tool** (Windows):
  [download from Espressif](https://www.espressif.com/en/support/download/other-tools) —
  or `esptool` on Linux/macOS

## Step 1: Download The Factory Image

Grab the file matching your device from the
[latest release](https://github.com/GalusPeres/HomeTiles/releases/latest):

| Device | File |
| --- | --- |
| M5Stack Tab5 | `hometiles_<version>_m5stacks_tab5_factory.bin` |
| Waveshare 4B | `hometiles_<version>_waveshare_4b_factory.bin` |
| Waveshare 8" | `hometiles_<version>_waveshare_touch_lcd_8_factory.bin` |

!!! note "`factory.bin` vs. plain `.bin`"
    The **factory** image is a complete flash image — bootloader, firmware, and empty
    configuration in one file. It is only for the first flash (or a full reset).
    The plain `.bin` of the same name is the small OTA update file used later by the
    on-device updater and the web admin — never flash that one over USB.

## Step 2: Flash

Connect the device to your PC via USB, then in the **Flash Download Tool**:

1. ChipType **ESP32-P4**, WorkMode **Develop**, LoadMode **UART** → OK.
2. In the first file row: select the `factory.bin`, set the address to `0x0`,
   and tick the row's checkbox.
3. **COM**: pick the device's serial port (if none appears, try another cable/port).
   Leave **BAUD** at `115200`.
4. Click **START** and wait for *FINISH*.
5. Unplug/replug or press the reset button — some boards don't restart on their own.

??? info "Linux / macOS: esptool instead"
    ```
    esptool --chip esp32p4 --port <PORT> write_flash 0x0 hometiles_<version>_<device>_factory.bin
    ```
    Replace `<PORT>` with the serial port (for example `/dev/ttyACM0`).

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

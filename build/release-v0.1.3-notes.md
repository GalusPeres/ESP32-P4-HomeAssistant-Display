# ESP32-P4 Home Assistant Display v0.1.3

Supported devices:
- M5Stacks Tab5
- Waveshare B4

Highlights:
- Runtime storage now uses internal LittleFS
- microSD is no longer required for normal runtime operation
- smaller popups on the M5Stacks Tab5 for better responsiveness

Included binaries:
- `esp32-p4-homeassistant-display-v0.1.3-m5stacks-tab5-factory.bin`
- `esp32-p4-homeassistant-display-v0.1.3-m5stacks-tab5-update.bin`
- `esp32-p4-homeassistant-display-v0.1.3-waveshare-b4-factory.bin`
- `esp32-p4-homeassistant-display-v0.1.3-waveshare-b4-update.bin`

Notes:
- `factory.bin` is intended for first installation / clean flash.
- `update.bin` is intended for updating an existing installation.
- `factory.bin` should be flashed at offset `0x00000`.
- `update.bin` should be flashed at offset `0x10000`.
- OTA updates can be installed from the web interface using the matching `update.bin`.
- A microSD card is now only needed for screenshot export and optional migration from older SD-based setups.
- A Home Assistant setup with MQTT and the bridge integration is required.

Known issues:
- Waveshare B4: software restart can leave the display black even though the firmware continues to run. Workaround: press the hardware reset button once.
- Waveshare B4: tile edits can briefly flash blue after the move from SD-backed storage to LittleFS.
- M5Stacks Tab5: Access Point mode is currently only reliable with a battery installed. Without a battery, keep brightness at the lowest available level; otherwise the device can crash.

Bridge integration:
- https://github.com/GalusPeres/ha-tab5-mqtt-bridge

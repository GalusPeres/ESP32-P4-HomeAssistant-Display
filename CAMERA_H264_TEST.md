# H.264 camera popup test (Waveshare 8")

This branch is an experimental end-to-end camera test. It does not change or
flash a connected device automatically.

## Test setup

1. Use branch `codex/camera-h264-test` in both repositories:
   `HomeTiles` and `HomeTiles_Bridge`.
2. Install/update the Home Assistant custom component from the bridge
   repository and restart Home Assistant.
3. Open the HomeTiles integration options. Under entity configuration, add the
   desired `camera.*` entity.
4. Flash the Waveshare 8" OTA image:
   `build/camera-h264-test-waveshare-8-final/HomeTiles.ino.bin`.
5. In the HomeTiles web admin, create a `Kamera` tile and select the configured
   camera entity.
6. Tap the tile. A 760x740 popup opens with a 640x480 video area.

The bridge resolves the Home Assistant camera stream and starts FFmpeg only
while the popup is open. It outputs a raw H.264 Annex-B stream using constrained
baseline, 640x480, 12 fps and 850 kbit/s. MQTT carries only the open/close
commands and the short-lived HTTP stream URL; video bytes do not pass through
MQTT.

## MQTT diagnostics

- Request: `<base_topic>/cmnd/camera`
- Status: `<base_topic>/stat/camera`
- Open payload:
  `{"command":"open","entity_id":"camera.example"}`
- Close payload:
  `{"command":"close","entity_id":"camera.example"}`

The bridge replies with `ready`, `error`, or `stopped`. A `ready` message
contains a single-use HTTP URL that expires after 30 seconds if it is not
consumed.

## Memory and recovery behavior

- Three fixed 640x480 RGB565 frame buffers use about 1.84 MB of P4 PSRAM.
- The H.264 input buffer uses 256 KB of P4 PSRAM.
- Frame buffers remain allocated after their first use to avoid repeated large
  allocations and PSRAM fragmentation.
- The decoder, HTTP input buffer and FFmpeg process are released when the popup
  closes or the connection ends.
- If memory, HTTP or decoder setup fails, the popup shows an error instead of
  retrying indefinitely.

Useful serial messages are prefixed with `[Camera]` where available. For a
long-run test, repeatedly open and close the popup and verify that the UI,
MQTT connection and free PSRAM remain stable.

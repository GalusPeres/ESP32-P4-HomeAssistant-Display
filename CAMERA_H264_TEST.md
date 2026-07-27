# Camera popup video test (Waveshare 8")

This branch is an experimental end-to-end camera test. It does not change or
flash a connected device automatically.

## Test setup

1. Use firmware branch `feature/camera-view` and bridge branch
   `feature/camera-stream`.
2. Install/update the Home Assistant custom component from the bridge
   repository and restart Home Assistant.
3. Open the HomeTiles integration options. Under entity configuration, add the
   desired `camera.*` entity.
4. Flash the current Waveshare 8" test image.
5. In the HomeTiles web admin, create a `Kamera` tile and select the configured
   camera entity.
6. Tap the tile. The regular project popup opens with a 752x424 video area.

The bridge resolves the Home Assistant camera stream and starts FFmpeg only
while the popup is open. Real video sources are converted to an up-to-24 FPS,
752x424 JPEG-frame stream; still-image cameras remain at 2 FPS. The image is
cropped to fill the widescreen area without stretching or letterboxing. MQTT
carries only the open/close commands and the short-lived stream URL; video
bytes do not pass through MQTT. The bridge opens a dedicated local TCP listener
on the first free port from 8124 through 8131. Every JPEG is sent in 8 KiB
blocks and the bridge waits for an application-level acknowledgement from the
display after each block. This bounds camera data in flight without pausing
MQTT. Camera transport does not use HTTP or TLS on the ESP32-P4.

## MQTT diagnostics

- Request: `<base_topic>/cmnd/camera`
- Status: `<base_topic>/stat/camera`
- Open payload:
  `{"command":"open","entity_id":"camera.example","width":752,"height":424,"fps":24,"transport":"tcp-ack-v1"}`
- Close payload:
  `{"command":"close","entity_id":"camera.example"}`

The bridge replies with `ready`, `error`, or `stopped`. A `ready` message
contains a single-use `tcp://` URL that expires after 30 seconds if it is not
consumed.

## Memory and recovery behavior

- Two fixed 752x424 RGB565 frame buffers use about 1.24 MiB of P4 PSRAM,
  including the hardware decoder's required 16-pixel height alignment.
- The framed-JPEG input buffer uses 256 KB of P4 PSRAM.
- Frame buffers are allocated for the active popup and released after the
  worker has stopped.
- The decoder engine remains available across popup opens to avoid DMA-memory
  churn. Frame buffers and the TCP input buffer are released after closing.
- MQTT continues receiving while the camera is open. Hidden tile redraws and
  full bridge configuration applies are coalesced and run after the popup
  closes.
- If a camera source briefly ends, the bridge restarts FFmpeg while keeping the
  display connection and MQTT session alive.
- If memory, TCP or decoder setup fails, the popup shows an error instead of
  retrying indefinitely.

Useful serial messages are prefixed with `[Camera]` where available. For a
long-run test, repeatedly open and close the popup and verify that the UI,
MQTT connection and free PSRAM remain stable.

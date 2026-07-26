# EspH264Decoder test library

This directory contains the decoder-only subset of Espressif's
`esp-h264-component` version 1.3.6.

- Upstream: https://github.com/espressif/esp-h264-component
- Component: https://components.espressif.com/components/espressif/esp_h264
- License: Apache-2.0 (see the SPDX headers in the copied sources)
- Target in this test branch: ESP32-P4 only

Only the TinyH264 decoder archive is included. The OpenH264 software encoder
and the ESP32-P4 hardware encoder are not part of the HomeTiles playback test.

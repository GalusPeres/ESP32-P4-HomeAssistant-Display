// SPDX-FileCopyrightText: 2026 HomeTiles contributors
// SPDX-License-Identifier: MIT

#if defined(DEVICE_GUITION_JC8012P4A1)

#include <esp_hosted_transport_config.h>

extern "C" struct esp_hosted_sdio_config
__real_esp_hosted_get_default_sdio_config(void);

extern "C" struct esp_hosted_sdio_config
__wrap_esp_hosted_get_default_sdio_config(void) {
  struct esp_hosted_sdio_config config =
      __real_esp_hosted_get_default_sdio_config();

  // Issue #30 field testing validated this exact 1-bit/40 MHz configuration
  // together with the Guition-only single-block RX transport object.
  config.clock_freq_khz = 40000;
  config.bus_width = 1;
  return config;
}

#endif

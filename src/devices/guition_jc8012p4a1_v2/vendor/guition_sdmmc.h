/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 HomeTiles contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "src/devices/device_select.h"

#if defined(DEVICE_GUITION_JC8012P4A1_V2)

#include <FS.h>
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_types.h>
#include <vfs_api.h>

enum GuitionSdCardType : uint8_t {
  GUITION_CARD_NONE,
  GUITION_CARD_SD,
  GUITION_CARD_SDHC,
};

namespace fs {

class GuitionSDMMCFS : public FS {
 public:
  explicit GuitionSDMMCFS(FSImplPtr impl);

  bool begin(const char* mountpoint = "/sdcard",
             int sdmmc_frequency = SDMMC_FREQ_HIGHSPEED,
             uint8_t max_open_files = 5);
  void end();

  GuitionSdCardType cardType() const;
  uint64_t cardSize() const;

 private:
  sdmmc_card_t* card_;
};

}  // namespace fs

extern fs::GuitionSDMMCFS GuitionSDMMC;

#endif  // defined(DEVICE_GUITION_JC8012P4A1_V2)

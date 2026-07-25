/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2026 HomeTiles contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "src/devices/device_select.h"

#if defined(DEVICE_GUITION_JC1060P470C)

#include <FS.h>
#include <driver/sdmmc_host.h>
#include <driver/sdmmc_types.h>
#include <vfs_api.h>

enum JC1060SdCardType : uint8_t {
  JC1060_CARD_NONE,
  JC1060_CARD_SD,
  JC1060_CARD_SDHC,
};

namespace fs {

class JC1060SDMMCFS : public FS {
 public:
  explicit JC1060SDMMCFS(FSImplPtr impl);

  bool begin(const char* mountpoint = "/sdcard",
             int sdmmc_frequency = SDMMC_FREQ_HIGHSPEED,
             uint8_t max_open_files = 5);
  void end();

  JC1060SdCardType cardType() const;
  uint64_t cardSize() const;

 private:
  sdmmc_card_t* card_;
};

}  // namespace fs

extern fs::JC1060SDMMCFS JC1060SDMMC;

#endif  // defined(DEVICE_GUITION_JC1060P470C)

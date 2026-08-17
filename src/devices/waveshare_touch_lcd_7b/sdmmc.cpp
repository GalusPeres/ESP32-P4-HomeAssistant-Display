#include "src/devices/waveshare_touch_lcd_7b/sdmmc.h"

#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7B)

#include <Arduino.h>
#include <esp_err.h>
#include <esp_ldo_regulator.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <sd_protocol_defs.h>

namespace {

static constexpr int kSdLdoChannel = 4;
static constexpr int kSdLdoVoltageMv = 3300;
static constexpr int kSdD0Pin = 39;
static constexpr int kSdD1Pin = 40;
static constexpr int kSdD2Pin = 41;
static constexpr int kSdD3Pin = 42;
static constexpr int kSdCmdPin = 44;
static constexpr int kSdClkPin = 43;
static_assert(kSdD0Pin == 39 && kSdD1Pin == 40 && kSdD2Pin == 41 &&
                  kSdD3Pin == 42 && kSdCmdPin == 44 && kSdClkPin == 43,
              "Waveshare 7B must use the ESP32-P4 SDMMC slot-0 IO-MUX");
static esp_ldo_channel_handle_t g_sd_ldo_handle = nullptr;

static void ensure_sd_ldo_power() {
  if (g_sd_ldo_handle != nullptr) {
    return;
  }

  esp_ldo_channel_config_t ldo_cfg = {};
  ldo_cfg.chan_id = kSdLdoChannel;
  ldo_cfg.voltage_mv = kSdLdoVoltageMv;

  esp_err_t err = esp_ldo_acquire_channel(&ldo_cfg, &g_sd_ldo_handle);
  if (err == ESP_OK) {
    Serial.printf("[SD] LDO VO%d set to %d mV\n", kSdLdoChannel, kSdLdoVoltageMv);
  } else {
    Serial.printf("[SD] WARN: LDO VO%d acquire failed: %s (0x%x)\n",
                  kSdLdoChannel,
                  esp_err_to_name(err),
                  static_cast<unsigned>(err));
  }
}

}  // namespace

namespace fs {

Waveshare7BSDMMCFS::Waveshare7BSDMMCFS(FSImplPtr impl)
    : FS(impl), _card(nullptr) {
}

bool Waveshare7BSDMMCFS::begin(const char* mountpoint,
                              bool format_if_mount_failed,
                              int sdmmc_frequency,
                              uint8_t maxOpenFiles) {
  if (_card != nullptr) {
    return true;
  }

  ensure_sd_ldo_power();

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = format_if_mount_failed;
  mount_config.max_files = maxOpenFiles;
  mount_config.allocation_unit_size = 64 * 1024;
  mount_config.disk_status_check_enable = false;
  mount_config.use_one_fat = false;

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0;
  host.max_freq_khz = sdmmc_frequency;
  host.flags = SDMMC_HOST_FLAG_4BIT;

  sdmmc_slot_config_t slot_config = {};
  slot_config.cd = SDMMC_SLOT_NO_CD;
  slot_config.wp = SDMMC_SLOT_NO_WP;
  slot_config.width = 4;
  // Waveshare uses the P4 slot-0 IO-MUX pins (43/44/39/40/41/42).
  // For slot 0 we must not force GPIO_NUM_NC here, otherwise the driver
  // rejects the configuration with "Invalid GPIO number -1".
  slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  esp_err_t err = esp_vfs_fat_sdmmc_mount(mountpoint, &host, &slot_config, &mount_config, &_card);
  if (err != ESP_OK) {
    Serial.printf("[SD] Mount failed: %s (0x%x), freq=%d kHz\n",
                  esp_err_to_name(err),
                  static_cast<unsigned>(err),
                  sdmmc_frequency);
    _card = nullptr;
    _impl->mountpoint(nullptr);
    return false;
  }

  _impl->mountpoint(mountpoint);
  return true;
}

void Waveshare7BSDMMCFS::end() {
  if (_card == nullptr) {
    return;
  }

  esp_vfs_fat_sdcard_unmount(_impl->mountpoint(), _card);
  _card = nullptr;
  _impl->mountpoint(nullptr);
}

sdcard_type_t Waveshare7BSDMMCFS::cardType() const {
  if (_card == nullptr) {
    return CARD_NONE;
  }
  return (_card->ocr & SD_OCR_SDHC_CAP) ? CARD_SDHC : CARD_SD;
}

uint64_t Waveshare7BSDMMCFS::cardSize() const {
  if (_card == nullptr) {
    return 0;
  }
  return static_cast<uint64_t>(_card->csd.capacity) * _card->csd.sector_size;
}

}  // namespace fs

fs::Waveshare7BSDMMCFS Waveshare7BSDMMC(FSImplPtr(new VFSImpl()));

#endif  // defined(DEVICE_WAVESHARE_TOUCH_LCD_7B)

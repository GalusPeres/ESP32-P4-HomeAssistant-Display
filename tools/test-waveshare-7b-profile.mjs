import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');
}

function requireMarker(source, marker, label) {
  if (!source.includes(marker)) {
    throw new Error(`${label} is missing: ${marker}`);
  }
}

function displayConfigValues(source) {
  const match = source.match(/SCREEN_DEFAULT\s*=\s*\{([\s\S]*?)\n\};/);
  if (!match) throw new Error('Waveshare 7B display config was not found');
  return match[1]
    .replace(/\/\/.*$/gm, '')
    .split(',')
    .map((value) => value.trim())
    .filter(Boolean);
}

function normalizedInitSequence(source) {
  const table = source.match(
    /static const ek79007_lcd_init_cmd_t vendor_specific_init_default\[\] = \{([\s\S]*?)\n\};/);
  if (!table) throw new Error('EK79007 default init sequence was not found');
  return [...table[1].matchAll(
    /\{(0x[0-9A-Fa-f]+), \(uint8_t \[\]\)\{([^}]*)\}, (\d+), (\d+)\},/g)]
    .map((match) => [
      match[1].toLowerCase(),
      match[2].replace(/\s+/g, '').toLowerCase(),
      match[3],
      match[4],
    ].join(':'));
}

const base = 'src/devices/waveshare_touch_lcd_7b';
const profile = read(`${base}/profile.h`);
const hardwareIo = read(`${base}/hardware_io_profile.h`);
const driver = read(`${base}/device_waveshare_touch_lcd_7b.cpp`);
const display = read(`${base}/vendor/displays_config.h`);
const i2c = read(`${base}/vendor/i2c.h`);
const touchHeader = read(`${base}/vendor/gt911.h`);
const touchDriver = read(`${base}/vendor/gt911.cpp`);
const sdHeader = read(`${base}/sdmmc.h`);
const sdDriver = read(`${base}/sdmmc.cpp`);
const panelHeader = read(`${base}/vendor/ek79007/esp_lcd_ek79007.h`);
const panelDriver = read(`${base}/vendor/ek79007/esp_lcd_ek79007.c`);
const vendorLicense = read(`${base}/vendor/LICENSE-APACHE-2.0.txt`);
const profileReadme = read(`${base}/README.md`);

for (const marker of [
  '"waveshare_touch_lcd_7b"',
  '"Waveshare Touch LCD 7B / 7B-C"',
  '    1024,\n    600,\n    6,\n    4,',
  'Device::RotationStepMode::FlipOnly,\n    2,\n    0,',
  'inline constexpr uint32_t kFlashSizeBytes = 32U * 1024U * 1024U;',
  'inline constexpr uint32_t kPsramSizeBytes = 32U * 1024U * 1024U;',
]) {
  requireMarker(profile, marker, 'Waveshare 7B profile');
}
requireMarker(hardwareIo, 'Device::kNoHardwareIoProfile',
              'Waveshare 7B reserved hardware I/O contract');

const expectedDisplay = [
  '"WAVESHARE-7B-EK79007"',
  '10', '160', '160',
  '1', '23', '12',
  '52000000', '1000',
  '1024', '600',
  '7', '8', '400000', '33',
];
const actualDisplay = displayConfigValues(display);
if (JSON.stringify(actualDisplay) !== JSON.stringify(expectedDisplay)) {
  throw new Error(`Waveshare 7B display contract mismatch: ${actualDisplay}`);
}

for (const marker of [
  '#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7B)',
  'vendor/ek79007/esp_lcd_ek79007.h',
  'constexpr gpio_num_t kBacklightPin = GPIO_NUM_32;',
  'constexpr uint32_t kBacklightFreq = 5000;',
  'constexpr ledc_timer_bit_t kBacklightBits = LEDC_TIMER_10_BIT;',
  'constexpr bool kBacklightActiveLow = true;',
  'constexpr uint8_t kBacklightInputMin = 1;',
  'constexpr uint8_t kBacklightInputMax = 255;',
  'if (value == 0) {',
  'brightness_percent = 100;',
  'duty = kMaxDuty - duty;',
  'constexpr uint32_t kPanelLaneCount = 2;',
  'constexpr int kMipiPhyLdoChannel = 3;',
  'constexpr int kMipiPhyLdoVoltageMv = 2500;',
  'dpi_cfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;',
  'dpi_cfg.in_color_format = LCD_COLOR_FMT_RGB565;',
  'dpi_cfg.out_color_format = LCD_COLOR_FMT_RGB565;',
  'panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;',
  'vendor_cfg.mipi_config.lane_num = kPanelLaneCount;',
  'esp_lcd_new_panel_ek79007(g_panel_io, &panel_cfg, &g_panel)',
]) {
  requireMarker(driver, marker, 'Waveshare 7B device source');
}
if (/jd9165/i.test(driver)) {
  throw new Error('Waveshare 7B must not use the unrelated JD9165 driver');
}

for (const marker of [
  '#define EXAMPLE_I2C_MASTER_SDA GPIO_NUM_7',
  '#define EXAMPLE_I2C_MASTER_SCL GPIO_NUM_8',
  '#define EXAMPLE_I2C_MASTER_FREQUENCY (400 * 1000)',
]) {
  requireMarker(i2c, marker, 'Waveshare 7B I2C wiring');
}
for (const marker of [
  '#define EXAMPLE_PIN_NUM_TOUCH_RST       (GPIO_NUM_NC)',
  '#define EXAMPLE_PIN_NUM_TOUCH_INT       (GPIO_NUM_NC)',
]) {
  requireMarker(touchHeader, marker, 'Waveshare 7B GT911 wiring');
}
for (const marker of [
  '.swap_xy = 0,',
  '.mirror_x = 1,',
  '.mirror_y = 1,',
]) {
  requireMarker(touchDriver, marker, 'Waveshare 7B GT911 transform');
}

for (const marker of [
  'int sdmmc_frequency = SDMMC_FREQ_HIGHSPEED,',
]) {
  requireMarker(sdHeader, marker, 'Waveshare 7B SDMMC interface');
}
for (const marker of [
  'static constexpr int kSdLdoChannel = 4;',
  'static constexpr int kSdD0Pin = 39;',
  'static constexpr int kSdD1Pin = 40;',
  'static constexpr int kSdD2Pin = 41;',
  'static constexpr int kSdD3Pin = 42;',
  'static constexpr int kSdCmdPin = 44;',
  'static constexpr int kSdClkPin = 43;',
  'host.slot = SDMMC_HOST_SLOT_0;',
  'host.flags = SDMMC_HOST_FLAG_4BIT;',
]) {
  requireMarker(sdDriver, marker, 'Waveshare 7B SDMMC contract');
}
requireMarker(driver,
              'Waveshare7BSDMMC.begin("/sdcard", false, SDMMC_FREQ_HIGHSPEED, 5)',
              'Waveshare 7B 40 MHz SD mount');

for (const marker of [
  'SPDX-License-Identifier: Apache-2.0',
  'ek79007_vendor_config_t',
  'EK79007_1024_600_PANEL_60HZ_CONFIG',
]) {
  requireMarker(panelHeader, marker, 'Vendored EK79007 header');
}
requireMarker(panelDriver, 'SPDX-License-Identifier: Apache-2.0',
              'Vendored EK79007 source');
requireMarker(panelDriver, '#if defined(DEVICE_WAVESHARE_TOUCH_LCD_7B)',
              'Vendored EK79007 build guard');
const initSequence = normalizedInitSequence(panelDriver);
const initHash = crypto.createHash('sha256')
  .update(initSequence.join('|')).digest('hex');
if (initSequence.length !== 8 ||
    initHash !== '4cd0037ee55d79e01ad05cff3983f4408fa5210dc72ed5c42e7cb6d4731ff633') {
  throw new Error(`Waveshare EK79007 init sequence changed: ${initHash}`);
}
requireMarker(vendorLicense, 'Apache License', 'Vendored EK79007 license');

for (const marker of [
  '7B-C uses the same display, touch, backlight and SDMMC contract',
  'does not enable or expose its optional camera',
]) {
  requireMarker(profileReadme, marker, 'Waveshare 7B-C scope');
}

const oldProfile = read('src/devices/waveshare_touch_lcd_7/profile.h');
for (const marker of [
  '"waveshare_touch_lcd_7"',
  '"Waveshare Touch LCD 7"',
  '    1280,\n    720,',
]) {
  requireMarker(oldProfile, marker, 'Existing Waveshare 7 profile');
}
if (oldProfile.includes('waveshare_touch_lcd_7b') ||
    oldProfile.includes('Waveshare Touch LCD 7B')) {
  throw new Error('The existing 1280x720 Waveshare 7 profile must stay distinct');
}

console.log('Waveshare Touch LCD 7B / 7B-C profile contract: PASS');

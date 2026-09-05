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

function requireOrder(source, markers, label) {
  let position = -1;
  for (const marker of markers) {
    const next = source.indexOf(marker, position + 1);
    if (next < 0) {
      throw new Error(`${label} is missing: ${marker}`);
    }
    if (next <= position) {
      throw new Error(`${label} has the wrong order near: ${marker}`);
    }
    position = next;
  }
}

const driver = read(
  'src/devices/guition_jc1060p470c/vendor/guition_sdmmc.cpp');
const hardwareIo = read(
  'src/devices/guition_jc1060p470c/hardware_io_profile.h');
const device = read(
  'src/devices/guition_jc1060p470c/device_guition_jc1060p470c.cpp');
const webDiagnostics = read('src/web/web_admin_diagnostics.cpp');

for (const marker of [
  'static_assert(BOARD_SDMMC_POWER_CHANNEL == kSdLdoChannel',
  'static_assert(BOARD_SDMMC_POWER_PIN == 45',
  'static_assert(BOARD_SDMMC_POWER_ON_LEVEL == LOW',
  'constexpr uint32_t kSdPowerOffMs = 200;',
]) {
  requireMarker(driver, marker, 'JC1060 SD power contract');
}

const resetStart = driver.indexOf('void reset_sd_card_power()');
const beginStart = driver.indexOf('bool JC1060SDMMCFS::begin');
if (resetStart < 0 || beginStart < 0) {
  throw new Error('JC1060 SD power reset or begin function was not found');
}
const resetBody = driver.slice(resetStart, beginStart);
requireOrder(resetBody, [
  'pinMode(BOARD_SDMMC_POWER_PIN, OUTPUT);',
  'digitalWrite(BOARD_SDMMC_POWER_PIN, !BOARD_SDMMC_POWER_ON_LEVEL);',
  'delay(kSdPowerOffMs);',
  'digitalWrite(BOARD_SDMMC_POWER_PIN, BOARD_SDMMC_POWER_ON_LEVEL);',
  'perimanSetPinBusExtraType(BOARD_SDMMC_POWER_PIN, "SDMMC_POWER");',
], 'JC1060 SD power reset');

const beginBody = driver.slice(beginStart);
requireOrder(beginBody, [
  'ensure_sd_power();',
  'reset_sd_card_power();',
  'esp_vfs_fat_sdmmc_mount(',
], 'JC1060 SD mount sequence');
requireMarker(beginBody, 'mount_config.format_if_mount_failed = false;',
              'JC1060 non-destructive mount');

if (/\{\s*45\s*,/.test(hardwareIo)) {
  throw new Error('GPIO45 must be reserved for the JC1060 SD power switch');
}

for (const marker of [
  'SDMMC_FREQ_HIGHSPEED',
  'SDMMC_FREQ_DEFAULT',
  '10000',
]) {
  requireMarker(device, marker, 'JC1060 SD frequency fallback');
}

for (const marker of [
  'SD power: LDO VO4, GPIO45 active-low, 200 ms reset before each mount attempt',
  'Last driver phase:',
  'Last driver result:',
]) {
  requireMarker(webDiagnostics, marker, 'JC1060 on-demand SD diagnostics');
}

console.log('Guition JC1060P470C SD power contract: PASS');

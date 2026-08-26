import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (relativePath) =>
  fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');

const defaults = read('tools/guition-s3-performance-sdkconfig.defaults');
const builder = read('tools/build-guition-s3-performance-sdk.sh');
const firmwareBuilder = read('tools/build-firmware-local.ps1');
const usbPatch = read(
  'tools/patches/arduino-esp32-3.3.7-usbmsc-initialize-lun.patch');

for (const marker of [
  '# CONFIG_COMPILER_OPTIMIZATION_SIZE is not set',
  'CONFIG_COMPILER_OPTIMIZATION_PERF=y',
  'CONFIG_SPIRAM_XIP_FROM_PSRAM=y',
  'CONFIG_SPIRAM_FETCH_INSTRUCTIONS=y',
  'CONFIG_SPIRAM_RODATA=y',
  '# CONFIG_ESP32S3_DATA_CACHE_LINE_32B is not set',
  'CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y',
]) {
  assert.ok(defaults.includes(marker),
            `Performance SDK config is missing: ${marker}`);
}

for (const marker of [
  '8cabf2c3eaa169754f55f58675e224c918815eb7',
  '87912cd291d68f4319f13695718af6754879a83f',
  '86c2c0046d4c732aa7cf6e049ac3b76a4da148b3',
  '2883403ed010c54c33a38c28358a4dd0c67d67c0',
  'defconfig.hometiles_s3_performance',
  './build.sh -s -c /arduino-esp32 -t esp32s3',
  'arduino-esp32-3.3.7-usbmsc-initialize-lun.patch',
]) {
  assert.ok(builder.includes(marker),
            `Pinned S3 SDK builder is missing: ${marker}`);
}

assert.ok(usbPatch.includes('USBMSC::USBMSC() : _lun(0)'),
          'The pinned Arduino component must initialize USBMSC::_lun under -O2');

for (const marker of [
  '[string]$GuitionS3SdkPath',
  "$Profile -ne 'guition_esp32_4848s040'",
  'compiler.sdk.path=$resolvedGuitionS3SdkPath',
  'compiler.optimization_flags=-O2',
  'compiler.optimization_flags.release=-O2',
  "Select-String -SimpleMatch 'release-xip-bounce10'",
  'Guition S3 performance display marker missing',
]) {
  assert.ok(firmwareBuilder.includes(marker),
            `Local firmware builder is missing: ${marker}`);
}

console.log('Guition ESP32-S3 performance SDK contract: PASS');

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DEVICE_PROFILES } from '../docs/assets/javascripts/installer-contract.mjs';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');
}

function requireMarker(source, marker, label) {
  assert.ok(source.includes(marker), `${label} is missing: ${marker}`);
}

const sources = {
  select: read('src/devices/device_select.h'),
  active: read('src/devices/active_device.h'),
  metadata: read('src/core/firmware_metadata.cpp'),
  sketch: read('sketch.yaml'),
  localBuild: read('tools/build-firmware-local.ps1'),
  workflow: read('.github/workflows/firmware.yml'),
  packager: read('release-helper/package-ci-build.js'),
};

const profiles = [
  {
    key: 'waveshare_touch_lcd_4_3',
    profile: 'waveshare_4_3',
    define: 'DEVICE_WAVESHARE_TOUCH_LCD_4_3',
    chipFamily: 'ESP32-P4',
    flashSize: 32 * 1024 * 1024,
    activeMarker: 'waveshare_touch_lcd_4_3/device_waveshare_touch_lcd_4_3.h',
    rxVariant: 'repo-a8204',
    status: 'validation-pending',
  },
  {
    key: 'waveshare_touch_lcd_7b',
    profile: 'waveshare_7b',
    define: 'DEVICE_WAVESHARE_TOUCH_LCD_7B',
    chipFamily: 'ESP32-P4',
    flashSize: 32 * 1024 * 1024,
    activeMarker: 'waveshare_touch_lcd_7b/device_waveshare_touch_lcd_7b.h',
    rxVariant: 'repo-a8204',
    status: 'validation-pending',
  },
  {
    key: 'guition_jc1060p470c_v2',
    profile: 'guition_jc1060p470c_v2',
    define: 'DEVICE_GUITION_JC1060P470C_V2',
    chipFamily: 'ESP32-P4',
    flashSize: 16 * 1024 * 1024,
    activeMarker: 'guition_jc1060p470c_v2/device_guition_jc1060p470c_v2.h',
    rxVariant: 'repo-a8204',
    status: 'validation-pending',
  },
  {
    key: 'waveshare_s3_touch_lcd_4',
    profile: 'waveshare_s3_touch_lcd_4',
    define: 'DEVICE_WAVESHARE_S3_TOUCH_LCD_4',
    chipFamily: 'ESP32-S3',
    flashSize: 16 * 1024 * 1024,
    activeMarker: 'waveshare_s3_touch_lcd_4/device_waveshare_s3_touch_lcd_4.h',
    rxVariant: 'native-s3',
    status: 'supported',
  },
  {
    key: 'waveshare_s3_touch_lcd_4b',
    profile: 'waveshare_s3_touch_lcd_4b',
    define: 'DEVICE_WAVESHARE_S3_TOUCH_LCD_4B',
    chipFamily: 'ESP32-S3',
    flashSize: 16 * 1024 * 1024,
    activeMarker: 'waveshare_s3_touch_lcd_4b/device_waveshare_s3_touch_lcd_4b.h',
    rxVariant: 'native-s3',
    status: 'validation-pending',
  },
];

for (const expected of profiles) {
  const allowedLocalProfiles = sources.localBuild.match(/\[ValidateSet\(([^)]*)\)\]/)?.[1];
  assert.ok(allowedLocalProfiles?.includes(`'${expected.profile}'`),
    `${expected.key} must be accepted by the local build command`);
  requireMarker(sources.select, `defined(${expected.define})`, `${expected.key} selector`);
  requireMarker(sources.active, expected.activeMarker, `${expected.key} active driver`);
  requireMarker(sources.metadata, `"${expected.key}"`, `${expected.key} metadata`);
  requireMarker(sources.sketch, `  ${expected.profile}:`, `${expected.key} sketch profile`);
  requireMarker(
    sources.localBuild,
    `${expected.profile} = '${expected.define}'`,
    `${expected.key} local build mapping`,
  );
  requireMarker(sources.workflow, `profile: ${expected.profile}`, `${expected.key} CI profile`);
  requireMarker(sources.workflow, `key: ${expected.key}`, `${expected.key} CI key`);
  requireMarker(sources.workflow, `define: ${expected.define}`, `${expected.key} CI define`);
  requireMarker(sources.workflow, `rx_variant: ${expected.rxVariant}`, `${expected.key} CI transport`);
  requireMarker(
    sources.packager,
    `['${expected.key}', { key: '${expected.key}', siliconVariant: '${
      expected.chipFamily === 'ESP32-P4' ? 'pre_v3' : 'default'
    }' }]`,
    `${expected.key} release package mapping`,
  );

  const installer = DEVICE_PROFILES.find((profile) => profile.key === expected.key);
  assert.ok(installer, `${expected.key} installer profile is missing`);
  assert.equal(installer.buildProfile, expected.profile);
  assert.equal(installer.chipFamily, expected.chipFamily);
  assert.equal(installer.flashSize, expected.flashSize);
  assert.equal(installer.status, expected.status);
  if (expected.chipFamily === 'ESP32-S3') {
    const nativeProfiles = sources.localBuild.match(/\$nativeS3Profiles\s*=\s*@\(([^)]*)\)/)?.[1];
    assert.ok(nativeProfiles?.includes(`'${expected.profile}'`),
      `${expected.key} must use native S3 compilation and skip ESP-Hosted patching`);
  }
}

assert.match(
  sources.localBuild,
  /\$nativeS3Profiles\s*=\s*@\([\s\S]*'guition_esp32_4848s040',[\s\S]*'waveshare_s3_touch_lcd_4b'[\s\S]*\)/,
  'both native-S3 profiles must bypass ESP-Hosted patching',
);
requireMarker(
  sources.workflow,
  "if: matrix.rx_variant != 'native-s3'",
  'native-S3 CI marker scope',
);
requireMarker(sources.workflow, '-eq 30', '15-build release asset count');

console.log('New device profile integration contract: PASS');

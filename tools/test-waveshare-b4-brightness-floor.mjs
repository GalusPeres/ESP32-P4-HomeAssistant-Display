import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');
}

function rawFromPercent(percent, inputMin) {
  percent = Math.max(1, Math.min(100, percent));
  const span = 255 - inputMin;
  return inputMin + Math.floor(((percent - 1) * span + 49) / 99);
}

function percentFromRaw(raw, inputMin) {
  raw = Math.max(inputMin, raw);
  const span = 255 - inputMin;
  return span === 0
    ? 100
    : 1 + Math.floor(((raw - inputMin) * 99 + Math.floor(span / 2)) / span);
}

const profile = read('src/devices/waveshare_4b/device_waveshare_4b.h');
const deviceHeader = read('src/devices/device.h');
const configManager = read('src/core/config_manager.cpp');
const settingsUi = read('src/ui/tab_settings.cpp');
const screensaver = read('src/ui/image_screensaver.cpp');
const mqttHandlers = read('src/network/mqtt_handlers.cpp');
const driver = read('src/devices/waveshare_4b/device_waveshare_4b.cpp');

assert.match(
  profile,
  /kVisibleBacklightRawMin = 122;[\s\S]*?Device::Profile kProfile[\s\S]*?kVisibleBacklightRawMin,/,
  'B4 must map visible 1 percent to its hardware-confirmed raw PWM floor',
);

assert.equal(rawFromPercent(1, 122), 122);
assert.equal(rawFromPercent(100, 122), 255);
assert.equal(percentFromRaw(122, 122), 1);
assert.equal(percentFromRaw(255, 122), 100);

assert.match(
  deviceHeader,
  /#else\s*inline constexpr uint8_t kConfiguredBrightnessPercentMin = 1;/,
  'B4-facing brightness controls must keep their public minimum at 1 percent',
);
assert.match(
  settingsUi,
  /Device::backlightPercentFromRaw[\s\S]*?Device::backlightRawFromPercent/,
  'normal display settings must use the shared calibrated conversion',
);
assert.match(
  screensaver,
  /setDisplayBrightness\(Device::backlightRawFromPercent\(/,
  'screensaver brightness must use the shared calibrated conversion',
);
assert.match(
  mqttHandlers,
  /brightnessPctFromRaw[\s\S]*?Device::backlightPercentFromRaw/,
  'Home Assistant reporting must use the shared calibrated conversion',
);
assert.match(
  mqttHandlers,
  /brightnessRawFromPct[\s\S]*?Device::backlightRawFromPercent/,
  'Home Assistant commands must use the shared calibrated conversion',
);
assert.match(
  configManager,
  /defined\(DEVICE_WAVESHARE_TOUCH_LCD_10_1\) \|\| \\\s*defined\(DEVICE_WAVESHARE_4B\)[\s\S]*?return configured_min;/,
  'persisted B4 values below the visible floor must migrate to 1 percent',
);

assert.match(
  driver,
  /void DeviceWaveshare4B::displaySleep\(\)[\s\S]*?apply_backlight\(0\);/,
  'raw 0 must remain available internally for display sleep',
);
assert.match(
  driver,
  /void DeviceWaveshare4B::prepareForRestart\(\)[\s\S]*?apply_backlight\(0\);/,
  'raw 0 must remain available internally for restart blanking',
);

console.log('Waveshare B4 brightness floor contract OK');

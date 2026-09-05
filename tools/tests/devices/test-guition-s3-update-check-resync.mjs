import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');
}

function requireMarker(source, marker, label) {
  assert.ok(source.includes(marker), `${label} is missing: ${marker}`);
}

function requireOrder(source, markers, label) {
  let position = -1;
  for (const marker of markers) {
    const next = source.indexOf(marker, position + 1);
    assert.ok(next >= 0, `${label} is missing: ${marker}`);
    position = next;
  }
}

const sketch = read('HomeTiles.ino');
const update = read('src/core/firmware/github_update.cpp');
const device = read(
  'src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.cpp');
const deviceHeader = read(
  'src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.h');
const deviceFacade = read('src/devices/device.cpp');
const waveshareDevice = read(
  'src/devices/waveshare_s3_touch_lcd_4b/device_waveshare_s3_touch_lcd_4b.cpp');
const deviceSelect = read('src/devices/device_select.h');

const checkStart = sketch.indexOf(
  'static GithubUpdate::CheckResult perform_fw_check()');
const applyStart = sketch.indexOf('static void apply_fw_check()', checkStart);
assert.ok(checkStart >= 0 && applyStart > checkStart,
          'Firmware update-check function boundaries were not found');
const checkBody = sketch.slice(checkStart, applyStart);

requireOrder(checkBody, [
  'const bool s3_rgb_network_active = networkTransport.isConnected();',
  '#if defined(DEVICE_GUITION_ESP32_4848S040)',
  'Device::displayUpdateCheckGuardBegin();',
  'GithubUpdate::CheckResult res = GithubUpdate::checkLatest();',
  '#if defined(DEVICE_GUITION_ESP32_4848S040)',
  'Device::displayUpdateCheckGuardEnd();',
  '#elif defined(DEVICE_WAVESHARE_S3_TOUCH_LCD_4B)',
  'Device::storageWriteBegin();',
  'Device::storageWriteEnd();',
  'fw_last_check_result = res;',
], 'ESP32-S3 RGB update-check guard order');

const checkCall = checkBody.indexOf('GithubUpdate::checkLatest()');
assert.ok(checkBody.indexOf('Device::displayUpdateCheckGuardBegin();') < checkCall,
          'Guition PCLK guard must begin before HTTPS');
assert.ok(checkBody.indexOf('Device::displayUpdateCheckGuardEnd();') > checkCall,
          'Guition PCLK guard must end after HTTPS');
assert.doesNotMatch(checkBody.slice(0, checkCall),
                    /Device::storageWriteBegin\s*\(/,
                    'The blocking HTTPS check must not run under a blackout');

for (const marker of [
  'constexpr uint32_t kUpdateCheckRgbPclkHz = 6000000;',
  'esp_err_t setPixelClockAndResynchronize(uint32_t pclk_hz,',
  'void DeviceGuitionESP324848S040::displayUpdateCheckGuardBegin()',
  'void DeviceGuitionESP324848S040::displayUpdateCheckGuardEnd()',
]) {
  requireMarker(device, marker, 'Guition update-check display guard');
}

const pclkMethodStart = device.indexOf(
  'esp_err_t setPixelClockAndResynchronize(uint32_t pclk_hz,');
const framebufferStart = device.indexOf(
  'uint16_t* framebuffer(uint8_t index) const', pclkMethodStart);
assert.ok(pclkMethodStart >= 0 && framebufferStart > pclkMethodStart,
          'Guition PCLK method boundaries were not found');
const pclkMethod = device.slice(pclkMethodStart, framebufferStart);
requireOrder(pclkMethod, [
  'canonicalizeForStorage()',
  'esp_lcd_rgb_panel_set_pclk(panel_handle_, pclk_hz)',
  'restartAfterStorage(wait_ms)',
], 'Guition VSYNC-safe PCLK change');

const guardBegin = device.slice(
  device.indexOf(
    'void DeviceGuitionESP324848S040::displayUpdateCheckGuardBegin()'),
  device.indexOf(
    'void DeviceGuitionESP324848S040::displayUpdateCheckGuardEnd()'));
const guardEnd = device.slice(
  device.indexOf(
    'void DeviceGuitionESP324848S040::displayUpdateCheckGuardEnd()'),
  device.indexOf('void DeviceGuitionESP324848S040::displayWaitDMA()'));
requireMarker(guardBegin,
              'setPixelClockAndResynchronize(kUpdateCheckRgbPclkHz, wait_ms)',
              'Guition reduced-PCLK guard');
requireMarker(guardEnd,
              'setPixelClockAndResynchronize(kRgbPclkHz, wait_ms)',
              'Guition normal-PCLK restore');
for (const marker of [
  'void displayUpdateCheckGuardBegin();',
  'void displayUpdateCheckGuardEnd();',
]) {
  requireMarker(deviceHeader, marker, 'Guition update-check guard API');
}
requireMarker(deviceFacade, '#if defined(DEVICE_GUITION_ESP32_4848S040)',
              'Exact-device update-check guard facade');

const allocatorStart = update.indexOf(
  'void* checkTlsPreferredCalloc(size_t count, size_t size)');
const allocatorEnd = update.indexOf('void checkTlsHeapFree', allocatorStart);
assert.ok(allocatorStart >= 0 && allocatorEnd > allocatorStart,
          'TLS allocator policy boundaries were not found');
const allocator = update.slice(allocatorStart, allocatorEnd);
const s3AllocatorStart = allocator.indexOf(
  '#if defined(DEVICE_ESP32_S3_RGB_480)');
const p4AllocatorStart = allocator.indexOf('#else', s3AllocatorStart);
const allocatorBranchEnd = allocator.indexOf('#endif', p4AllocatorStart);
assert.ok(s3AllocatorStart >= 0 && p4AllocatorStart > s3AllocatorStart &&
          allocatorBranchEnd > p4AllocatorStart,
          'TLS allocator must retain separate S3 and P4 policies');
const s3Allocator = allocator.slice(s3AllocatorStart, p4AllocatorStart);
const p4Allocator = allocator.slice(p4AllocatorStart, allocatorBranchEnd);
assert.ok(s3Allocator.indexOf('checkTlsInternalCalloc') <
          s3Allocator.indexOf('MALLOC_CAP_SPIRAM'),
          'S3 TLS must prefer internal RAM before PSRAM fallback');
assert.ok(p4Allocator.indexOf('MALLOC_CAP_SPIRAM') <
          p4Allocator.indexOf('checkTlsInternalCalloc'),
          'P4 TLS must retain PSRAM-first allocation');

for (const [label, source] of [
  ['Guition S3', device],
  ['Waveshare S3', waveshareDevice],
]) {
  requireOrder(source, [
    'bool canonicalizeForStorage()',
    'Cache_WriteBack_Addr(',
    'esp_lcd_panel_draw_bitmap(',
    'esp_err_t restartAfterStorage(uint32_t& wait_ms)',
    'esp_lcd_rgb_panel_restart(panel_handle_)',
    'enableVsyncInterruptOneShot();',
    'maskVsyncInterrupt();',
  ], `${label} canonical framebuffer recovery`);
}

for (const marker of [
  'defined(DEVICE_GUITION_ESP32_4848S040) ||',
  'defined(DEVICE_WAVESHARE_S3_TOUCH_LCD_4B)',
  '#define DEVICE_ESP32_S3_RGB_480',
]) {
  requireMarker(deviceSelect, marker, 'S3 RGB family mapping');
}

console.log('ESP32-S3 RGB update-check prevention: PASS');

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
    position = next;
  }
}

const sketch = read('HomeTiles.ino');
const device = read(
  'src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.cpp');

const checkStart = sketch.indexOf(
  'static GithubUpdate::CheckResult perform_fw_check()');
const applyStart = sketch.indexOf('static void apply_fw_check()', checkStart);
if (checkStart < 0 || applyStart < 0) {
  throw new Error('Firmware update-check function boundaries were not found');
}
const checkBody = sketch.slice(checkStart, applyStart);

requireOrder(checkBody, [
  'if (fw_last_check_valid &&',
  'return fw_last_check_result;',
  'const bool s3_rgb_resync_required = networkTransport.isConnected();',
  'GithubUpdate::CheckResult res = GithubUpdate::checkLatest();',
  'if (s3_rgb_resync_required) {',
  'Device::storageWriteBegin();',
  'Device::storageWriteEnd();',
  'fw_last_check_result = res;',
], 'Guition S3 update-check recovery order');

const checkCall = checkBody.indexOf('GithubUpdate::checkLatest()');
const recoveryGuard = checkBody.slice(checkCall).match(
  /#if defined\(DEVICE_ESP32_S3_RGB_480\)([\s\S]*?)#endif/);
if (!recoveryGuard) {
  throw new Error('Update-check recovery is not guarded for the exact S3 profile');
}
for (const marker of [
  'if (s3_rgb_resync_required)',
  'Device::storageWriteBegin();',
  'Device::storageWriteEnd();',
]) {
  requireMarker(recoveryGuard[1], marker, 'Guition S3 guarded recovery');
}

if (checkBody.slice(0, checkCall).includes('Device::storageWriteBegin();')) {
  throw new Error('S3 RGB blackout must not cover the blocking TLS check');
}

requireOrder(device, [
  'bool canonicalizeForStorage()',
  'Cache_WriteBack_Addr(',
  'esp_lcd_panel_draw_bitmap(',
  'esp_err_t restartAfterStorage(uint32_t& wait_ms)',
  'esp_lcd_rgb_panel_restart(panel_handle_)',
  'enableVsyncInterruptOneShot();',
  'maskVsyncInterrupt();',
], 'Guition S3 canonical framebuffer recovery');

console.log('Guition ESP32-4848S040 update-check RGB resync: PASS');

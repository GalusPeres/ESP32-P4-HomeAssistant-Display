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

const batchedNvs = read('src/core/batched_nvs_write.h');
const configManager = read('src/core/config_manager.cpp');
const webHandlers = read('src/web/web_admin_handlers.cpp');
const device = read(
  'src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.cpp');

const s3Start = batchedNvs.indexOf(
  '#if defined(DEVICE_GUITION_ESP32_4848S040)');
const fallbackStart = batchedNvs.indexOf('\n#else', s3Start);
if (s3Start < 0 || fallbackStart < 0) {
  throw new Error('The exact Guition S3 NVS branch was not found');
}
const s3Branch = batchedNvs.slice(s3Start, fallbackStart);

requireMarker(
  s3Branch,
  'inline constexpr bool kNeedsDisplayGuard = true;',
  'Guition S3 native NVS transaction display guard');
requireMarker(
  s3Branch,
  'display_guard=1 result=%s(0x%X)',
  'Guition S3 NVS diagnostic');

if (s3Branch.includes('kNeedsDisplayGuard = false')) {
  throw new Error('Guition S3 NVS writes must never bypass the display guard');
}

requireMarker(
  configManager,
  'Device::ScopedStorageWrite storage_write(\n      BatchedNvsWrite::kNeedsDisplayGuard);',
  'ConfigManager NVS transaction guard');

const saveMqttStart = webHandlers.indexOf(
  'void WebAdminServer::handleSaveMQTT()');
const saveBridgeStart = webHandlers.indexOf(
  'void WebAdminServer::handleSaveBridge()', saveMqttStart);
if (saveMqttStart < 0 || saveBridgeStart < 0) {
  throw new Error('Web Admin settings-save handler boundaries were not found');
}
const saveMqttBody = webHandlers.slice(saveMqttStart, saveBridgeStart);
for (const marker of [
  'server.hasArg("settings_access_present")',
  'server.hasArg("settings_tile_snapshot_present")',
  'configManager.save(cfg)',
  'tileConfig.setSettingsTileVisible(',
]) {
  requireMarker(
    saveMqttBody,
    marker,
    'Web Admin Settings tile/access storage path');
}

const combinedGuardMarker =
  'Device::ScopedStorageWrite settings_visibility_storage_write(\n' +
  '        settings_visibility_commit_needed);';
requireMarker(
  saveMqttBody,
  '#if defined(DEVICE_GUITION_ESP32_4848S040)\n' +
    '  // Hiding or restoring Settings updates NVS and LittleFS.',
  'Exact-profile combined Settings visibility guard');
requireMarker(
  saveMqttBody,
  combinedGuardMarker,
  'Combined Settings NVS/LittleFS display guard');

const commitStart = saveMqttBody.indexOf('auto commit_settings = [&]()');
const combinedGuardStart = saveMqttBody.indexOf(combinedGuardMarker);
const combinedCommit = saveMqttBody.indexOf('commit_settings();', combinedGuardStart);
const combinedGuardEnd = saveMqttBody.indexOf('#endif', combinedCommit);
const responseStart = saveMqttBody.indexOf('if (settings_config_saved)', combinedGuardEnd);
const configSave = saveMqttBody.indexOf('configManager.save(cfg)', commitStart);
const visibilitySave = saveMqttBody.indexOf(
  'tileConfig.setSettingsTileVisible(', configSave);
if (!(commitStart >= 0 && configSave > commitStart &&
      visibilitySave > configSave && combinedGuardStart > visibilitySave &&
      combinedCommit > combinedGuardStart && combinedGuardEnd > combinedCommit &&
      responseStart > combinedGuardEnd)) {
  throw new Error(
    'Settings NVS and LittleFS commits are not enclosed by one S3 guard');
}

for (const marker of [
  'void DeviceGuitionESP324848S040::storageWriteBegin()',
  'g_gfx->canonicalizeForStorage()',
  'void DeviceGuitionESP324848S040::storageWriteEnd()',
  'g_gfx ? g_gfx->restartAfterStorage(restart_wait_ms)',
]) {
  requireMarker(device, marker, 'Guition S3 RGB storage recovery');
}

console.log('Guition ESP32-4848S040 NVS display guard: PASS');

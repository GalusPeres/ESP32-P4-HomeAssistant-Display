import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const toolsDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolsDir, '..');
const read = (relativePath) =>
  fs.readFileSync(path.join(root, relativePath), 'utf8');

const sdmmc = read(
  'src/devices/guition_jc8012p4a1/vendor/guition_sdmmc.cpp',
);
const version = read('src/core/firmware_version.h');
const sketch = read('HomeTiles.ino');
const network = read('src/network/network_manager.cpp');
const webConfig = read('src/web/web_config.cpp');

assert.match(
  sdmmc,
  /host\.flags \|= SDMMC_HOST_FLAG_4BIT;/,
  'JC8012 V1 must preserve the flags from SDMMC_HOST_DEFAULT()',
);
assert.doesNotMatch(
  sdmmc,
  /host\.flags = SDMMC_HOST_FLAG_4BIT;/,
  'JC8012 V1 must not replace the default SDMMC host flags',
);

const betaGate =
  /defined\(DEVICE_GUITION_JC8012P4A1\)[\s\S]{0,100}defined\(HOMETILES_ISSUE30_SAFE_BETA\)/;
assert.match(version, betaGate);
assert.match(version, /#define FW_VERSION "v0\.6\.8b2"/);

const stationStart = network.match(
  /bool HomeTilesNetworkManager::ensureWifiStationStarted\(\)[\s\S]*?\r?\n}\r?\n\r?\nvoid HomeTilesNetworkManager::stopWifiForWired/,
)?.[0] ?? '';
assert.match(
  stationStart,
  /defined\(DEVICE_GUITION_JC8012P4A1\)[\s\S]*Device::suspendSDCardForNetworkTransition\(\)[\s\S]*WiFi\.mode\(WIFI_STA\)[\s\S]*Device::resumeSDCardAfterNetworkTransition\(\)/,
  'JC8012 V1 must not keep SDMMC slot 0 mounted while Hosted starts on slot 1',
);

const configModeChange = webConfig.match(
  /bool setWifiModeWithSdRemount\(wifi_mode_t mode\)[\s\S]*?\r?\n}/,
)?.[0] ?? '';
assert.match(
  configModeChange,
  /defined\(DEVICE_GUITION_JC8012P4A1\)[\s\S]*Device::suspendSDCardForNetworkTransition\(\)[\s\S]*WiFi\.mode\(mode\)[\s\S]*Device::resumeSDCardAfterNetworkTransition\(\)/,
  'JC8012 V1 configuration mode changes must preserve both SDMMC slots',
);
assert.doesNotMatch(
  configModeChange,
  /DEVICE_GUITION_JC8012P4A1_FAMILY|DEVICE_GUITION_JC8012P4A1_V2/,
  'The unverified JC8012 workaround must remain limited to the exact V1 profile',
);

for (const source of [sketch, network]) {
  assert.doesNotMatch(source, /Jc8012C6Recovery|HOMETILES_JC8012_C6_RECOVERY/);
}

assert.equal(
  fs.existsSync(path.join(root, 'src/network/jc8012_c6_recovery.cpp')),
  false,
  'The safe beta must not contain a C6 firmware updater',
);
assert.equal(
  fs.existsSync(path.join(root, 'tools/package-jc8012-c6-recovery.ps1')),
  false,
  'The safe beta must not contain a C6 recovery packager',
);

console.log('JC8012P4A1 V1 safe SDMMC host flag tests passed.');

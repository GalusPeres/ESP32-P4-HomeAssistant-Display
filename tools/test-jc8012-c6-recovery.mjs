import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const toolsDir = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(toolsDir, '..');
const read = (relativePath) =>
  fs.readFileSync(path.join(root, relativePath), 'utf8');

const recovery = read('src/network/jc8012_c6_recovery.cpp');
const network = read('src/network/network_manager.cpp');
const sketch = read('HomeTiles.ino');
const sdmmc = read(
  'src/devices/guition_jc8012p4a1/vendor/guition_sdmmc.cpp',
);
const version = read('src/core/firmware_version.h');
const packager = read('tools/package-jc8012-c6-recovery.ps1');

const exactGate =
  /defined\(DEVICE_GUITION_JC8012P4A1\)[\s\S]{0,100}defined\(HOMETILES_JC8012_C6_RECOVERY\)/;
assert.match(recovery, exactGate, 'recovery implementation must use the exact V1 gate');
assert.match(network, exactGate, 'network integration must use the exact V1 gate');
assert.doesNotMatch(
  recovery.slice(0, recovery.indexOf('#else')),
  /DEVICE_GUITION_JC8012P4A1_FAMILY|DEVICE_GUITION_JC8012P4A1_V2/,
  'recovery must not include V2 or the JC8012 family gate',
);

assert.match(recovery, /constexpr uint32_t kWriteChunkSize = 1400;/);
assert.match(recovery, /constexpr uint32_t kWriteYieldMs = 10;/);
assert.match(recovery, /not retrying/);
assert.match(recovery, /hostedBeginUpdate\(\)/);
assert.match(recovery, /hostedWriteUpdate\(buffer, length\)/);
assert.match(recovery, /hostedEndUpdate\(\)/);
assert.match(
  recovery,
  /versionSupportsActivate[\s\S]*esp_hosted_slave_ota_activate\(\)/,
  'Activate must remain version-gated for legacy Guition firmware',
);
assert.match(recovery, /E3, 0x2F, 0xBA, 0x38/);
assert.match(recovery, /kPayloadSize = 1191424;/);
assert.match(recovery, /kPayloadOffset = 4096;/);
assert.match(recovery, /RecoveryState::InProgress/);
assert.match(recovery, /RecoveryState::AwaitVerify/);
assert.match(recovery, /RecoveryState::FailedUncertain/);
assert.match(recovery, /ESP_CHIP_ID_ESP32C6/);
assert.match(recovery, /kApp0Address = 0x10000/);
assert.match(recovery, /kApp1Address = 0x690000/);
assert.doesNotMatch(recovery, /hostedActivateUpdate\(\)/);

const recoveryCall = sketch.indexOf('Jc8012C6Recovery::runIfPresent()');
const networkInit = sketch.indexOf('networkManager.init()', recoveryCall);
assert.ok(recoveryCall >= 0 && networkInit > recoveryCall);
assert.match(recovery, /Device::suspendSDCardForNetworkTransition\(\)/);
assert.match(recovery, /WiFi\.mode\(WIFI_STA\)/);
assert.match(recovery, /Device::resumeSDCardAfterNetworkTransition\(\)/);
assert.match(network, /HomeTilesNetworkManager::connectWifi[\s\S]*Jc8012C6Recovery::isBlocked\(\)/);
assert.match(network, /HomeTilesNetworkManager::update[\s\S]*Jc8012C6Recovery::isBlocked\(\)/);
assert.match(sketch, /enable && Jc8012C6Recovery::isBlocked\(\)/);

assert.match(
  sdmmc,
  /host\.flags \|= SDMMC_HOST_FLAG_4BIT;/,
  'V1 SD mount must preserve SDMMC_HOST_FLAG_DEINIT_ARG',
);
assert.match(version, /#define FW_VERSION "v0\.6\.8b2"/);

assert.match(packager, /esp32c6-v2\.11\.6\.bin/);
assert.match(
  packager,
  /E32FBA3864AB4DB82C287A922DB83B7093D7D8592730D7A620887B7CFDF401E0/,
);
assert.match(packager, /\$app1Offset = 0x690000/);
assert.match(packager, /\$payloadOffset = 0x1000/);
assert.match(packager, /\$emptyApp1Sha256/);
assert.match(packager, /HomeTilesBeta\.bin/);
assert.match(packager, /-EspHostedRxVariant repo-a8204/);

console.log('JC8012P4A1 V1 C6 recovery contract tests passed.');

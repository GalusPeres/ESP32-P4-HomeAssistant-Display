import assert from 'node:assert/strict';
import fs from 'node:fs';

const sketch = fs.readFileSync(new URL('../HomeTiles.ino', import.meta.url), 'utf8');
const device = fs.readFileSync(
  new URL(
    '../src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.cpp',
    import.meta.url,
  ),
  'utf8',
);

const setupStart = sketch.indexOf('void setup()');
const setupEnd = sketch.indexOf('\nvoid loop()', setupStart);
assert.ok(setupStart >= 0 && setupEnd > setupStart, 'setup() body must exist');
const setup = sketch.slice(setupStart, setupEnd);

const networkSetup = setup.indexOf('networkManager.beginMqttWorker();');
const noConfigBranch = setup.indexOf(
  'Serial.println("[Setup] Ueberspringe Netzwerk (keine Config)");',
);
const guard = setup.indexOf('#if defined(DEVICE_GUITION_ESP32_4848S040)', noConfigBranch);
const begin = setup.indexOf('Device::storageWriteBegin();', guard);
const end = setup.indexOf('Device::storageWriteEnd();', begin);
const activityReset = setup.indexOf('displayManager.resetActivityTimer();', end);

assert.ok(networkSetup >= 0, 'network/MQTT setup marker must exist');
assert.ok(noConfigBranch > networkSetup, 'no-config branch must follow network setup');
assert.ok(guard > noConfigBranch, 'S3 resync must run after both network branches');
assert.ok(begin > guard && end > begin, 'S3 boot resync guard must be balanced');
assert.ok(
  activityReset > end,
  'S3 boot resync must finish before setup becomes ready for interaction',
);
assert.match(
  setup.slice(guard, begin),
  /Resynchronizing RGB scanout after boot/,
  'boot resync must remain diagnosable',
);
assert.doesNotMatch(
  setup.slice(guard, activityReset),
  /ESP\.restart|BoardHAL::restart/,
  'boot resync must not reboot the device',
);

for (const marker of [
  'bool canonicalizeForStorage()',
  'esp_err_t restartAfterStorage(uint32_t& wait_ms)',
  'enableVsyncInterruptOneShot();',
  'maskVsyncInterrupt();',
]) {
  assert.ok(device.includes(marker), `missing guarded RGB recovery marker: ${marker}`);
}

console.log('Guition S3 boot RGB resync contract OK');

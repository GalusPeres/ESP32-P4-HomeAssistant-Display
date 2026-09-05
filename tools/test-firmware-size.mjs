import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { compareFirmwareSizes } from './compare-firmware-size.mjs';
import { getBuildProfile } from './device-catalog.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const build = path.join(root, 'build');
fs.mkdirSync(build, { recursive: true });
const directory = fs.mkdtempSync(path.join(build, 'test-firmware-size-'));
const baseline = path.join(directory, 'baseline');
const candidate = path.join(directory, 'candidate');
const profileNames = ['waveshare_8', 'guition_esp32_4848s040'];
function writeFirmware(target, name, size = 512, metadataName = name) {
  const profile = getBuildProfile(metadataName);
  const bytes = Buffer.alloc(size);
  bytes[0] = 0xe9;
  bytes.writeUInt32LE(0xabcd5432, 32);
  bytes.writeUInt32LE(0x44565034, 288);
  bytes.write(profile.metadataDeviceKey, 324);
  bytes.writeUInt32LE(0x53525634, 388);
  bytes.writeUInt16LE(profile.minimumRevision, 392);
  bytes.writeUInt16LE(profile.maximumRevision, 394);
  bytes.write(profile.siliconVariant, 396);
  fs.mkdirSync(path.join(target, name), { recursive: true });
  fs.writeFileSync(path.join(target, name, 'HomeTiles.ino.bin'), bytes);
}
try {
  for (const name of profileNames) {
    writeFirmware(baseline, name);
    writeFirmware(candidate, name);
  }
  const equal = compareFirmwareSizes(baseline, candidate, profileNames);
  assert.equal(equal.passed, true);
  assert.equal(equal.results[0].baseline.sha256, equal.results[0].candidate.sha256);
  writeFirmware(candidate, profileNames[0], 448);
  assert.equal(compareFirmwareSizes(baseline, candidate, profileNames).results[0].deltaBytes, -64);
  writeFirmware(candidate, profileNames[1], 513);
  const growth = compareFirmwareSizes(baseline, candidate, profileNames);
  assert.equal(growth.passed, false, 'Even one byte of growth must fail');
  assert.equal(growth.results[1].deltaBytes, 1);
  const reportPath = path.join(directory, 'result.json');
  const cli = spawnSync(process.execPath, ['tools/compare-firmware-size.mjs',
    '--baseline', baseline, '--candidate', candidate,
    '--profile', profileNames[0], '--profile', profileNames[1], '--report', reportPath],
  { cwd: root, encoding: 'utf8' });
  assert.notEqual(cli.status, 0);
  assert.deepEqual(JSON.parse(fs.readFileSync(reportPath, 'utf8')), growth);
  writeFirmware(candidate, profileNames[1], 512, profileNames[0]);
  assert.throws(() => compareFirmwareSizes(baseline, candidate, profileNames), /device mismatch/);
  assert.throws(() => compareFirmwareSizes(baseline, baseline, [profileNames[0]]), /different files/);
  assert.throws(() => compareFirmwareSizes(baseline, candidate, []), /distinct build profiles/);
  assert.throws(() => compareFirmwareSizes(baseline, candidate, [profileNames[0], profileNames[0]]), /distinct/);
} finally {
  assert.equal(path.dirname(path.resolve(directory)), path.resolve(build));
  fs.rmSync(directory, { recursive: true });
}
console.log('Firmware size comparison and board identity tests passed.');

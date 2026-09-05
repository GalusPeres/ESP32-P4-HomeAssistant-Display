import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const file = 'src/web/server/handlers/web_admin_diagnostics.cpp';
// Preprocess the real owner with its real diagnostic header. Platform type
// headers are irrelevant to this conditional-code test and remain substituted.
const source = fs.readFileSync(path.join(root, file), 'utf8')
  .replace(/^\s*#include[^\r\n]+/gm, line => line.includes('/s3_diagnostics.h"') ? line : '');
const compiler = [process.env.CXX, 'clang++', 'g++', 'c++'].filter(Boolean)
  .find(candidate => spawnSync(candidate, ['--version'], {encoding: 'utf8'}).status === 0);
if (!compiler) {
  console.log('SKIP: Screenshot diagnostic guard needs a C++ preprocessor');
  process.exit(0);
}
for (const [device, enabled, expected] of [
  ['DEVICE_GUITION_ESP32_4848S040', 1, true],
  ['DEVICE_GUITION_ESP32_4848S040', 0, false],
  ['DEVICE_WAVESHARE_S3_TOUCH_LCD_4', 1, false],
]) {
  const run = spawnSync(compiler, ['-E', '-P', '-x', 'c++', '-I', root,
    '-DCONFIG_IDF_TARGET_ESP32S3=1', `-D${device}=1`,
    `-DHOMETILES_GUITION_S3_DIAGNOSTICS=${enabled}`, '-'],
  {encoding: 'utf8', input: source});
  assert.equal(run.status, 0, run.stderr);
  assert.equal(run.stdout.includes('[S3Diag/Screenshot]'), expected,
    `${device} diagnostics=${enabled} lost its exact-profile screenshot guard`);
}
console.log('S3 screenshot diagnostic branch preserves enabled and disabled profiles.');

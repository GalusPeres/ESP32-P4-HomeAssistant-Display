import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), 'utf8')
    .replace(/\r\n?/g, '\n');
}

function requireMarker(source, marker, label) {
  if (!source.includes(marker)) {
    throw new Error(`${label} is missing: ${marker}`);
  }
}

function constantValue(source, name) {
  const match = source.match(new RegExp(
    `constexpr\\s+(?:u?int(?:8|16|32)_t|size_t)\\s+${name}\\s*=\\s*([^;]+);`));
  if (!match) throw new Error(`Constant was not found: ${name}`);
  return match[1].trim();
}

function parseNumber(token) {
  return Number(token.replace(/[uUlL]+$/g, ''));
}

function panelCommands(source) {
  const match = source.match(
    /const uint8_t kPanelInitOperations\[\] = \{([\s\S]*?)\};/);
  if (!match) throw new Error('Official ST7701 operation table was not found');

  const body = match[1].replace(/\/\/.*$/gm, '');
  const tokens = body.match(
    /BEGIN_WRITE|END_WRITE|WRITE_COMMAND_8|WRITE_C8_D8|WRITE_C8_D16|WRITE_BYTES|DELAY|0x[0-9A-Fa-f]+|\d+/g) || [];
  const commands = [];

  for (let index = 0; index < tokens.length;) {
    const operation = tokens[index++];
    if (operation === 'BEGIN_WRITE' || operation === 'END_WRITE') continue;
    if (operation === 'WRITE_COMMAND_8') {
      commands.push({ command: parseNumber(tokens[index++]), data: [], delay: 0 });
      continue;
    }
    if (operation === 'WRITE_C8_D8' || operation === 'WRITE_C8_D16') {
      const count = operation === 'WRITE_C8_D8' ? 1 : 2;
      const command = parseNumber(tokens[index++]);
      const data = tokens.slice(index, index + count).map(parseNumber);
      index += count;
      commands.push({ command, data, delay: 0 });
      continue;
    }
    if (operation === 'WRITE_BYTES') {
      if (commands.length === 0) throw new Error('ST7701 data has no command');
      const count = parseNumber(tokens[index++]);
      const data = tokens.slice(index, index + count).map(parseNumber);
      if (data.length !== count) throw new Error('ST7701 data is truncated');
      index += count;
      commands.at(-1).data.push(...data);
      continue;
    }
    if (operation === 'DELAY') {
      if (commands.length === 0) throw new Error('ST7701 delay has no command');
      commands.at(-1).delay = parseNumber(tokens[index++]);
      continue;
    }
    throw new Error(`Unsupported ST7701 operation token: ${operation}`);
  }
  return commands;
}

const header = read(
  'src/devices/waveshare_s3_touch_lcd_4/device_waveshare_s3_touch_lcd_4.h');
const driver = read(
  'src/devices/waveshare_s3_touch_lcd_4/device_waveshare_s3_touch_lcd_4.cpp');
const hardwareIo = read(
  'src/devices/waveshare_s3_touch_lcd_4/hardware_io_profile.h');

const manualSelection = read('src/devices/device_select.h').split(
  '#if !defined(HOMETILES_CI_TARGET)\n')[1].split('#endif')[0];
assert.deepEqual(
  [...manualSelection.matchAll(/^#define (DEVICE_\w+)$/gm)].map((match) => match[1]),
  ['DEVICE_WAVESHARE_4B'],
  'Adding LCD-4 must preserve the existing manual-build default',
);
assert.doesNotMatch(driver, /kTca|HelperType::TCA9554/,
  'Rev 4.0 must not claim the unvalidated older-board expander path');
assert.match(driver,
  /if \(!initHelperController\(\)\) \{[^}]*return false;/,
  'The exact Rev 4.0 helper must be detected before starting the panel');

for (const marker of [
  'namespace DeviceWaveshareS3TouchLCD4',
  '"waveshare_s3_touch_lcd_4"',
  '"Waveshare ESP32-S3-Touch-LCD-4"',
  '    480,\n    480,\n    4,\n    4,',
  'Device::RotationStepMode::QuarterTurns,\n    0,\n    2,',
  'Device::Capabilities{false, false, false, false, false, false}',
]) {
  requireMarker(header, marker, 'Waveshare S3 LCD-4 device profile');
}

const expectedConstants = new Map([
  ['kPanelSpiCs', '42'],
  ['kPanelSpiSck', '2'],
  ['kPanelSpiMosi', '1'],
  ['kPanelVsync', '39'],
  ['kPanelHsync', '38'],
  ['kPanelDe', '40'],
  ['kPanelPclk', '41'],
  ['kPanelB0', '5'],
  ['kPanelB1', '45'],
  ['kPanelB2', '48'],
  ['kPanelB3', '47'],
  ['kPanelB4', '21'],
  ['kPanelG0', '14'],
  ['kPanelG1', '13'],
  ['kPanelG2', '12'],
  ['kPanelG3', '11'],
  ['kPanelG4', '10'],
  ['kPanelG5', '9'],
  ['kPanelR0', '46'],
  ['kPanelR1', '3'],
  ['kPanelR2', '8'],
  ['kPanelR3', '18'],
  ['kPanelR4', '17'],
  ['kI2cSda', '15'],
  ['kI2cScl', '7'],
  ['kCh32Address', '0x24'],
  ['kRgbPclkHz', '10000000'],
  ['kRgbHsyncPulseWidth', '8'],
  ['kRgbHsyncBackPorch', '50'],
  ['kRgbHsyncFrontPorch', '10'],
  ['kRgbVsyncPulseWidth', '8'],
  ['kRgbVsyncBackPorch', '20'],
  ['kRgbVsyncFrontPorch', '10'],
]);
for (const [name, expected] of expectedConstants) {
  const actual = constantValue(driver, name);
  if (actual !== expected) {
    throw new Error(`${name} mismatch: expected ${expected}, got ${actual}`);
  }
}

for (const marker of [
  'constexpr uint32_t kExpectedFlashBytes = 16U * 1024U * 1024U;',
  'constexpr uint32_t kExpectedPsramBytes = 8U * 1024U * 1024U;',
  'flash_bytes != kExpectedFlashBytes',
  'psram_bytes != kExpectedPsramBytes',
  'kPanelB0, kPanelB1, kPanelB2, kPanelB3, kPanelB4,\n'
    + '        kPanelG0, kPanelG1, kPanelG2, kPanelG3, kPanelG4, kPanelG5,\n'
    + '        kPanelR0, kPanelR1, kPanelR2, kPanelR3, kPanelR4',
  'new Arduino_SWSPI(\n'
    + '      GFX_NOT_DEFINED /* DC */,\n'
    + '      kPanelSpiCs /* CS = 42 */,\n'
    + '      kPanelSpiSck /* SCK = 2 */,\n'
    + '      kPanelSpiMosi /* MOSI = 1 */,\n'
    + '      GFX_NOT_DEFINED /* MISO */);',
]) {
  requireMarker(driver, marker, 'Official Waveshare N16R8/RGB contract');
}

const expectedPanelCommands = [
  [0x11, [], 120],
  [0xFF, [0x77, 0x01, 0x00, 0x00, 0x10], 0],
  [0xC0, [0x3B, 0x00], 0],
  [0xC1, [0x0D, 0x02], 0],
  [0xC2, [0x21, 0x08], 0],
  [0xCD, [0x08], 0],
  [0xB0, [0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08,
          0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18], 0],
  [0xB1, [0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08,
          0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18], 0],
  [0xFF, [0x77, 0x01, 0x00, 0x00, 0x11], 0],
  [0xB0, [0x60], 0], [0xB1, [0x30], 0], [0xB2, [0x87], 0],
  [0xB3, [0x80], 0], [0xB5, [0x49], 0], [0xB7, [0x85], 0],
  [0xB8, [0x21], 0], [0xC1, [0x78], 0], [0xC2, [0x78], 20],
  [0xE0, [0x00, 0x1B, 0x02], 0],
  [0xE1, [0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00,
          0x00, 0x44, 0x44], 0],
  [0xE2, [0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00,
          0xEC, 0xA0, 0x00, 0x00], 0],
  [0xE3, [0x00, 0x00, 0x11, 0x11], 0], [0xE4, [0x44, 0x44], 0],
  [0xE5, [0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0,
          0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0], 0],
  [0xE6, [0x00, 0x00, 0x11, 0x11], 0], [0xE7, [0x44, 0x44], 0],
  [0xE8, [0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0,
          0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0], 0],
  [0xEB, [0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40], 0],
  [0xEC, [0x3C, 0x00], 0],
  [0xED, [0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF,
          0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA], 0],
  [0xFF, [0x77, 0x01, 0x00, 0x00, 0x00], 0],
  [0x36, [0x00], 0], [0x3A, [0x66], 0], [0x21, [], 120], [0x29, [], 0],
].map(([command, data, delay]) => ({ command, data, delay }));

const actualPanelCommands = panelCommands(driver);
if (JSON.stringify(actualPanelCommands) !== JSON.stringify(expectedPanelCommands)) {
  throw new Error('ST7701 init table differs from the official Waveshare BSP');
}

for (const marker of [
  'writeI2cRegister(kCh32Address, kCh32RegDirection, kCh32DirDefault);',
  'writeI2cRegister(kCh32Address, kCh32RegOutput, kCh32OutReset);',
  'writeI2cRegister(kCh32Address, kCh32RegOutput, kCh32OutDisplayOn);',
  'x = static_cast<int16_t>(raw_x);\n      y = static_cast<int16_t>(raw_y);',
  'writeI2cRegister(kCh32Address, kCh32RegPwm, pwm_val);',
]) {
  requireMarker(driver, marker, 'Official touch/backlight contract');
}

for (const marker of [
  'bool DeviceWaveshareS3TouchLCD4::initSDCard() {\n  return false;\n}',
  'bool DeviceWaveshareS3TouchLCD4::sdReady() {\n  return false;\n}',
  'bool DeviceWaveshareS3TouchLCD4::resumeSDCardAfterNetworkTransition() {\n'
    + '  return false;\n}',
  'return LittleFS;',
]) {
  requireMarker(driver, marker, 'No-SD cached behavior');
}

requireMarker(hardwareIo, 'kHardwareIoProfile{}', 'Hardware I/O profile');
if (/\{\s*\d+\s*,/.test(hardwareIo)) {
  throw new Error('Reserved Waveshare S3 LCD-4 pins must not be exposed as I/O');
}

console.log('Waveshare ESP32-S3-Touch-LCD-4 profile contract: PASS');

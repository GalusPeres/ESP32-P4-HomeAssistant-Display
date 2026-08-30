import crypto from 'node:crypto';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), 'utf8')
    .replace(/\r\n?/g, '\n');
}

function requireMarker(source, marker, label) {
  if (!source.includes(marker)) {
    throw new Error(`${label} is missing: ${marker}`);
  }
}

function displayConfigValues(source) {
  const match = source.match(/SCREEN_DEFAULT\s*=\s*\{([\s\S]*?)\n\};/);
  if (!match) throw new Error('Display config initializer was not found');
  return match[1]
    .replace(/\/\/.*$/gm, '')
    .split(',')
    .map((value) => value.trim())
    .filter(Boolean);
}

function normalizedInitSequence(source, tableName) {
  const table = source.match(new RegExp(
    `static const jd9165_lcd_init_cmd_t ${tableName}\\[\\] = \\{([\\s\\S]*?)\\n\\};`));
  if (!table) throw new Error(`${tableName} was not found`);

  return [...table[1].matchAll(
    /\{(0x[0-9A-Fa-f]+), \(uint8_t\[\]\)\{([^}]*)\}, (\d+), (\d+)\},/g)]
    .map((match) => [
      match[1].toLowerCase(),
      match[2].replace(/\s+/g, '').toLowerCase(),
      match[3],
      match[4],
    ].join(':'));
}

const v2Header = read(
  'src/devices/guition_jc1060p470c_v2/device_guition_jc1060p470c_v2.h');
const v2Driver = read(
  'src/devices/guition_jc1060p470c_v2/device_guition_jc1060p470c_v2.cpp');
const v2DisplaySource = read(
  'src/devices/guition_jc1060p470c_v2/vendor/displays_config.h');
const v2PanelSource = read(
  'src/devices/guition_jc1060p470c_v2/vendor/esp_lcd_jd9165.c');
const v2PanelHeader = read(
  'src/devices/guition_jc1060p470c_v2/vendor/esp_lcd_jd9165.h');
const v2TouchHeader = read(
  'src/devices/guition_jc1060p470c_v2/vendor/gt911.h');
const v2HardwareIo = read(
  'src/devices/guition_jc1060p470c_v2/hardware_io_profile.h');

for (const marker of [
  'namespace DeviceGuitionJC1060P470CV2',
  '"guition_jc1060p470c_v2"',
  '"Guition JC1060P470C V2"',
  'Device::RotationStepMode::FlipOnly,\n    0,\n    0,',
]) {
  requireMarker(v2Header, marker, 'JC1060 V2 device header');
}

for (const marker of [
  '#if defined(DEVICE_GUITION_JC1060P470C_V2)',
  'src/devices/guition_jc1060p470c_v2/vendor/displays_config.h',
  'constexpr gpio_num_t kBacklightPin = GPIO_NUM_23;',
  'constexpr uint32_t kBacklightFreq = 20000;',
  'constexpr ledc_timer_bit_t kBacklightBits = LEDC_TIMER_10_BIT;',
  'constexpr bool kBacklightActiveLow = false;',
]) {
  requireMarker(v2Driver, marker, 'JC1060 V2 device source');
}
if (v2Driver.includes('guition_jc1060p470c_v2_v2')) {
  throw new Error('JC1060 V2 driver contains a duplicated profile path');
}

const v2Display = displayConfigValues(v2DisplaySource);
const expectedV2Display = [
  '"JC1060P470C V2 New Panel"',
  '24', '136', '160',
  '2', '21', '12',
  '52000000', '750',
  '1024', '600',
  '7', '8', '400000', '0',
];
if (JSON.stringify(v2Display) !== JSON.stringify(expectedV2Display)) {
  throw new Error(`JC1060 V2 display contract mismatch: ${v2Display}`);
}

for (const marker of [
  '.lane_bit_rate_mbps = 750,',
  '.dpi_clock_freq_mhz = 52,',
  '.hsync_back_porch = 136,',
  '.hsync_pulse_width = 24,',
  '.hsync_front_porch = 160,',
  '.vsync_back_porch = 21,',
  '.vsync_pulse_width = 2,',
  '.vsync_front_porch = 12,',
]) {
  requireMarker(v2PanelHeader, marker, 'JC1060 V2 JD9165 header contract');
}

const v2Init = normalizedInitSequence(
  v2PanelSource, 'guition_jc1060p470c_v2_init_cmds');
for (const marker of [
  'init_cmds = guition_jc1060p470c_v2_init_cmds;',
  'sizeof(guition_jc1060p470c_v2_init_cmds)',
]) {
  requireMarker(v2PanelSource, marker, 'JC1060 V2 init selection');
}
requireMarker(v2Driver, 'vendor_cfg.init_cmds = nullptr;',
              'JC1060 V2 vendor default selection');
if (v2Init.length !== 60) {
  throw new Error(`JC1060 V2 init sequence is incomplete: ${v2Init.length} commands`);
}
const v2InitHash = crypto.createHash('sha256').update(v2Init.join('|')).digest('hex');
const expectedV2InitHash =
  'c63aba3524a765bf861c48c997ba90053a7e2cad9c417c8d739cfe213661b514';
if (v2InitHash !== expectedV2InitHash) {
  throw new Error(`JC1060 V2 init sequence changed: ${v2InitHash}`);
}
const expectedTail = [
  '0x3a:0x55:1:0',
  '0x11:0x00:1:120',
  '0x29:0x00:1:20',
];
if (JSON.stringify(v2Init.slice(-3)) !== JSON.stringify(expectedTail)) {
  throw new Error(`JC1060 V2 init tail mismatch: ${v2Init.slice(-3)}`);
}

for (const marker of [
  '#define EXAMPLE_PIN_NUM_TOUCH_RST       (GPIO_NUM_22)',
  '#define EXAMPLE_PIN_NUM_TOUCH_INT       (GPIO_NUM_21)',
]) {
  requireMarker(v2TouchHeader, marker, 'JC1060 V2 GT911 wiring');
}
if (/\{\s*0\s*,/.test(v2HardwareIo)) {
  throw new Error('GPIO0 must stay reserved for the JC1060 V2 LCD reset');
}
requireMarker(v2HardwareIo, '{5, Device::kHardwareIoPinSwitch',
              'JC1060 V2 expansion GPIO5');

const v1DisplaySource = read(
  'src/devices/guition_jc1060p470c/vendor/displays_config.h');
const v1PanelSource = read(
  'src/devices/guition_jc1060p470c/vendor/esp_lcd_jd9165.c');
const v1Driver = read(
  'src/devices/guition_jc1060p470c/device_guition_jc1060p470c.cpp');
const v1Display = displayConfigValues(v1DisplaySource);
for (const [index, expected] of [
  [7, '51200000'],
  [14, '27'],
]) {
  if (v1Display[index] !== expected) {
    throw new Error(`JC1060 V1 display behavior changed at field ${index}: ${v1Display[index]}`);
  }
}
for (const marker of [
  '{0x05, (uint8_t[]){0x00}, 1, 0}',
  '{0x23, (uint8_t[]){0x00}, 1, 0}',
  '{0x29, (uint8_t[]){0x04}, 1, 0}',
]) {
  requireMarker(v1PanelSource, marker, 'JC1060 V1 init sequence');
}
for (const source of [v1DisplaySource, v1PanelSource, v1Driver]) {
  if (source.includes('DEVICE_GUITION_JC1060P470C_V2') ||
      source.includes('guition_jc1060p470c_v2')) {
    throw new Error('JC1060 V1 source must stay independent from V2');
  }
}
if (v1PanelSource.includes('guition_jc1060p470c_v2_init_cmds')) {
  throw new Error('JC1060 V1 must not use the V2 New Panel init sequence');
}

console.log('Guition JC1060P470C V2 profile contract: PASS');

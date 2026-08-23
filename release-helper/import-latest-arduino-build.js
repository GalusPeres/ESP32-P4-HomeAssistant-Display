const fs = require('fs');
const os = require('os');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const projectName = 'HomeTiles.ino';
const releaseDir = path.join(repoRoot, 'build', 'releases');
const deviceSelectPath = path.join(repoRoot, 'src', 'devices', 'device_select.h');
const versionFilePath = path.join(repoRoot, 'version.txt');
const factoryAppOffset = 0x10000;

const releaseTargets = new Map([
  ['DEVICE_M5STACKS_TAB5', {
    key: 'm5stacks_tab5',
    slug: 'm5stacks-tab5',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_WAVESHARE_4B', {
    key: 'waveshare_4b',
    slug: 'waveshare-b4',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_WAVESHARE_TOUCH_LCD_7', {
    key: 'waveshare_touch_lcd_7',
    slug: 'waveshare-touch-lcd-7',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_WAVESHARE_TOUCH_LCD_7B', {
    key: 'waveshare_touch_lcd_7b',
    slug: 'waveshare-touch-lcd-7b',
    siliconVariants: new Map([
      ['pre_v3', {
        key: 'waveshare_touch_lcd_7b',
        slug: 'waveshare-touch-lcd-7b',
      }],
      ['rev3_1', {
        key: 'waveshare_touch_lcd_7b_rev3_1',
        slug: 'waveshare-touch-lcd-7b-rev3-1',
      }],
    ]),
  }],
  ['DEVICE_WAVESHARE_TOUCH_LCD_8', {
    key: 'waveshare_touch_lcd_8',
    slug: 'waveshare-touch-lcd-8',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_WAVESHARE_TOUCH_LCD_10_1', {
    key: 'waveshare_touch_lcd_10_1',
    slug: 'waveshare-touch-lcd-10-1',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_WAVESHARE_S3_TOUCH_LCD_4B', {
    key: 'waveshare_s3_touch_lcd_4b',
    slug: 'waveshare-s3-touch-lcd-4b',
    expectedSiliconVariant: 'default',
  }],
  ['DEVICE_GUITION_JC8012P4A1', {
    key: 'guition_jc8012p4a1',
    slug: 'guition-jc8012p4a1',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_GUITION_JC8012P4A1_V2', {
    key: 'guition_jc8012p4a1_v2',
    slug: 'guition-jc8012p4a1-v2',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_GUITION_JC1060P470C', {
    key: 'guition_jc1060p470c',
    slug: 'guition-jc1060p470c',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_GUITION_JC1060P470C_V2', {
    key: 'guition_jc1060p470c_v2',
    slug: 'guition-jc1060p470c-v2',
    expectedSiliconVariant: 'pre_v3',
  }],
  ['DEVICE_GUITION_ESP32_4848S040', {
    key: 'guition_esp32_4848s040',
    slug: 'guition-esp32-4848s040',
    expectedSiliconVariant: 'default',
  }],
]);

const nonReleaseTargets = new Set([
  'DEVICE_LAYOUT_TEST_1024X600',
  'DEVICE_LAYOUT_TEST_480X480',
]);

function readVersion() {
  const text = fs.readFileSync(versionFilePath, 'utf8');
  const defineMatch = text.match(/^\s*#define\s+FW_VERSION\s+"([^"]+)"/m);
  if (defineMatch) {
    return defineMatch[1].trim();
  }

  const firstLine = text
    .split(/\r?\n/)
    .map((line) => line.trim())
    .find((line) => line.length > 0);

  if (!firstLine) {
    throw new Error('version.txt is empty');
  }

  return firstLine;
}

function readManualDeviceSelection(source) {
  const lines = source.split(/\r?\n/);
  const selectedDefines = [];
  let inManualBlock = false;
  let conditionalDepth = 0;

  for (const line of lines) {
    if (!inManualBlock) {
      if (/^\s*#if\s+!defined\(HOMETILES_CI_TARGET\)\s*$/.test(line)) {
        inManualBlock = true;
        conditionalDepth = 1;
      }
      continue;
    }

    if (/^\s*#if(?:def|ndef)?\b/.test(line)) {
      conditionalDepth += 1;
      continue;
    }
    if (/^\s*#endif\b/.test(line)) {
      conditionalDepth -= 1;
      if (conditionalDepth === 0) break;
      continue;
    }
    if (conditionalDepth !== 1) continue;

    const defineMatch = line.match(/^\s*#define\s+(DEVICE_[A-Z0-9_]+)\b/);
    if (defineMatch) selectedDefines.push(defineMatch[1]);
  }

  if (!inManualBlock) {
    throw new Error('Manual device selection block is missing from src/devices/device_select.h');
  }
  if (selectedDefines.length === 0) {
    throw new Error(
      'No explicit release device is enabled in src/devices/device_select.h; refusing to assume Waveshare 4B.'
    );
  }
  if (selectedDefines.length > 1) {
    throw new Error(
      `Multiple device targets are enabled in src/devices/device_select.h: ${selectedDefines.join(', ')}`
    );
  }

  const selectedDefine = selectedDefines[0];
  if (nonReleaseTargets.has(selectedDefine)) {
    throw new Error(`${selectedDefine} is a layout-test target and cannot be packaged as release firmware.`);
  }
  const target = releaseTargets.get(selectedDefine);
  if (!target) {
    throw new Error(`Unsupported release device target in src/devices/device_select.h: ${selectedDefine}`);
  }

  return { define: selectedDefine, ...target };
}

function readDeviceInfo() {
  return readManualDeviceSelection(fs.readFileSync(deviceSelectPath, 'utf8'));
}

function readNullTerminatedString(buffer, offset, maxLength) {
  const endLimit = Math.min(buffer.length, offset + maxLength);
  let end = offset;
  while (end < endLimit && buffer[end] !== 0) end += 1;
  return buffer.subarray(offset, end).toString('utf8');
}

function parseFirmwareMetadata(image, appOffset = 0) {
  const imageHeaderMagic = 0xe9;
  const appDescriptorMagic = 0xabcd5432;
  const deviceDescriptorMagic = 0x44565034;
  const siliconDescriptorMagic = 0x53525634;
  const deviceDescriptorOffset = appOffset + 24 + 8 + 256;
  const deviceDescriptorLength = 4 + 32 + 32 + 32;
  const siliconOffset = deviceDescriptorOffset + deviceDescriptorLength;
  const siliconLength = 4 + 2 + 2 + 16;

  if (image.length < deviceDescriptorOffset + deviceDescriptorLength) {
    throw new Error('Firmware image is too small for HomeTiles device metadata.');
  }
  if (image[appOffset] !== imageHeaderMagic) {
    throw new Error(`Firmware has no ESP image header at 0x${appOffset.toString(16)}.`);
  }
  if (image.readUInt32LE(appOffset + 24 + 8) !== appDescriptorMagic) {
    throw new Error('Firmware has no ESP application descriptor.');
  }
  if (image.readUInt32LE(deviceDescriptorOffset) !== deviceDescriptorMagic) {
    throw new Error('Firmware has no HomeTiles device descriptor.');
  }

  const metadata = {
    deviceKey: readNullTerminatedString(image, deviceDescriptorOffset + 4 + 32, 32),
    silicon: null,
  };
  if (
    image.length >= siliconOffset + siliconLength &&
    image.readUInt32LE(siliconOffset) === siliconDescriptorMagic
  ) {
    const silicon = {
      minimumRevision: image.readUInt16LE(siliconOffset + 4),
      maximumRevision: image.readUInt16LE(siliconOffset + 6),
      variant: readNullTerminatedString(image, siliconOffset + 8, 16),
    };
    if (silicon.minimumRevision > silicon.maximumRevision) {
      throw new Error(
        `Firmware has an invalid silicon revision range: ${silicon.minimumRevision}-${silicon.maximumRevision}`
      );
    }
    metadata.silicon = silicon;
  }
  return metadata;
}

function resolveReleaseDevice(selection, metadata) {
  if (metadata.deviceKey !== selection.key) {
    throw new Error(
      `Latest Arduino build is for ${metadata.deviceKey || '(unknown)'}, but ${selection.define} is selected.`
    );
  }

  if (!selection.siliconVariants) {
    if (!metadata.silicon) {
      throw new Error(
        `${selection.define} firmware has no silicon-revision metadata; rebuild it before packaging.`
      );
    }
    if (metadata.silicon.variant !== selection.expectedSiliconVariant) {
      throw new Error(
        `${selection.define} firmware silicon variant mismatch: expected ${selection.expectedSiliconVariant}, got ${metadata.silicon.variant || '(empty)'}.`
      );
    }
    if (
      selection.expectedSiliconVariant === 'pre_v3' &&
      (metadata.silicon.minimumRevision !== 1 || metadata.silicon.maximumRevision !== 199)
    ) {
      throw new Error(
        `Unsafe ${selection.define} pre-v3 revision range: ${metadata.silicon.minimumRevision}-${metadata.silicon.maximumRevision}`
      );
    }
    return { key: selection.key, slug: selection.slug };
  }
  if (!metadata.silicon) {
    throw new Error(
      'Waveshare 7B firmware has no silicon-revision metadata; rebuild it before packaging.'
    );
  }

  const variantTarget = selection.siliconVariants.get(metadata.silicon.variant);
  if (!variantTarget) {
    throw new Error(
      `Unsupported Waveshare 7B silicon variant: ${metadata.silicon.variant || '(empty)'}`
    );
  }
  if (
    metadata.silicon.variant === 'pre_v3' &&
    (metadata.silicon.minimumRevision !== 1 || metadata.silicon.maximumRevision !== 199)
  ) {
    throw new Error(
      `Unsafe Waveshare 7B pre-v3 revision range: ${metadata.silicon.minimumRevision}-${metadata.silicon.maximumRevision}`
    );
  }
  if (
    metadata.silicon.variant === 'rev3_1' &&
    (metadata.silicon.minimumRevision !== 301 || metadata.silicon.maximumRevision !== 301)
  ) {
    throw new Error(
      `Unsafe Waveshare 7B v3.1 revision range: ${metadata.silicon.minimumRevision}-${metadata.silicon.maximumRevision}`
    );
  }
  return variantTarget;
}

function getArduinoSketchesPath() {
  if (process.env.ARDUINO_SKETCHES_PATH) {
    return process.env.ARDUINO_SKETCHES_PATH;
  }

  if (process.env.LOCALAPPDATA) {
    return path.join(process.env.LOCALAPPDATA, 'arduino', 'sketches');
  }

  return path.join(os.homedir(), 'AppData', 'Local', 'arduino', 'sketches');
}

function findLatestSuccessfulBuild(sketchesPath) {
  if (!fs.existsSync(sketchesPath)) {
    throw new Error(`Arduino sketches path not found: ${sketchesPath}`);
  }

  const dirs = fs
    .readdirSync(sketchesPath, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .map((entry) => {
      const fullPath = path.join(sketchesPath, entry.name);
      const stat = fs.statSync(fullPath);
      return { fullPath, mtimeMs: stat.mtimeMs };
    })
    .sort((a, b) => b.mtimeMs - a.mtimeMs);

  for (const dir of dirs) {
    const updatePath = path.join(dir.fullPath, `${projectName}.bin`);
    const factoryPath = path.join(dir.fullPath, `${projectName}.merged.bin`);
    if (fs.existsSync(updatePath) && fs.existsSync(factoryPath)) {
      return { buildPath: dir.fullPath, updatePath, factoryPath };
    }
  }

  throw new Error(`No successful Arduino build with ${projectName}.bin and ${projectName}.merged.bin was found.`);
}

function main() {
  const version = readVersion().replace(/[^A-Za-z0-9._-]/g, '-');
  const selection = readDeviceInfo();
  const sketchesPath = getArduinoSketchesPath();
  const build = findLatestSuccessfulBuild(sketchesPath);
  const updateMetadata = parseFirmwareMetadata(fs.readFileSync(build.updatePath));
  const factoryMetadata = parseFirmwareMetadata(
    fs.readFileSync(build.factoryPath),
    factoryAppOffset
  );
  const device = resolveReleaseDevice(selection, updateMetadata);
  const factoryDevice = resolveReleaseDevice(selection, factoryMetadata);
  if (factoryDevice.key !== device.key) {
    throw new Error(
      `Arduino update/factory build mismatch: ${device.key} vs ${factoryDevice.key}.`
    );
  }

  fs.mkdirSync(releaseDir, { recursive: true });

  const deviceFilePatterns = [
    new RegExp(`^hometiles_.*_${device.key}(_factory)?\\.bin$`),
    new RegExp(`^esp32-p4-homeassistant-display-.*-${device.slug}-(update|factory)\\.bin$`),
  ];
  for (const entry of fs.readdirSync(releaseDir, { withFileTypes: true })) {
    if (!entry.isFile()) continue;
    if (!deviceFilePatterns.some((pattern) => pattern.test(entry.name))) continue;
    fs.unlinkSync(path.join(releaseDir, entry.name));
  }

  const updateDest = path.join(releaseDir, `hometiles_${version}_${device.key}.bin`);
  const factoryDest = path.join(releaseDir, `hometiles_${version}_${device.key}_factory.bin`);

  fs.copyFileSync(build.updatePath, updateDest);
  fs.copyFileSync(build.factoryPath, factoryDest);

  console.log(`[release-helper] Build: ${build.buildPath}`);
  console.log(`[release-helper] ${path.basename(updateDest)}`);
  console.log(`[release-helper] ${path.basename(factoryDest)}`);
}

if (require.main === module) main();

module.exports = {
  parseFirmwareMetadata,
  readManualDeviceSelection,
  resolveReleaseDevice,
};

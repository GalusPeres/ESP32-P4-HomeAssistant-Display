const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const projectName = 'HomeTiles.ino';
const versionFilePath = path.join(repoRoot, 'version.txt');
const otaSlotSize = 0x680000;

const devices = new Map([
  ['m5stacks_tab5', { key: 'm5stacks_tab5' }],
  ['waveshare_4b', { key: 'waveshare_4b' }],
  ['waveshare_touch_lcd_7', { key: 'waveshare_touch_lcd_7' }],
  ['waveshare_touch_lcd_8', { key: 'waveshare_touch_lcd_8' }],
  ['waveshare_touch_lcd_8_short_tail_test', { key: 'waveshare_touch_lcd_8_short_tail_test' }],
  ['waveshare_touch_lcd_10_1', { key: 'waveshare_touch_lcd_10_1' }],
  ['guition_jc8012p4a1', { key: 'guition_jc8012p4a1' }],
  ['guition_jc1060p470c', { key: 'guition_jc1060p470c' }],
  ['guition_esp32_4848s040', { key: 'guition_esp32_4848s040' }],
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

function readArgs(argv) {
  const args = {};
  for (let i = 2; i < argv.length; i += 1) {
    const arg = argv[i];
    if (!arg.startsWith('--')) {
      throw new Error(`Unexpected argument: ${arg}`);
    }
    const key = arg.slice(2);
    const value = argv[i + 1];
    if (!value || value.startsWith('--')) {
      throw new Error(`Missing value for --${key}`);
    }
    args[key] = value;
    i += 1;
  }
  return args;
}

function walkFiles(root) {
  const files = [];
  const entries = fs.readdirSync(root, { withFileTypes: true });

  for (const entry of entries) {
    const fullPath = path.join(root, entry.name);
    if (entry.isDirectory()) {
      files.push(...walkFiles(fullPath));
    } else if (entry.isFile()) {
      files.push(fullPath);
    }
  }

  return files;
}

function selectSingleFile(files, predicate, label) {
  const matches = files.filter(predicate).sort();
  if (matches.length !== 1) {
    const shown = matches.length ? `\n${matches.join('\n')}` : '';
    throw new Error(`Expected exactly one ${label} binary, found ${matches.length}.${shown}`);
  }
  return matches[0];
}

function ensureCleanDeviceOutputs(outDir, deviceKey) {
  fs.mkdirSync(outDir, { recursive: true });
  const pattern = new RegExp(`^hometiles_.*_${deviceKey}(_factory)?\\.bin$`);

  for (const entry of fs.readdirSync(outDir, { withFileTypes: true })) {
    if (!entry.isFile()) continue;
    if (!pattern.test(entry.name)) continue;
    fs.unlinkSync(path.join(outDir, entry.name));
  }
}

function readNullTerminatedString(buffer, offset, maxLength) {
  const end = buffer.indexOf(0, offset);
  const limit = Math.min(end === -1 ? buffer.length : end, offset + maxLength);
  return buffer.subarray(offset, limit).toString('utf8');
}

function verifyDeviceDescriptor(imagePath, expectedDeviceKey) {
  const image = fs.readFileSync(imagePath);
  const descriptorOffset = 24 + 8 + 256;
  const descriptorLength = 4 + 32 + 32 + 32;
  const descriptorMagic = 0x44565034;

  if (image.length < descriptorOffset + descriptorLength) {
    throw new Error(`Firmware image is too small for metadata: ${imagePath}`);
  }
  if (image.readUInt32LE(descriptorOffset) !== descriptorMagic) {
    throw new Error(`Firmware metadata magic is missing: ${imagePath}`);
  }

  const deviceKey = readNullTerminatedString(image, descriptorOffset + 4 + 32, 32);
  if (deviceKey !== expectedDeviceKey) {
    throw new Error(
      `Firmware metadata device mismatch: expected ${expectedDeviceKey}, got ${deviceKey || '(empty)'}.`
    );
  }
}

function main() {
  const args = readArgs(process.argv);
  const buildDirArg = args['build-dir'] || process.env.BUILD_DIR;
  const outDir = path.resolve(repoRoot, args['out-dir'] || process.env.OUT_DIR || 'build/releases');
  const deviceKey = args['device-key'] || process.env.DEVICE_KEY;
  const metadataDeviceKey = args['metadata-device-key'] || deviceKey;

  if (!buildDirArg) {
    throw new Error('Missing --build-dir');
  }
  if (!deviceKey || !devices.has(deviceKey)) {
    throw new Error(`Unknown or missing device key: ${deviceKey || '(none)'}`);
  }

  const buildDir = path.resolve(repoRoot, buildDirArg);
  if (!fs.existsSync(buildDir)) {
    throw new Error(`Build directory not found: ${buildDir}`);
  }

  const version = readVersion().replace(/[^A-Za-z0-9._-]/g, '-');
  const files = walkFiles(buildDir);

  const updatePath = selectSingleFile(
    files,
    (file) => path.basename(file) === `${projectName}.bin`,
    `${deviceKey} OTA`
  );
  const factoryPath = selectSingleFile(
    files,
    (file) => path.basename(file) === `${projectName}.merged.bin`,
    `${deviceKey} factory`
  );

  verifyDeviceDescriptor(updatePath, metadataDeviceKey);
  const updateSize = fs.statSync(updatePath).size;
  if (updateSize > otaSlotSize) {
    throw new Error(
      `Firmware image is ${updateSize} bytes but the OTA slot is only ${otaSlotSize} bytes.`
    );
  }

  ensureCleanDeviceOutputs(outDir, deviceKey);

  const updateDest = path.join(outDir, `hometiles_${version}_${deviceKey}.bin`);
  const factoryDest = path.join(outDir, `hometiles_${version}_${deviceKey}_factory.bin`);

  fs.copyFileSync(updatePath, updateDest);
  fs.copyFileSync(factoryPath, factoryDest);

  console.log(`[release-helper] ${path.relative(repoRoot, updateDest)}`);
  console.log(`[release-helper] ${path.relative(repoRoot, factoryDest)}`);
}

main();

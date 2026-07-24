const fs = require('fs');
const os = require('os');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const projectName = 'HomeTiles.ino';
const releaseDir = path.join(repoRoot, 'build', 'releases');
const deviceSelectPath = path.join(repoRoot, 'src', 'devices', 'device_select.h');
const versionFilePath = path.join(repoRoot, 'version.txt');

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

function readDeviceInfo() {
  const lines = fs.readFileSync(deviceSelectPath, 'utf8').split(/\r?\n/);
  let hasTab5 = false;
  let hasB4 = false;
  let hasTouch7 = false;
  let hasTouch8 = false;
  let hasTouch10 = false;

  for (const line of lines) {
    if (/^\s*#if\b/.test(line)) {
      break;
    }
    if (/^\s*#define\s+DEVICE_M5STACKS_TAB5\b/.test(line)) {
      hasTab5 = true;
    }
    if (/^\s*#define\s+DEVICE_WAVESHARE_4B\b/.test(line)) {
      hasB4 = true;
    }
    if (/^\s*#define\s+DEVICE_WAVESHARE_TOUCH_LCD_7\b/.test(line)) {
      hasTouch7 = true;
    }
    if (/^\s*#define\s+DEVICE_WAVESHARE_TOUCH_LCD_8\b/.test(line)) {
      hasTouch8 = true;
    }
    if (/^\s*#define\s+DEVICE_WAVESHARE_TOUCH_LCD_10_1\b/.test(line)) {
      hasTouch10 = true;
    }
  }

  const selected = [
    hasTab5,
    hasB4,
    hasTouch7,
    hasTouch8,
    hasTouch10,
  ].filter(Boolean).length;
  if (selected > 1) {
    throw new Error('Multiple device targets are enabled in src/devices/device_select.h');
  }

  if (hasTab5) return { key: 'm5stacks_tab5', slug: 'm5stacks-tab5' };
  if (hasTouch7) return { key: 'waveshare_touch_lcd_7', slug: 'waveshare-touch-lcd-7' };
  if (hasTouch8) return { key: 'waveshare_touch_lcd_8', slug: 'waveshare-touch-lcd-8' };
  if (hasTouch10) return { key: 'waveshare_touch_lcd_10_1', slug: 'waveshare-touch-lcd-10-1' };
  if (hasB4) return { key: 'waveshare_4b', slug: 'waveshare-b4' };
  return { key: 'waveshare_4b', slug: 'waveshare-b4' };
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
  const device = readDeviceInfo();
  const sketchesPath = getArduinoSketchesPath();
  const build = findLatestSuccessfulBuild(sketchesPath);

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

main();

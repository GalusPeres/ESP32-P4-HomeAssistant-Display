import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const read = (relativePath) =>
  fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');

const device = read(
  'src/devices/guition_esp32_4848s040/device_guition_esp32_4848s040.cpp');
const loop = read('HomeTiles.ino');

const displayStart = device.indexOf('class GuitionAtomicRgbDisplay final');
const beginStart = device.indexOf('bool begin(', displayStart);
assert.ok(displayStart >= 0 && beginStart > displayStart,
          'Guition RGB display subclass was not found');
const displayOverrides = device.slice(displayStart, beginStart);
assert.doesNotMatch(
  displayOverrides,
  /draw16bitRGBBitmap\(/,
  'Guition must retain the proven Arduino_GFX rotation and cache path');

const mqttStart = loop.indexOf('mqttServicePostConnect();');
const dynamicReload = loop.indexOf('mqttServiceDynamicSlotsReload();', mqttStart);
assert.ok(mqttStart >= 0 && dynamicReload > mqttStart,
          'Active MQTT drain section was not found');
const mqttDrain = loop.slice(mqttStart, dynamicReload);
assert.match(mqttDrain,
             /#if defined\(DEVICE_ESP32_S3_RGB_480\)[\s\S]*mqtt_process_inbound_queue\(camera_popup_is_busy\(\) \? 4 : 8\);/,
             'S3 MQTT work must be bounded between UI cycles');
assert.match(mqttDrain,
             /#else[\s\S]*mqtt_process_inbound_queue\(camera_popup_is_busy\(\) \? 4 : 0\);/,
             'Non-S3 targets must retain the established drain behavior');

console.log('ESP32-S3 render backpressure contract: PASS');

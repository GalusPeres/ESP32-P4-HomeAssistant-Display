import assert from 'node:assert/strict';
import fs from 'node:fs';

function read(relativePath) {
  return fs.readFileSync(new URL(`../${relativePath}`, import.meta.url), 'utf8');
}

const shared = read('src/tiles/tile_renderer_shared.h');
const weather = read('src/types/weather/renderer.cpp');
const settings = read('src/ui/tab_settings.cpp');
const deviceSelect = read('src/devices/device_select.h');

const helperStart = shared.indexOf(
  'static inline void finish_press_before_popup(lv_event_t* event)');
const helperEnd = shared.indexOf(
  '// Weather and other large preloaded popups', helperStart);
assert.ok(helperStart >= 0 && helperEnd > helperStart,
          'Shared popup press helper must exist');
const helper = shared.slice(helperStart, helperEnd);
const s3Start = helper.indexOf('#if defined(DEVICE_ESP32_S3_RGB_480)');
const fallbackStart = helper.indexOf('#else', s3Start);
const branchEnd = helper.indexOf('#endif', fallbackStart);
assert.ok(s3Start >= 0 && fallbackStart > s3Start && branchEnd > fallbackStart,
          'Shared popup helper must have an S3-specific refresh branch');
const s3Branch = helper.slice(s3Start, fallbackStart);
const fallbackBranch = helper.slice(fallbackStart, branchEnd);
assert.doesNotMatch(s3Branch, /lv_refr_now\s*\(/,
                    'S3 popup path must not force a nested refresh');
assert.match(fallbackBranch, /lv_refr_now\s*\(/,
             'Non-S3 popup behavior must retain the immediate refresh');

const callbackStart = weather.indexOf('auto show_popup = [](lv_event_t* e)');
const callbackEnd = weather.indexOf(
  'lv_obj_add_event_cb(card, show_popup, LV_EVENT_ALL, data);', callbackStart);
assert.ok(callbackStart >= 0 && callbackEnd > callbackStart,
          'Weather popup callback must exist');
const callback = weather.slice(callbackStart, callbackEnd);
const weatherS3Start = callback.indexOf(
  '#if defined(DEVICE_ESP32_S3_RGB_480)');
const weatherFallback = callback.indexOf('#else', weatherS3Start);
const weatherBranchEnd = callback.indexOf('#endif', weatherFallback);
assert.ok(weatherS3Start >= 0 && weatherFallback > weatherS3Start &&
          weatherBranchEnd > weatherFallback,
          'Weather popup must have an S3-specific open path');
const weatherS3Branch = callback.slice(weatherS3Start, weatherFallback);
const weatherFallbackBranch = callback.slice(weatherFallback, weatherBranchEnd);
assert.match(weatherS3Branch, /open_current_weather_popup\(e, init\)/,
             'S3 weather popup must show its prepared shell immediately');
assert.doesNotMatch(weatherS3Branch, /defer_popup_until_source_refreshed/,
                    'S3 weather popup must not wait for another display frame');
assert.match(weatherFallbackBranch,
             /weather_popup_has_current_cached_payload/,
             'Non-S3 cached weather behavior must remain intact');
assert.match(weatherFallbackBranch, /defer_popup_until_source_refreshed/,
             'Non-S3 cold-cache behavior must remain intact');

const settingsOpenStart = settings.indexOf('static void open_settings_popup(');
const settingsClickStart = settings.indexOf(
  'static void on_settings_tile_clicked(', settingsOpenStart);
assert.ok(settingsOpenStart >= 0 && settingsClickStart > settingsOpenStart,
          'Settings popup open function must exist');
const settingsOpen = settings.slice(settingsOpenStart, settingsClickStart);
assert.match(settingsOpen,
             /#if !defined\(DEVICE_ESP32_S3_RGB_480\)[\s\S]*lv_refr_now\s*\(/,
             'Settings nested refresh must be disabled on S3 RGB');

for (const marker of [
  'defined(DEVICE_GUITION_ESP32_4848S040) ||',
  'defined(DEVICE_WAVESHARE_S3_TOUCH_LCD_4B)',
  '#define DEVICE_ESP32_S3_RGB_480',
]) {
  assert.ok(deviceSelect.includes(marker),
            `S3 RGB family mapping is missing: ${marker}`);
}

for (const renderer of [
  'src/types/camera/renderer.cpp',
  'src/types/climate/renderer.cpp',
  'src/types/cover/renderer.cpp',
  'src/types/energy/renderer.cpp',
  'src/types/media/renderer.cpp',
  'src/types/sensor/renderer.cpp',
  'src/types/switch/renderer.cpp',
  'src/ui/tab_settings.cpp',
]) {
  assert.match(read(renderer), /finish_press_before_popup\s*\(/,
               `${renderer} must use the shared popup refresh path`);
}

console.log('ESP32-S3 popup refresh contract OK');

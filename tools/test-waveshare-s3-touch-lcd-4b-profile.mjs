import assert from "node:assert/strict";
import fs from "node:fs";

const read = (path) => fs.readFileSync(path, "utf8");
const driver = read("src/devices/waveshare_s3_touch_lcd_4b/device_waveshare_s3_touch_lcd_4b.cpp");
const header = read("src/devices/waveshare_s3_touch_lcd_4b/device_waveshare_s3_touch_lcd_4b.h");
const selector = read("src/devices/device_select.h");
const active = read("src/devices/active_device.h");
const profiles = read("sketch.yaml");
const metadata = read("src/core/firmware_metadata.cpp");
const packager = read("release-helper/package-ci-build.js");

for (const marker of [
  "constexpr uint8_t kExpanderAddress = 0x20;",
  "constexpr int8_t kTouchSda = 47;",
  "constexpr int8_t kTouchScl = 48;",
  "constexpr int8_t kBacklightPin = 4;",
  "digitalWrite(kBacklightPin, HIGH);",
  "const uint32_t duty = kBacklightMaxDuty - visible_duty;",
  "st7701_type1_init_operations",
  "Arduino_XCA9554SWSPI",
  "kExpanderTouchReset",
  "kExpanderTouchInterrupt",
]) {
  assert.ok(driver.includes(marker), `Waveshare S3 driver marker missing: ${marker}`);
}

for (const pin of [
  "kPanelDe = 17", "kPanelVsync = 3", "kPanelHsync = 46", "kPanelPclk = 9",
  "kPanelR0 = 10", "kPanelG0 = 21", "kPanelB0 = 40",
]) {
  assert.ok(driver.includes(pin), `Waveshare S3 RGB assignment missing: ${pin}`);
}

assert.match(header, /"waveshare_s3_touch_lcd_4b"/);
assert.match(header, /480,\s*\n\s*480,/);
assert.match(selector, /DEVICE_WAVESHARE_S3_TOUCH_LCD_4B/);
assert.match(active, /DeviceWaveshareS3TouchLCD4B/);
assert.match(profiles, /\n  waveshare_s3_touch_lcd_4b:\n/);
assert.match(metadata, /DEVICE_GUITION_ESP32_4848S040\)[\s\S]*?FW_META_TARGET_DISPLAY_NAME "GUITION ESP32-4848S040"/);
assert.match(metadata, /DEVICE_WAVESHARE_S3_TOUCH_LCD_4B\)[\s\S]*?FW_META_TARGET_DISPLAY_NAME "Waveshare ESP32-S3-Touch-LCD-4B"/);
assert.match(packager, /\['waveshare_s3_touch_lcd_4b', \{ key: 'waveshare_s3_touch_lcd_4b' \}\]/);
assert.match(driver, /bool DeviceWaveshareS3TouchLCD4B::initSDCard\(\) \{[\s\S]*?return false;[\s\S]*?#if 0/);

console.log("Waveshare ESP32-S3-Touch-LCD-4B profile contract: PASS");

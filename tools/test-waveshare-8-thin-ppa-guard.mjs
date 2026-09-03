import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const source = fs.readFileSync(
  path.join(
    repoRoot,
    'src/devices/waveshare_touch_lcd_8/device_waveshare_touch_lcd_8.cpp',
  ),
  'utf8',
);

const widthMatch = source.match(/constexpr int32_t kPpaMinRotateWidth = (\d+);/);
const heightMatch = source.match(/constexpr int32_t kPpaMinRotateHeight = (\d+);/);
assert.ok(widthMatch, 'Waveshare 8 PPA minimum width is missing');
assert.ok(heightMatch, 'Waveshare 8 PPA minimum height is missing');

const minWidth = Number(widthMatch[1]);
const minHeight = Number(heightMatch[1]);
assert.equal(minHeight, 8, 'thin-strip guard must match the proven Tab5 limit');

const drawStart = source.indexOf('bool draw_landscape_area(');
const drawEnd = source.indexOf('\nbool init_backlight()', drawStart);
assert.ok(drawStart >= 0 && drawEnd > drawStart, 'draw_landscape_area body is missing');
const drawBody = source.slice(drawStart, drawEnd);
assert.match(
  drawBody,
  /w >= kPpaMinRotateWidth\s*&&\s*h >= kPpaMinRotateHeight/,
  'Waveshare 8 must check width and height before submitting LVGL work to PPA',
);

const ppaEligible = (width, height) =>
  width >= minWidth && height >= minHeight;
assert.equal(
  ppaEligible(752, 3),
  false,
  'the reproduced 752x3 activity scroll strip must use CPU rotation',
);
assert.equal(
  ppaEligible(752, 8),
  true,
  'normal wide redraw bands must remain PPA accelerated',
);

console.log('Waveshare 8 thin PPA guard: PASS');

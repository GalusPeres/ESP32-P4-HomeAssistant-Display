import fs from 'node:fs';

const renderer = fs.readFileSync(
  new URL('../src/types/weather/renderer.cpp', import.meta.url),
  'utf8');

const callbackStart = renderer.indexOf('auto show_popup = [](lv_event_t* e)');
const callbackEnd = renderer.indexOf(
  'lv_obj_add_event_cb(card, show_popup, LV_EVENT_ALL, data);',
  callbackStart);
if (callbackStart < 0 || callbackEnd < 0) {
  throw new Error('Weather popup event callback or all-event registration is missing');
}

const callback = renderer.slice(callbackStart, callbackEnd);
const requiredMarkers = [
  'if (code == LV_EVENT_PRESSED)',
  'data->popup_opened_for_press = false;',
  'if (code == LV_EVENT_PRESS_LOST)',
  'if (code == LV_EVENT_LONG_PRESSED)',
  'data->long_press_pending = true;',
  'if (code == LV_EVENT_RELEASED)',
  'should_open = data->long_press_pending;',
  'data->long_press_pending = false;',
  'lv_indev_get_scroll_obj(indev)',
  '} else if (code == LV_EVENT_SHORT_CLICKED)',
  'should_open = true;',
  'if (!should_open || data->popup_opened_for_press) return;',
  'data->popup_opened_for_press = true;',
  'WeatherPopupInit init;',
];

let markerPosition = -1;
for (const marker of requiredMarkers) {
  markerPosition = callback.indexOf(marker, markerPosition + 1);
  if (markerPosition < 0) {
    throw new Error(`Weather popup release contract is missing or reordered: ${marker}`);
  }
}

const longPressBranch = callback.slice(
  callback.indexOf('if (code == LV_EVENT_LONG_PRESSED)'),
  callback.indexOf('bool should_open = false;'));
if (/show_weather_popup|open_current_weather_popup|defer_popup/.test(longPressBranch)) {
  throw new Error('Weather popup still opens before a long press is released');
}

if (!renderer.includes('bool long_press_pending = false;') ||
    !renderer.includes('bool popup_opened_for_press = false;')) {
  throw new Error('Weather popup per-press state is missing');
}

function send(state, code, scrolling = false) {
  let opened = false;
  if (code === 'pressed') {
    state.longPressPending = false;
    state.openedForPress = false;
  } else if (code === 'pressLost') {
    state.longPressPending = false;
  } else if (code === 'longPressed') {
    state.longPressPending = true;
  } else {
    let shouldOpen = false;
    if (code === 'released') {
      shouldOpen = state.longPressPending;
      state.longPressPending = false;
      if (shouldOpen && scrolling) return false;
    } else if (code === 'shortClicked') {
      shouldOpen = true;
    }
    if (shouldOpen && !state.openedForPress) {
      state.openedForPress = true;
      opened = true;
    }
  }
  return opened;
}

const shortState = {};
if (send(shortState, 'pressed') || send(shortState, 'released') ||
    !send(shortState, 'shortClicked')) {
  throw new Error('Short press does not open exactly once after release');
}

const longState = {};
if (send(longState, 'pressed') || send(longState, 'longPressed') ||
    !send(longState, 'released') || send(longState, 'released')) {
  throw new Error('Long press does not open exactly once on release');
}

const scrolledState = {};
send(scrolledState, 'pressed');
send(scrolledState, 'longPressed');
if (send(scrolledState, 'released', true)) {
  throw new Error('A scrolled long press still opens the Weather popup');
}

const lostState = {};
send(lostState, 'pressed');
send(lostState, 'longPressed');
send(lostState, 'pressLost');
if (send(lostState, 'released')) {
  throw new Error('A lost long press still opens the Weather popup');
}

console.log('Weather popup release interaction tests passed.');

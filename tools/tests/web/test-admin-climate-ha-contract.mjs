import fs from 'node:fs';
import vm from 'node:vm';

const noop = () => {};
const classList = {
  add: noop,
  remove: noop,
  toggle: noop,
  contains: () => false
};

class TestElement {
  constructor(value = '') {
    this.value = value;
    this.dataset = {};
    this.listeners = {};
    this.attributes = {};
    this.options = [];
    this.selectedOptions = [];
    this.classList = classList;
    this.style = { removeProperty: noop, setProperty: noop };
    this.className = '';
    this.innerHTML = '';
  }

  setAttribute(name, value) {
    this.attributes[name] = String(value);
  }

  removeAttribute(name) {
    delete this.attributes[name];
  }

  getAttribute(name) {
    return Object.prototype.hasOwnProperty.call(this.attributes, name)
      ? this.attributes[name]
      : null;
  }

  addEventListener(name, handler) {
    (this.listeners[name] ??= []).push(handler);
  }

  removeEventListener(name, handler) {
    this.listeners[name] = (this.listeners[name] || [])
      .filter(candidate => candidate !== handler);
  }

  appendChild(option) {
    this.options.push(option);
  }

  querySelector() {
    return null;
  }
}

const elements = {};
const storage = {
  getItem: () => null,
  setItem: noop,
  removeItem: noop,
  key: () => null,
  length: 0
};
const document = {
  documentElement: { lang: 'en', dataset: {}, addEventListener: noop },
  addEventListener: noop,
  getElementById: id => elements[id] || null,
  querySelector: () => null,
  querySelectorAll: () => [],
  body: { classList, dataset: {} },
  createElement: () => new TestElement()
};

const sandbox = {
  console,
  document,
  localStorage: storage,
  sessionStorage: storage,
  setTimeout: () => 0,
  clearTimeout: noop,
  setInterval: () => 0,
  clearInterval: noop,
  requestAnimationFrame: noop,
  addEventListener: noop,
  fetch: () => Promise.reject(new Error('unexpected fetch')),
  FormData,
  URLSearchParams,
  Intl,
  Date,
  Math,
  JSON,
  Number,
  String,
  Object,
  Array,
  Map,
  Set,
  Promise,
  parseInt,
  parseFloat,
  isNaN,
  isFinite,
  APP_I18N: {},
  CLIMATE_I18N: {
    automatic: 'Automatic',
    empty: 'Empty',
    emptyRemove: 'Remove',
    emptyField: 'Empty field',
    selectedField: 'Selected field',
    horizontal: 'Horizontal',
    vertical: 'Vertical',
    currentTemperature: 'Current',
    currentHumidity: 'Current humidity',
    targetTemperature: 'Target temperature',
    targetHumidity: 'Target humidity',
    mode: 'Mode',
    off: 'Off',
    heat: 'Heat',
    cool: 'Cool',
    heatCool: 'Heat/Cool',
    autoMode: 'Auto',
    dry: 'Dry',
    fanOnly: 'Fan only',
    genericClimate: 'Climate',
    unavailable: 'Unavailable',
    unknown: 'Unknown'
  },
  GRID_COLS: 6,
  GRID_ROWS: 4,
  TILES_PER_GRID: 24,
  ADMIN_WEB_SESSION_TOKEN: 'test',
  TILE_TYPE_REGISTRY: {
    17: {
      fields: 'climate',
      preview: 'climate',
      load: 'loadClimateFields',
      save: 'saveClimateFields',
      reset: 'resetClimateFields'
    }
  },
  TILE_TABS: [],
  TAB_BY_FOLDER: {},
  FOLDER_BY_TAB: { folder1: 1 },
  SCREENSAVER_FOLDER_ID: 65535,
  SCREENSAVER_TILE_DEFAULT_OPACITY: 0,
  SCREENSAVER_TILE_DEFAULT_COLOR: '#000000',
  MEDIA_TILE_TYPE: 15,
  MEDIA_TILE_MIN_SPAN: 1,
  MEDIA_TILE_MAX_SPAN: 6,
  APP_LOCALE: 'en',
  navigator: {},
  location: {},
  confirm: () => true
};
sandbox.window = sandbox;

vm.createContext(sandbox);
vm.runInContext(
  fs.readFileSync(new URL('../../../src/web/assets/admin.js', import.meta.url), 'utf8'),
  sandbox,
  { filename: 'admin.js' }
);

const parsed = vm.runInContext(`(() => {
  const unavailable = parseClimatePreviewPayload(JSON.stringify({
    state: 'unavailable',
    available: false,
    supported_features: 5,
    attributes: { temperature: 22, humidity: 55 }
  }));
  const unknown = parseClimatePreviewPayload('unknown');
  return {
    unavailable,
    unavailableText: climateModeText(unavailable),
    unavailableColor: climatePreviewColor(unavailable),
    unknownText: climateModeText(unknown),
    unavailableHtml: climatePreviewSlots(unavailable, 1, 1),
    supportedTemperature: climatePreviewSlots(
      { ...unavailable, available: true, mode: 'heat',
        hasSupportedFeatures: true, supportedFeatures: 1 },
      2, 1, [4, 1, 1, 1, 1, 1]),
    unsupportedTemperature: climatePreviewSlots(
      { ...unavailable, available: true, mode: 'heat',
        hasSupportedFeatures: true, supportedFeatures: 4 },
      2, 1, [4, 1, 1, 1, 1, 1]),
    supportedHumidity: climatePreviewSlots(
      { ...unavailable, available: true, mode: 'heat',
        hasSupportedFeatures: true, supportedFeatures: 4 },
      2, 1, [7, 1, 1, 1, 1, 1])
  };
})()`, sandbox);

if (parsed.unavailable.available !== false ||
    parsed.unavailable.hasSupportedFeatures !== true ||
    parsed.unavailable.supportedFeatures !== 5) {
  throw new Error('Climate preview lost HA availability/feature metadata');
}
if (parsed.unavailableText !== 'Unavailable' ||
    parsed.unknownText !== 'Unknown' ||
    parsed.unavailableColor !== '#9e9e9e') {
  throw new Error('Climate preview state localization/color contract failed');
}
if (!parsed.unavailableHtml.includes('Unavailable') ||
    parsed.unavailableHtml.includes('climate-minus') ||
    parsed.unavailableHtml.includes('climate-plus')) {
  throw new Error('Unavailable Climate preview still exposes target actions');
}
if (!parsed.supportedTemperature.includes('climate-minus') ||
    !parsed.supportedTemperature.includes('climate-plus')) {
  throw new Error('Supported target-temperature controls disappeared');
}
if (parsed.unsupportedTemperature.includes('climate-minus') ||
    parsed.unsupportedTemperature.includes('climate-plus')) {
  throw new Error('Unsupported target-temperature controls remain visible');
}
if (!parsed.supportedHumidity.includes('climate-minus') ||
    !parsed.supportedHumidity.includes('climate-plus')) {
  throw new Error('Supported target-humidity controls disappeared');
}

for (const [id, value] of Object.entries({
  folder1_tile_title: 'Climate',
  folder1_tile_icon: '',
  folder1_tile_color: '#2A2A2A',
  folder1_tile_col: '0',
  folder1_tile_row: '0',
  folder1_tile_span_w: '1',
  folder1_tile_span_h: '1',
  folder1_tile_type: '17',
  folder1_climate_entity: 'climate.test',
  folder1_climate_geometry: ''
})) {
  elements[id] = new TestElement(value);
}
for (let index = 0; index < 6; ++index) {
  elements[`folder1_climate_slot_${index}`] = new TestElement('0');
  elements[`folder1_climate_layout_${index}`] = new TestElement('0');
}
elements['folder1-tile-3'] = new TestElement();
elements['folder1-tile-3'].dataset.type = '17';

vm.runInContext(`
  currentTileIndex = 3;
  currentTileTab = 'folder1';
  tilesData.folder1 = [];
  sensorMetaCache.values['climate.test'] = JSON.stringify({
    state: 'unavailable', available: false, supported_features: 1
  });
  updateTilePreview('folder1');
`, sandbox);

const livePreview = elements['folder1-tile-3'].innerHTML;
if (!livePreview.includes('Unavailable') ||
    !livePreview.includes('style="color:#9e9e9e"') ||
    livePreview.includes('climate-minus') ||
    livePreview.includes('climate-plus')) {
  throw new Error('Real Climate tile preview did not apply unavailable state');
}

const webScripts = fs.readFileSync(
  new URL('../../../src/types/climate/web_scripts.cpp', import.meta.url), 'utf8');
for (const key of ['unavailable', 'unknown']) {
  if (!new RegExp(`append_i18n\\(\\s*"${key}"`).test(webScripts)) {
    throw new Error(`Climate WebUI translation is missing: ${key}`);
  }
}

console.log('Climate Home Assistant WebUI contract tests passed.');

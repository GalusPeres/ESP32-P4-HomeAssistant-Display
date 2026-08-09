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
    this.inline = {};
    this.options = [];
    this.selectedOptions = [];
    this.classList = classList;
    this.style = {
      removeProperty: noop,
      setProperty: noop
    };
    this.className = '';
    this.innerHTML = '';
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

  dispatch(name) {
    // Inline HTML handlers run before listeners registered later through
    // addEventListener. A listener removed by that inline handler must not be
    // invoked for the same event; this models the real tile-type select.
    const handlers = [...(this.listeners[name] || [])];
    this.inline[name]?.({ target: this });
    for (const handler of handlers) {
      if (!(this.listeners[name] || []).includes(handler)) continue;
      handler({ target: this });
    }
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
const documentElement = { lang: 'en', dataset: {}, addEventListener: noop };
const document = {
  documentElement,
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
  COVER_I18N: {
    open: 'Offen',
    opening: '\u00d6ffnet',
    closed: 'Geschlossen',
    closing: 'Schlie\u00dft',
    unavailable: 'Nicht verf\u00fcgbar',
    unknown: 'Unbekannt'
  },
  GRID_COLS: 6,
  GRID_ROWS: 4,
  TILES_PER_GRID: 24,
  ADMIN_WEB_SESSION_TOKEN: 'test',
  TILE_TYPE_REGISTRY: {
    19: {
      fields: 'cover',
      preview: 'cover',
      load: 'loadCoverFields',
      save: 'saveCoverFields',
      reset: 'resetCoverFields'
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
  fs.readFileSync(new URL('../src/web/assets/admin.js', import.meta.url), 'utf8'),
  sandbox,
  { filename: 'admin.js' }
);

for (const [id, value] of Object.entries({
  folder1_tile_title: '',
  folder1_tile_icon: '',
  folder1_tile_color: '#2A2A2A',
  folder1_tile_col: '5',
  folder1_tile_row: '1',
  folder1_tile_span_w: '1',
  folder1_tile_span_h: '1',
  folder1_tile_type: '19',
  folder1_cover_entity: '',
  folder1_cover_popup_open_mode: '0'
})) {
  elements[id] = new TestElement(value);
}
elements['folder1-tile-3'] = new TestElement();
elements['folder1-tile-3'].dataset.type = '0';

const placeholder = { value: '', textContent: 'No selection' };
const coverOption = {
  value: 'cover.test',
  textContent: 'Test Cover - cover.test'
};
const coverSelect = elements.folder1_cover_entity;
coverSelect.options = [placeholder, coverOption];
coverSelect.selectedOptions = [placeholder];

vm.runInContext(`
  currentTileIndex = 3;
  currentTileTab = 'folder1';
  tilesData.folder1 = [];
  let __preview = 0;
  let __draft = 0;
  let __save = 0;
  const __realUpdateTilePreview = updateTilePreview;
  const __realUpdateDraft = updateDraft;
  updateTilePreview = () => { __preview += 1; };
  updateDraft = tab => {
    __draft += 1;
    __realUpdateDraft(tab);
  };
  scheduleAutoSave = () => { __save += 1; };
  maybeFillTitleFromEntity('folder1', '_cover_entity');
  setupLivePreview('folder1');
  setupLivePreview('folder1');
`, sandbox);

if (elements.folder1_tile_title.value !== '') {
  throw new Error('placeholder became the Cover title');
}

elements.folder1_tile_type.inline.change = () => {
  vm.runInContext("updateTileType('folder1')", sandbox);
};
elements.folder1_tile_type.dispatch('change');
coverSelect.value = coverOption.value;
coverSelect.selectedOptions = [coverOption];
elements.folder1_cover_popup_open_mode.value = '0';
coverSelect.dispatch('change');
elements.folder1_tile_title.value = 'Manual title';
elements.folder1_tile_title.dispatch('input');

const result = vm.runInContext(`(() => {
  const form = new FormData();
  saveCoverFields('folder1', form);
  return {
    preview: __preview,
    draft: __draft,
    save: __save,
    title: document.getElementById('folder1_tile_title').value,
    entity: form.get('cover_entity'),
    popup: form.get('popup_open_mode'),
    draftEntity: getTileSnapshotForSave('folder1', 3)?.cover_entity,
    draftTitle: getTileSnapshotForSave('folder1', 3)?.title
  };
})()`, sandbox);

const expected = {
  preview: 3,
  draft: 3,
  save: 3,
  title: 'Manual title',
  entity: 'cover.test',
  popup: '0',
  draftEntity: 'cover.test',
  draftTitle: 'Manual title'
};
if (JSON.stringify(result) !== JSON.stringify(expected)) {
  throw new Error(`Cover editor contract failed: ${JSON.stringify(result)}`);
}

vm.runInContext(`
  updateTilePreview = __realUpdateTilePreview;
  tilesData.folder1 = [];
  sensorMetaCache.values['cover.test'] = JSON.stringify({
    state: 'open',
    available: true,
    current_position: 40,
    device_class: 'shutter'
  });
  updateTilePreview('folder1');
`, sandbox);
const previewHtml = elements['folder1-tile-3'].innerHTML;
if (!previewHtml.includes('tile-value tile-cover-value') ||
    !previewHtml.includes('Offen<br>40%') ||
    !previewHtml.includes('style="color:#926bc7"') ||
    previewHtml.includes('tile-cover-state')) {
  throw new Error('Cover preview did not render through the real WebUI path');
}

const localizedCoverStates = vm.runInContext(`[
  coverPreviewStateText({ state: 'open', available: true }),
  coverPreviewStateText({ state: 'opening', available: true }),
  coverPreviewStateText({ state: 'closed', available: true }),
  coverPreviewStateText({ state: 'closing', available: true }),
  coverPreviewStateText({ state: 'unavailable', available: false }),
  coverPreviewStateText({ state: 'unknown', available: true })
]`, sandbox);
const expectedCoverStates = [
  'Offen', '\u00d6ffnet', 'Geschlossen', 'Schlie\u00dft',
  'Nicht verf\u00fcgbar', 'Unbekannt'
];
if (JSON.stringify(localizedCoverStates) !== JSON.stringify(expectedCoverStates)) {
  throw new Error(
    `Cover state localization failed: ${JSON.stringify(localizedCoverStates)}`);
}

const adminCss = fs.readFileSync(
  new URL('../src/web/assets/admin.css', import.meta.url), 'utf8');
if (!/\.tile\.sensor,\s*\.tile\.cover\s*\{/.test(adminCss) ||
    !/\.tile\.sensor \.tile-title,\s*\.tile\.cover \.tile-title\s*\{/.test(adminCss) ||
    !/\.tile\.sensor \.tile-icon,\s*\.tile\.cover \.tile-icon\s*\{/.test(adminCss)) {
  throw new Error('Cover preview no longer shares the Sensor tile layout');
}

const coverRenderer = fs.readFileSync(
  new URL('../src/types/cover/renderer.cpp', import.meta.url), 'utf8');
for (const marker of [
  'tile_layout::scale_480(-8)',
  'tile_layout::header_title_font()',
  'lv_obj_set_width(widget.title_label, LV_PCT(70))',
  'lv_obj_set_style_text_line_space(widget.value_label, 8, 0)',
  'tile_layout::scale(28)'
]) {
  if (!coverRenderer.includes(marker)) {
    throw new Error(`Cover renderer lost Sensor layout marker: ${marker}`);
  }
}
if (!/set_label_style\(widget\.value_label,\s*lv_color_white\(\),\s*tile_layout::header_title_font\(\)\)/s.test(coverRenderer)) {
  throw new Error('Cover value no longer uses the smaller title font');
}

const coverWebScriptSource = fs.readFileSync(
  new URL('../src/types/cover/web_scripts.cpp', import.meta.url), 'utf8');
for (const key of ['open', 'opening', 'closed', 'closing', 'unavailable', 'unknown']) {
  const injection = new RegExp(`append_i18n\\(\\s*"${key}"`);
  if (!injection.test(coverWebScriptSource)) {
    throw new Error(`Cover WebUI translation is missing: ${key}`);
  }
}
const i18nSource = fs.readFileSync(
  new URL('../src/core/i18n.cpp', import.meta.url), 'utf8');
if (!i18nSource.includes('return locale(language_code).cover_labels[index]') ||
    !i18nSource.includes('return profile.cover_states[index]')) {
  throw new Error('Cover texts no longer use the central LocaleProfile');
}

const coverPopupSource = fs.readFileSync(
  new URL('../src/ui/cover_popup.cpp', import.meta.url), 'utf8');
if (!/if \(remote_update_blocked\(g_ctx\)\) \{\s*defer_remote_apply\(g_ctx, init\);\s*apply_remote_state_preserving_active_value\(g_ctx, init\);/s
    .test(coverPopupSource)) {
  throw new Error(
    'Cover popup may discard a live state update during the remote-value guard');
}
for (const marker of [
  'lv_timer_create(remote_apply_timer_cb, delay_ms, ctx)',
  'apply_init(ctx, pending)',
  'cancel_deferred_remote_apply(g_ctx)',
  'CoverPopupMode::Position',
  'CoverPopupMode::Controls',
  'create_slider_view(',
  'on_tilt_slider_draw',
  'kTiltStripeTopHeight',
  'kTiltStripeBottomHeight',
  'kTiltStripePitch',
  '"format-list-bulleted"',
  '"swap-vertical"'
]) {
  if (!coverPopupSource.includes(marker)) {
    throw new Error(`Cover popup deferred-update contract is missing: ${marker}`);
  }
}
if (/preset_row|preset_buttons|kPresets/.test(coverPopupSource)) {
  throw new Error('Cover popup restored the removed fixed-position presets');
}
if (coverPopupSource.includes('ctx->state.position > 0') ||
    !coverPopupSource.includes(
      'lv_area_t middle = {area.x1, top_center_y, area.x2, center_y}')) {
  throw new Error(
    'Cover position fill must run from the fixed top cap to the moving edge');
}

const adminSource = fs.readFileSync(
  new URL('../src/web/assets/admin.js', import.meta.url), 'utf8');
if (/\bhumanizeIdentifier\s*\(/.test(adminSource)) {
  throw new Error(
    'Admin WebUI calls the firmware-only humanizeIdentifier helper');
}

console.log('Admin Cover editor contract: PASS');

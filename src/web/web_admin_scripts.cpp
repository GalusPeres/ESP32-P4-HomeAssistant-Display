#include "src/web/web_admin_scripts.h"
#include "src/types/types_registry.h"
#include "src/tiles/tile_config.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/ui/screensaver_config.h"

static void appendJsStringLiteral(String& html, const char* value) {
  html += "'";
  if (value) {
    for (const char* p = value; *p; ++p) {
      switch (*p) {
        case '\\': html += "\\\\"; break;
        case '\'': html += "\\'"; break;
        case '\r': break;
        case '\n': html += "\\n"; break;
        default: html += *p; break;
      }
    }
  }
  html += "'";
}

void appendAdminScripts(String& html) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
  <script>
)html";
  append_tile_type_registry_js(html);
  html += "\n  const APP_I18N = {\n";
  auto appendJsEntry = [&](const char* key, const char* value) {
    html += "    ";
    html += key;
    html += ": ";
    appendJsStringLiteral(html, value);
    html += ",\n";
  };
  appendJsEntry("folderPrefix", tr.folder_prefix);
  appendJsEntry("selectTileFirst", tr.js_select_tile_first);
  appendJsEntry("tileCopied", tr.js_tile_copied);
  appendJsEntry("noCopiedTile", tr.js_no_copied_tile);
  appendJsEntry("tilePasted", tr.js_tile_pasted);
  appendJsEntry("settingsTileFixed", tr.js_settings_tile_fixed);
  appendJsEntry("backTileFixed", tr.js_back_tile_fixed);
  appendJsEntry("tileCannotDelete", tr.js_tile_cannot_delete);
  appendJsEntry("folderCannotDelete", tr.js_folder_cannot_delete);
  appendJsEntry("deleteFolderConfirm", tr.js_delete_folder_confirm);
  appendJsEntry("folderDeleted", tr.js_folder_deleted);
  appendJsEntry("deleteFailed", tr.js_delete_failed);
  appendJsEntry("folderNotFound", tr.js_folder_not_found);
  appendJsEntry("tileSaved", tr.js_tile_saved);
  appendJsEntry("unknownError", tr.js_unknown_error);
  appendJsEntry("networkError", tr.js_network_error);
  appendJsEntry("networkErrorSave", tr.js_network_error_save);
  appendJsEntry("exportCreated", tr.js_export_created);
  appendJsEntry("exportFailed", tr.js_export_failed);
  appendJsEntry("importInvalidJson", tr.js_import_invalid_json);
  appendJsEntry("importFailed", tr.js_import_failed);
  appendJsEntry("importRunning", tr.js_import_running);
  appendJsEntry("importComplete", tr.js_import_complete);
  appendJsEntry("tileDoesNotFit", tr.js_tile_does_not_fit);
  appendJsEntry("noLayoutFound", tr.js_no_layout_found);
  appendJsEntry("tilesMovedSaved", tr.js_tiles_moved_saved);
  appendJsEntry("screensaverSaved", tr.js_screensaver_saved);
  appendJsEntry("screensaverSaveFailed", tr.js_screensaver_save_failed);
  appendJsEntry("screensaverLoadFailed", tr.js_screensaver_load_failed);
  appendJsEntry("screensaverNoWallpapers", tr.screensaver_no_wallpapers);
  appendJsEntry("moveFailed", tr.js_move_failed);
  appendJsEntry("networkErrorMove", tr.js_network_error_move);
  appendJsEntry("screenshotCreating", tr.js_screenshot_creating);
  appendJsEntry("screenshotSaved", tr.js_screenshot_saved);
  appendJsEntry("screenshotFailed", tr.js_screenshot_failed);
  appendJsEntry("otaSelectFile", tr.js_ota_select_file);
  appendJsEntry("otaUploading", tr.js_ota_uploading);
  appendJsEntry("otaInstalling", tr.js_ota_installing);
  appendJsEntry("otaReconnecting", tr.js_ota_reconnecting);
  appendJsEntry("otaSuccess", tr.js_ota_success);
  appendJsEntry("otaFailed", tr.js_ota_failed);
  appendJsEntry("otaChooseFile", tr.ota_choose_file);
  appendJsEntry("otaNoFileSelected", tr.ota_no_file_selected);
  appendJsEntry("otaGithubCheck", tr.system_check_updates_btn);
  appendJsEntry("otaGithubChecking", tr.system_checking);
  appendJsEntry("otaGithubUpToDate", tr.system_up_to_date);
  appendJsEntry("otaGithubAvailable", tr.system_update_available_fmt);
  appendJsEntry("otaGithubInstall", tr.system_install_btn_fmt);
  appendJsEntry("otaGithubCheckFailed", tr.system_check_failed);
  appendJsEntry("otaGithubDownloading", tr.system_downloading);
  html += R"html(  };
  function t(key) {
    return Object.prototype.hasOwnProperty.call(APP_I18N, key) ? APP_I18N[key] : key;
  }
  function tf(key, replacements) {
    let out = t(key);
    if (!replacements) return out;
    Object.keys(replacements).forEach(name => {
      out = out.replaceAll('{' + name + '}', String(replacements[name]));
    });
    return out;
  }
  let APP_LOCALE = document.documentElement.lang || 'en';
  function formatLocalizedNumber(value, decimals = 0, trimTrailingZeros = false) {
    const numeric = Number(String(value ?? '').trim().replace(',', '.'));
    if (!Number.isFinite(numeric)) return '--';
    const digits = Math.max(0, Math.min(6, Number.parseInt(decimals, 10) || 0));
    return new Intl.NumberFormat(APP_LOCALE, {
      useGrouping: false,
      minimumFractionDigits: trimTrailingZeros ? 0 : digits,
      maximumFractionDigits: digits
    }).format(numeric);
  }
  function localizeNumericText(value) {
    const text = String(value ?? '').trim();
    if (!text.length) return text;
    const normalized = text.replace(',', '.');
    if (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)$/.test(normalized)) return text;
    const fraction = normalized.includes('.') ? normalized.split('.')[1].length : 0;
    return formatLocalizedNumber(Number(normalized), fraction, false);
  }

  let normalTileBordersSaveSequence = 0;
  function applyNormalTileBordersPreview(enabled) {
    document.querySelectorAll('.normal-tile-border-toggle').forEach(input => {
      input.checked = !!enabled;
    });
    document.querySelectorAll('.tile-grid:not(.screensaver-tile-grid)').forEach(grid => {
      grid.classList.toggle('tiles-bordered', !!enabled);
    });
  }
  async function saveNormalTileBorders(enabled) {
    const wanted = !!enabled;
    const previous = !wanted;
    const sequence = ++normalTileBordersSaveSequence;
    applyNormalTileBordersPreview(wanted);
    try {
      const response = await fetch('/api/display/tile-borders', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'enabled=' + (wanted ? '1' : '0')
      });
      if (!response.ok) throw new Error('HTTP ' + response.status);
    } catch (error) {
      if (sequence !== normalTileBordersSaveSequence) return;
      applyNormalTileBordersPreview(previous);
      showNotification(t('networkErrorSave'), false);
    }
  }
)html";
  html += R"html(
  function switchTab(tabName) {
    const tabs = document.querySelectorAll('.tab-content');
    tabs.forEach(tab => tab.classList.remove('active'));
    const btns = document.querySelectorAll('.tab-btn');
    btns.forEach(btn => btn.classList.remove('active'));
    const target = document.getElementById(tabName);
    if (target) target.classList.add('active');
    // Find and activate the button that switches to this tab
    const activeBtn = Array.from(btns).find(btn => btn.getAttribute('onclick')?.includes("'" + tabName + "'"));
    if (activeBtn) activeBtn.classList.add('active');
    try { localStorage.setItem('activeAdminTab', tabName); } catch (e) {}
    updateTileSettingsMaxHeight();
    if (tabName.startsWith('tab-tiles-')) {
      const tileTab = tabName.substring('tab-tiles-'.length);
      if (tileTab === 'screensaver') {
        initScreensaverEditor();
      } else {
        const rememberedIndex = getRememberedTileIndex(tileTab);
        selectTile(rememberedIndex === null ? getTopLeftConfiguredTileIndex(tileTab) : rememberedIndex, tileTab);
        window.requestAnimationFrame(restoreCurrentTileSelectionUi);
      }
    }
    if (tabName === 'tab-network') {
      window.setTimeout(() => {
        if (typeof loadFileManager === 'function' && !fileManagerLoaded) loadFileManager();
      }, 0);
    }
  }

  // Deckelt das Tile-Settings-Panel exakt auf den Platz unterhalb von
  // Header/Tabs, damit es intern scrollt statt die Seite zu strecken.
  function updateTileSettingsMaxHeight() {
    document.querySelectorAll('.tile-settings').forEach(panel => {
      panel.style.maxHeight = '';
      if (window.innerWidth <= 1180) return;
      const tab = panel.closest('.tab-content');
      if (!tab || !tab.classList.contains('active')) return;
      const top = panel.getBoundingClientRect().top + window.scrollY;
      // Unterhalb des Panels liegen nur noch Card-Padding und Wrapper-Abstaende.
      // Aus den Styles lesen (nicht ueber scrollHeight messen - der ist bei
      // grossen Fenstern mindestens Viewport-Hoehe und wuerde das Panel
      // faelschlich klein deckeln).
      let below = 24;
      const card = panel.closest('.card');
      if (card) {
        const ccs = getComputedStyle(card);
        below = (parseFloat(ccs.paddingBottom) || 0) + (parseFloat(ccs.borderBottomWidth) || 0);
        const wrapper = card.parentElement;
        if (wrapper) {
          const wcs = getComputedStyle(wrapper);
          below += (parseFloat(wcs.paddingBottom) || 0) + (parseFloat(wcs.marginBottom) || 0);
        }
      }
      const h = window.innerHeight - top - below;
      if (h > 240) panel.style.maxHeight = h + 'px';
    });
  }
  window.addEventListener('resize', updateTileSettingsMaxHeight);

  // Fuellt die statisch gerenderten Uhr-Kacheln (--:-- Platzhalter) mit der
  // aktuellen Zeit und haelt sie aktuell. Vom JS neu gerenderte Uhr-Kacheln
  // bekommen ihre Zeit (inkl. Format) direkt beim Rendern.
  function fillStaticClockPreviews() {
    if (typeof getClockPreviewTime !== 'function') return;
    document.querySelectorAll('.tile-clock-time').forEach(el => {
      if (el.dataset.autoClock === '1' || el.textContent.trim() === '--:--') {
        el.dataset.autoClock = '1';
        el.textContent = getClockPreviewTime(0);
      }
    });
    document.querySelectorAll('.tile-clock-date').forEach(el => {
      if (el.dataset.autoClock === '1' || el.textContent.trim() === '--.--.----') {
        el.dataset.autoClock = '1';
        el.textContent = getClockPreviewDate(0);
      }
    });
    if (screensaverDraft) {
      const time = document.getElementById('screensaverClockTime');
      const date = document.getElementById('screensaverClockDate');
      if (time) time.textContent = getClockPreviewTime(screensaverDraft.time_format);
      if (date) date.textContent = getScreensaverClockPreviewDate(screensaverDraft);
    }
  }

  function toggleStaticIpFields(checkboxId, fieldsId, noteId) {
    const checkbox = document.getElementById(checkboxId);
    const fields = document.getElementById(fieldsId);
    const note = document.getElementById(noteId);
    if (!checkbox || !fields) return;
    const useStatic = checkbox.checked;
    fields.classList.toggle('is-hidden', !useStatic);
    if (note) {
      note.textContent = useStatic
        ? (note.dataset.staticNote || '')
        : (note.dataset.dhcpNote || '');
    }
  }

  function toggleStaticNetworkFields() {
    toggleStaticIpFields(
      'network_use_static', 'network_static_fields',
      'network_ip_mode_note');
  }

  function toggleNetworkSettings() {
    const mode = document.getElementById('network_mode');
    const wifi = document.getElementById('wifi_network_settings');
    if (!mode || !wifi) return;
    const useEthernet = mode.value === 'ethernet';
    wifi.classList.toggle('is-hidden', useEthernet);
  }

  let adminRefreshSerial = 0;

  function adminNodesMatch(current, fresh) {
    return current && fresh &&
      current.nodeType === fresh.nodeType &&
      (current.nodeType !== Node.ELEMENT_NODE ||
       current.tagName === fresh.tagName);
  }

  function syncAdminNode(current, fresh, preserveSettingsValues) {
    if (!adminNodesMatch(current, fresh)) return;
    if (current.nodeType === Node.TEXT_NODE) {
      if (current.nodeValue !== fresh.nodeValue) {
        current.nodeValue = fresh.nodeValue;
      }
      return;
    }
    if (current.nodeType !== Node.ELEMENT_NODE) return;

    const tag = current.tagName;
    // Die laufenden Scripts und Styles bleiben unangetastet. Insbesondere
    // bleiben dadurch alle Event-Handler und Timer der Admin-Seite erhalten.
    if (tag === 'SCRIPT' || tag === 'STYLE' || tag === 'NOSCRIPT') return;

    const insideSettingsForm =
      current.id === 'admin_settings_form' ||
      Boolean(current.closest && current.closest('#admin_settings_form'));
    const preserveControl =
      preserveSettingsValues && insideSettingsForm &&
      (tag === 'INPUT' || tag === 'SELECT' ||
       tag === 'TEXTAREA' || tag === 'OPTION');

    Array.from(current.attributes).forEach(attribute => {
      if (preserveControl &&
          (attribute.name === 'value' || attribute.name === 'checked' ||
           attribute.name === 'selected')) {
        return;
      }
      if (!fresh.hasAttribute(attribute.name)) {
        current.removeAttribute(attribute.name);
      }
    });
    Array.from(fresh.attributes).forEach(attribute => {
      if (preserveControl &&
          (attribute.name === 'value' || attribute.name === 'checked' ||
           attribute.name === 'selected')) {
        return;
      }
      if (current.getAttribute(attribute.name) !== attribute.value) {
        current.setAttribute(attribute.name, attribute.value);
      }
    });

    const currentChildren = Array.from(current.childNodes);
    const freshChildren = Array.from(fresh.childNodes);
    if (currentChildren.length === freshChildren.length) {
      for (let index = 0; index < currentChildren.length; index += 1) {
        if (adminNodesMatch(currentChildren[index], freshChildren[index])) {
          syncAdminNode(
            currentChildren[index], freshChildren[index],
            preserveSettingsValues);
        }
      }
    } else {
      // Dynamisch gefuellte Bereiche wie Dateimanager und Tile-Vorschauen
      // koennen zusaetzliche Kinder besitzen. Dort nur eindeutig per ID
      // zuordenbare direkte Kinder aktualisieren, niemals den Bereich ersetzen.
      const currentById = new Map();
      currentChildren.forEach(child => {
        if (child.nodeType === Node.ELEMENT_NODE && child.id) {
          currentById.set(child.id, child);
        }
      });
      freshChildren.forEach(child => {
        if (child.nodeType !== Node.ELEMENT_NODE || !child.id) return;
        const existing = currentById.get(child.id);
        if (adminNodesMatch(existing, child)) {
          syncAdminNode(existing, child, preserveSettingsValues);
        }
      });
    }

    if (!preserveControl) {
      if (tag === 'INPUT') {
        if (current.type !== 'file') current.value = fresh.value;
        current.checked = fresh.checked;
      } else if (tag === 'SELECT') {
        current.value = fresh.value;
      } else if (tag === 'TEXTAREA') {
        current.value = fresh.value;
      }
    }
  }

  async function refreshAdminPageInPlace(submittedSnapshot) {
    const refreshSerial = ++adminRefreshSerial;
    try {
      const response = await fetch('/?admin_refresh=' + Date.now(), {
        cache: 'no-store'
      });
      if (!response.ok) throw new Error('Refresh HTTP ' + response.status);
      const html = await response.text();
      if (refreshSerial !== adminRefreshSerial) return;

      const freshDocument =
        new DOMParser().parseFromString(html, 'text/html');
      if (!freshDocument.getElementById('admin_settings_form')) {
        throw new Error('Invalid admin page');
      }

      const activeTab =
        document.querySelector('.tab-content.active')?.id || 'tab-network';
      const settingsForm = document.getElementById('admin_settings_form');
      const currentSnapshot = settingsForm
        ? new URLSearchParams(new FormData(settingsForm)).toString()
        : '';
      const preserveSettingsValues =
        currentSnapshot !== submittedSnapshot;
      const formScrollTop = settingsForm ? settingsForm.scrollTop : 0;
      const pageScrollX = window.scrollX;
      const pageScrollY = window.scrollY;

      syncAdminNode(
        document.documentElement, freshDocument.documentElement,
        preserveSettingsValues);
      APP_LOCALE = freshDocument.documentElement.lang || APP_LOCALE;

      toggleStaticNetworkFields();
      toggleNetworkSettings();
      if (document.getElementById(activeTab)) switchTab(activeTab);
      const refreshedForm = document.getElementById('admin_settings_form');
      if (refreshedForm) refreshedForm.scrollTop = formScrollTop;
      window.scrollTo(pageScrollX, pageScrollY);
    } catch (error) {
      // Speichern war bereits erfolgreich. Wenn nur der nachgelagerte
      // Ansichtsabgleich scheitert, bleibt die funktionierende Seite stehen.
      console.warn('Admin refresh failed:', error);
    }
  }

  function initAdminSettingsSave() {
    const form = document.getElementById('admin_settings_form');
    if (!form) return;
    form.addEventListener('submit', async event => {
      event.preventDefault();
      const submitButton =
        document.querySelector('button[form="admin_settings_form"][type="submit"]');
      const originalLabel = submitButton ? submitButton.textContent : '';
      if (submitButton) submitButton.disabled = true;
      try {
        const body = new URLSearchParams(new FormData(form));
        const submittedSnapshot = body.toString();
        body.set('_ajax', '1');
        const response = await fetch('/mqtt', {
          method: 'POST',
          headers: {'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'},
          body
        });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const result = await response.json();
        if (!result.ok) throw new Error('Save rejected');
        if (submitButton) submitButton.textContent = '\u2713 ' + originalLabel;
        refreshAdminPageInPlace(submittedSnapshot);
        setTimeout(() => {
          if (submitButton) submitButton.textContent = originalLabel;
        }, 1800);
      } catch (error) {
        showNotification(t('networkErrorSave'), false);
      } finally {
        if (submitButton) submitButton.disabled = false;
      }
    });
  }

  function togglePasswordVisibility(inputId, buttonEl) {
    const input = document.getElementById(inputId);
    if (!input || !buttonEl) return;
    const showLabel = buttonEl.dataset.labelShow || 'Show';
    const hideLabel = buttonEl.dataset.labelHide || 'Hide';
    const isHidden = input.type === 'password';
    input.type = isHidden ? 'text' : 'password';
    buttonEl.textContent = isHidden ? hideLabel : showLabel;
  }

  function updateOtaFileName(inputEl) {
    const nameEl = document.getElementById('ota_file_name');
    if (!nameEl) return;
    const file = inputEl && inputEl.files && inputEl.files.length ? inputEl.files[0] : null;
    nameEl.textContent = file ? file.name : t('otaNoFileSelected');
  }

  async function createScreenshotAndDownload() {
    showNotification(t('screenshotCreating'));
    try {
      const res = await fetch('/api/screenshot', { method: 'POST' });
      const data = await res.json().catch(() => ({}));
      if (!res.ok || !data.success) {
        throw new Error(data.error || t('screenshotFailed'));
      }
      showNotification(t('screenshotSaved'));
      const link = document.createElement('a');
      link.href = '/api/screenshot/download?ts=' + Date.now();
      link.download = 'ui_screenshot.jpg';
      document.body.appendChild(link);
      link.click();
      link.remove();
    } catch (err) {
      showNotification(err?.message || t('screenshotFailed'), false);
    }
  }

  let fileManagerLoaded = false;
  const fileManagerState = { fs: 'sd', path: '/', selected: null, sdAvailable: null };

  function fileManagerText(de, en) {
    return (document.documentElement.lang || '').toLowerCase().startsWith('de') ? de : en;
  }

  async function downloadCrashLog() {
    try {
      const res = await fetch('/api/crashlog?ts=' + Date.now());
      if (res.status === 404) {
        showNotification(fileManagerText('Kein Absturz aufgezeichnet.', 'No crash recorded.'));
        return;
      }
      if (!res.ok) throw new Error();
      const blob = await res.blob();
      const url = URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = 'crashlog.txt';
      document.body.appendChild(link);
      link.click();
      link.remove();
      URL.revokeObjectURL(url);
    } catch (err) {
      showNotification(fileManagerText('Download fehlgeschlagen.', 'Download failed.'), false);
    }
  }

  async function eraseCoreDump() {
    if (!confirm(fileManagerText('Gespeicherten Core-Dump wirklich l\u00f6schen?', 'Really delete the stored core dump?'))) return;
    try {
      const res = await fetch('/api/coredump/erase', { method: 'POST' });
      const data = await res.json().catch(() => ({}));
      if (!res.ok || !data.success) {
        throw new Error(data.error || fileManagerText('L\u00f6schen fehlgeschlagen.', 'Delete failed.'));
      }
      const actions = document.getElementById('coredump_actions');
      if (actions) actions.style.display = 'none';
      showNotification(fileManagerText('Core-Dump gel\u00f6scht.', 'Core dump deleted.'));
    } catch (err) {
      showNotification(err?.message || fileManagerText('L\u00f6schen fehlgeschlagen.', 'Delete failed.'), false);
    }
  }

  function normalizeFileManagerClientPath(raw) {
    let path = String(raw || '').trim().replaceAll('\\', '/');
    if (!path) path = '/';
    if (!path.startsWith('/')) path = '/' + path;
    while (path.includes('//')) path = path.replaceAll('//', '/');
    while (path.length > 1 && path.endsWith('/')) path = path.slice(0, -1);
    return path;
  }

  function setFileManagerStatus(message, success = null) {
    const el = document.getElementById('file_manager_status');
    if (!el) return;
    el.textContent = message || '';
    el.classList.remove('error', 'success');
    if (success === true) el.classList.add('success');
    if (success === false) el.classList.add('error');
  }

  function setFileManagerStorageState(available, message = null) {
    fileManagerState.sdAvailable = available;
    const badge = document.getElementById('file_manager_sd_state');
    if (badge) {
      badge.classList.remove('ok', 'error', 'checking');
      if (available === true) {
        badge.classList.add('ok');
        badge.textContent = message || fileManagerText('microSD erkannt', 'microSD detected');
      } else if (available === false) {
        badge.classList.add('error');
        badge.textContent = message || fileManagerText('Keine microSD', 'No microSD');
      } else {
        badge.classList.add('checking');
        badge.textContent = message || fileManagerText('Pr\u00fcfe...', 'Checking...');
      }
    }
    document.querySelectorAll('.file-manager-requires-sd').forEach(el => {
      el.disabled = available !== true;
    });
    updateFileManagerSelectionBar();
  }

  function updateFileManagerUploadName(inputEl) {
    const nameEl = document.getElementById('file_manager_upload_name');
    if (!nameEl) return;
    const files = inputEl && inputEl.files ? Array.from(inputEl.files) : [];
    if (!files.length) {
      nameEl.textContent = fileManagerText('Keine Datei ausgew\u00e4hlt', 'No file selected');
    } else if (files.length === 1) {
      nameEl.textContent = files[0].name;
    } else {
      nameEl.textContent = files.length + fileManagerText(
        ' Dateien ausgew\u00e4hlt', ' files selected');
    }
  }

  function formatFileManagerSize(entry) {
    if (!entry || entry.dir) return '-';
    let size = Number(entry.size || 0);
    if (!Number.isFinite(size) || size < 0) size = 0;
    if (size < 1024) return size + ' B';
    if (size < 1024 * 1024) {
      return formatLocalizedNumber(size / 1024, size < 10 * 1024 ? 1 : 0) + ' KB';
    }
    return formatLocalizedNumber(
      size / (1024 * 1024), size < 10 * 1024 * 1024 ? 1 : 0) + ' MB';
  }

  function formatFileManagerModified(entry) {
    const ts = Number(entry && entry.modified ? entry.modified : 0);
    if (!Number.isFinite(ts) || ts <= 0) return '-';
    const date = new Date(ts * 1000);
    if (!Number.isFinite(date.getTime()) || date.getFullYear() < 2020) return '-';
    return date.toLocaleString(undefined, {
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit'
    });
  }

  function fileManagerParentOf(path) {
    const current = normalizeFileManagerClientPath(path || '/');
    if (current === '/') return '/';
    const idx = current.lastIndexOf('/');
    return idx <= 0 ? '/' : current.slice(0, idx);
  }

  function fileManagerUrl(endpoint, params = {}) {
    const query = new URLSearchParams(params);
    return endpoint + '?' + query.toString();
  }

  function updateFileManagerRowSelection() {
    const selectedPath = fileManagerState.selected && fileManagerState.selected.path;
    document.querySelectorAll('#file_manager_entries tr[data-file-path]').forEach(row => {
      row.classList.toggle('file-manager-row-selected', !!selectedPath && row.dataset.filePath === selectedPath);
    });
  }

  function updateFileManagerSelectionBar() {
    const selectionEl = document.getElementById('file_manager_selection');
    const primaryBtn = document.getElementById('file_manager_primary_btn');
    const renameBtn = document.getElementById('file_manager_rename_btn');
    const deleteBtn = document.getElementById('file_manager_delete_btn');
    const selected = fileManagerState.selected;
    const hasSelection = !!(selected && selected.path);
    const isParent = !!(selected && selected.parent);
    const hasStorage = fileManagerState.sdAvailable !== false;

    if (selectionEl) {
      if (!hasSelection) {
        selectionEl.textContent = fileManagerText('Keine Auswahl', 'No selection');
      } else {
        const size = selected.dir ? '' : ' - ' + formatFileManagerSize(selected);
        selectionEl.textContent = fileManagerText('Auswahl: ', 'Selection: ') + (selected.name || selected.path) + size;
      }
    }

    if (primaryBtn) {
      primaryBtn.disabled = !hasSelection || !hasStorage;
      primaryBtn.textContent = selected && !selected.dir ? fileManagerText('Download', 'Download') : fileManagerText('\u00d6ffnen', 'Open');
      primaryBtn.title = primaryBtn.textContent;
    }
    if (renameBtn) renameBtn.disabled = !hasSelection || isParent || !hasStorage;
    if (deleteBtn) deleteBtn.disabled = !hasSelection || isParent || !hasStorage;
    updateFileManagerRowSelection();
  }

  function clearFileManagerSelection() {
    fileManagerState.selected = null;
    updateFileManagerSelectionBar();
  }

  function selectFileManagerEntry(entry) {
    if (!entry) {
      clearFileManagerSelection();
      return;
    }
    fileManagerState.selected = {
      path: normalizeFileManagerClientPath(entry.path || '/'),
      name: entry.name || entry.path || '',
      dir: !!entry.dir,
      size: Number(entry.size || 0),
      modified: Number(entry.modified || 0),
      parent: !!entry.parent
    };
    updateFileManagerSelectionBar();
  }

  function appendFileManagerCell(row, className, text) {
    const cell = document.createElement('td');
    if (className) cell.className = className;
    cell.textContent = text;
    row.appendChild(cell);
    return cell;
  }

  function fileManagerIconClass(entry) {
    if (entry && entry.parent) return 'mdi mdi-arrow-up-bold';
    return entry && entry.dir ? 'mdi mdi-folder-outline' : 'mdi mdi-file-outline';
  }

  function appendFileManagerNameCell(row, entry) {
    const cell = document.createElement('td');
    const nameWrap = document.createElement('div');
    nameWrap.className = 'file-manager-name';
    const icon = document.createElement('i');
    icon.className = 'file-manager-name-icon ' + fileManagerIconClass(entry);
    nameWrap.appendChild(icon);
    const label = entry.dir ? document.createElement('button') : document.createElement('span');
    label.className = entry.dir ? 'file-manager-name-link file-manager-folder-name' : '';
    label.textContent = entry.name || entry.path || '';
    if (entry.dir) {
      label.type = 'button';
      label.addEventListener('click', event => {
        event.stopPropagation();
        loadFileManager(entry.path);
      });
    }
    nameWrap.appendChild(label);
    cell.appendChild(nameWrap);
    row.appendChild(cell);
    return cell;
  }

  function renderFileManagerBreadcrumb(path) {
    const el = document.getElementById('file_manager_breadcrumb');
    if (!el) return;
    el.innerHTML = '';

    const current = normalizeFileManagerClientPath(path || '/');
    const rootBtn = document.createElement('button');
    rootBtn.type = 'button';
    rootBtn.className = 'file-manager-breadcrumb-item';
    rootBtn.innerHTML = '<i class="mdi mdi-home-outline"></i><span>root</span>';
    rootBtn.addEventListener('click', () => loadFileManager('/'));
    el.appendChild(rootBtn);

    if (current === '/') return;

    const parts = current.split('/').filter(Boolean);
    let acc = '';
    parts.forEach((part, index) => {
      acc += '/' + part;
      const sep = document.createElement('span');
      sep.className = 'file-manager-breadcrumb-separator';
      sep.textContent = '/';
      el.appendChild(sep);

      const item = document.createElement('button');
      item.type = 'button';
      item.className = 'file-manager-breadcrumb-item';
      item.textContent = part;
      const target = acc;
      item.disabled = index === parts.length - 1;
      item.addEventListener('click', () => loadFileManager(target));
      el.appendChild(item);
    });
  }

  function renderFileManagerEntries(entries, emptyMessage = null) {
    const tbody = document.getElementById('file_manager_entries');
    if (!tbody) return;
    tbody.innerHTML = '';
    renderFileManagerBreadcrumb(fileManagerState.path || '/');

    const addSelectableRow = (row, entry) => {
      row.dataset.filePath = normalizeFileManagerClientPath(entry.path || '/');
      row.tabIndex = 0;
      row.addEventListener('click', () => selectFileManagerEntry(entry));
      row.addEventListener('dblclick', () => {
        selectFileManagerEntry(entry);
        if (entry.dir) {
          loadFileManager(entry.path);
        } else {
          downloadFileManagerFile(entry.path);
        }
      });
      row.addEventListener('keydown', event => {
        if (event.key !== 'Enter') return;
        event.preventDefault();
        selectFileManagerEntry(entry);
        openSelectedFileManagerEntry();
      });
      row.title = entry.dir
        ? fileManagerText('Ausw\u00e4hlen, doppelklicken zum \u00d6ffnen', 'Select, double-click to open')
        : fileManagerText('Ausw\u00e4hlen, doppelklicken zum Download', 'Select, double-click to download');
    };

    const currentPath = normalizeFileManagerClientPath(fileManagerState.path || '/');
    if (currentPath !== '/') {
      const parentEntry = {
        dir: true,
        path: fileManagerParentOf(currentPath),
        name: '.. ' + fileManagerText('Elternordner', 'Parent Directory'),
        parent: true
      };
      const parentRow = document.createElement('tr');
      parentRow.className = 'file-manager-parent-row';
      addSelectableRow(parentRow, parentEntry);
      appendFileManagerNameCell(parentRow, parentEntry);
      appendFileManagerCell(parentRow, 'file-manager-muted', '-');
      appendFileManagerCell(parentRow, 'file-manager-size-cell', '-');
      tbody.appendChild(parentRow);
    }

    if (!Array.isArray(entries) || entries.length === 0) {
      const row = document.createElement('tr');
      const cell = document.createElement('td');
      cell.colSpan = 3;
      cell.textContent = emptyMessage || fileManagerText('Ordner ist leer.', 'Folder is empty.');
      row.appendChild(cell);
      tbody.appendChild(row);
      updateFileManagerRowSelection();
      return;
    }

    entries.forEach(entry => {
      const row = document.createElement('tr');
      addSelectableRow(row, entry);
      appendFileManagerNameCell(row, entry);

      appendFileManagerCell(row, 'file-manager-muted', formatFileManagerModified(entry));
      appendFileManagerCell(row, 'file-manager-size-cell', formatFileManagerSize(entry));

      tbody.appendChild(row);
    });
    updateFileManagerRowSelection();
  }

  async function loadFileManager(path = null) {
    fileManagerState.fs = 'sd';
    if (path !== null) fileManagerState.path = normalizeFileManagerClientPath(path);
    if (!fileManagerState.path) fileManagerState.path = '/';
    clearFileManagerSelection();
    setFileManagerStorageState(null);
    setFileManagerStatus(fileManagerText('Lade Dateien...', 'Loading files...'));

    try {
      const res = await fetch(fileManagerUrl('/api/files/list', {
        fs: fileManagerState.fs,
        path: fileManagerState.path
      }), { cache: 'no-store' });
      const data = await res.json().catch(() => ({}));
      if (!res.ok || !data.success) {
        const loadError = new Error(data.error || fileManagerText('Dateiliste konnte nicht geladen werden.', 'Could not load file list.'));
        loadError.status = res.status;
        throw loadError;
      }
      fileManagerState.fs = 'sd';
      fileManagerState.path = data.path || fileManagerState.path;
      setFileManagerStorageState(true);
      renderFileManagerEntries(data.entries || []);
      const count = Array.isArray(data.entries) ? data.entries.length : 0;
      setFileManagerStatus(fileManagerText('Bereit. Eintr\u00e4ge: ', 'Ready. Entries: ') + count, true);
      fileManagerLoaded = true;
    } catch (err) {
      const rawMessage = err?.message || fileManagerText('Dateimanager-Fehler.', 'File manager error.');
      const sdMissing = err?.status === 503 || /microsd/i.test(rawMessage);
      const message = sdMissing
        ? fileManagerText('Keine microSD erkannt.', 'No microSD detected.')
        : rawMessage;
      setFileManagerStorageState(sdMissing ? false : true, sdMissing ? null : fileManagerText('microSD erkannt', 'microSD detected'));
      renderFileManagerEntries([], message);
      setFileManagerStatus(message, false);
      if (!sdMissing) showNotification(message, false);
    }
  }

  function changeFileManagerFs() {
    fileManagerState.fs = 'sd';
    fileManagerState.path = '/';
    loadFileManager('/');
  }

  function downloadFileManagerFile(path) {
    window.location.href = fileManagerUrl('/api/files/download', {
      fs: fileManagerState.fs,
      path: path
    });
  }

  function openSelectedFileManagerEntry() {
    const entry = fileManagerState.selected;
    if (!entry || !entry.path) return;
    if (entry.dir) {
      loadFileManager(entry.path);
      return;
    }
    downloadFileManagerFile(entry.path);
  }

  function renameSelectedFileManagerEntry() {
    const entry = fileManagerState.selected;
    if (!entry || !entry.path || entry.parent) return;
    renameFileManagerEntry(entry.path, entry.name);
  }

  function deleteSelectedFileManagerEntry() {
    const entry = fileManagerState.selected;
    if (!entry || !entry.path || entry.parent) return;
    deleteFileManagerEntry(entry.path, entry.name, !!entry.dir);
  }

  async function postFileManagerForm(endpoint, fields) {
    const body = new URLSearchParams(fields);
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok || !data.success) {
      throw new Error(data.error || fileManagerText('Aktion fehlgeschlagen.', 'Action failed.'));
    }
    return data;
  }

  async function createFileManagerFolder() {
    const name = prompt(fileManagerText('Name des neuen Ordners:', 'New folder name:'));
    if (name === null) return;
    const trimmed = String(name || '').trim();
    if (!trimmed) return;
    try {
      await postFileManagerForm('/api/files/mkdir', {
        fs: fileManagerState.fs,
        path: fileManagerState.path || '/',
        name: trimmed
      });
      showNotification(fileManagerText('Ordner erstellt.', 'Folder created.'));
      loadFileManager();
    } catch (err) {
      showNotification(err?.message || fileManagerText('Ordner konnte nicht erstellt werden.', 'Could not create folder.'), false);
    }
  }

  async function renameFileManagerEntry(path, currentName) {
    const name = prompt(fileManagerText('Neuer Name:', 'New name:'), currentName || '');
    if (name === null) return;
    const trimmed = String(name || '').trim();
    if (!trimmed || trimmed === currentName) return;
    try {
      await postFileManagerForm('/api/files/rename', {
        fs: fileManagerState.fs,
        path,
        name: trimmed
      });
      invalidateScreensaverEditor();
      showNotification(fileManagerText('Umbenannt.', 'Renamed.'));
      loadFileManager();
    } catch (err) {
      showNotification(err?.message || fileManagerText('Umbenennen fehlgeschlagen.', 'Rename failed.'), false);
    }
  }

  async function deleteFileManagerEntry(path, name, isDir) {
    const message = isDir
      ? fileManagerText('Ordner wirklich l\u00f6schen? Inhalt wird mit gel\u00f6scht: ', 'Delete folder and all contents: ')
      : fileManagerText('Datei wirklich l\u00f6schen: ', 'Delete file: ');
    if (!confirm(message + (name || path))) return;
    try {
      await postFileManagerForm('/api/files/delete', {
        fs: fileManagerState.fs,
        path
      });
      invalidateScreensaverEditor();
      showNotification(fileManagerText('Gel\u00f6scht.', 'Deleted.'));
      loadFileManager();
    } catch (err) {
      showNotification(err?.message || fileManagerText('L\u00f6schen fehlgeschlagen.', 'Delete failed.'), false);
    }
  }

  // Waehrend eines laufenden Uploads keine parallelen Requests starten
  // (Sensor-Polling): der Server arbeitet eine Verbindung nach der anderen
  // ab, zusaetzliche Verbindungen stauen sich nur auf und belasten den
  // knappen internen Puffer-Pool des Geraets.
  let fileManagerUploadBusy = false;

  // Sequenzielle kleine Teile statt eines grossen POST: das Geraet hat nur
  // wenig internen RAM fuer WLAN-Empfangspuffer. Ein grosser Upload laesst
  // den Browser bis zu 64KB unbestaetigt vorausschicken und hat den
  // SDIO-Empfangspfad reproduzierbar zum Absturz gebracht -- mit 16KB pro
  // Request ist die maximale Menge "in der Luft" hart begrenzt.
  const FILE_MANAGER_UPLOAD_PART_SIZE = 16 * 1024;

  async function uploadFileManagerFile() {
    const input = document.getElementById('file_manager_upload');
    if (!input || !input.files || !input.files.length) {
      showNotification(fileManagerText('Bitte zuerst eine Datei ausw\u00e4hlen.', 'Select a file first.'), false);
      return;
    }
    if (fileManagerUploadBusy) return;
    const files = Array.from(input.files);
    fileManagerUploadBusy = true;
    try {
      for (let fileIndex = 0; fileIndex < files.length; fileIndex++) {
        const file = files[fileIndex];
        let offset = 0;
        let firstPart = true;
        while (offset < file.size || firstPart) {
          const end = Math.min(offset + FILE_MANAGER_UPLOAD_PART_SIZE, file.size);
          const formData = new FormData();
          formData.append('file', new File([file.slice(offset, end)], file.name));
          const pct = file.size ? Math.round((end / file.size) * 100) : 100;
          setFileManagerStatus(
            fileManagerText('Upload ', 'Upload ') + (fileIndex + 1) + '/' + files.length +
            ': ' + file.name + ' - ' + pct + '%');
          const res = await fetch(fileManagerUrl('/api/files/upload', {
            fs: fileManagerState.fs,
            path: fileManagerState.path || '/',
            append: firstPart ? '0' : '1'
          }), { method: 'POST', body: formData });
          const data = await res.json().catch(() => ({}));
          if (!res.ok || !data.success) {
            throw new Error(data.error || fileManagerText('Upload fehlgeschlagen.', 'Upload failed.'));
          }
          offset = end;
          firstPart = false;
        }
      }
      input.value = '';
      updateFileManagerUploadName(input);
      const completeMessage = files.length === 1
        ? fileManagerText('Upload abgeschlossen.', 'Upload complete.')
        : files.length + fileManagerText(' Dateien hochgeladen.', ' files uploaded.');
      setFileManagerStatus(completeMessage, true);
      showNotification(completeMessage);
      invalidateScreensaverEditor();
      loadFileManager();
    } catch (err) {
      const message = err?.message || fileManagerText('Upload fehlgeschlagen.', 'Upload failed.');
      setFileManagerStatus(message, false);
      showNotification(message, false);
    } finally {
      fileManagerUploadBusy = false;
    }
  }

  let githubFirmwareUpdateTag = '';

  function formatGithubFirmwareText(key, tag) {
    return String(t(key) || '').replace('%s', String(tag || ''));
  }

  function setGithubOtaUi(message, phase = 'idle', tone = '') {
    const statusEl = document.getElementById('ota_github_status');
    const progressEl = document.getElementById('ota_github_progress');
    const progressBarEl = document.getElementById('ota_github_progress_bar');
    if (statusEl) {
      statusEl.textContent = message || '';
      statusEl.classList.remove('error', 'success');
      if (tone === 'error' || tone === 'success') statusEl.classList.add(tone);
    }
    if (progressEl) {
      progressEl.classList.toggle('is-hidden', phase === 'idle');
      progressEl.classList.toggle('active', phase === 'busy');
    }
    if (progressBarEl) progressBarEl.style.width = phase === 'done' ? '100%' : '0%';
  }

  function setFirmwareOtaControlsDisabled(disabled) {
    const ids = ['ota_github_btn', 'ota_upload_btn', 'ota_choose_btn', 'ota_file'];
    ids.forEach(id => {
      const element = document.getElementById(id);
      if (element) element.disabled = disabled;
    });
  }

  function waitForGithubFirmwareResult(targetTag) {
    const startedAt = Date.now();
    const poll = async () => {
      try {
        const res = await fetch('/api/ota/github/status?ts=' + Date.now(), {
          method: 'GET',
          cache: 'no-store',
          credentials: 'same-origin'
        });
        const data = await res.json().catch(() => ({}));
        if (!res.ok || !data.success) throw new Error('offline');
        if (data.install_error) {
          githubFirmwareUpdateTag = '';
          const button = document.getElementById('ota_github_btn');
          if (button) button.textContent = t('otaGithubCheck');
          setGithubOtaUi(data.install_error, 'idle', 'error');
          showNotification(data.install_error, false);
          setFirmwareOtaControlsDisabled(false);
          return;
        }
        if (!data.install_requested && data.current_version === targetTag) {
          window.location.reload();
          return;
        }
      } catch (e) {}

      if (Date.now() - startedAt < 180000) {
        window.setTimeout(poll, 1500);
      } else {
        setGithubOtaUi(t('otaReconnecting'), 'idle', 'error');
        setFirmwareOtaControlsDisabled(false);
      }
    };
    window.setTimeout(poll, 1200);
  }

  async function installGithubFirmware(tag) {
    setFirmwareOtaControlsDisabled(true);
    setGithubOtaUi(t('otaGithubDownloading'), 'busy');
    showNotification(t('otaGithubDownloading'));
    try {
      const res = await fetch('/api/ota/github/install', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'tag=' + encodeURIComponent(tag),
        cache: 'no-store',
        credentials: 'same-origin'
      });
      const data = await res.json().catch(() => ({}));
      if (!res.ok || !data.success) {
        throw new Error(data.error || t('otaFailed'));
      }
      setGithubOtaUi(t('otaGithubDownloading') + ' ' + t('otaReconnecting'), 'busy');
      waitForGithubFirmwareResult(tag);
    } catch (err) {
      const message = err?.message || t('otaFailed');
      setGithubOtaUi(message, 'idle', 'error');
      showNotification(message, false);
      setFirmwareOtaControlsDisabled(false);
    }
  }

  async function checkOrInstallGithubFirmware() {
    if (githubFirmwareUpdateTag) {
      await installGithubFirmware(githubFirmwareUpdateTag);
      return;
    }

    const button = document.getElementById('ota_github_btn');
    setFirmwareOtaControlsDisabled(true);
    if (button) button.textContent = t('otaGithubChecking');
    setGithubOtaUi(t('otaGithubChecking'));
    try {
      const res = await fetch('/api/ota/github/check', {
        method: 'POST',
        cache: 'no-store',
        credentials: 'same-origin'
      });
      const data = await res.json().catch(() => ({}));
      if (!res.ok || !data.success) {
        throw new Error(data.error || t('otaGithubCheckFailed'));
      }

      if (data.update_available && data.latest_tag) {
        githubFirmwareUpdateTag = data.latest_tag;
        if (button) button.textContent = formatGithubFirmwareText('otaGithubInstall', data.latest_tag);
        const message = formatGithubFirmwareText('otaGithubAvailable', data.latest_tag);
        setGithubOtaUi(message, 'idle', 'success');
        showNotification(message);
      } else {
        if (button) button.textContent = t('otaGithubCheck');
        setGithubOtaUi(t('otaGithubUpToDate'), 'idle', 'success');
        showNotification(t('otaGithubUpToDate'));
      }
    } catch (err) {
      if (button) button.textContent = t('otaGithubCheck');
      const message = err?.message || t('otaGithubCheckFailed');
      setGithubOtaUi(message, 'idle', 'error');
      showNotification(message, false);
    } finally {
      setFirmwareOtaControlsDisabled(false);
    }
  }

  async function uploadOtaFirmware() {
    const input = document.getElementById('ota_file');
    const button = document.getElementById('ota_upload_btn');
    const chooseBtn = document.getElementById('ota_choose_btn');
    const githubBtn = document.getElementById('ota_github_btn');
    const statusEl = document.getElementById('ota_status');
    const progressEl = document.getElementById('ota_progress');
    const progressBarEl = document.getElementById('ota_progress_bar');
    if (!input || !input.files || !input.files.length) {
      showNotification(t('otaSelectFile'), false);
      return;
    }

    const file = input.files[0];
    if (!file || !String(file.name || '').toLowerCase().endsWith('.bin')) {
      showNotification(t('otaSelectFile'), false);
      return;
    }

    const setOtaUi = (message, phase = 'idle', tone = '', percent = null) => {
      if (statusEl) {
        statusEl.textContent = message || '';
        statusEl.classList.remove('error', 'success');
        if (tone === 'error' || tone === 'success') statusEl.classList.add(tone);
      }
      if (progressEl) {
        progressEl.classList.toggle('is-hidden', phase === 'idle');
        progressEl.classList.toggle('active', phase === 'busy' && percent === null);
      }
      if (progressBarEl) {
        if (percent !== null) {
          const safePercent = Math.max(0, Math.min(100, percent));
          progressBarEl.style.width = safePercent + '%';
        } else if (phase === 'done') {
          progressBarEl.style.width = '100%';
        } else {
          progressBarEl.style.width = '0%';
        }
      }
    };

    const waitForDeviceReload = () => {
      const startedAt = Date.now();
      const tryReload = () => {
        fetch(window.location.pathname + '?ota_ping=' + Date.now(), {
          method: 'GET',
          cache: 'no-store',
          credentials: 'same-origin'
        })
        .then((res) => {
          if (!res.ok) throw new Error('offline');
          window.location.reload();
        })
        .catch(() => {
          if (Date.now() - startedAt < 120000) {
            window.setTimeout(tryReload, 1500);
          }
        });
      };
      window.setTimeout(tryReload, 2500);
    };

    if (button) {
      if (!button.dataset.defaultLabel) button.dataset.defaultLabel = button.textContent;
      button.disabled = true;
      button.textContent = button.dataset.defaultLabel || 'Update';
    }
    if (chooseBtn) chooseBtn.disabled = true;
    if (githubBtn) githubBtn.disabled = true;
    input.disabled = true;
    setOtaUi(t('otaUploading') + ' 0%', 'busy', '', 0);
    showNotification(t('otaUploading'));

    try {
      const prepRes = await fetch('/api/ota/prepare?size=' + encodeURIComponent(String(file.size || 0)), {
        method: 'POST',
        cache: 'no-store',
        credentials: 'same-origin'
      });
      const prepData = await prepRes.json().catch(() => ({}));
      if (!prepRes.ok || !prepData.success) {
        throw new Error(prepData.error || t('otaFailed'));
      }
      await new Promise(resolve => window.setTimeout(resolve, 250));
    } catch (err) {
      const message = err?.message || t('otaFailed');
      setOtaUi(message, 'idle', 'error');
      showNotification(message, false);
      if (button) {
        button.disabled = false;
        button.textContent = button.dataset.defaultLabel || 'Update';
      }
      if (chooseBtn) chooseBtn.disabled = false;
      if (githubBtn) githubBtn.disabled = false;
      input.disabled = false;
      return;
    }

    const formData = new FormData();
    formData.append('firmware', file);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/ota/upload?size=' + encodeURIComponent(String(file.size || 0)), true);

    xhr.upload.onprogress = (event) => {
      if (!event.lengthComputable) return;
      const percent = Math.max(1, Math.min(100, Math.round((event.loaded / event.total) * 100)));
      setOtaUi(t('otaUploading') + ' ' + percent + '%', 'busy', '', percent);
    };

    xhr.upload.onload = () => {
      setOtaUi(t('otaInstalling'), 'busy', '', null);
    };

    xhr.onload = () => {
      let data = {};
      try {
        data = JSON.parse(xhr.responseText || '{}');
      } catch (e) {}

      if (xhr.status < 200 || xhr.status >= 300 || !data.success) {
        const message = data.error || t('otaFailed');
        setOtaUi(message, 'idle', 'error');
        showNotification(message, false);
        if (button) {
          button.disabled = false;
          button.textContent = button.dataset.defaultLabel || 'Update';
        }
        if (chooseBtn) chooseBtn.disabled = false;
        if (githubBtn) githubBtn.disabled = false;
        input.disabled = false;
        return;
      }

      const message = t('otaSuccess');
      setOtaUi(message, 'done', 'success', 100);
      showNotification(message);
      if (statusEl) statusEl.textContent = message + ' ' + t('otaReconnecting');
      input.value = '';
      updateOtaFileName(input);
      waitForDeviceReload();
    };

    xhr.onerror = () => {
      const message = t('otaFailed');
      setOtaUi(message, 'idle', 'error');
      showNotification(message, false);
      if (button) {
        button.disabled = false;
        button.textContent = button.dataset.defaultLabel || 'Update';
      }
      if (chooseBtn) chooseBtn.disabled = false;
      if (githubBtn) githubBtn.disabled = false;
      input.disabled = false;
    };

    xhr.send(formData);
  }

  // Tile Editor State
)html";
  html += "  const GRID_COLS = " + String(GRID_COLS) + ";\n";
  html += "  const GRID_ROWS = " + String(GRID_ROWS) + ";\n";
  html += "  const MEDIA_TILE_TYPE = " + String(static_cast<unsigned>(TILE_MEDIA)) + ";\n";
  html += "  const MEDIA_TILE_MIN_SPAN = " + String(MEDIA_TILE_MIN_SPAN) + ";\n";
  html += "  const MEDIA_TILE_MAX_SPAN = " + String(MEDIA_TILE_MAX_SPAN) + ";\n";
  html += "  const SCREENSAVER_TILE_DEFAULT_OPACITY = " +
          String(kScreensaverDefaultTileOpacity) + ";\n";
  html += R"html(
  const tileTabs = [];
  const folderByTab = {};
  const tabByFolder = {};
  let currentTileTab = '';
  let currentTileIndex = -1;
  let drafts = {};
  let tilesData = {};
  let autoSaveTimers = {};
  let saveRequestSeq = 0;
  let latestSaveRequestByTab = {};
  let saveInFlightByTile = {};
  let queuedSaveByTile = {};
  let sensorMetaCache = { values: {}, units: {}, icons: {}, names: {}, loaded: false };
  let sensorMetaFetchInFlight = null;
  let lastSensorMetaFetchMs = 0;
  const SELECTED_TILE_STORAGE_KEY = 'selectedAdminTile';
  let selectedTileByTab = {};

  function normalizeSensorMetaPayload(payload) {
    if (!payload || typeof payload !== 'object') {
      return { values: {}, units: {}, icons: {}, names: {}, loaded: false };
    }
    const hasMeta = Object.prototype.hasOwnProperty.call(payload, 'values') ||
                    Object.prototype.hasOwnProperty.call(payload, 'units') ||
                    Object.prototype.hasOwnProperty.call(payload, 'icons') ||
                    Object.prototype.hasOwnProperty.call(payload, 'names') ||
                    Object.prototype.hasOwnProperty.call(payload, 'energy_values') ||
                    Object.prototype.hasOwnProperty.call(payload, 'energy_units') ||
                    Object.prototype.hasOwnProperty.call(payload, 'climate_values');
    if (!hasMeta) {
      return { values: payload || {}, units: {}, icons: {}, names: {}, loaded: true };
    }
    return {
      values: Object.assign(
        {},
        payload.values || {},
        payload.energy_values || {},
        payload.climate_values || {}
      ),
      units: Object.assign({}, payload.units || {}, payload.energy_units || {}),
      icons: payload.icons || {},
      names: payload.names || {},
      loaded: true
    };
  }

  function isSensorMetaCacheLoaded() {
    return !!(sensorMetaCache && sensorMetaCache.loaded);
  }

  function fetchSensorMetaCache(force = false) {
    const now = Date.now();
    if (!force && sensorMetaFetchInFlight) return sensorMetaFetchInFlight;
    if (!force && sensorMetaCache.loaded && (now - lastSensorMetaFetchMs) < 15000) {
      return Promise.resolve(sensorMetaCache);
    }
    sensorMetaFetchInFlight = fetch('/api/sensor_values')
      .then(res => res.json())
      .then(raw => {
        sensorMetaCache = normalizeSensorMetaPayload(raw || {});
        lastSensorMetaFetchMs = Date.now();
        return sensorMetaCache;
      })
      .catch(() => sensorMetaCache)
      .finally(() => { sensorMetaFetchInFlight = null; });
    return sensorMetaFetchInFlight;
  }

  function isExplicitlyDisabledValue(raw) {
    if (raw === undefined || raw === null) return false;
    const text = String(raw);
    if (!text.length) return false;
    const trimmed = text.trim().toLowerCase();
    if (!trimmed.length) return true;
    return trimmed === '-' || trimmed === 'none' || trimmed === 'null' || trimmed === 'no' || trimmed === 'off';
  }

  function normalizeMdiIconName(raw) {
    let iconName = String(raw || '').trim().toLowerCase();
    if (iconName.startsWith('mdi:')) iconName = iconName.substring(4);
    else if (iconName.startsWith('mdi-')) iconName = iconName.substring(4);
    return iconName;
  }

  function resolveIconName(rawIcon, entityId, metaIcons) {
    if (isExplicitlyDisabledValue(rawIcon)) return '';
    const direct = normalizeMdiIconName(rawIcon);
    if (direct) return direct;
    if (entityId && metaIcons && metaIcons[entityId]) {
      return normalizeMdiIconName(metaIcons[entityId]);
    }
    return '';
  }

  function resolveUnitValue(rawUnit, entityId, metaUnits) {
    if (isExplicitlyDisabledValue(rawUnit)) return '';
    const direct = String(rawUnit || '').trim();
    if (direct.length) return rawUnit;
    if (entityId && metaUnits && metaUnits[entityId]) {
      return metaUnits[entityId];
    }
    return '';
  }

  function getTileTypeMeta(typeValue) {
    const key = String(typeValue ?? '0');
    return TILE_TYPE_REGISTRY[key] || TILE_TYPE_REGISTRY['0'] || {};
  }

  function syncTileTypeSelectValue(selectEl, typeValue) {
    if (!selectEl) return;
    const key = String(typeValue ?? '0');
    Array.from(selectEl.querySelectorAll('option[data-locked-only="1"]')).forEach(opt => {
      if (opt.value !== key) opt.remove();
    });
    let option = Array.from(selectEl.options).find(opt => String(opt.value) === key);
    if (!option) {
      const meta = getTileTypeMeta(key);
      option = document.createElement('option');
      option.value = key;
      option.textContent = meta.label || key;
      option.disabled = !!meta.locked;
      option.dataset.lockedOnly = '1';
      selectEl.appendChild(option);
    }
    selectEl.value = key;
  }

  function callTypeHandler(meta, handlerKey, ...args) {
    if (!meta || !handlerKey) return;
    const fnName = meta[handlerKey];
    if (!fnName) return;
    const fn = window[fnName];
    if (typeof fn === 'function') return fn(...args);
  }

  function rebuildEntitySelect(id, entries) {
    const el = document.getElementById(id);
    if (!el || !Array.isArray(entries)) return;
    const keep = el.value || el.dataset.configuredValue || '';
    const placeholder = el.options.length ? el.options[0].cloneNode(true) : null;
    el.innerHTML = '';
    if (placeholder) el.appendChild(placeholder);
    for (const entry of entries) {
      if (!entry || entry.v === undefined) continue;
      const opt = document.createElement('option');
      opt.value = entry.v;
      opt.textContent = entry.t || entry.v;
      el.appendChild(opt);
    }
    el.value = keep;
    if (keep && el.value !== keep) {
      // Gespeicherte Entity fehlt in der aktuellen Bridge-Liste (z.B. Bridge
      // offline): Auswahl behalten statt sie beim naechsten Autosave zu leeren.
      const opt = document.createElement('option');
      opt.value = keep;
      opt.textContent = keep;
      el.appendChild(opt);
      el.value = keep;
    }
    if (keep) el.dataset.configuredValue = keep;
  }

  // Holt die Entity-Listen frisch von der Firmware und baut alle Editor-
  // Dropdowns des Tabs neu auf. Neue Entitaeten tauchen so ohne Seiten-
  // Reload auf, sobald man eine Kachel oeffnet.
  function refreshEntityOptionLists(tab) {
    fetch('/api/entity_options')
      .then(res => res.json())
      .then(data => {
        if (!data || !data.success) return;
        rebuildEntitySelect(tab + '_sensor_entity', data.sensors);
        rebuildEntitySelect(tab + '_energy_entity', data.energy);
        rebuildEntitySelect(tab + '_weather_entity', data.weathers);
        rebuildEntitySelect(tab + '_switch_entity', data.switches);
        rebuildEntitySelect(tab + '_media_entity', data.media);
        rebuildEntitySelect(tab + '_climate_entity', data.climates);
        rebuildEntitySelect(tab + '_scene_alias', data.scenes);
      })
      .catch(() => {});
  }

  function collectTypeFieldValues(tab) {
    const prefix = tab;
    const typeValue = document.getElementById(prefix + '_tile_type')?.value || '0';
    const meta = getTileTypeMeta(typeValue);
    if (!meta.save) return {};
    const fd = new FormData();
    callTypeHandler(meta, 'save', prefix, fd);
    const out = {};
    for (const [key, value] of fd.entries()) {
      out[key] = value;
    }
    return out;
  }

  function normalizeSnapshotLayout(snapshot, index, tab = currentTileTab) {
    const fallbackCol = (index >= 0) ? ((index % GRID_COLS) + 1) : 1;
    const firstRow = firstAllowedGridRow(tab);
    const fallbackRow = (index >= 0)
      ? (Math.max(firstRow, Math.floor(index / GRID_COLS)) + 1)
      : (firstRow + 1);
    let col = clampInt(snapshot?.col, 1, GRID_COLS, fallbackCol);
    let row = clampInt(snapshot?.row, firstRow + 1, GRID_ROWS, fallbackRow);
    let spanW = clampInt(snapshot?.span_w, 1, GRID_COLS, 1);
    let spanH = clampInt(snapshot?.span_h, 1, GRID_ROWS, 1);
    return constrainLayoutToTab(
      normalizeLayoutForTileType(snapshot?.type, col - 1, row - 1,
                                 spanW, spanH),
      tab);
  }

  function buildTileSnapshotFromInputs(tab) {
    const prefix = tab;
    const colorEl = document.getElementById(prefix + '_tile_color');
    const snapshot = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: colorEl?.value || '#2A2A2A',
      bg_color_default: tileColorInputIsDefault(tab) ? '1' : '0',
      col: document.getElementById(prefix + '_tile_col')?.value || '1',
      row: document.getElementById(prefix + '_tile_row')?.value || '1',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
    if (isScreensaverTileTab(tab)) {
      snapshot.background_opacity = document.getElementById('screensaver_tile_opacity')?.value || '0';
    }
    Object.assign(snapshot, collectTypeFieldValues(tab));
    return snapshot;
  }

  function getTileSnapshotForSave(tab, index) {
    const draft = drafts[tab] && drafts[tab][index];
    if (draft && draft._dirty) return Object.assign({}, draft);
    if (currentTileTab === tab && currentTileIndex === index) return buildTileSnapshotFromInputs(tab);
    return null;
  }

  function applySnapshotToTileData(tab, index, snapshot) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles) || index < 0) return;

    const prev = tiles[index] || {};
    const tile = Object.assign({}, prev);
    const layout = normalizeSnapshotLayout(snapshot, index, tab);
    const numericFields = ['type', 'sensor_decimals', 'sensor_value_font', 'sensor_display_mode', 'sensor_gauge_min', 'sensor_gauge_max', 'switch_style', 'navigate_target', 'popup_open_mode', 'key_code', 'key_modifier', 'background_opacity'];

    tile.type = clampInt(snapshot?.type, 0, 255, Number(prev.type) || 0);
    tile.title = snapshot?.title || '';
    tile.icon_name = snapshot?.icon || '';
    tile.bg_color = snapshotBgColorIsDefault(snapshot)
                        ? 0
                        : makeTileBgValue(hexToRgb(snapshot?.color || '#2A2A2A'));
    tile.col = layout.col;
    tile.row = layout.row;
    tile.span_w = layout.span_w;
    tile.span_h = layout.span_h;

    for (const [key, value] of Object.entries(snapshot || {})) {
      if (key === '_dirty' || key === '_rev' || key === 'icon' || key === 'color' || key === 'bg_color_default' || key === 'col' || key === 'row' || key === 'span_w' || key === 'span_h' || key === 'type' || key === 'title') continue;
      if (numericFields.includes(key)) {
        const num = Number(value);
        tile[key] = Number.isFinite(num) ? num : value;
      } else {
        tile[key] = value;
      }
    }

    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'switch_entity')) {
      tile.sensor_entity = snapshot.switch_entity || '';
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'weather_entity')) {
      tile.sensor_entity = snapshot.weather_entity || '';
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'energy_entity')) {
      tile.sensor_entity = snapshot.energy_entity || '';
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'climate_entity')) {
      tile.sensor_entity = snapshot.climate_entity || '';
    }
    if (snapshot && (Object.prototype.hasOwnProperty.call(snapshot, 'clock_show_time') || Object.prototype.hasOwnProperty.call(snapshot, 'clock_show_date'))) {
      let flags = 0;
      if (String(snapshot.clock_show_time || '0') === '1') flags |= 1;
      if (String(snapshot.clock_show_date || '0') === '1') flags |= 2;
      if (flags === 0) flags = 1;
      tile.sensor_decimals = flags;
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'clock_time_format')) {
      const num = Number(snapshot.clock_time_format);
      tile.sensor_gauge_min = Number.isFinite(num) ? num : 0;
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'clock_date_format')) {
      const num = Number(snapshot.clock_date_format);
      tile.sensor_gauge_max = Number.isFinite(num) ? num : 0;
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'animation_fit')) {
      const num = Number(snapshot.animation_fit);
      tile.sensor_display_mode = Number.isFinite(num) ? num : 0;
    }
    if (snapshot && Object.prototype.hasOwnProperty.call(snapshot, 'animation_zoom')) {
      const num = Number(snapshot.animation_zoom);
      tile.sensor_gauge_max = Number.isFinite(num) ? num : 100;
    }

    tiles[index] = tile;
    tilesData[tab] = tiles;
  }

  function markLatestSaveRequest(tab, index, requestId) {
    if (!latestSaveRequestByTab[tab]) latestSaveRequestByTab[tab] = {};
    latestSaveRequestByTab[tab][index] = requestId;
  }

  function isLatestSaveRequest(tab, index, requestId) {
    return !!(latestSaveRequestByTab[tab] && latestSaveRequestByTab[tab][index] === requestId);
  }

  function getTileSaveKey(tab, index) {
    return tab + ':' + index;
  }

  function queueSaveAfterFlight(tab, index, silent = true) {
    const saveKey = getTileSaveKey(tab, index);
    const existing = queuedSaveByTile[saveKey];
    queuedSaveByTile[saveKey] = {
      silent: existing ? (existing.silent && silent) : silent
    };
  }

  function flushQueuedSave(tab, index) {
    const saveKey = getTileSaveKey(tab, index);
    if (saveInFlightByTile[saveKey]) return;
    const queued = queuedSaveByTile[saveKey];
    if (!queued) return;
    delete queuedSaveByTile[saveKey];
    saveTile(tab, queued.silent, index);
  }

  function getTileResizeHandlesHtml(typeValue) {
    if (String(typeValue || '0') === '0') return '';
    return '' +
      '<div class="tile-resize-handle tile-resize-handle-e" data-resize-dir="e"></div>' +
      '<div class="tile-resize-handle tile-resize-handle-s" data-resize-dir="s"></div>' +
      '<div class="tile-resize-handle tile-resize-handle-se" data-resize-dir="se"></div>';
  }

  function initTileTabs() {
    tileTabs.length = 0;
    Object.keys(folderByTab).forEach(k => delete folderByTab[k]);
    Object.keys(tabByFolder).forEach(k => delete tabByFolder[k]);
    document.querySelectorAll('.tile-tab').forEach(tabEl => {
      const tabId = tabEl.dataset.tabId || '';
      if (!tabId) return;
      tileTabs.push(tabId);
      const folderId = parseInt(tabEl.dataset.folderId, 10);
      if (!isNaN(folderId)) {
        folderByTab[tabId] = folderId;
        tabByFolder[folderId] = tabId;
      }
      if (!drafts[tabId]) drafts[tabId] = {};
      if (!tilesData[tabId]) tilesData[tabId] = [];
      if (!autoSaveTimers[tabId]) autoSaveTimers[tabId] = null;
    });
    if (!currentTileTab && tileTabs.length) currentTileTab = tileTabs[0];
  }

  function getFolderIdForTab(tab) {
    return folderByTab[tab];
  }

  function getTilesData(tab) {
    return tilesData[tab] || [];
  }
  function isScreensaverTileTab(tab) {
    return tab === 'screensaver';
  }
  function firstAllowedGridRow(tab) {
    return isScreensaverTileTab(tab) ? Math.max(0, GRID_ROWS - 2) : 0;
  }
  function restoreCurrentTileSelectionUi() {
    if (currentTileIndex === -1 || !currentTileTab) return;
    document.querySelectorAll('.tile').forEach(t => t.classList.remove('active'));
    const settingsId = currentTileTab + 'Settings';
    document.getElementById(settingsId)?.classList.remove('hidden');
    const activeTile = document.getElementById(currentTileTab + '-tile-' + currentTileIndex);
    if (activeTile) {
      activeTile.classList.add('active');
      window.requestAnimationFrame(() => {
        if (currentTileTab && currentTileIndex >= 0) {
          document.getElementById(currentTileTab + '-tile-' + currentTileIndex)?.classList.add('active');
        }
      });
    }
  }
  function ensureNavigateTargetOption(folderId, label) {
    const folderValue = String(folderId);
    document.querySelectorAll('select[id$="_navigate_target"]').forEach(select => {
      let opt = select.querySelector('option[value="' + folderValue + '"]');
      if (!opt) {
        opt = document.createElement('option');
        opt.value = folderValue;
        select.appendChild(opt);
      }
      opt.textContent = label;
    });
  }
  async function ensureFolderTabUi(folderId, name = '', icon = '') {
    const folderNum = parseInt(folderId, 10);
    if (isNaN(folderNum) || folderNum <= 0) return false;
    if (tabByFolder[folderNum]) {
      updateFolderTabUi(folderNum, name, icon);
      ensureNavigateTargetOption(folderNum, formatFolderLabel(name, folderNum));
      return true;
    }
    const res = await fetch('/api/folders/tab?folder_id=' + encodeURIComponent(folderNum));
    const data = await res.json().catch(() => ({}));
    if (!res.ok || !data.success || !data.tab_id || !data.button_html || !data.tab_html) {
      return false;
    }

    const nav = document.querySelector('.tab-nav');
    const networkTab = document.getElementById('tab-network');
    if (!nav || !networkTab) return false;

    const buttonTpl = document.createElement('template');
    buttonTpl.innerHTML = String(data.button_html || '').trim();
    const tabTpl = document.createElement('template');
    tabTpl.innerHTML = String(data.tab_html || '').trim();
    const buttonEl = buttonTpl.content.firstElementChild;
    const tabEl = tabTpl.content.firstElementChild;
    if (!buttonEl || !tabEl) return false;

    const fixedBtn = Array.from(nav.querySelectorAll('.tab-btn')).find(btn =>
      btn.getAttribute('onclick')?.includes("'tab-tiles-screensaver'")) ||
      Array.from(nav.querySelectorAll('.tab-btn')).find(btn =>
        btn.getAttribute('onclick')?.includes("'tab-network'"));
    if (fixedBtn) nav.insertBefore(buttonEl, fixedBtn);
    else nav.appendChild(buttonEl);
    const screensaverTab = document.getElementById('tab-tiles-screensaver');
    networkTab.parentNode.insertBefore(tabEl, screensaverTab || networkTab);

    initTileTabs();
    enableTileDrag(String(data.tab_id));
    enableTileResize(String(data.tab_id));
    ensureNavigateTargetOption(folderNum, formatFolderLabel(name, folderNum));
    updateFolderTabUi(folderNum, name, icon);
    loadSensorValues(true, true);
    return true;
  }
  function persistSelectedTileState() {
    try {
      if (currentTileTab && currentTileIndex >= 0) {
        selectedTileByTab[currentTileTab] = currentTileIndex;
        localStorage.setItem(SELECTED_TILE_STORAGE_KEY, JSON.stringify(selectedTileByTab));
      } else {
        localStorage.removeItem(SELECTED_TILE_STORAGE_KEY);
      }
    } catch (e) {}
  }
  function loadSelectedTileStates() {
    try {
      const raw = localStorage.getItem(SELECTED_TILE_STORAGE_KEY);
      if (!raw) return;
      const saved = JSON.parse(raw);
      // Migration vom bisherigen Format { tab, index } auf die Auswahl pro Tab.
      if (saved && typeof saved.tab === 'string') {
        const index = Number(saved.index);
        if (Number.isInteger(index) && index >= 0 && index < TILES_PER_GRID) {
          selectedTileByTab[saved.tab] = index;
        }
        return;
      }
      if (!saved || typeof saved !== 'object') return;
      Object.entries(saved).forEach(([tab, rawIndex]) => {
        const index = Number(rawIndex);
        if (Number.isInteger(index) && index >= 0 && index < TILES_PER_GRID) {
          selectedTileByTab[tab] = index;
        }
      });
    } catch (e) {
      selectedTileByTab = {};
    }
  }
  function getRememberedTileIndex(tab) {
    const markedTile = document.querySelector('#tab-tiles-' + tab + ' .tile[data-selected="1"]');
    if (markedTile && Number(markedTile.dataset.type || 0) !== 0) {
      const markedIndex = Number(markedTile.dataset.index);
      if (Number.isInteger(markedIndex) && markedIndex >= 0 && markedIndex < TILES_PER_GRID) {
        return markedIndex;
      }
    }
    const index = Number(selectedTileByTab[tab]);
    if (!Number.isInteger(index) || index < 0 || index >= TILES_PER_GRID) return null;
    const tile = document.getElementById(tab + '-tile-' + index);
    if (!tile || Number(tile.dataset.type || 0) === 0) {
      delete selectedTileByTab[tab];
      return null;
    }
    return index;
  }
  function restoreSelectedTileState(tab) {
    const targetTab = tab || currentTileTab;
    const index = targetTab ? getRememberedTileIndex(targetTab) : null;
    if (index === null) return false;
    selectTile(index, targetTab);
    return true;
  }
  function openPreviewNavigation(tileEl, tab) {
    if (!tileEl) return;
    const type = Number(tileEl.dataset.type);
    if (type === 7) {
      switchTab('tab-network');
      return;
    }
    if (type === 8) {
      const folderTab = document.getElementById('tab-tiles-' + tab);
      const parentId = Number(folderTab?.dataset.folderParent);
      const parentTab = tabByFolder[parentId];
      if (parentTab) switchTab('tab-tiles-' + parentTab);
      return;
    }
    if (type !== 4) return;
    const index = Number(tileEl.dataset.index);
    const tile = Number.isInteger(index) ? getTilesData(tab)[index] : null;
    const targetId = Number(tile?.navigate_target ?? tileEl.dataset.navigateTarget);
    const targetTab = tabByFolder[targetId];
    if (targetTab) switchTab('tab-tiles-' + targetTab);
  }
  function clampInt(value, min, max, fallback) {
    const v = parseInt(value, 10);
    if (isNaN(v)) return fallback !== undefined ? fallback : min;
    if (v < min) return min;
    if (v > max) return max;
    return v;
  }

  function normalizeLayoutForTileType(typeValue, col, row, spanW, spanH) {
    let safeCol = clampInt(col, 0, GRID_COLS - 1, 0);
    let safeRow = clampInt(row, 0, GRID_ROWS - 1, 0);
    let safeW = clampInt(spanW, 1, GRID_COLS, 1);
    let safeH = clampInt(spanH, 1, GRID_ROWS, 1);
    if (Number(typeValue) === MEDIA_TILE_TYPE) {
      const minW = Math.min(MEDIA_TILE_MIN_SPAN, GRID_COLS);
      const minH = Math.min(MEDIA_TILE_MIN_SPAN, GRID_ROWS);
      safeW = clampInt(safeW, minW, Math.min(MEDIA_TILE_MAX_SPAN, GRID_COLS), minW);
      safeH = clampInt(safeH, minH, Math.min(MEDIA_TILE_MAX_SPAN, GRID_ROWS), minH);
      safeCol = Math.min(safeCol, GRID_COLS - safeW);
      safeRow = Math.min(safeRow, GRID_ROWS - safeH);
    } else {
      safeW = Math.min(safeW, GRID_COLS - safeCol);
      safeH = Math.min(safeH, GRID_ROWS - safeRow);
    }
    return { col: safeCol, row: safeRow, span_w: safeW, span_h: safeH };
  }

  function constrainLayoutToTab(layout, tab) {
    const firstRow = firstAllowedGridRow(tab);
    if (layout.row < firstRow) layout.row = firstRow;
    if (layout.span_h > GRID_ROWS - layout.row) {
      layout.span_h = GRID_ROWS - layout.row;
    }
    return layout;
  }

  function normalizeTileLayout(tile, index, tab = currentTileTab) {
    const fallbackCol = index % GRID_COLS;
    const firstRow = firstAllowedGridRow(tab);
    const fallbackRow = Math.max(firstRow, Math.floor(index / GRID_COLS));
    const col = clampInt(tile?.col, 0, GRID_COLS - 1, fallbackCol);
    const row = clampInt(tile?.row, firstRow, GRID_ROWS - 1, fallbackRow);
    let spanW = clampInt(tile?.span_w, 1, GRID_COLS, 1);
    let spanH = clampInt(tile?.span_h, 1, GRID_ROWS, 1);
    return constrainLayoutToTab(
      normalizeLayoutForTileType(tile?.type, col, row, spanW, spanH), tab);
  }

  function setGridItemPosition(el, col, row, spanW, spanH) {
    if (!el) return;
    el.style.gridColumn = (col + 1) + ' / span ' + spanW;
    el.style.gridRow = (row + 1) + ' / span ' + spanH;
    el.dataset.col = String(col);
    el.dataset.row = String(row);
    el.dataset.spanW = String(spanW);
    el.dataset.spanH = String(spanH);
  }

  function setTileGridPosition(el, col, row, spanW, spanH) {
    setGridItemPosition(el, col, row, spanW, spanH);
  }

  function getTileElementLayout(tab, index) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return null;
    const col = clampInt(el.dataset.col, 0, GRID_COLS - 1, null);
    const row = clampInt(el.dataset.row, firstAllowedGridRow(tab), GRID_ROWS - 1, null);
    const spanW = clampInt(el.dataset.spanW, 1, GRID_COLS, null);
    const spanH = clampInt(el.dataset.spanH, 1, GRID_ROWS, null);
    if (col === null || row === null || spanW === null || spanH === null) return null;
    return { col, row, span_w: spanW, span_h: spanH };
  }

  function layoutTiles(tab, tiles) {
    if (!Array.isArray(tiles)) return;
    const occupied = Array.from({ length: GRID_ROWS }, () => Array(GRID_COLS).fill(false));
    const emptyIndices = [];

    tiles.forEach((tile, idx) => {
      const typeNum = Number(tile?.type);
      if (!tile || isNaN(typeNum) || typeNum === 0) {
        emptyIndices.push(idx);
        return;
      }
      const layout = normalizeTileLayout(tile, idx, tab);
      const el = document.getElementById(tab + '-tile-' + idx);
      if (el) {
        setTileGridPosition(el, layout.col, layout.row, layout.span_w, layout.span_h);
        el.style.display = '';
      }
      for (let r = layout.row; r < layout.row + layout.span_h; r++) {
        for (let c = layout.col; c < layout.col + layout.span_w; c++) {
          if (r < GRID_ROWS && c < GRID_COLS) occupied[r][c] = true;
        }
      }
    });

    const freeCells = [];
    for (let r = firstAllowedGridRow(tab); r < GRID_ROWS; r++) {
      for (let c = 0; c < GRID_COLS; c++) {
        if (!occupied[r][c]) freeCells.push({ col: c, row: r });
      }
    }

    emptyIndices.forEach((idx, i) => {
      const el = document.getElementById(tab + '-tile-' + idx);
      if (!el) return;
      if (i < freeCells.length) {
        const cell = freeCells[i];
        setTileGridPosition(el, cell.col, cell.row, 1, 1);
        el.style.display = '';
      } else {
        el.style.display = 'none';
      }
    });
  }

  function normalizeLayoutInputs(tab) {
    const prefix = tab;
    const colEl = document.getElementById(prefix + '_tile_col');
    const rowEl = document.getElementById(prefix + '_tile_row');
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    const spanHEl = document.getElementById(prefix + '_tile_span_h');

    if (!colEl || !rowEl || !spanWEl || !spanHEl) {
      const fallback = getTileElementLayout(tab, currentTileIndex);
      if (fallback) return fallback;
      return { col: 0, row: 0, span_w: 1, span_h: 1 };
    }

    let col = clampInt(colEl.value, 1, GRID_COLS, 1);
    const firstRow = firstAllowedGridRow(tab);
    let row = clampInt(rowEl.value, firstRow + 1, GRID_ROWS, firstRow + 1);
    let spanW = clampInt(spanWEl.value, 1, GRID_COLS, 1);
    let spanH = clampInt(spanHEl.value, 1, GRID_ROWS, 1);

    const typeValue = document.getElementById(prefix + '_tile_type')?.value || '0';
    const layout = constrainLayoutToTab(
      normalizeLayoutForTileType(typeValue, col - 1, row - 1, spanW, spanH),
      tab);
    col = layout.col + 1;
    row = layout.row + 1;
    spanW = layout.span_w;
    spanH = layout.span_h;

    colEl.value = String(col);
    rowEl.value = String(row);
    spanWEl.value = String(spanW);
    spanHEl.value = String(spanH);

    return { col: col - 1, row: row - 1, span_w: spanW, span_h: spanH };
  }

  function updateLayoutFromInputs(tab) {
    if (currentTileIndex === -1) return;
    const layout = normalizeLayoutInputs(tab);
    const tiles = getTilesData(tab);
    const tileEl = document.getElementById(tab + '-tile-' + currentTileIndex);
    if (tileEl && (!Array.isArray(tiles) || tiles.length === 0)) {
      setTileGridPosition(tileEl, layout.col, layout.row, layout.span_w, layout.span_h);
      return;
    }
    if (!Array.isArray(tiles) || currentTileIndex >= tiles.length) return;
    const tile = tiles[currentTileIndex] || {};
    tile.col = layout.col;
    tile.row = layout.row;
    tile.span_w = layout.span_w;
    tile.span_h = layout.span_h;
    const typeEl = document.getElementById(tab + '_tile_type');
    const typeNum = typeEl ? parseInt(typeEl.value, 10) : 0;
    tile.type = isNaN(typeNum) ? 0 : typeNum;
    tiles[currentTileIndex] = tile;
    layoutTiles(tab, tiles);
  }

  function applyLayoutInputsFromLayout(tab, layout, persistDraft = true) {
    if (!layout) return;
    const colEl = document.getElementById(tab + '_tile_col');
    const rowEl = document.getElementById(tab + '_tile_row');
    const spanWEl = document.getElementById(tab + '_tile_span_w');
    const spanHEl = document.getElementById(tab + '_tile_span_h');
    const colVal = String(layout.col + 1);
    const rowVal = String(layout.row + 1);
    if (colEl) colEl.value = colVal;
    if (rowEl) rowEl.value = rowVal;
    if (spanWEl && layout.span_w !== undefined) spanWEl.value = String(layout.span_w);
    if (spanHEl && layout.span_h !== undefined) spanHEl.value = String(layout.span_h);
    const tabDrafts = persistDraft ? drafts[tab] : null;
    if (tabDrafts && tabDrafts[currentTileIndex]) {
      tabDrafts[currentTileIndex].col = colVal;
      tabDrafts[currentTileIndex].row = rowVal;
      if (layout.span_w !== undefined) tabDrafts[currentTileIndex].span_w = String(layout.span_w);
      if (layout.span_h !== undefined) tabDrafts[currentTileIndex].span_h = String(layout.span_h);
      persistDrafts();
    }
  }

  function persistDrafts() { try { localStorage.setItem('tileDrafts', JSON.stringify(drafts)); } catch (e) {} }
  function loadDraftsFromStorage() {
    try {
      const raw = localStorage.getItem('tileDrafts');
      if (raw) {
        drafts = JSON.parse(raw);
        // Drafts sollen nach Page-Refresh nicht gespeicherte Werte ueberschreiben.
        for (const tab in drafts) {
          const tabDrafts = drafts[tab];
          if (!tabDrafts) continue;
          Object.keys(tabDrafts).forEach(key => {
            if (tabDrafts[key]) tabDrafts[key]._dirty = false;
          });
        }
      }
    } catch (e) {
      drafts = {};
    }
  }
  function clearDraft(tab, index) {
    if (drafts[tab] && drafts[tab][index]) {
      delete drafts[tab][index];
      persistDrafts();
    }
  }

  function swapDrafts(tab, fromIdx, toIdx) {
    if (!drafts[tab]) return;
    const fromVal = drafts[tab][fromIdx];
    const toVal = drafts[tab][toIdx];
    if (fromVal === undefined && toVal === undefined) return;
    drafts[tab][toIdx] = fromVal;
    if (toVal === undefined) delete drafts[tab][fromIdx];
    else drafts[tab][fromIdx] = toVal;
    persistDrafts();
  }

  function updateDraft(tab) {
    if (currentTileIndex === -1) return;
    if (!drafts[tab]) drafts[tab] = {};
    const prefix = tab;
    const prevDraft = drafts[tab][currentTileIndex];
    const colorEl = document.getElementById(prefix + '_tile_color');
    const d = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: colorEl?.value || '#2A2A2A',
      bg_color_default: tileColorInputIsDefault(tab) ? '1' : '0',
      col: document.getElementById(prefix + '_tile_col')?.value || '1',
      row: document.getElementById(prefix + '_tile_row')?.value || '1',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
    if (isScreensaverTileTab(tab)) {
      d.background_opacity = document.getElementById('screensaver_tile_opacity')?.value || '0';
    }
    Object.assign(d, collectTypeFieldValues(tab));
    d._dirty = true;
    d._rev = (prevDraft && prevDraft._rev) ? (prevDraft._rev + 1) : 1;
    drafts[tab][currentTileIndex] = d;
    applySnapshotToTileData(tab, currentTileIndex, d);
    persistDrafts();
  }

  function applyDraft(tab, index) {
    const d = drafts[tab] && drafts[tab][index];
    if (!d || !d._dirty) return false;
    const prefix = tab;
    syncTileTypeSelectValue(document.getElementById(prefix + '_tile_type'), d.type || '0');
    resetAllTypeFields(tab);
    updateTileType(tab);
    document.getElementById(prefix + '_tile_title').value = d.title || '';
    document.getElementById(prefix + '_tile_icon').value = d.icon || '';
    setTileColorInputFromSnapshot(tab, d);
    if (isScreensaverTileTab(tab)) {
      const opacity = document.getElementById('screensaver_tile_opacity');
      if (opacity) opacity.value = String(d.background_opacity ?? 0);
    }
    const colEl = document.getElementById(prefix + '_tile_col');
    if (colEl) colEl.value = d.col || '1';
    const rowEl = document.getElementById(prefix + '_tile_row');
    if (rowEl) rowEl.value = d.row || '1';
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    if (spanWEl) spanWEl.value = d.span_w || '1';
    const spanHEl = document.getElementById(prefix + '_tile_span_h');
    if (spanHEl) spanHEl.value = d.span_h || '1';
    const meta = getTileTypeMeta(d.type || '0');
    callTypeHandler(meta, 'load', prefix, d);
    refreshEntityOptionLists(prefix);
    syncGaugeUi(tab);
    updateTilePreview(tab);
    return true;
  }

  let tileClipboard = null;
  function persistTileClipboard() { try { localStorage.setItem('tileClipboard', JSON.stringify(tileClipboard)); } catch (e) {} }
  function loadTileClipboard() {
    try {
      const raw = localStorage.getItem('tileClipboard');
      if (raw) tileClipboard = JSON.parse(raw);
    } catch (e) {
      tileClipboard = null;
    }
  }

  function collectTileFormData(tab) {
    const prefix = tab;
    const colorEl = document.getElementById(prefix + '_tile_color');
    const data = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: colorEl?.value || '#2A2A2A',
      bg_color_default: tileColorInputIsDefault(tab) ? '1' : '0',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
    if (isScreensaverTileTab(tab)) {
      data.background_opacity = document.getElementById('screensaver_tile_opacity')?.value || '0';
    }
    Object.assign(data, collectTypeFieldValues(tab));
    return data;
  }

  function applyTileFormData(tab, data) {
    if (!data) return;
    const prefix = tab;
    const typeValue = data.type || '0';
    const typeEl = document.getElementById(prefix + '_tile_type');
    syncTileTypeSelectValue(typeEl, typeValue);
    resetAllTypeFields(tab);
    updateTileType(tab);

    const titleEl = document.getElementById(prefix + '_tile_title');
    if (titleEl) titleEl.value = data.title || '';
    const iconEl = document.getElementById(prefix + '_tile_icon');
    if (iconEl) iconEl.value = data.icon || '';
    setTileColorInputFromSnapshot(tab, data);
    if (isScreensaverTileTab(tab)) {
      const opacity = document.getElementById('screensaver_tile_opacity');
      if (opacity) opacity.value = String(data.background_opacity ?? 0);
    }
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    if (spanWEl) spanWEl.value = data.span_w || '1';
    const spanHEl = document.getElementById(prefix + '_tile_span_h');
    if (spanHEl) spanHEl.value = data.span_h || '1';
    const meta = getTileTypeMeta(typeValue);
    callTypeHandler(meta, 'load', prefix, data);
    refreshEntityOptionLists(prefix);
    syncGaugeUi(tab);
  }

  function copyTile(tab) {
    if (currentTileIndex === -1 || currentTileTab !== tab) {
      showNotification(t('selectTileFirst'), false);
      return;
    }
    tileClipboard = collectTileFormData(tab);
    persistTileClipboard();
    showNotification(t('tileCopied'));
  }

  function pasteTile(tab) {
    if (currentTileIndex === -1 || currentTileTab !== tab) {
      showNotification(t('selectTileFirst'), false);
      return;
    }
    if (!tileClipboard) {
      showNotification(t('noCopiedTile'), false);
      return;
    }
    applyTileFormData(tab, tileClipboard);
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
    showNotification(t('tilePasted'));
  }

  function selectTile(index, tab) {
    if (currentTileTab &&
        typeof parkClimateMiniEditor === 'function') {
      parkClimateMiniEditor(currentTileTab);
    }
    currentTileIndex = index;
    currentTileTab = tab;
    persistSelectedTileState();
    document.querySelectorAll(
      '#tab-tiles-' + tab + ' .tile-grid > .tile')
      .forEach(t => delete t.dataset.selected);
    document.querySelectorAll(
      '.tile-grid > .tile, .screensaver-tile-grid > .tile')
      .forEach(t =>
        t.classList.remove(
          'active', 'drop-target', 'dragging'));
    const tileId = tab + '-tile-' + index;
    const selectedTile = document.getElementById(tileId);
    if (selectedTile) {
      selectedTile.dataset.selected = '1';
      selectedTile.classList.add('active');
    }
    const settingsId = tab + 'Settings';
    const settingsPanel = document.getElementById(settingsId);
    if (settingsPanel) {
      if (isScreensaverTileTab(tab)) {
        screensaverSelected = { kind: 'tile', index };
        document.getElementById('screensaverBackgroundSettings')?.classList.add('hidden');
        document.getElementById('screensaverClockSettings')?.classList.add('hidden');
        document.getElementById('screensaverGrid')?.classList.remove('selected-background');
        document.getElementById('screensaverClock')?.classList.remove('selected-clock');
      }
      const tileSpecific = settingsPanel.querySelector('.tile-specific-settings');
      if (tileSpecific) tileSpecific.classList.remove('hidden');
    }
    // Die Live-Handler sofort anbinden. Bisher geschah das erst nach dem
    // asynchronen GET der Kacheldaten; bis dahin reagierten Groesse, Position
    // und Stil in der Screensaver-Vorschau sichtbar nicht.
    setupLivePreview(tab);
    loadTileData(index, tab);
  }

  function titleFromOption(option) {
    if (!option) return '';
    const label = String(option.textContent || option.innerText || '').trim();
    if (!label.length) return '';
    const sep = label.indexOf(' - ');
    if (sep > 0) return label.substring(0, sep).trim();
    return label;
  }

  function titleFromEntity(entity) {
    let name = String(entity || '').trim();
    if (!name.length) return '';
    const dot = name.indexOf('.');
    if (dot !== -1) name = name.substring(dot + 1);
    name = name.replace(/[_-]+/g, ' ').trim();
    if (!name.length) return '';
    return name.replace(/\b\w/g, (m) => m.toUpperCase());
  }

  function maybeFillTitleFromEntity(tab, selectSuffix) {
    const prefix = tab;
    const titleInput = document.getElementById(prefix + '_tile_title');
    const selectEl = document.getElementById(prefix + selectSuffix);
    if (!titleInput || !selectEl) return;
    if (titleInput.value && titleInput.value.trim().length) return;
    const opt = selectEl.selectedOptions && selectEl.selectedOptions[0];
    let title = titleFromOption(opt);
    if (!title.length) title = titleFromEntity(selectEl.value);
    if (title.length) titleInput.value = title;
  }

  function setupLivePreview(tab) {
    const prefix = tab;
    const bindLive = (el, eventName, key, handler) => {
      if (!el) return;
      const flag = 'liveBound' + key + eventName;
      if (el.dataset[flag] === '1') return;
      el.addEventListener(eventName, handler);
      el.dataset[flag] = '1';
    };

    const titleInput = document.getElementById(prefix + '_tile_title');
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const colorInput = document.getElementById(prefix + '_tile_color');
    const colInput = document.getElementById(prefix + '_tile_col');
    const rowInput = document.getElementById(prefix + '_tile_row');
    const spanWInput = document.getElementById(prefix + '_tile_span_w');
    const spanHInput = document.getElementById(prefix + '_tile_span_h');
    const typeSelect = document.getElementById(prefix + '_tile_type');
    const opacityInput = isScreensaverTileTab(tab)
      ? document.getElementById('screensaver_tile_opacity') : null;
    const entitySelect = document.getElementById(prefix + '_sensor_entity');
      const unitInput = document.getElementById(prefix + '_sensor_unit');
      const decimalsInput = document.getElementById(prefix + '_sensor_decimals');
      const valueFontSelect = document.getElementById(prefix + '_sensor_value_font');
      const sensorPopupModeSelect = document.getElementById(prefix + '_sensor_popup_open_mode');
      const displayModeSelect = document.getElementById(prefix + '_sensor_display_mode');
      const gaugeMinInput = document.getElementById(prefix + '_sensor_gauge_min');
      const gaugeMaxInput = document.getElementById(prefix + '_sensor_gauge_max');
      const gaugeArcInput = document.getElementById(prefix + '_sensor_gauge_arc');
      const gaugeSizeInput = document.getElementById(prefix + '_sensor_gauge_size');
      const gaugeYOffsetInput = document.getElementById(prefix + '_sensor_gauge_y_offset');
      const valueYOffsetInput = document.getElementById(prefix + '_sensor_value_y_offset');
      const graphHeightInput = document.getElementById(prefix + '_sensor_graph_height');
      const weatherSelect = document.getElementById(prefix + '_weather_entity');
      const weatherPopupModeSelect = document.getElementById(prefix + '_weather_popup_open_mode');
      const energySelect = document.getElementById(prefix + '_energy_entity');
      const energyUnitInput = document.getElementById(prefix + '_energy_unit');
      const energyDecimalsInput = document.getElementById(prefix + '_energy_decimals');
      const energyValueFontSelect = document.getElementById(prefix + '_energy_value_font');
      const energyPopupModeSelect = document.getElementById(prefix + '_energy_popup_open_mode');
      const energyValueYOffsetInput = document.getElementById(prefix + '_energy_value_y_offset');
      const sceneInput = document.getElementById(prefix + '_scene_alias');
    const keyInput = document.getElementById(prefix + '_key_macro');
    const textInput = document.getElementById(prefix + '_text_value');
    const textFontInput = document.getElementById(prefix + '_text_value_font');
    const navigateSelect = document.getElementById(prefix + '_navigate_target');
    const switchSelect = document.getElementById(prefix + '_switch_entity');
    const switchStyleSelect = document.getElementById(prefix + '_switch_style');
    const switchPopupModeSelect = document.getElementById(prefix + '_switch_popup_open_mode');
    const mediaSelect = document.getElementById(prefix + '_media_entity');
    const climateSelect = document.getElementById(prefix + '_climate_entity');
    const climatePopupModeSelect = document.getElementById(prefix + '_climate_popup_open_mode');
    const climateSlotSelects = Array.from(
      { length: 6 },
      (_, index) => document.getElementById(
        prefix + '_climate_slot_' + index));
    const climateLayoutSelects = Array.from(
      { length: 6 },
      (_, index) => document.getElementById(
        prefix + '_climate_layout_' + index));
    const animationSelect = document.getElementById(prefix + '_animation_file');
    const animationFpsInput = document.getElementById(prefix + '_animation_fps');
    const animationFitSelect = document.getElementById(prefix + '_animation_fit');
    const animationZoomInput = document.getElementById(prefix + '_animation_zoom');
    const clockTimeCheck = document.getElementById(prefix + '_clock_show_time');
    const clockDateCheck = document.getElementById(prefix + '_clock_show_date');
    const clockTimeFontSelect = document.getElementById(prefix + '_clock_time_font');
    const clockDateFontSelect = document.getElementById(prefix + '_clock_date_font');
    const clockTimeFormatSelect = document.getElementById(prefix + '_clock_time_format');
    const clockDateFormatSelect = document.getElementById(prefix + '_clock_date_format');
    const counterInput = document.getElementById(prefix + '_counter_value');
    const settingsPanel = document.getElementById(prefix + 'Settings');

    bindLive(titleInput, 'input', 'tileTitle', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(iconInput, 'input', 'tileIcon', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(colorInput, 'input', 'tileColor', () => { markTileColorInputExplicit(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(opacityInput, 'input', 'tileOpacity', () => { updateTilePreview(tab); updateDraft(tab); });
    bindLive(opacityInput, 'change', 'tileOpacitySave', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(colInput, 'input', 'tileCol', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(rowInput, 'input', 'tileRow', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(spanWInput, 'input', 'tileSpanW', () => { syncClimateSlotFields(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(spanHInput, 'input', 'tileSpanH', () => { syncClimateSlotFields(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(typeSelect, 'change', 'tileType', () => {
      const tileEl = document.getElementById(tab + '-tile-' + currentTileIndex);
      const previousType = Number(tileEl?.dataset.type ?? 0);
      if (isScreensaverTileTab(tab) && previousType === 0 &&
          Number(typeSelect.value) !== 0 && opacityInput) {
        opacityInput.value = String(SCREENSAVER_TILE_DEFAULT_OPACITY);
      }
      updateTileType(tab);
      normalizeLayoutInputs(tab);
      updateLayoutFromInputs(tab);
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
    bindLive(entitySelect, 'change', 'sensorEntity', () => { maybeFillTitleFromSensor(tab); updateTilePreview(tab); updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(weatherSelect, 'change', 'weatherEntity', () => { maybeFillTitleFromWeather(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(weatherPopupModeSelect, 'change', 'weatherPopupMode', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(energySelect, 'change', 'energyEntity', () => {
      energySelect.dataset.configuredValue = energySelect.value || '';
      maybeFillTitleFromEnergy(tab);
      updateTilePreview(tab);
      updateEnergyValuePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
    bindLive(energyUnitInput, 'input', 'energyUnit', () => { updateEnergyValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(energyDecimalsInput, 'input', 'energyDecimals', () => { updateEnergyValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(energyValueFontSelect, 'change', 'energyValueFont', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(energyPopupModeSelect, 'change', 'energyPopupMode', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(energyValueYOffsetInput, 'input', 'energyValueYOffset', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(unitInput, 'input', 'sensorUnit', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(decimalsInput, 'input', 'sensorDecimals', () => { updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(valueFontSelect, 'change', 'sensorValueFont', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(sensorPopupModeSelect, 'change', 'sensorPopupMode', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(displayModeSelect, 'change', 'sensorDisplayMode', () => { syncGaugeUi(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(gaugeMinInput, 'input', 'sensorGaugeMin', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(gaugeMaxInput, 'input', 'sensorGaugeMax', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(gaugeArcInput, 'input', 'sensorGaugeArc', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(gaugeSizeInput, 'input', 'sensorGaugeSize', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(gaugeYOffsetInput, 'input', 'sensorGaugeYOffset', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(valueYOffsetInput, 'input', 'sensorValueYOffset', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(graphHeightInput, 'input', 'sensorGraphHeight', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(sceneInput, 'input', 'sceneAlias', () => { maybeFillTitleFromScene(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(keyInput, 'input', 'keyMacro', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(textInput, 'input', 'textValue', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(textFontInput, 'change', 'textFont', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(counterInput, 'input', 'counterValue', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(navigateSelect, 'change', 'navigateTarget', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(switchSelect, 'change', 'switchEntity', () => { maybeFillTitleFromSwitch(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(switchStyleSelect, 'change', 'switchStyle', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(switchPopupModeSelect, 'change', 'switchPopupMode', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(mediaSelect, 'change', 'mediaEntity', () => { maybeFillTitleFromMedia(tab); updateTilePreview(tab); updateMediaValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(climateSelect, 'change', 'climateEntity', () => {
      if (climateSelect.value) {
        climateSelect.dataset.configuredValue = climateSelect.value;
      } else {
        delete climateSelect.dataset.configuredValue;
      }
      maybeFillTitleFromEntity(tab, '_climate_entity');
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
    bindLive(climatePopupModeSelect, 'change', 'climatePopupMode', () => { updateDraft(tab); scheduleAutoSave(tab); });
    climateSlotSelects.forEach((select, index) => {
      bindLive(select, 'change', 'climateSlot' + index, () => {
        syncClimateSlotFields(tab);
        updateTilePreview(tab);
        updateDraft(tab);
        scheduleAutoSave(tab);
      });
    });
    climateLayoutSelects.forEach((select, index) => {
      bindLive(select, 'change', 'climateLayout' + index, () => {
        updateTilePreview(tab);
        updateDraft(tab);
        scheduleAutoSave(tab);
      });
    });
    bindLive(animationSelect, 'change', 'animationFile', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(animationFpsInput, 'input', 'animationFps', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(animationFitSelect, 'change', 'animationFit', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(animationZoomInput, 'input', 'animationZoom', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(clockTimeCheck, 'change', 'clockShowTime', () => {
      ensureClockSelection(prefix);
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
    bindLive(clockDateCheck, 'change', 'clockShowDate', () => {
      ensureClockSelection(prefix);
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    });
    if (clockTimeFontSelect) {
      const onClockTimeFontChanged = () => { updateClockValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); };
      bindLive(clockTimeFontSelect, 'change', 'clockTimeFont', onClockTimeFontChanged);
      bindLive(clockTimeFontSelect, 'input', 'clockTimeFont', onClockTimeFontChanged);
    }
    if (clockDateFontSelect) {
      const onClockDateFontChanged = () => { updateClockValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); };
      bindLive(clockDateFontSelect, 'change', 'clockDateFont', onClockDateFontChanged);
      bindLive(clockDateFontSelect, 'input', 'clockDateFont', onClockDateFontChanged);
    }
    if (clockTimeFormatSelect) {
      const onClockTimeFormatChanged = () => { updateClockValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); };
      bindLive(clockTimeFormatSelect, 'change', 'clockTimeFormat', onClockTimeFormatChanged);
      bindLive(clockTimeFormatSelect, 'input', 'clockTimeFormat', onClockTimeFormatChanged);
    }
    if (clockDateFormatSelect) {
      const onClockDateFormatChanged = () => { updateClockValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); };
      bindLive(clockDateFormatSelect, 'change', 'clockDateFormat', onClockDateFormatChanged);
      bindLive(clockDateFormatSelect, 'input', 'clockDateFormat', onClockDateFormatChanged);
    }
    if (settingsPanel && settingsPanel.dataset.clockLiveBound !== '1') {
      const delegatedClockRefresh = (e) => {
        const target = e && e.target;
        if (!target || !target.id) return;
        if (
          target.id === (prefix + '_clock_show_time') ||
          target.id === (prefix + '_clock_show_date')
        ) {
          ensureClockSelection(prefix);
          updateTilePreview(tab);
          updateDraft(tab);
          scheduleAutoSave(tab);
          return;
        }
        if (
          target.id === (prefix + '_clock_time_font') ||
          target.id === (prefix + '_clock_date_font') ||
          target.id === (prefix + '_clock_time_format') ||
          target.id === (prefix + '_clock_date_format')
        ) {
          updateClockValuePreview(tab);
          updateDraft(tab);
          scheduleAutoSave(tab);
          return;
        }
      };
      settingsPanel.addEventListener('change', delegatedClockRefresh);
      settingsPanel.addEventListener('input', delegatedClockRefresh);
      settingsPanel.dataset.clockLiveBound = '1';
    }
  }

  function updateTilePreview(tab) {
    if (currentTileIndex === -1) return;
    if (typeof parkClimateMiniEditor === 'function') {
      // Eine Live-Aenderung baut den Preview-Inhalt neu auf. Die Auswahl
      // des bearbeiteten Mini-Tiles muss diesen Render-Zyklus ueberleben.
      parkClimateMiniEditor(tab, true);
    }
    const prefix = tab;
    const tileId = tab + '-tile-' + currentTileIndex;
    const tileElem = document.getElementById(tileId);
    if (!tileElem) return;

    const wasActive = currentTileTab === tab && currentTileIndex >= 0;
    const typeWas = tileElem.dataset.type || '0';
    const title = document.getElementById(prefix + '_tile_title').value;
    const color = document.getElementById(prefix + '_tile_color').value;
    const type = document.getElementById(prefix + '_tile_type').value;
    const meta = getTileTypeMeta(type);
    const iconInput = document.getElementById(prefix + '_tile_icon');
    const switchStyle = document.getElementById(prefix + '_switch_style')?.value || '0';
    const isEnergyType = type === '14';
    const sensorValueFont = isEnergyType
      ? (document.getElementById(prefix + '_energy_value_font')?.value || '0')
      : (document.getElementById(prefix + '_sensor_value_font')?.value || '0');
    const sensorValueClass = getSensorValueFontClass(sensorValueFont);
    const previewKind = meta.preview || 'none';
    const sensorEntity = document.getElementById(prefix + '_sensor_entity')?.value || '';
    const energyEntity = document.getElementById(prefix + '_energy_entity')?.value || '';
    const weatherEntity = document.getElementById(prefix + '_weather_entity')?.value || '';
    const switchEntity = document.getElementById(prefix + '_switch_entity')?.value || '';
    const mediaEntity = document.getElementById(prefix + '_media_entity')?.value || '';
    const climateEntity = document.getElementById(prefix + '_climate_entity')?.value || '';
    const iconEntity = (previewKind === 'sensor')
      ? (isEnergyType ? energyEntity : sensorEntity)
      : (previewKind === 'switch'
        ? switchEntity
        : (previewKind === 'weather'
          ? weatherEntity
          : (previewKind === 'media'
            ? mediaEntity
            : (previewKind === 'climate' ? climateEntity : ''))));
    const rawIcon = iconInput ? iconInput.value : '';
    let iconName = resolveIconName(
      rawIcon,
      iconEntity,
      sensorMetaCache.icons);
    let climatePreviewState = null;
    if (previewKind === 'climate') {
      climatePreviewState = parseClimatePreviewPayload(
        climateEntity ? (sensorMetaCache.values[climateEntity] ?? '') : '');
      if (!normalizeMdiIconName(rawIcon) &&
          !isExplicitlyDisabledValue(rawIcon)) {
        iconName = climatePreviewIcon(climatePreviewState, iconName);
      }
    }

    tileElem.className = 'tile';
    if (meta.css) tileElem.classList.add(meta.css);
    if (type === '5' && switchStyle === '1') tileElem.classList.add('switch-toggle');
    tileElem.style.background = '';
    tileElem.dataset.type = type;

    if (type === '0') {
      tileElem.classList.add('empty');
      tileElem.style.background = 'transparent';
      tileElem.innerHTML = '';
      if (wasActive) tileElem.classList.add('active');
      updateLayoutFromInputs(tab);
      return;
    }

    const defaultBg = meta.defaultBg || '#353535';
    if (tileColorInputIsDefault(tab)) {
      const colorInput = document.getElementById(prefix + '_tile_color');
      if (colorInput) colorInput.value = defaultBg;
    }
    const tileBg = tileColorInputIsDefault(tab) ? defaultBg : (color || defaultBg);
    if (isScreensaverTileTab(tab)) {
      const opacity = clampInt(
        document.getElementById('screensaver_tile_opacity')?.value,
        0, 255, 0);
      tileElem.style.background = tileBg + opacity.toString(16).padStart(2, '0');
    } else {
      tileElem.style.background = tileBg;
    }
    tileElem.style.removeProperty('--switch-knob-color');
    tileElem.style.removeProperty('--switch-on-color');
    if (type === '5' && switchStyle === '1') {
      tileElem.style.setProperty('--switch-knob-color', tileBg);
      tileElem.style.setProperty('--switch-on-color', '#3B82F6');
    }

    let html = '';

    if (iconName) {
      const iconStyle = previewKind === 'climate'
        ? ' style="color:' + climatePreviewColor(climatePreviewState) + '"'
        : '';
      html += '<i class="mdi mdi-' + iconName + ' tile-icon"' + iconStyle + '></i>';
    }

    if (title) {
      html += '<div class="tile-title" id="' + tileId + '-title">' + title + '</div>';
    }

    if (previewKind === 'weather') {
      html += '<div class="tile-ghost-icon"><i class="mdi mdi-weather-partly-cloudy"></i></div>';
    }
    if (previewKind === 'media') {
      html += '<div class="tile-ghost-icon"><i class="mdi mdi-music"></i></div>';
    }
    if (previewKind === 'climate') {
      const climateSpanW = document.getElementById(
        prefix + '_tile_span_w')?.value || 1;
      const climateSpanH = document.getElementById(
        prefix + '_tile_span_h')?.value || 1;
      html += climatePreviewSlots(
        climatePreviewState, climateSpanW, climateSpanH,
        currentClimateSlotConfig(tab),
        currentClimateTargetLayouts(tab),
        currentClimateGeometry(tab));
    }

    if (previewKind === 'sensor') {
      const entitySelect = document.getElementById(prefix + (isEnergyType ? '_energy_entity' : '_sensor_entity'));
      const unitInput = document.getElementById(prefix + (isEnergyType ? '_energy_unit' : '_sensor_unit'));
      const entity = entitySelect ? entitySelect.value : '';
      const unit = resolveUnitValue(unitInput ? unitInput.value : '', entity, sensorMetaCache.units);
      html += '<div class="tile-value ' + sensorValueClass + '" id="' + tileId + '-value">--';
      if (unit) html += '<span class="tile-unit">' + unit + '</span>';
      html += '</div>';
      if (entity) {
        tileElem.innerHTML = html;
        if (wasActive) tileElem.classList.add('active');
        if (isEnergyType) updateEnergyValuePreview(tab);
        else updateSensorValuePreview(tab);
      }
    }

    if (previewKind === 'clock') {
      const flags = getClockFlagsFromInputs(prefix);
      const clockTimeFont = document.getElementById(prefix + '_clock_time_font')?.value || '40';
      const clockDateFont = Math.min(72,
        Number(document.getElementById(prefix + '_clock_date_font')?.value || 20));
      const clockTimeFormat = document.getElementById(prefix + '_clock_time_format')?.value || '0';
      const clockDateFormat = document.getElementById(prefix + '_clock_date_format')?.value || '0';
      if (flags & 1) html += '<div class="tile-clock-time" ' + getClockPreviewTextStyle(clockTimeFont, 40, '#fff') + '>' + getClockPreviewTime(clockTimeFormat) + '</div>';
      if (flags & 2) html += '<div class="tile-clock-date" ' + getClockPreviewTextStyle(clockDateFont, 24, '#fff') + '>' + getClockPreviewDate(clockDateFormat) + '</div>';
    }

    if (previewKind === 'text') {
      const textValue = document.getElementById(prefix + '_text_value')?.value || '';
      if (textValue) {
        const textFont = document.getElementById(prefix + '_text_value_font')?.value || '0';
        const textClass = getSensorValueFontClass(textFont);
        html += '<div class="tile-text ' + textClass + '">' + textValue + '</div>';
      }
    }

    if (previewKind === 'counter') {
      const counterVal = document.getElementById(prefix + '_counter_value')?.value || '0';
      html += '<div class="tile-counter-value">' + counterVal + '</div>';
    }

    if (previewKind === 'switch' && switchStyle === '1') {
      html += '<div class="tile-switch" id="' + tileId + '-switch"><div class="tile-switch-knob"></div></div>';
    }

    html += getTileResizeHandlesHtml(type);
    tileElem.innerHTML = html;
    if (wasActive) tileElem.classList.add('active');
    if (typeWas !== type && wasActive) {
      tileElem.classList.add('active');
      const settingsId = tab + 'Settings';
      document.getElementById(settingsId)?.classList.remove('hidden');
    }
    if (type === '5') updateSwitchValuePreview(tab);
    updateLayoutFromInputs(tab);
    if (previewKind === 'climate' &&
        typeof mountClimateMiniEditor === 'function') {
      mountClimateMiniEditor(tab);
      syncClimateSlotFields(tab);
    }
  }

  function loadTileData(index, tab) {
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined) return;
    fetch('/api/tiles?folder=' + encodeURIComponent(folderId) + '&index=' + index)
      .then(res => res.json())
      .then(data => {
        if (!tilesData[tab]) tilesData[tab] = [];
        tilesData[tab][index] = data;
        const draftForTile = drafts[tab] && drafts[tab][index];
        if (draftForTile && draftForTile._dirty) {
          applySnapshotToTileData(tab, index, draftForTile);
        }
        if (currentTileTab !== tab || currentTileIndex !== index) return;
        const prefix = tab;
        syncTileTypeSelectValue(document.getElementById(prefix + '_tile_type'), data.type || 0);
        applyFolderTypeLock(prefix, Number(data.type) === 4 && data.folder_empty === false);
        resetAllTypeFields(tab);
        updateTileType(tab);
        document.getElementById(prefix + '_tile_title').value = data.title || '';
        document.getElementById(prefix + '_tile_icon').value = data.icon_name || '';
        const colorMeta = getTileTypeMeta(data.type || 0);
        setTileColorInputFromStored(tab, data.bg_color, colorMeta.defaultBg || '#2A2A2A');
        if (isScreensaverTileTab(tab)) {
          const opacity = document.getElementById('screensaver_tile_opacity');
          if (opacity) opacity.value = String(data.background_opacity ?? 0);
        }
        const colEl = document.getElementById(prefix + '_tile_col');
        const rowEl = document.getElementById(prefix + '_tile_row');
        const spanWEl = document.getElementById(prefix + '_tile_span_w');
        const spanHEl = document.getElementById(prefix + '_tile_span_h');
        if (colEl && rowEl && spanWEl && spanHEl) {
          const fallbackLayout = (data.type === 0) ? getTileElementLayout(tab, index) : null;
          const layoutInput = {
            col: data.col,
            row: data.row,
            span_w: data.span_w,
            span_h: data.span_h
          };
          if (fallbackLayout) {
            layoutInput.col = fallbackLayout.col;
            layoutInput.row = fallbackLayout.row;
            layoutInput.span_w = fallbackLayout.span_w;
            layoutInput.span_h = fallbackLayout.span_h;
          }
          const layout = normalizeTileLayout(layoutInput, index, tab);
          colEl.value = String(layout.col + 1);
          rowEl.value = String(layout.row + 1);
          spanWEl.value = String(layout.span_w);
          spanHEl.value = String(layout.span_h);
        }
        const meta = colorMeta;
        callTypeHandler(meta, 'load', prefix, data);
        refreshEntityOptionLists(prefix);
        syncGaugeUi(tab);
        const tileElem = document.getElementById(tab + '-tile-' + index);
        if (tileElem) {
          tileElem.classList.toggle('active', currentTileTab === tab && currentTileIndex === index);
        }
        const draft = (drafts[tab] || {})[index];
        if (draft && draft._dirty) {
          applyDraft(tab, index);
        } else {
          if (draft && data.type === 0 && draft.type !== data.type) clearDraft(tab, index);
          updateTilePreview(tab);
        }
        setupLivePreview(tab);
        restoreCurrentTileSelectionUi();
    });
  }

  function getCurrentTileType(tab) {
    const tiles = getTilesData(tab);
    if (tiles && currentTileIndex >= 0 && tiles[currentTileIndex]) {
      return String(tiles[currentTileIndex].type ?? '0');
    }
    const typeEl = document.getElementById(tab + '_tile_type');
    return typeEl ? String(typeEl.value) : '0';
  }

  function isLockedTileType(typeValue) {
    const meta = getTileTypeMeta(typeValue);
    return !!meta.locked;
  }

  function applySpecialTileUiState(tab) {
    const prefix = tab;
    const typeEl = document.getElementById(prefix + '_tile_type');
    const navSelect = document.getElementById(prefix + '_navigate_target');
    const noteEl = document.getElementById(prefix + '_navigate_note');
    const typeValue = typeEl ? String(typeEl.value) : '0';
    const meta = getTileTypeMeta(typeValue);
    const locked = !!meta.locked;
    if (typeEl) typeEl.disabled = locked;
    if (navSelect) navSelect.disabled = (!meta.fields || meta.fields !== 'navigate' || locked);
    if (noteEl) {
      if (typeValue === '7') noteEl.textContent = t('settingsTileFixed');
      else if (typeValue === '8') noteEl.textContent = t('backTileFixed');
      else noteEl.textContent = '';
    }
  }

  function updateTileType(tab) {
    const prefix = tab;
    const typeEl = document.getElementById(prefix + '_tile_type');
    let typeValue = typeEl ? typeEl.value : '0';
    const mediaType = Number(typeValue) === MEDIA_TILE_TYPE;
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    const spanHEl = document.getElementById(prefix + '_tile_span_h');
    if (spanWEl) {
      spanWEl.min = String(mediaType ? Math.min(MEDIA_TILE_MIN_SPAN, GRID_COLS) : 1);
      spanWEl.max = String(mediaType ? Math.min(MEDIA_TILE_MAX_SPAN, GRID_COLS) : GRID_COLS);
    }
    if (spanHEl) {
      const availableRows = GRID_ROWS - firstAllowedGridRow(tab);
      spanHEl.min = String(mediaType ? Math.min(MEDIA_TILE_MIN_SPAN, availableRows) : 1);
      spanHEl.max = String(mediaType
        ? Math.min(MEDIA_TILE_MAX_SPAN, availableRows)
        : GRID_ROWS);
    }
    document.querySelectorAll('#' + prefix + 'Settings .type-fields').forEach(f => f.classList.remove('show'));
    const meta = getTileTypeMeta(typeValue);
    if (meta.fields) {
      const fieldsEl = document.getElementById(prefix + '_' + meta.fields + '_fields');
      if (fieldsEl) fieldsEl.classList.add('show');
    }
    if (meta.onSelect) {
      callTypeHandler(meta, 'onSelect', tab);
    }
    if (Number(typeValue) === 17) {
      syncClimateSlotFields(tab);
    }
    syncGaugeUi(tab);
    applySpecialTileUiState(tab);
  }

  function showNotification(message, success = true) {
    const notification = document.getElementById('notification');
    notification.textContent = message;
    notification.style.background = success ? '#43a047' : '#ef4444';
    notification.classList.add('show');
    setTimeout(() => { notification.classList.remove('show'); }, 3000);
  }

  function scheduleAutoSave(tab, tileIndexOverride = null) {
    const tileIndex = tileIndexOverride !== null ? tileIndexOverride : currentTileIndex;
    if (tileIndex === -1) return;
    const timerKey = tab + ':' + tileIndex;
    if (autoSaveTimers[timerKey]) clearTimeout(autoSaveTimers[timerKey]);
    autoSaveTimers[timerKey] = setTimeout(() => {
      delete autoSaveTimers[timerKey];
      saveTile(tab, true, tileIndex);
    }, 250);
  }

  function resetAllTypeFields(tab) {
    const metas = Object.values(TILE_TYPE_REGISTRY || {});
    metas.forEach(meta => callTypeHandler(meta, 'reset', tab));
  }

  function applyFolderTypeLock(tab, locked) {
    const sel = document.getElementById(tab + '_tile_type');
    if (sel) {
      for (const opt of sel.options) {
        // Ordner behalten und Leeren/Loeschen bleiben erlaubt; alle anderen
        // Typen sind gesperrt, solange der Ordner noch Kacheln enthaelt.
        opt.disabled = locked && opt.value !== '4' && opt.value !== '0';
      }
    }
    const hint = document.getElementById(tab + '_tile_type_hint');
    if (hint) hint.classList.toggle('hidden', !locked);
  }

  function resetTile(tab) {
    if (currentTileIndex === -1) return;
    const tileType = getCurrentTileType(tab);
    if (isLockedTileType(tileType)) {
      showNotification(t('tileCannotDelete'), false);
      return;
    }
    const prefix = tab;
    document.getElementById(prefix + '_tile_type').value = '0';
    document.getElementById(prefix + '_tile_title').value = '';
    document.getElementById(prefix + '_tile_icon').value = '';
    setTileColorInputFromStored(tab, 0, '#2A2A2A');
    if (isScreensaverTileTab(tab)) {
      const opacity = document.getElementById('screensaver_tile_opacity');
      if (opacity) opacity.value = '0';
    }
    resetAllTypeFields(tab);
    syncGaugeUi(tab);
    updateTileType(tab);
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  function deleteFolder(tab) {
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined || folderId === 0) {
      showNotification(t('folderCannotDelete'), false);
      return;
    }
    const tabEl = document.getElementById('tab-tiles-' + tab);
    const folderName = tabEl ? (tabEl.dataset.folderName || t('folderPrefix').trim()) : t('folderPrefix').trim();
    if (!confirm(tf('deleteFolderConfirm', { name: folderName }))) {
      return;
    }
    const formData = new FormData();
    formData.append('folder_id', folderId);
    fetch('/api/folders/delete', { method: 'POST', body: formData })
      .then(res => res.json())
      .then(data => {
        if (data.success) {
          showNotification(t('folderDeleted'));
          setTimeout(() => location.reload(), 500);
        } else {
          showNotification(data.error || t('deleteFailed'), false);
        }
      })
      .catch(() => showNotification(t('networkError'), false));
  }

  function saveTile(tab, silent = false, tileIndexOverride = null) {
    const tileIndex = tileIndexOverride !== null ? tileIndexOverride : currentTileIndex;
    if (tileIndex === -1) return;
    const saveKey = getTileSaveKey(tab, tileIndex);
    if (saveInFlightByTile[saveKey]) {
      queueSaveAfterFlight(tab, tileIndex, silent);
      return;
    }
    const tiles = getTilesData(tab);
    const previousTile = Array.isArray(tiles) ? tiles[tileIndex] : null;
    const previousType = previousTile ? Number(previousTile.type) : NaN;
    const snapshot = getTileSnapshotForSave(tab, tileIndex);
    if (!snapshot) return;
    const formData = new FormData();
    const layout = normalizeSnapshotLayout(snapshot, tileIndex, tab);
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined) {
      showNotification(t('folderNotFound'), false);
      return;
    }
    formData.append('folder', folderId);
    formData.append('index', tileIndex);
    formData.append('col', layout.col);
    formData.append('row', layout.row);
    formData.append('span_w', layout.span_w);
    formData.append('span_h', layout.span_h);
    formData.append('type', snapshot.type || '0');
    formData.append('title', snapshot.title || '');
    formData.append('icon_name', snapshot.icon || '');
    if (snapshotBgColorIsDefault(snapshot)) {
      formData.append('bg_color_default', '1');
    } else {
      formData.append('bg_color', hexToRgb(snapshot.color || '#2A2A2A'));
    }
    const typeValue = String(snapshot.type || '0');
    for (const [key, value] of Object.entries(snapshot)) {
      if (key === '_dirty' || key === '_rev' || key === 'type' || key === 'title' || key === 'icon' || key === 'color' || key === 'bg_color_default' || key === 'col' || key === 'row' || key === 'span_w' || key === 'span_h') continue;
      formData.append(key, value);
    }
    applySnapshotToTileData(tab, tileIndex, snapshot);
    const requestId = ++saveRequestSeq;
    const draftRev = Number(snapshot._rev || 0);
    markLatestSaveRequest(tab, tileIndex, requestId);
    saveInFlightByTile[saveKey] = true;
    fetch('/api/tiles', { method:'POST', body:formData })
      .then(res => res.json())
      .then(data => {
        if (!isLatestSaveRequest(tab, tileIndex, requestId)) return;
        if (data.success) {
          if (!silent) showNotification(t('tileSaved'));
          const currentDraft = drafts[tab] && drafts[tab][tileIndex];
          if (currentDraft && currentDraft._dirty && Number(currentDraft._rev || 0) !== draftRev) {
            queueSaveAfterFlight(tab, tileIndex, true);
            return;
          }
          clearDraft(tab, tileIndex);
          if (!silent) loadSensorValues(true);
          if (typeValue === '4') {
            const resolvedNavTarget = String((data && data.navigate_target !== undefined && data.navigate_target !== null)
              ? data.navigate_target
              : (snapshot.navigate_target || '0'));
            snapshot.navigate_target = resolvedNavTarget;
            if (tilesData[tab] && tilesData[tab][tileIndex]) {
              tilesData[tab][tileIndex].navigate_target = parseInt(resolvedNavTarget, 10) || 0;
            }
            const navTargetNum = parseInt(resolvedNavTarget, 10);
            const titleVal = snapshot.title || '';
            const iconVal = snapshot.icon || '';
            ensureFolderTabUi(navTargetNum, titleVal, iconVal).then(ok => {
              restoreCurrentTileSelectionUi();
              if (!ok) {
                persistSelectedTileState();
                setTimeout(() => location.reload(), 400);
              }
            });
          }
          if (previousType === 4 && typeValue === '0') {
            persistSelectedTileState();
            setTimeout(() => location.reload(), 400);
          }
        } else {
          showNotification(data.error || t('unknownError'), false);
        }
      })
      .catch(() => {
        if (!isLatestSaveRequest(tab, tileIndex, requestId)) return;
        showNotification(t('networkErrorSave'), false);
      })
      .finally(() => {
        delete saveInFlightByTile[saveKey];
        flushQueuedSave(tab, tileIndex);
      });
  }

  function downloadJsonFile(filename, content) {
    const blob = new Blob([content], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    setTimeout(() => URL.revokeObjectURL(url), 5000);
  }

  function parseBgColorValue(value) {
    if (value === undefined || value === null) return 0;
    if (typeof value === 'string') {
      let v = value.trim();
      if (!v.length) return 0;
      if (v.startsWith('#')) return parseInt(v.substring(1), 16) || 0;
      if (v.startsWith('0x') || v.startsWith('0X')) return parseInt(v, 16) || 0;
    }
    const num = parseInt(value, 10);
    return isNaN(num) ? 0 : num;
  }

  function buildScreensaverExportConfig(data) {
    return {
      version: Number(data?.version || 1),
      use_wallpapers: !!data?.use_wallpapers,
      shuffle: !!data?.shuffle,
      tile_shadow: !!data?.tile_shadow,
      tile_border: data?.tile_border !== false,
      show_time: !!data?.show_time,
      show_date: !!data?.show_date,
      show_weekday: !!data?.show_weekday,
      clock_shadow: !!data?.clock_shadow,
      time_format: Number(data?.time_format || 0),
      date_format: Number(data?.date_format || 0),
      time_alignment: Math.round(ssClamp(data?.time_alignment ?? 1, 0, 2)),
      date_alignment: Math.round(ssClamp(data?.date_alignment ?? 1, 0, 2)),
      time_font_size: Number(data?.time_font_size || 48),
      date_font_size: Number(data?.date_font_size || 28),
      clock_x: Number(data?.clock_x ?? 500),
      clock_y: Number(data?.clock_y ?? 350),
      duration_seconds: Number(data?.duration_seconds ?? 15),
      wallpapers: Array.isArray(data?.wallpapers) ? data.wallpapers.map(wallpaper => ({
        file_name: String(wallpaper?.file_name || ''),
        enabled: !!wallpaper?.enabled,
        focus_x: Number(wallpaper?.focus_x ?? 500),
        focus_y: Number(wallpaper?.focus_y ?? 500),
        zoom: Number(wallpaper?.zoom ?? 1000)
      })) : []
    };
  }

  async function exportTilesConfig() {
    try {
      const tabs = tileTabs.filter(tab => !isScreensaverTileTab(tab));
      const screensaverFolderId = getFolderIdForTab('screensaver');
      if (screensaverFolderId === undefined) throw new Error('Screensaver grid unavailable');
      const tileRequests = tabs.map(tab => {
        const folderId = getFolderIdForTab(tab);
        if (folderId === undefined) return Promise.resolve([]);
        return fetch('/api/tiles?folder=' + encodeURIComponent(folderId))
          .then(res => res.json())
          .catch(() => []);
      });
      const screensaverConfigRequest = fetch('/api/screensaver').then(async res => {
        const data = await res.json();
        if (!res.ok || !data?.success) throw new Error('Screensaver config export failed');
        return data;
      });
      const screensaverGridRequest = fetch(
        '/api/tiles?folder=' + encodeURIComponent(screensaverFolderId)
      ).then(async res => {
        const data = await res.json();
        if (!res.ok || !Array.isArray(data)) throw new Error('Screensaver grid export failed');
        return data;
      });
      const [tilesLists, screensaverData, screensaverGrid] = await Promise.all([
        Promise.all(tileRequests), screensaverConfigRequest, screensaverGridRequest
      ]);
      const folders = tabs.map(tab => {
        const folderId = getFolderIdForTab(tab);
        const tabEl = document.getElementById('tab-tiles-' + tab);
        const parentId = tabEl ? parseInt(tabEl.dataset.folderParent || '0', 10) : 0;
        return {
          id: folderId,
          parent_id: isNaN(parentId) ? 0 : parentId,
          name: tabEl ? (tabEl.dataset.folderName || '') : '',
          icon_name: tabEl ? (tabEl.dataset.folderIcon || '') : ''
        };
      }).filter(entry => entry.id !== undefined);

      const grids = {};
      tabs.forEach((tab, idx) => {
        const folderId = getFolderIdForTab(tab);
        if (folderId === undefined) return;
        grids[String(folderId)] = Array.isArray(tilesLists[idx]) ? tilesLists[idx] : [];
      });

      const payload = {
        version: 3,
        exported_at: new Date().toISOString(),
        folders: folders,
        grids: grids,
        screensaver: {
          version: 2,
          config: buildScreensaverExportConfig(screensaverData),
          grid: screensaverGrid,
          source_layout: {
            screen_width: Number(screensaverData.screen_width || 0),
            screen_height: Number(screensaverData.screen_height || 0),
            grid_cols: Number(screensaverData.grid_cols || 0),
            grid_rows: Number(screensaverData.grid_rows || 0)
          }
        }
      };
      const ts = new Date().toISOString().replace(/[:.]/g, '-');
      downloadJsonFile('waveshare_tiles_' + ts + '.json', JSON.stringify(payload, null, 2));
      showNotification(t('exportCreated'));
    } catch (e) {
      showNotification(t('exportFailed'), false);
    }
  }

  function triggerTilesImport(tab) {
    const input = document.getElementById(tab + '_tile_import');
    if (!input) return;
    input.value = '';
    input.click();
  }

  function importTilesConfig(tab, files) {
    if (!files || !files.length) return;
    const file = files[0];
    const reader = new FileReader();
    reader.onload = async () => {
      try {
        const payload = JSON.parse(reader.result);
        await importTilesPayload(payload);
      } catch (e) {
        showNotification(t('importInvalidJson'), false);
      }
    };
    reader.onerror = () => showNotification(t('importFailed'), false);
    reader.readAsText(file);
  }

  function normalizeImportFolderName(value) {
    return String(value || '').trim().toLowerCase();
  }

  function normalizeImportFolderIcon(value) {
    return normalizeIconName(value || '');
  }

  async function fetchFoldersForImport() {
    const res = await fetch('/api/folders');
    if (!res.ok) throw new Error('Folder fetch failed');
    const data = await res.json();
    return Array.isArray(data) ? data : [];
  }

  async function fetchTilesForImport(folderId) {
    const res = await fetch('/api/tiles?folder=' + encodeURIComponent(folderId));
    if (!res.ok) throw new Error('Tile fetch failed');
    const data = await res.json();
    return Array.isArray(data) ? data : [];
  }

  function buildEmptyImportTile(index) {
    return {
      type: 0,
      title: '',
      icon_name: '',
      bg_color: 0,
      col: index % GRID_COLS,
      row: Math.floor(index / GRID_COLS),
      span_w: 1,
      span_h: 1
    };
  }

  function updateFolderImportMap(sourceFolders, targetFolders, sourceToTarget) {
    let changed = false;
    sourceFolders.forEach(sourceFolder => {
      const sourceId = parseInt(sourceFolder && sourceFolder.id, 10);
      const sourceParentId = parseInt(sourceFolder && sourceFolder.parent_id, 10);
      if (isNaN(sourceId) || sourceId === 0 || isNaN(sourceParentId)) return;
      const targetParentId = sourceToTarget[sourceParentId];
      if (targetParentId === undefined) return;
      const sourceName = normalizeImportFolderName(sourceFolder.name);
      const sourceIcon = normalizeImportFolderIcon(sourceFolder.icon_name);
      const match = targetFolders.find(targetFolder =>
        Number(targetFolder.parent_id) === Number(targetParentId) &&
        normalizeImportFolderName(targetFolder.name) === sourceName &&
        normalizeImportFolderIcon(targetFolder.icon_name) === sourceIcon
      );
      if (match && sourceToTarget[sourceId] !== Number(match.id)) {
        sourceToTarget[sourceId] = Number(match.id);
        changed = true;
      }
    });
    return changed;
  }

  async function replaceFolderGridForImport(folderId, sourceTiles, systemType, sourceToTarget = null) {
    const currentTiles = await fetchTilesForImport(folderId);
    const sourceList = Array.isArray(sourceTiles) ? sourceTiles.slice(0, GRID_COLS * GRID_ROWS) : [];
    const currentSystemIndex = currentTiles.findIndex(tile => Number(tile && tile.type) === systemType);
    const sourceSystemTile = sourceList.find(tile => Number(tile && tile.type) === systemType) || null;

    for (let i = 0; i < (GRID_COLS * GRID_ROWS); i++) {
      if (i === currentSystemIndex) continue;
      await postTile(folderId, i, buildEmptyImportTile(i), sourceToTarget);
    }

    if (currentSystemIndex >= 0 && sourceSystemTile) {
      await postTile(folderId, currentSystemIndex, sourceSystemTile, sourceToTarget);
    }

    const availableIndices = [];
    for (let i = 0; i < (GRID_COLS * GRID_ROWS); i++) {
      if (i === currentSystemIndex) continue;
      availableIndices.push(i);
    }

    const nonSystemTiles = sourceList.filter(tile => Number(tile && tile.type) !== systemType);
    for (let i = 0; i < nonSystemTiles.length && i < availableIndices.length; i++) {
      await postTile(folderId, availableIndices[i], nonSystemTiles[i] || {}, sourceToTarget);
    }
  }

  function prepareScreensaverTilesForImport(sourceTiles, sourceLayout) {
    const tileCount = GRID_COLS * GRID_ROWS;
    const sourceCols = Number(sourceLayout?.grid_cols || 0);
    const sourceRows = Number(sourceLayout?.grid_rows || 0);
    const sameLayout = sourceCols === GRID_COLS && sourceRows === GRID_ROWS;
    const sourceEntries = (Array.isArray(sourceTiles) ? sourceTiles : [])
      .map((tile, index) => ({ tile: tile || {}, sourceIndex: index }))
      .filter(entry => Number(entry.tile.type || 0) !== 0);

    if (sameLayout) {
      return sourceEntries.slice(0, tileCount).map(entry => ({
        targetIndex: entry.sourceIndex,
        tile: entry.tile
      }));
    }

    // Bei einem Import zwischen 7xN und 4xN wird die relative Anordnung in
    // den beiden unteren Reihen beibehalten und auf das Zielraster gepackt.
    const firstTargetRow = Math.max(0, GRID_ROWS - 2);
    const firstSourceRow = sourceRows > 1 ? sourceRows - 2 : 0;
    const occupied = Array.from({ length: GRID_ROWS }, () => Array(GRID_COLS).fill(false));
    const prepared = [];
    for (const entry of sourceEntries) {
      if (prepared.length >= tileCount) throw new Error('Screensaver grid does not fit target device');
      const tile = entry.tile;
      const mediaTile = Number(tile.type) === MEDIA_TILE_TYPE;
      let spanW = Math.max(1, Number(tile.span_w || 1));
      let spanH = Math.max(1, Number(tile.span_h || 1));
      if (mediaTile) {
        spanW = Math.max(MEDIA_TILE_MIN_SPAN, spanW);
        spanH = Math.max(MEDIA_TILE_MIN_SPAN, spanH);
      }
      spanW = Math.min(spanW, GRID_COLS, mediaTile ? MEDIA_TILE_MAX_SPAN : GRID_COLS);
      spanH = Math.min(spanH, 2, mediaTile ? MEDIA_TILE_MAX_SPAN : 2);

      const sourceSpanW = Math.max(1, Number(tile.span_w || 1));
      const sourceColRange = Math.max(0, sourceCols - sourceSpanW);
      const targetColRange = Math.max(0, GRID_COLS - spanW);
      const relativeCol = sourceColRange > 0
        ? Math.max(0, Math.min(1, Number(tile.col || 0) / sourceColRange))
        : 0;
      const desiredCol = Math.round(relativeCol * targetColRange);
      const sourceRowOffset = Math.max(0, Math.min(1, Number(tile.row || 0) - firstSourceRow));
      const desiredRow = Math.min(GRID_ROWS - spanH, firstTargetRow + sourceRowOffset);

      let best = null;
      for (let row = firstTargetRow; row <= GRID_ROWS - spanH; row++) {
        for (let col = 0; col <= GRID_COLS - spanW; col++) {
          let free = true;
          for (let y = row; y < row + spanH && free; y++) {
            for (let x = col; x < col + spanW; x++) {
              if (occupied[y][x]) { free = false; break; }
            }
          }
          if (!free) continue;
          const score = Math.abs(row - desiredRow) * (GRID_COLS + 1) + Math.abs(col - desiredCol);
          if (!best || score < best.score) best = { row, col, score };
        }
      }
      if (!best) throw new Error('Screensaver grid does not fit target device');
      for (let y = best.row; y < best.row + spanH; y++) {
        for (let x = best.col; x < best.col + spanW; x++) occupied[y][x] = true;
      }
      prepared.push({
        targetIndex: prepared.length,
        tile: { ...tile, col: best.col, row: best.row, span_w: spanW, span_h: spanH }
      });
    }
    return prepared;
  }

  async function replaceScreensaverGridForImport(sourceTiles, sourceLayout = null) {
    const folderId = getFolderIdForTab('screensaver');
    if (folderId === undefined) throw new Error('Screensaver grid unavailable');
    const currentTiles = await fetchTilesForImport(folderId);
    const tileCount = GRID_COLS * GRID_ROWS;
    const preparedTiles = prepareScreensaverTilesForImport(sourceTiles, sourceLayout);
    const supportedTypes = new Set([1, 2, 5, 14, MEDIA_TILE_TYPE]);
    for (const entry of preparedTiles) {
      if (!supportedTypes.has(Number(entry.tile.type || 0))) {
        throw new Error('Unsupported screensaver tile type');
      }
    }

    // Erst vorhandene Kacheln entfernen, damit die importierten Positionen
    // nicht an temporaeren Ueberlappungen mit dem alten Grid scheitern.
    for (let i = 0; i < tileCount; i++) {
      if (Number(currentTiles[i]?.type || 0) !== 0) {
        await postTile(folderId, i, buildEmptyImportTile(i));
      }
    }

    for (const entry of preparedTiles) {
      const tile = entry.tile;
      await postTile(folderId, entry.targetIndex, tile);
    }
  }

  async function importScreensaverConfig(config) {
    const res = await fetch('/api/screensaver', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config)
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok || !data.success) {
      throw new Error(data.error || 'Screensaver config import failed');
    }
  }

  async function importTilesPayload(payload) {
    try {
      if (!payload || typeof payload !== 'object') {
        showNotification(t('importInvalidJson'), false);
        return;
      }
      const grids = (payload.grids && typeof payload.grids === 'object') ? payload.grids : {};
      if (!Object.keys(grids).length) {
        if (Array.isArray(payload.tab0)) grids['0'] = payload.tab0;
        if (Array.isArray(payload.tab1)) grids['1'] = payload.tab1;
        if (Array.isArray(payload.tab2)) grids['2'] = payload.tab2;
      }
      if (!Object.keys(grids).length) {
        showNotification(t('importInvalidJson'), false);
        return;
      }

      showNotification(t('importRunning'));

      const sourceFolders = Array.isArray(payload.folders) ? payload.folders : [{ id: 0, parent_id: 0, name: 'Home', icon_name: '' }];
      const sourceToTarget = { 0: 0 };

      if (Array.isArray(grids['0'])) {
        await replaceFolderGridForImport(0, grids['0'], 7, sourceToTarget);
      }

      let targetFolders = await fetchFoldersForImport();
      updateFolderImportMap(sourceFolders, targetFolders, sourceToTarget);

      const pendingFolderIds = sourceFolders
        .map(folder => parseInt(folder && folder.id, 10))
        .filter(folderId => !isNaN(folderId) && folderId !== 0 && Array.isArray(grids[String(folderId)]));

      let progressed = true;
      while (pendingFolderIds.length && progressed) {
        progressed = false;
        for (let i = 0; i < pendingFolderIds.length; ) {
          const sourceFolderId = pendingFolderIds[i];
          const targetFolderId = sourceToTarget[sourceFolderId];
          if (targetFolderId === undefined) {
            i++;
            continue;
          }
          await replaceFolderGridForImport(targetFolderId, grids[String(sourceFolderId)], 8, sourceToTarget);
          pendingFolderIds.splice(i, 1);
          progressed = true;
          targetFolders = await fetchFoldersForImport();
          updateFolderImportMap(sourceFolders, targetFolders, sourceToTarget);
        }
      }

      if (pendingFolderIds.length) {
        throw new Error('Folder mapping failed');
      }

      // Version 1/2 hatten keinen Screensaver-Block und bleiben unveraendert
      // importierbar. Auch alternative flache Feldnamen werden akzeptiert,
      // falls ein Zwischenstand dieser Exportfunktion verwendet wurde.
      const screensaverBlock = payload.screensaver && typeof payload.screensaver === 'object'
        ? payload.screensaver
        : null;
      const screensaverConfig = screensaverBlock?.config || payload.screensaver_config;
      const screensaverGrid = screensaverBlock?.grid || payload.screensaver_grid;
      if (screensaverConfig && typeof screensaverConfig === 'object') {
        await importScreensaverConfig(screensaverConfig);
      }
      if (Array.isArray(screensaverGrid)) {
        await replaceScreensaverGridForImport(
          screensaverGrid, screensaverBlock?.source_layout || null);
      }

      try { localStorage.removeItem('tileDrafts'); } catch (e) {}
      showNotification(t('importComplete'));
      setTimeout(() => location.reload(), 600);
    } catch (e) {
      console.error('Import fehlgeschlagen:', e);
      showNotification(t('importFailed'), false);
    }
  }

  async function postTile(folderId, index, tile, sourceToTarget = null) {
    const fd = new FormData();
    const type = Number(tile.type);
    let safeType = isNaN(type) ? 0 : type;
    if (safeType === 4 && tile.navigate_kind !== undefined && tile.navigate_kind !== null) {
      const kind = Number(tile.navigate_kind);
      if (kind === 1) safeType = 7;
      else if (kind === 2) safeType = 8;
    }
    fd.append('folder', folderId);
    fd.append('index', index);
    fd.append('type', safeType);
    fd.append('title', tile.title || '');
    fd.append('icon_name', tile.icon_name || '');
    const parsedBgColor = parseBgColorValue(tile.bg_color);
    if (parsedBgColor !== 0 || (typeof tile.bg_color === 'string' && tile.bg_color.trim().startsWith('#'))) {
      fd.append('bg_color', parsedBgColor);
    } else {
      fd.append('bg_color_default', '1');
    }
    const layout = normalizeTileLayout(tile, index, tabByFolder[folderId] || '');
    fd.append('col', layout.col);
    fd.append('row', layout.row);
    fd.append('span_w', layout.span_w);
    fd.append('span_h', layout.span_h);
    if (tile.background_opacity !== undefined && tile.background_opacity !== null) {
      fd.append('background_opacity', tile.background_opacity);
    }

    if (safeType === 1) {
      fd.append('sensor_entity', tile.sensor_entity || '');
      fd.append('sensor_unit', tile.sensor_unit || '');
      const dec = tile.sensor_decimals;
      if (dec !== undefined && dec !== null && Number(dec) >= 0) {
        fd.append('sensor_decimals', dec);
      }
      if (tile.sensor_value_font !== undefined && tile.sensor_value_font !== null) {
        fd.append('sensor_value_font', tile.sensor_value_font);
      }
      const isScreensaverTile = Number(folderId) === Number(getFolderIdForTab('screensaver'));
      const displayMode = isScreensaverTile
        ? 0
        : ((tile.sensor_display_mode !== undefined) ? tile.sensor_display_mode : (tile.sensor_gauge ? 1 : 0));
      fd.append('sensor_display_mode', String(displayMode));
      if (tile.sensor_gauge_min !== undefined && tile.sensor_gauge_min !== null && String(tile.sensor_gauge_min).length > 0) {
        fd.append('sensor_gauge_min', tile.sensor_gauge_min);
      }
      if (tile.sensor_gauge_max !== undefined && tile.sensor_gauge_max !== null && String(tile.sensor_gauge_max).length > 0) {
        fd.append('sensor_gauge_max', tile.sensor_gauge_max);
      }
      if (tile.sensor_gauge_arc !== undefined && tile.sensor_gauge_arc !== null && String(tile.sensor_gauge_arc).length > 0) {
        fd.append('sensor_gauge_arc', tile.sensor_gauge_arc);
      }
      if (tile.sensor_gauge_size !== undefined && tile.sensor_gauge_size !== null && String(tile.sensor_gauge_size).length > 0) {
        fd.append('sensor_gauge_size', tile.sensor_gauge_size);
      }
      if (tile.sensor_gauge_y_offset !== undefined && tile.sensor_gauge_y_offset !== null && String(tile.sensor_gauge_y_offset).length > 0) {
        fd.append('sensor_gauge_y_offset', tile.sensor_gauge_y_offset);
      }
      if (tile.sensor_value_y_offset !== undefined && tile.sensor_value_y_offset !== null && String(tile.sensor_value_y_offset).length > 0) {
        fd.append('sensor_value_y_offset', tile.sensor_value_y_offset);
      }
      if (tile.sensor_graph_height !== undefined && tile.sensor_graph_height !== null && String(tile.sensor_graph_height).length > 0) {
        fd.append('sensor_graph_height', tile.sensor_graph_height);
      }
      if (tile.popup_open_mode !== undefined && tile.popup_open_mode !== null) {
        fd.append('popup_open_mode', tile.popup_open_mode);
      }
    } else if (safeType === 2) {
      fd.append('scene_alias', tile.scene_alias || '');
    } else if (safeType === 3) {
      fd.append('key_macro', tile.key_macro || '');
    } else if (safeType === 4) {
      const rawTarget = Number(tile.navigate_target);
      let target = 0;
      if (!isNaN(rawTarget) && rawTarget > 0) {
        if (sourceToTarget && sourceToTarget[rawTarget] !== undefined) {
          target = sourceToTarget[rawTarget];
        }
      }
      fd.append('navigate_target', target);
    } else if (safeType === 5) {
      fd.append('switch_entity', tile.sensor_entity || '');
      const style = (tile.switch_style !== undefined && tile.switch_style !== null)
        ? tile.switch_style
        : (tile.sensor_decimals === 1 ? 1 : 0);
      fd.append('switch_style', style);
      if (tile.popup_open_mode !== undefined && tile.popup_open_mode !== null) {
        fd.append('popup_open_mode', tile.popup_open_mode);
      }
    } else if (safeType === 10) {
      fd.append('text_value', tile.text_value || tile.scene_alias || tile.key_macro || '');
      fd.append('text_value_font', tile.text_value_font || tile.sensor_value_font || '0');
    } else if (safeType === 9) {
      fd.append('clock_show_time', ((Number(tile.sensor_decimals || 1) & 1) !== 0) ? '1' : '0');
      fd.append('clock_show_date', ((Number(tile.sensor_decimals || 1) & 2) !== 0) ? '1' : '0');
      fd.append('key_code', tile.key_code || 40);
      fd.append('key_modifier', tile.key_modifier || 20);
      fd.append('clock_time_format', (tile.sensor_gauge_min !== undefined && tile.sensor_gauge_min !== null) ? tile.sensor_gauge_min : 0);
      fd.append('clock_date_format', (tile.sensor_gauge_max !== undefined && tile.sensor_gauge_max !== null) ? tile.sensor_gauge_max : 0);
    } else if (safeType === 11) {
      fd.append('counter_value', tile.counter_value || tile.scene_alias || '0');
    } else if (safeType === 12) {
      fd.append('weather_entity', tile.sensor_entity || tile.weather_entity || '');
      if (tile.popup_open_mode !== undefined && tile.popup_open_mode !== null) {
        fd.append('popup_open_mode', tile.popup_open_mode);
      }
    } else if (safeType === 14) {
      fd.append('energy_entity', tile.sensor_entity || tile.energy_entity || '');
      fd.append('sensor_unit', tile.sensor_unit || '');
      const dec = tile.sensor_decimals;
      fd.append('sensor_decimals', (dec !== undefined && dec !== null && Number(dec) >= 0) ? dec : '1');
      if (tile.sensor_value_font !== undefined && tile.sensor_value_font !== null) {
        fd.append('sensor_value_font', tile.sensor_value_font);
      }
      if (tile.popup_open_mode !== undefined && tile.popup_open_mode !== null) {
        fd.append('popup_open_mode', tile.popup_open_mode);
      }
      if (tile.sensor_value_y_offset !== undefined && tile.sensor_value_y_offset !== null && String(tile.sensor_value_y_offset).length > 0) {
        fd.append('sensor_value_y_offset', tile.sensor_value_y_offset);
      }
    } else if (safeType === MEDIA_TILE_TYPE) {
      fd.append('media_entity', tile.sensor_entity || tile.media_entity || '');
    } else if (safeType === 17) {
      fd.append('climate_entity', tile.sensor_entity || tile.climate_entity || '');
      fd.append(
        'climate_slots_packed',
        (tile.sensor_gauge_min !== undefined &&
         tile.sensor_gauge_min !== null)
          ? tile.sensor_gauge_min : 0);
      fd.append(
        'climate_layouts_packed',
        getClimateLayoutPayload(tile.sensor_gauge_max));
      fd.append(
        'climate_geometry',
        tile.climate_geometry || tile.scene_alias || '');
      if (tile.popup_open_mode !== undefined && tile.popup_open_mode !== null) {
        fd.append('popup_open_mode', tile.popup_open_mode);
      }
    } else if (safeType === 16) {
      fd.append('animation_file', tile.animation_file || tile.scene_alias || '');
      fd.append('animation_fps', tile.animation_fps || tile.image_slideshow_sec || '10');
      fd.append('animation_fit',
        (tile.animation_fit !== undefined && tile.animation_fit !== null)
          ? tile.animation_fit
          : (tile.sensor_display_mode || '0'));
      fd.append('animation_zoom',
        (tile.animation_zoom !== undefined && tile.animation_zoom !== null)
          ? tile.animation_zoom
          : (tile.sensor_gauge_max || '100'));
    }

    const res = await fetch('/api/tiles', { method: 'POST', body: fd });
    const data = await res.json();
    if (!data.success) {
      throw new Error('Tile speichern fehlgeschlagen');
    }
  }

  function rgbToHex(rgb) {
    const num = Number(rgb);
    const masked = Number.isFinite(num) ? (num & 0xFFFFFF) : 0;
    return '#' + ('000000' + masked.toString(16)).slice(-6);
  }
  function hexToRgb(hex) {
    const parsed = parseInt(String(hex || '').replace('#', ''), 16);
    return isNaN(parsed) ? 0 : (parsed & 0xFFFFFF);
  }
  function makeTileBgValue(rgb) {
    return (Number(rgb) & 0xFFFFFF) | 0x01000000;
  }
  function tileBgValueIsSet(value) {
    const num = Number(value);
    return Number.isFinite(num) && num !== 0;
  }
  function tileBgToHex(value, fallback) {
    const num = Number(value);
    if (!Number.isFinite(num) || num === 0) return fallback || '#353535';
    return rgbToHex(num);
  }
  function snapshotBgColorIsDefault(snapshot) {
    return String(snapshot?.bg_color_default || '0') === '1';
  }
  function tileColorInputIsDefault(tab) {
    const input = document.getElementById(tab + '_tile_color');
    return !!input && input.dataset.bgColorDefault === '1';
  }
  function setTileColorInputFromStored(tab, value, fallback) {
    const input = document.getElementById(tab + '_tile_color');
    if (!input) return;
    input.value = tileBgToHex(value, fallback || '#2A2A2A');
    input.dataset.bgColorDefault = tileBgValueIsSet(value) ? '0' : '1';
  }
  function setTileColorInputFromSnapshot(tab, snapshot) {
    const input = document.getElementById(tab + '_tile_color');
    if (!input) return;
    const meta = getTileTypeMeta(snapshot?.type || '0');
    const isDefault = snapshotBgColorIsDefault(snapshot);
    input.value = isDefault ? (meta.defaultBg || '#2A2A2A') : (snapshot?.color || meta.defaultBg || '#2A2A2A');
    input.dataset.bgColorDefault = isDefault ? '1' : '0';
  }
  function markTileColorInputExplicit(tab) {
    const input = document.getElementById(tab + '_tile_color');
    if (input) input.dataset.bgColorDefault = '0';
  }
  function resetTileColor(tab) {
    const input = document.getElementById(tab + '_tile_color');
    if (!input) return;
    input.value = '#2A2A2A';
    input.dataset.bgColorDefault = '0';
    if (isScreensaverTileTab(tab)) {
      const opacity = document.getElementById('screensaver_tile_opacity');
      if (opacity) opacity.value = String(SCREENSAVER_TILE_DEFAULT_OPACITY);
    }
    updateTilePreview(tab);
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  function renderTileFromData(tab, index, tile, sensorMeta) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return;
    const metaValues = sensorMeta?.values || {};
    const metaUnits = sensorMeta?.units || {};
    const metaIcons = sensorMeta?.icons || {};
    el.dataset.index = index.toString();
    const typeValue = String(tile?.type ?? '0');
    const meta = getTileTypeMeta(typeValue);
    if (typeValue === '17' &&
        currentTileTab === tab &&
        currentTileIndex === index &&
        el.classList.contains('climate-content-editing')) {
      syncClimateSlotFields(tab);
      return;
    }
    let cls = ['tile'];
    if (meta.css) cls.push(meta.css);
    if (typeValue === '5' && tile.switch_style === 1) cls.push('switch-toggle');
    if (typeValue === '0' && (!meta.css || meta.css !== 'empty')) cls.push('empty');
    el.className = cls.join(' ');
    el.dataset.type = typeValue;
    if (typeValue === '4') el.dataset.navigateTarget = String(tile.navigate_target || 0);
    else delete el.dataset.navigateTarget;
    if (typeValue === '0') el.style.background = 'transparent';
    else {
      const bg = tileBgToHex(tile.bg_color, meta.defaultBg || '#353535');
      if (isScreensaverTileTab(tab)) {
        const opacity = clampInt(tile.background_opacity, 0, 255,
                                 SCREENSAVER_TILE_DEFAULT_OPACITY);
        el.style.background = bg + opacity.toString(16).padStart(2, '0');
      } else {
        el.style.background = bg;
      }
      el.style.removeProperty('--switch-knob-color');
      el.style.removeProperty('--switch-on-color');
      if (typeValue === '5' && tile.switch_style === 1) {
        el.style.setProperty('--switch-knob-color', bg);
        el.style.setProperty('--switch-on-color', '#3B82F6');
      }
    }
    const sensorValueClass = getSensorValueFontClass(tile.sensor_value_font);
    if (typeValue === '0') { el.innerHTML = ''; }
    else {
      const previewKind = meta.preview || 'none';
      const iconEntity = (previewKind === 'sensor' || previewKind === 'switch' ||
                          previewKind === 'weather' || previewKind === 'media' ||
                          previewKind === 'climate')
        ? (tile.sensor_entity || '')
        : '';
      const rawIcon = tile.icon_name || '';
      let iconName = resolveIconName(
        rawIcon,
        iconEntity,
        metaIcons);
      let climatePreviewState = null;
      if (previewKind === 'climate') {
        climatePreviewState = parseClimatePreviewPayload(
          tile.sensor_entity ? (metaValues[tile.sensor_entity] ?? '') : '');
        if (!normalizeMdiIconName(rawIcon) &&
            !isExplicitlyDisabledValue(rawIcon)) {
          iconName = climatePreviewIcon(climatePreviewState, iconName);
        }
      }

      let html = '';

      if (iconName) {
        const iconStyle = previewKind === 'climate'
          ? ' style="color:' + climatePreviewColor(climatePreviewState) + '"'
          : '';
        html += '<i class="mdi mdi-' + iconName + ' tile-icon"' + iconStyle + '></i>';
      }

      if (tile.title && tile.title.length) {
        html += '<div class="tile-title" id="' + tab + '-tile-' + index + '-title">' + tile.title + '</div>';
      }

      if (previewKind === 'weather') {
        html += '<div class="tile-ghost-icon"><i class="mdi mdi-weather-partly-cloudy"></i></div>';
      }
      if (previewKind === 'media') {
        html += '<div class="tile-ghost-icon"><i class="mdi mdi-music"></i></div>';
      }

      if (previewKind === 'sensor') {
        let value = '--';
        if (tile.sensor_entity) value = formatSensorValue(metaValues[tile.sensor_entity] ?? '--', tile.sensor_decimals);
        const unit = resolveUnitValue(tile.sensor_unit || '', tile.sensor_entity || '', metaUnits);
        html += '<div class="tile-value ' + sensorValueClass + '" id="' + tab + '-tile-' + index + '-value">' + value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '') + '</div>';
      }
      if (previewKind === 'climate') {
        html += climatePreviewSlots(
          climatePreviewState,
          tile.span_w || 1,
          tile.span_h || 1,
          decodeClimateSlotConfig(tile.sensor_gauge_min || 0),
          decodeClimateTargetLayouts(tile.sensor_gauge_max || 0),
          tile.climate_geometry || tile.scene_alias || '');
      }
      if (previewKind === 'clock') {
        const flags = normalizeClockFlags(tile.sensor_decimals);
        const clockTimeFont = tile.key_code || 40;
        const clockDateFont = Math.min(72, Number(tile.key_modifier || 20));
        const clockTimeFormat = (tile.sensor_gauge_min !== undefined) ? tile.sensor_gauge_min : 0;
        const clockDateFormat = (tile.sensor_gauge_max !== undefined) ? tile.sensor_gauge_max : 0;
        if (flags & 1) html += '<div class="tile-clock-time" ' + getClockPreviewTextStyle(clockTimeFont, 40, '#fff') + '>' + getClockPreviewTime(clockTimeFormat) + '</div>';
        if (flags & 2) html += '<div class="tile-clock-date" ' + getClockPreviewTextStyle(clockDateFont, 24, '#fff') + '>' + getClockPreviewDate(clockDateFormat) + '</div>';
      }
      if (previewKind === 'text') {
        const textValue = tile.text_value || tile.scene_alias || tile.key_macro || '';
        if (textValue) {
          const textClass = getSensorValueFontClass(tile.sensor_value_font);
          html += '<div class="tile-text ' + textClass + '">' + textValue + '</div>';
        }
      }
      if (previewKind === 'counter') {
        const counterVal = tile.counter_value || tile.scene_alias || '0';
        html += '<div class="tile-counter-value">' + counterVal + '</div>';
      }
      if (previewKind === 'switch' && tile.switch_style === 1) {
        html += '<div class="tile-switch" id="' + tab + '-tile-' + index + '-switch"><div class="tile-switch-knob"></div></div>';
      }
      html += getTileResizeHandlesHtml(typeValue);
      el.innerHTML = html;
    }
    if (currentTileTab === tab && currentTileIndex === index) el.classList.add('active');
    if (typeValue === '5' && tile.sensor_entity) {
      const state = parseSwitchPayload(metaValues[tile.sensor_entity] ?? '');
      applySwitchPreviewState(el, state);
    }
  }

  function loadSensorValues(refreshTiles = false, forceMetaFetch = false) {
    if (dragSource || resizeState) {
      queueDeferredSensorRefresh(refreshTiles);
      return;
    }
    const tabs = tileTabs.slice();
    const tileRequests = refreshTiles
      ? tabs.map(tab => {
          const folderId = getFolderIdForTab(tab);
          if (folderId === undefined) return Promise.resolve([]);
          return fetch('/api/tiles?folder=' + encodeURIComponent(folderId))
            .then(res => res.json())
            .catch(() => []);
        })
      : tabs.map(tab => Promise.resolve(getTilesData(tab)));

    Promise.all([fetchSensorMetaCache(forceMetaFetch), ...tileRequests])
    .then(results => {
      // Eine Aktualisierung kann kurz vor dem Drag gestartet worden sein und
      // erst waehrenddessen eintreffen. In diesem Fall darf sie die lokale
      // Vorschau nicht mit dem alten Geraetezustand ueberschreiben.
      if (dragSource || resizeState) {
        queueDeferredSensorRefresh(refreshTiles);
        return;
      }
      const sensorMeta = normalizeSensorMetaPayload(results[0] || {});
      sensorMetaCache = sensorMeta;
      tabs.forEach((tab, idx) => {
        const tiles = Array.isArray(results[idx + 1]) ? results[idx + 1] : [];
        if (refreshTiles) {
          tilesData[tab] = tiles;
        }
        const tilesForRender = refreshTiles ? tiles : getTilesData(tab);
        if (!Array.isArray(tilesForRender)) return;
        tilesForRender.forEach((tile, i) => renderTileFromData(tab, i, tile, sensorMeta));
        layoutTiles(tab, tilesForRender);
      });
      if (currentTileIndex !== -1 && currentTileTab) {
        restoreCurrentTileSelectionUi();
      } else if (!isScreensaverTileTab(currentTileTab)) {
        restoreSelectedTileState();
      }
    })
    .catch(err => console.error('Fehler beim Laden der Sensorwerte:', err));
  }

  let dragSource = null;
  let dragPreview = null;
  let dragPlaceholder = null;
  let resizeState = null;
  let resizePlaceholder = null;
  let deferredSensorRefresh = false;
  let deferredSensorRefreshTiles = false;

  function queueDeferredSensorRefresh(refreshTiles = false) {
    deferredSensorRefresh = true;
    deferredSensorRefreshTiles = deferredSensorRefreshTiles || refreshTiles;
  }

  function clearDeferredSensorRefresh() {
    deferredSensorRefresh = false;
    deferredSensorRefreshTiles = false;
  }

  function flushDeferredSensorRefresh() {
    if (!deferredSensorRefresh || dragSource || resizeState) return;
    const refreshTiles = deferredSensorRefreshTiles;
    clearDeferredSensorRefresh();
    loadSensorValues(refreshTiles, true);
  }

  function createDragPreview(tile) {
    const clone = tile.cloneNode(true);
    const rect = tile.getBoundingClientRect();
    clone.style.position = 'absolute';
    clone.style.top = '-9999px';
    clone.style.left = '-9999px';
    clone.style.width = rect.width + 'px';
    clone.style.height = rect.height + 'px';
    clone.style.opacity = '0.9';
    clone.style.pointerEvents = 'none';
    clone.style.boxShadow = '0 10px 30px rgba(0,0,0,0.35)';
    clone.style.backgroundClip = 'padding-box';
    clone.style.clipPath = 'inset(0 round 11px)';
    clone.style.display = 'block';
    document.body.appendChild(clone);
    return clone;
  }

  function getTileGrid(tab) {
    return document.querySelector('#tab-tiles-' + tab + ' .tile-grid');
  }

  function parseGridTrackSizes(value) {
    return String(value || '')
      .split(' ')
      .map(part => parseFloat(part))
      .filter(part => !isNaN(part) && part > 0);
  }

  function getGridElementMetrics(grid, columns, rows) {
    if (!grid) return null;
    const style = window.getComputedStyle(grid);
    const rect = grid.getBoundingClientRect();
    const gapX = parseFloat(style.columnGap || style.gap || '0') || 0;
    const gapY = parseFloat(style.rowGap || style.gap || '0') || 0;
    const padLeft = parseFloat(style.paddingLeft || '0') || 0;
    const padTop = parseFloat(style.paddingTop || '0') || 0;
    const padRight = parseFloat(style.paddingRight || '0') || 0;
    const padBottom = parseFloat(style.paddingBottom || '0') || 0;
    const cols = parseGridTrackSizes(style.gridTemplateColumns);
    const gridRows = parseGridTrackSizes(style.gridTemplateRows);
    const columnCount = Math.max(1, Number(columns) || cols.length || 1);
    const rowCount = Math.max(1, Number(rows) || gridRows.length || 1);
    const cellW = cols.length
      ? cols[0]
      : ((rect.width - padLeft - padRight -
          (gapX * (columnCount - 1))) / columnCount);
    const cellH = gridRows.length
      ? gridRows[0]
      : ((rect.height - padTop - padBottom -
          (gapY * (rowCount - 1))) / rowCount);
    // Bei align-content:space-between (Klima-Slots) liegt der wirksame
    // Reihenabstand ueber dem nominalen gap; aus der Restflaeche ableiten,
    // damit Pointer->Zelle auch dort stimmt. Fuer 1fr-Grids identisch.
    let effGapX = gapX;
    let effGapY = gapY;
    if (columnCount > 1 && isFinite(cellW)) {
      effGapX = Math.max(gapX,
        (rect.width - padLeft - padRight - (columnCount * cellW)) /
        (columnCount - 1));
    }
    if (rowCount > 1 && isFinite(cellH)) {
      effGapY = Math.max(gapY,
        (rect.height - padTop - padBottom - (rowCount * cellH)) /
        (rowCount - 1));
    }
    return {
      rect, gapX: effGapX, gapY: effGapY, padLeft, padTop,
      cellW, cellH, columns: columnCount, rows: rowCount
    };
  }

  function getGridElementCellFromPointer(
      grid, columns, rows, clientX, clientY) {
    const metrics = getGridElementMetrics(grid, columns, rows);
    if (!metrics) return null;
    const stepX = metrics.cellW + metrics.gapX;
    const stepY = metrics.cellH + metrics.gapY;
    let relX = clientX - metrics.rect.left - metrics.padLeft;
    let relY = clientY - metrics.rect.top - metrics.padTop;
    if (!isFinite(relX) || !isFinite(relY)) return null;
    relX = Math.max(0, relX);
    relY = Math.max(0, relY);
    const col = Math.max(
      0, Math.min(
        metrics.columns - 1,
        Math.floor((relX + (metrics.gapX / 2)) / stepX)));
    const row = Math.max(
      0, Math.min(
        metrics.rows - 1,
        Math.floor((relY + (metrics.gapY / 2)) / stepY)));
    return { col, row };
  }

  function getTileGridMetrics(tab) {
    const grid = getTileGrid(tab);
    return getGridElementMetrics(grid, GRID_COLS, GRID_ROWS);
  }

  function getRawGridCellFromPointer(tab, clientX, clientY) {
    const metrics = getTileGridMetrics(tab);
    if (!metrics) return null;
    const stepX = metrics.cellW + metrics.gapX;
    const stepY = metrics.cellH + metrics.gapY;
    let relX = clientX - metrics.rect.left - metrics.padLeft;
    let relY = clientY - metrics.rect.top - metrics.padTop;
    if (!isFinite(relX) || !isFinite(relY)) return null;
    relX = Math.max(0, relX);
    relY = Math.max(0, relY);
    let col = Math.floor((relX + (metrics.gapX / 2)) / stepX);
    let row = Math.floor((relY + (metrics.gapY / 2)) / stepY);
    if (!isFinite(col)) col = 0;
    if (!isFinite(row)) row = 0;
    if (col < 0) col = 0;
    const firstRow = firstAllowedGridRow(tab);
    if (row < firstRow) row = firstRow;
    if (col >= GRID_COLS) col = GRID_COLS - 1;
    if (row >= GRID_ROWS) row = GRID_ROWS - 1;
    return { col, row };
  }

  function getGridCellFromPointer(tab, clientX, clientY) {
    const rawCell = getRawGridCellFromPointer(tab, clientX, clientY);
    if (!rawCell) return null;
    if (!dragSource || dragSource.tab !== tab) return rawCell;
    const anchorCol = clampInt(dragSource.grabCellCol, 0, GRID_COLS - 1, 0);
    const anchorRow = clampInt(dragSource.grabCellRow, 0, GRID_ROWS - 1, 0);
    return {
      col: rawCell.col - anchorCol,
      row: rawCell.row - anchorRow
    };
  }

  function getTileLayoutFromData(tab, index) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles) || index < 0 || index >= tiles.length) return null;
    return normalizeTileLayout(tiles[index], index, tab);
  }

  function getDragSourceLayout() {
    if (!dragSource) return null;
    return dragSource.layout ||
      getTileElementLayout(dragSource.tab, dragSource.index) ||
      getTileLayoutFromData(dragSource.tab, dragSource.index);
  }

  function getDragAnchorCell(tab, layout, clientX, clientY) {
    const rawCell = getRawGridCellFromPointer(tab, clientX, clientY);
    if (!layout || !rawCell) return { col: 0, row: 0 };
    const col = clampInt(rawCell.col - layout.col, 0, Math.max(0, layout.span_w - 1), 0);
    const row = clampInt(rawCell.row - layout.row, 0, Math.max(0, layout.span_h - 1), 0);
    return { col, row };
  }

  function getDragAnchorOffset(tab, layout, grabCellCol, grabCellRow, tileRect) {
    const metrics = getTileGridMetrics(tab);
    const rect = tileRect || { width: 0, height: 0 };
    if (!layout || !metrics) {
      return {
        x: Math.max(0, (rect.width / 2) || 0),
        y: Math.max(0, (rect.height / 2) || 0)
      };
    }
    const x = (grabCellCol * (metrics.cellW + metrics.gapX)) + (metrics.cellW / 2);
    const y = (grabCellRow * (metrics.cellH + metrics.gapY)) + (metrics.cellH / 2);
    const maxX = Math.max(0, rect.width - 1);
    const maxY = Math.max(0, rect.height - 1);
    return {
      x: Math.max(0, Math.min(maxX, x)),
      y: Math.max(0, Math.min(maxY, y))
    };
  }

  function cloneLayout(layout) {
    if (!layout) return null;
    return {
      col: layout.col,
      row: layout.row,
      span_w: layout.span_w,
      span_h: layout.span_h
    };
  }

  function captureLayoutSnapshot(tab) {
    const tiles = getTilesData(tab);
    const count = Math.max(Array.isArray(tiles) ? tiles.length : 0, GRID_COLS * GRID_ROWS);
    const snapshot = [];
    for (let i = 0; i < count; i++) {
      const layout = getTileElementLayout(tab, i) || getTileLayoutFromData(tab, i);
      snapshot[i] = cloneLayout(layout);
    }
    return snapshot;
  }

  function clearReflowPreviewClasses(tab) {
    document.querySelectorAll('#tab-tiles-' + tab + ' .tile').forEach(tile => {
      tile.classList.remove('reflow-preview');
    });
  }

  function layoutsEqual(a, b) {
    if (!a || !b) return false;
    return a.col === b.col &&
           a.row === b.row &&
           a.span_w === b.span_w &&
           a.span_h === b.span_h;
  }

  function restoreDragPreview(tab) {
    if (!dragSource || dragSource.tab !== tab || !Array.isArray(dragSource.baseLayouts)) {
      clearReflowPreviewClasses(tab);
      return;
    }
    const tiles = getTilesData(tab);
    dragSource.previewResult = null;
    dragSource.appliedPreviewResult = null;
    dragSource.previewKey = '';
    for (let i = 0; i < dragSource.baseLayouts.length; i++) {
      const tile = Array.isArray(tiles) ? tiles[i] : null;
      if (!tile || Number(tile.type || 0) === 0) continue;
      const el = document.getElementById(tab + '-tile-' + i);
      const layout = dragSource.baseLayouts[i];
      if (!el || !layout) continue;
      setTileGridPosition(el, layout.col, layout.row, layout.span_w, layout.span_h);
    }
    clearReflowPreviewClasses(tab);
  }

  function applyDragPreviewLayouts(tab, previewResult) {
    if (!dragSource || dragSource.tab !== tab || !previewResult || !Array.isArray(previewResult.layouts)) return;
    const tiles = getTilesData(tab);
    for (let i = 0; i < previewResult.layouts.length; i++) {
      const tile = Array.isArray(tiles) ? tiles[i] : null;
      if (!tile || Number(tile.type || 0) === 0) continue;
      const el = document.getElementById(tab + '-tile-' + i);
      if (!el) continue;

      const baseLayout = dragSource.baseLayouts && dragSource.baseLayouts[i] ? dragSource.baseLayouts[i] : null;
      const previewLayout = previewResult.layouts[i] || baseLayout;
      if (i === dragSource.index) {
        if (previewLayout) {
          setTileGridPosition(el, previewLayout.col, previewLayout.row, previewLayout.span_w, previewLayout.span_h);
        } else if (baseLayout) {
          setTileGridPosition(el, baseLayout.col, baseLayout.row, baseLayout.span_w, baseLayout.span_h);
        }
        el.classList.remove('reflow-preview');
        continue;
      }
      if (!previewLayout) continue;
      setTileGridPosition(el, previewLayout.col, previewLayout.row, previewLayout.span_w, previewLayout.span_h);

      const changed = !!(baseLayout &&
        (baseLayout.col !== previewLayout.col || baseLayout.row !== previewLayout.row));
      el.classList.toggle('reflow-preview', changed);
    }
    dragSource.appliedPreviewResult = previewResult;
  }

  function rectsOverlap(a, b) {
    if (!a || !b) return false;
    return !(a.col + a.span_w <= b.col ||
             b.col + b.span_w <= a.col ||
             a.row + a.span_h <= b.row ||
             b.row + b.span_h <= a.row);
  }

  function canPlaceGridLayout(
      layouts, activeIndices, index, candidateLayout,
      columns, rows, firstRow = 0) {
    if (!candidateLayout) return false;
    if (candidateLayout.col < 0 ||
        candidateLayout.row < firstRow ||
        candidateLayout.span_w < 1 ||
        candidateLayout.span_h < 1 ||
        candidateLayout.col + candidateLayout.span_w > columns ||
        candidateLayout.row + candidateLayout.span_h > rows) {
      return false;
    }
    const active = activeIndices instanceof Set
      ? activeIndices : new Set(activeIndices || []);
    for (const otherIndex of active) {
      if (otherIndex === index) continue;
      const otherLayout = layouts?.[otherIndex];
      if (otherLayout && rectsOverlap(candidateLayout, otherLayout)) {
        return false;
      }
    }
    return true;
  }

  function canPlaceTileLayout(tab, index, candidateLayout) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles)) return false;
    const layouts = tiles.map((tile, tileIndex) =>
      getTileElementLayout(tab, tileIndex) ||
      getTileLayoutFromData(tab, tileIndex));
    const active = new Set();
    tiles.forEach((tile, tileIndex) => {
      if (tile && Number(tile.type || 0) !== 0) active.add(tileIndex);
    });
    return canPlaceGridLayout(
      layouts, active, index, candidateLayout,
      GRID_COLS, GRID_ROWS, firstAllowedGridRow(tab));
  }

  function manhattanDistance(colA, rowA, colB, rowB) {
    return Math.abs(colA - colB) + Math.abs(rowA - rowB);
  }

  function buildGridPlacementCandidates(
      columns, rows, firstRow,
      spanW, spanH, preferredCol, preferredRow) {
    const candidates = [];
    for (let row = firstRow; row < rows; row++) {
      for (let col = 0; col < columns; col++) {
        if ((col + spanW) > columns || (row + spanH) > rows) continue;
        let distance = (row * columns) + col;
        if (preferredCol >= 0 && preferredRow >= 0) {
          distance = manhattanDistance(col, row, preferredCol, preferredRow);
        }
        candidates.push({ col, row, distance });
      }
    }
    candidates.sort((a, b) => {
      if (a.distance !== b.distance) return a.distance - b.distance;
      if (a.row !== b.row) return a.row - b.row;
      return a.col - b.col;
    });
    return candidates;
  }

  function buildPlacementCandidates(
      tab, spanW, spanH, preferredCol, preferredRow) {
    return buildGridPlacementCandidates(
      GRID_COLS, GRID_ROWS, firstAllowedGridRow(tab),
      spanW, spanH, preferredCol, preferredRow);
  }

  function simulateGridReorderLayouts(
      baseLayouts, activeIndices, fromIdx,
      targetCol, targetRow, columns, rows, firstRow = 0) {
    const active = activeIndices instanceof Set
      ? new Set(activeIndices) : new Set(activeIndices || []);
    if (!active.has(fromIdx)) return null;
    const movingBase = baseLayouts?.[fromIdx]
      ? cloneLayout(baseLayouts[fromIdx]) : null;
    if (!movingBase ||
        targetRow < firstRow ||
        targetCol < 0 ||
        targetCol + movingBase.span_w > columns ||
        targetRow + movingBase.span_h > rows) {
      return null;
    }

    const workingLayouts = (baseLayouts || [])
      .map(layout => cloneLayout(layout));
    const targetLayout = {
      col: targetCol,
      row: targetRow,
      span_w: movingBase.span_w,
      span_h: movingBase.span_h
    };
    const displacedIndices = [];
    active.forEach(index => {
      if (index === fromIdx) return;
      const layout = baseLayouts[index];
      if (layout && rectsOverlap(targetLayout, layout)) {
        displacedIndices.push(index);
      }
    });
    displacedIndices.sort((a, b) => {
      const layoutA = baseLayouts[a];
      const layoutB = baseLayouts[b];
      if (layoutA.row !== layoutB.row) return layoutA.row - layoutB.row;
      if (layoutA.col !== layoutB.col) return layoutA.col - layoutB.col;
      return a - b;
    });

    workingLayouts[fromIdx] = targetLayout;
    const floating = new Set(displacedIndices);
    for (let order = 0; order < displacedIndices.length; ++order) {
      const displacedIndex = displacedIndices[order];
      const layout = baseLayouts[displacedIndex];
      if (!layout) return null;
      floating.delete(displacedIndex);
      const preferredCol = order === 0 ? movingBase.col : layout.col;
      const preferredRow = order === 0 ? movingBase.row : layout.row;
      const candidates = buildGridPlacementCandidates(
        columns, rows, firstRow,
        layout.span_w, layout.span_h,
        preferredCol, preferredRow);
      let placed = false;
      for (const candidate of candidates) {
        const nextLayout = {
          col: candidate.col,
          row: candidate.row,
          span_w: layout.span_w,
          span_h: layout.span_h
        };
        let blocked = false;
        for (const otherIndex of active) {
          if (otherIndex === displacedIndex ||
              floating.has(otherIndex)) {
            continue;
          }
          const otherLayout = workingLayouts[otherIndex];
          if (otherLayout &&
              rectsOverlap(nextLayout, otherLayout)) {
            blocked = true;
            break;
          }
        }
        if (blocked) continue;
        workingLayouts[displacedIndex] = nextLayout;
        placed = true;
        break;
      }
      if (!placed) return null;
    }
    return {
      targetCol,
      targetRow,
      layouts: workingLayouts
    };
  }

  function simulateSmartReorderLayouts(tab, fromIdx, targetCol, targetRow) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles) || fromIdx < 0 || fromIdx >= tiles.length) return null;

    const baseLayouts = (dragSource && dragSource.tab === tab && Array.isArray(dragSource.baseLayouts))
      ? dragSource.baseLayouts
      : captureLayoutSnapshot(tab);

    const active = new Set();
    tiles.forEach((tile, index) => {
      if (tile && Number(tile.type || 0) !== 0) active.add(index);
    });
    return simulateGridReorderLayouts(
      baseLayouts, active, fromIdx,
      targetCol, targetRow,
      GRID_COLS, GRID_ROWS, firstAllowedGridRow(tab));
  }

  function clearDragPlaceholder() {
    if (dragPlaceholder && dragPlaceholder.parentNode) {
      dragPlaceholder.parentNode.removeChild(dragPlaceholder);
    }
    if (dragPlaceholder) {
      dragPlaceholder.classList.remove('show', 'invalid');
    }
    dragPlaceholder = null;
  }

  function ensureDragPlaceholder(tab) {
    const grid = getTileGrid(tab);
    if (!grid) return null;
    if (!dragPlaceholder) {
      dragPlaceholder = document.createElement('div');
      dragPlaceholder.className = 'tile-drop-placeholder';
    }
    if (dragPlaceholder.parentNode !== grid) grid.appendChild(dragPlaceholder);
    return dragPlaceholder;
  }

  function clearResizePlaceholder() {
    if (resizePlaceholder && resizePlaceholder.parentNode) {
      resizePlaceholder.parentNode.removeChild(resizePlaceholder);
    }
    if (resizePlaceholder) {
      resizePlaceholder.classList.remove('show', 'invalid');
    }
    resizePlaceholder = null;
  }

  function ensureResizePlaceholder(tab) {
    const grid = getTileGrid(tab);
    if (!grid) return null;
    if (!resizePlaceholder) {
      resizePlaceholder = document.createElement('div');
      resizePlaceholder.className = 'tile-resize-placeholder';
    }
    if (resizePlaceholder.parentNode !== grid) grid.appendChild(resizePlaceholder);
    return resizePlaceholder;
  }

  function renderResizePlaceholderPreview(
      tab, placeholder, layout) {
    const source = resizeState
      ? document.getElementById(resizeState.tileId)
      : null;
    if (!source) {
      placeholder.replaceChildren();
      return;
    }
    const preview = source.cloneNode(true);
    preview.removeAttribute('id');
    preview.removeAttribute('onclick');
    preview.removeAttribute('ondblclick');
    preview.removeAttribute('draggable');
    delete preview.dataset.selected;
    preview.classList.remove(
      'active',
      'resizing',
      'resize-invalid',
      'climate-content-editing',
      'climate-mini-selection-active',
      'climate-parent-hover');
    preview.classList.add('tile-resize-preview-card');
    preview.style.removeProperty('grid-column');
    preview.style.removeProperty('grid-row');
    preview.querySelectorAll(
      '.tile-resize-handle, .climate-mini-editor-shell')
      .forEach(element => element.remove());
    preview.querySelectorAll('[id]')
      .forEach(element => element.removeAttribute('id'));

    if (resizeState?.climateState &&
        typeof climateOuterResizePreviewHtml === 'function') {
      const slots = preview.querySelector(
        ':scope > .climate-slots');
      const html = climateOuterResizePreviewHtml(
        tab,
        resizeState.climateState,
        layout.span_w,
        layout.span_h);
      if (slots && html) slots.outerHTML = html;
    }
    placeholder.replaceChildren(preview);
  }

  function updateResizePlaceholder(tab, layout, valid) {
    const placeholder = ensureResizePlaceholder(tab);
    if (!placeholder || !layout) return;
    placeholder.classList.add('show');
    placeholder.classList.toggle('invalid', !valid);
    setTileGridPosition(placeholder, layout.col, layout.row, layout.span_w, layout.span_h);
    const previewKey = [
      layout.col,
      layout.row,
      layout.span_w,
      layout.span_h,
      valid ? 1 : 0
    ].join(':');
    if (placeholder.dataset.previewKey !== previewKey) {
      placeholder.dataset.previewKey = previewKey;
      renderResizePlaceholderPreview(
        tab, placeholder, layout);
    }
  }

  function buildResizeCandidate(layout, direction, clientX, clientY, tab) {
    const rawCell = getRawGridCellFromPointer(tab, clientX, clientY);
    if (!layout || !rawCell) return null;

    let spanW = layout.span_w;
    let spanH = layout.span_h;
    const tiles = getTilesData(tab);
    const tile = Array.isArray(tiles) && currentTileIndex >= 0
      ? tiles[currentTileIndex] : null;
    const typeValue = document.getElementById(tab + '_tile_type')?.value ?? tile?.type ?? 0;
    const isMedia = Number(typeValue) === MEDIA_TILE_TYPE;
    const minW = isMedia ? Math.min(MEDIA_TILE_MIN_SPAN, GRID_COLS) : 1;
    const minH = isMedia ? Math.min(MEDIA_TILE_MIN_SPAN, GRID_ROWS) : 1;
    const maxW = isMedia
      ? Math.min(MEDIA_TILE_MAX_SPAN, GRID_COLS - layout.col)
      : GRID_COLS - layout.col;
    const maxH = isMedia
      ? Math.min(MEDIA_TILE_MAX_SPAN, GRID_ROWS - layout.row)
      : GRID_ROWS - layout.row;
    if (String(direction || '').includes('e')) {
      spanW = clampInt(rawCell.col - layout.col + 1, minW, maxW, layout.span_w);
    }
    if (String(direction || '').includes('s')) {
      spanH = clampInt(rawCell.row - layout.row + 1, minH, maxH, layout.span_h);
    }

    return {
      col: layout.col,
      row: layout.row,
      span_w: spanW,
      span_h: spanH
    };
  }

  function stopTileResize(commit = true) {
    if (!resizeState) return;
    const state = resizeState;
    resizeState = null;

    window.removeEventListener('pointermove', handleTileResizeMove);
    window.removeEventListener('pointerup', handleTileResizeEnd);
    window.removeEventListener('pointercancel', handleTileResizeCancel);
    document.body.classList.remove('tile-resize-active');

    const tile = document.getElementById(state.tileId);
    if (tile) {
      tile.classList.remove('resizing', 'resize-invalid');
      tile.draggable = true;
    }
    clearResizePlaceholder();

    const finalLayout = commit ? (state.lastValidLayout || state.originalLayout) : state.originalLayout;
    if (state.tab === currentTileTab && state.index === currentTileIndex && finalLayout) {
      if (!commit && state.climateState &&
          typeof restoreClimateOuterResizeState === 'function') {
        restoreClimateOuterResizeState(
          state.tab, state.climateState);
      }
      applyLayoutInputsFromLayout(state.tab, finalLayout, false);
      if (commit && state.climateState &&
          typeof previewClimateOuterResize === 'function') {
        // Die neue Parent- und Mini-Geometrie wird erst beim Loslassen im
        // selben JavaScript-Schritt angewendet. Dadurch gibt es keinen Frame,
        // in dem das alte Mini-Raster in die neue Parent-Groesse gequetscht
        // oder gestreckt wird.
        previewClimateOuterResize(
          state.tab, state.climateState);
      }
      updateLayoutFromInputs(state.tab);
      updateTilePreview(state.tab);
      if (commit && !layoutsEqual(finalLayout, state.originalLayout)) {
        updateDraft(state.tab);
        scheduleAutoSave(state.tab);
      }
    }
    flushDeferredSensorRefresh();
  }

  function handleTileResizeMove(e) {
    if (!resizeState) return;
    e.preventDefault();

    const candidate = buildResizeCandidate(
      resizeState.originalLayout,
      resizeState.direction,
      e.clientX,
      e.clientY,
      resizeState.tab
    );
    if (!candidate) return;

    const valid = canPlaceTileLayout(resizeState.tab, resizeState.index, candidate);
    const tile = document.getElementById(resizeState.tileId);
    if (tile) tile.classList.toggle('resize-invalid', !valid);
    updateResizePlaceholder(resizeState.tab, candidate, valid);
    if (!valid) return;

    // Beim Ziehen bleibt die echte Kachel unveraendert. Nur der gestrichelte
    // Resize-Platzhalter zeigt das Ziel. Das verhindert fuer alle Richtungen
    // und Groessen, dass Mini-Tiles zwischen Pointer-Frames gestreckt oder
    // zusammengestaucht werden.
    resizeState.lastValidLayout = cloneLayout(candidate);
  }

  function handleTileResizeEnd() {
    stopTileResize(true);
  }

  function handleTileResizeCancel() {
    stopTileResize(false);
  }

  function beginTileResize(tab, tile, direction, e) {
    if (!tile || dragSource || resizeState) return;
    const tileIndex = parseInt(tile.dataset.index, 10);
    if (isNaN(tileIndex)) return;
    if (currentTileTab !== tab || currentTileIndex !== tileIndex) return;

    const layout = getTileElementLayout(tab, tileIndex) || getTileLayoutFromData(tab, tileIndex);
    if (!layout) return;

    e.preventDefault();
    e.stopPropagation();
    clearDragPlaceholder();

    resizeState = {
      tab,
      index: tileIndex,
      tileId: tile.id,
      direction,
      originalLayout: cloneLayout(layout),
      lastValidLayout: cloneLayout(layout),
      climateState:
        String(tile.dataset.type || '') === '17' &&
        typeof captureClimateOuterResizeState === 'function'
          ? captureClimateOuterResizeState(tab)
          : null
    };

    tile.classList.add('resizing');
    tile.draggable = false;
    document.body.classList.add('tile-resize-active');
    window.addEventListener('pointermove', handleTileResizeMove);
    window.addEventListener('pointerup', handleTileResizeEnd);
    window.addEventListener('pointercancel', handleTileResizeCancel);
  }

  function updateDragPlaceholder(tab, col, row) {
    if (!dragSource || dragSource.tab !== tab) return;
    const sourceLayout = getDragSourceLayout();
    const placeholder = ensureDragPlaceholder(tab);
    if (!sourceLayout || !placeholder) return;

    const targetCol = clampInt(col, 0, GRID_COLS - 1, sourceLayout.col);
    const targetRow = clampInt(row, firstAllowedGridRow(tab), GRID_ROWS - 1, sourceLayout.row);
    const fits = (targetCol + sourceLayout.span_w <= GRID_COLS) &&
                 (targetRow + sourceLayout.span_h <= GRID_ROWS);
    const spanW = Math.max(1, Math.min(sourceLayout.span_w, GRID_COLS - targetCol));
    const spanH = Math.max(1, Math.min(sourceLayout.span_h, GRID_ROWS - targetRow));

    placeholder.classList.toggle('invalid', !fits);
    placeholder.classList.add('show');
    setTileGridPosition(placeholder, targetCol, targetRow, spanW, spanH);
  }

  function handleGridDragMove(tab, e) {
    if (!dragSource || dragSource.tab !== tab) return;
    const cell = getGridCellFromPointer(tab, e.clientX, e.clientY);
    if (!cell) return;
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
    const sourceLayout = getDragSourceLayout();
    if (!sourceLayout) return;
    const targetCol = clampInt(cell.col, 0, GRID_COLS - 1, sourceLayout.col);
    const targetRow = clampInt(cell.row, firstAllowedGridRow(tab), GRID_ROWS - 1, sourceLayout.row);
    updateDragPlaceholder(tab, targetCol, targetRow);

    if (targetCol === sourceLayout.col && targetRow === sourceLayout.row) {
      dragSource.previewKey = targetCol + ':' + targetRow;
      dragSource.previewResult = null;
      restoreDragPreview(tab);
      return;
    }

    const previewKey = targetCol + ':' + targetRow;
    if (dragSource.previewKey === previewKey && dragSource.previewResult) return;

    const previewResult = simulateSmartReorderLayouts(tab, dragSource.index, targetCol, targetRow);
    dragSource.previewKey = previewKey;
    if (!previewResult) {
      dragSource.previewResult = null;
      const placeholder = ensureDragPlaceholder(tab);
      if (placeholder) placeholder.classList.add('invalid');
      if (!dragSource.appliedPreviewResult) restoreDragPreview(tab);
      return;
    }
    dragSource.previewResult = previewResult;
    applyDragPreviewLayouts(tab, previewResult);
  }

  function handleGridDrop(tab, e) {
    if (!dragSource || dragSource.tab !== tab) return;
    const sourceLayout = getDragSourceLayout();
    const cell = getGridCellFromPointer(tab, e.clientX, e.clientY);
    clearDragPlaceholder();
    if (!sourceLayout || !cell) return;

    e.preventDefault();
    e.stopPropagation();
    const targetCol = clampInt(cell.col, 0, GRID_COLS - 1, sourceLayout.col);
    const targetRow = clampInt(cell.row, firstAllowedGridRow(tab), GRID_ROWS - 1, sourceLayout.row);
    const fits = (targetCol + sourceLayout.span_w <= GRID_COLS) &&
                 (targetRow + sourceLayout.span_h <= GRID_ROWS);

    if (!fits) {
      showNotification(t('tileDoesNotFit'), false);
      return;
    }
    if (targetCol === sourceLayout.col && targetRow === sourceLayout.row) return;

    let previewResult = dragSource.previewResult;
    if (!previewResult || previewResult.targetCol !== targetCol || previewResult.targetRow !== targetRow) {
      previewResult = simulateSmartReorderLayouts(tab, dragSource.index, targetCol, targetRow);
    }
    if (!previewResult) {
      showNotification(t('noLayoutFound'), false);
      return;
    }

    dragSource.previewResult = previewResult;
    dragSource.dropCommitted = true;
    reorderTiles(dragSource.tab, dragSource.index, dragSource.index, targetCol, targetRow);
  }

  function syncSelectedLayoutInputs(tab, layout) {
    if (!layout) return;
    if (currentTileTab !== tab || currentTileIndex === -1) return;
    applyLayoutInputsFromLayout(tab, layout);
  }

  function captureTilePositionSnapshot(tab) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles)) return [];
    return tiles.map(tile => {
      if (!tile) return null;
      return { col: tile.col, row: tile.row };
    });
  }

  function applyLocalTileReorder(tab, previewResult) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles) || !previewResult || !Array.isArray(previewResult.layouts)) return;

    for (let i = 0; i < tiles.length; i++) {
      const tile = tiles[i];
      const layout = previewResult.layouts[i];
      if (!tile || Number(tile.type || 0) === 0 || !layout) continue;
      tile.col = layout.col;
      tile.row = layout.row;
    }

    tilesData[tab] = tiles;
    layoutTiles(tab, tiles);
    clearReflowPreviewClasses(tab);
    if (previewResult.layouts[currentTileIndex]) {
      syncSelectedLayoutInputs(tab, previewResult.layouts[currentTileIndex]);
    }
  }

  function restoreLocalTileReorder(tab, snapshot) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles) || !Array.isArray(snapshot)) return;
    for (let i = 0; i < tiles.length; i++) {
      const tile = tiles[i];
      const saved = snapshot[i];
      if (!tile || !saved) continue;
      tile.col = saved.col;
      tile.row = saved.row;
    }
    tilesData[tab] = tiles;
    layoutTiles(tab, tiles);
    clearReflowPreviewClasses(tab);
    if (currentTileIndex >= 0 && snapshot[currentTileIndex]) {
      syncSelectedLayoutInputs(tab, {
        col: snapshot[currentTileIndex].col,
        row: snapshot[currentTileIndex].row
      });
    }
  }

  function restoreDragPreviewFromSnapshot(tab, snapshot) {
    restoreLocalTileReorder(tab, snapshot);
    restoreDragPreview(tab);
  }

  function enableTileDrag(tab) {
    const grid = getTileGrid(tab);
    const tiles = document.querySelectorAll('#tab-tiles-' + tab + ' .tile');
    tiles.forEach(tile => {
      tile.addEventListener('dragstart', (e) => {
        if (resizeState) {
          e.preventDefault();
          return;
        }
        const tileIndex = parseInt(tile.dataset.index, 10);
        if (currentTileIndex !== tileIndex || currentTileTab !== tab) {
          selectTile(tileIndex, tab);
        }
        const layout = getTileElementLayout(tab, tileIndex) ||
                       getTileLayoutFromData(tab, tileIndex);
        const anchorCell = getDragAnchorCell(tab, layout, e.clientX, e.clientY);
        const grabOffset = getDragAnchorOffset(tab, layout, anchorCell.col, anchorCell.row, tile.getBoundingClientRect());
        dragSource = {
          tab,
          index: tileIndex,
          layout,
          baseLayouts: captureLayoutSnapshot(tab),
          grabCellCol: anchorCell.col,
          grabCellRow: anchorCell.row,
          previewResult: null,
          appliedPreviewResult: null,
          previewKey: '',
          dropCommitted: false
        };
        e.dataTransfer.effectAllowed = 'move';
        tile.classList.add('dragging');
        if (e.dataTransfer.setDragImage) {
          dragPreview = createDragPreview(tile);
          e.dataTransfer.setDragImage(dragPreview, grabOffset.x, grabOffset.y);
        }
      });
      tile.addEventListener('dragend', () => {
        const committedDrop = !!(dragSource && dragSource.tab === tab && dragSource.dropCommitted);
        tile.classList.remove('dragging');
        tiles.forEach(t => t.classList.remove('drop-target'));
        if (dragSource && dragSource.tab === tab && !dragSource.dropCommitted) {
          restoreDragPreview(tab);
        }
        clearReflowPreviewClasses(tab);
        clearDragPlaceholder();
        if (dragPreview && dragPreview.parentNode) dragPreview.parentNode.removeChild(dragPreview);
        dragPreview = null;
        dragSource = null;
        if (committedDrop) clearDeferredSensorRefresh();
        else flushDeferredSensorRefresh();
      });
      tile.addEventListener('dragenter', (e) => {
        handleGridDragMove(tab, e);
      });
      tile.addEventListener('dragover', (e) => {
        handleGridDragMove(tab, e);
      });
      tile.addEventListener('dragleave', () => {});
      tile.addEventListener('drop', (e) => {
        handleGridDrop(tab, e);
      });
    });
    if (!grid) return;
    grid.addEventListener('dragenter', (e) => handleGridDragMove(tab, e));
    grid.addEventListener('dragover', (e) => handleGridDragMove(tab, e));
    grid.addEventListener('drop', (e) => handleGridDrop(tab, e));
  }

  function enableTileResize(tab) {
    const grid = getTileGrid(tab);
    if (!grid || grid.dataset.resizeBound === '1') return;
    grid.dataset.resizeBound = '1';
    grid.addEventListener('pointerdown', (e) => {
      const handle = e.target.closest('.tile-resize-handle');
      if (!handle) return;
      const tile = handle.closest('.tile');
      if (!tile || tile.classList.contains('empty')) return;
      beginTileResize(tab, tile, handle.dataset.resizeDir || 'se', e);
    });
  }

  function reorderTiles(tab, fromIdx, toIdx, targetCol, targetRow) {
    let col = parseInt(targetCol, 10);
    let row = parseInt(targetRow, 10);
    if (isNaN(col)) col = -1;
    if (isNaN(row)) row = -1;
    const folderId = getFolderIdForTab(tab);
    if (folderId === undefined) {
      if (dragSource && dragSource.tab === tab) dragSource.dropCommitted = false;
      restoreDragPreview(tab);
      showNotification(t('folderNotFound'), false);
      return;
    }
    let previewResult = dragSource && dragSource.tab === tab ? dragSource.previewResult : null;
    if (!previewResult || previewResult.targetCol !== col || previewResult.targetRow !== row) {
      previewResult = simulateSmartReorderLayouts(tab, fromIdx, col, row);
    }
    if (!previewResult) {
      if (dragSource && dragSource.tab === tab) dragSource.dropCommitted = false;
      restoreDragPreview(tab);
      showNotification(t('noLayoutFound'), false);
      return;
    }
    const localSnapshot = captureTilePositionSnapshot(tab);
    applyLocalTileReorder(tab, previewResult);
    clearDragPlaceholder();
    fetch('/api/tiles/reorder', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'folder=' + encodeURIComponent(folderId) +
            '&from=' + encodeURIComponent(fromIdx) +
            '&to=' + encodeURIComponent(toIdx) +
            '&target_col=' + encodeURIComponent(col) +
            '&target_row=' + encodeURIComponent(row)
    })
    .then(res => res.json())
    .then(data => {
      if (data.success) {
        showNotification(t('tilesMovedSaved'));
        clearDeferredSensorRefresh();
        // applyLocalTileReorder hat den bestaetigten Stand bereits gesetzt.
        // Kein komplettes Grid-Reload: das wuerde sichtbar zum alten Stand
        // und wieder zur neuen Position springen koennen.
      } else {
        if (dragSource && dragSource.tab === tab) dragSource.dropCommitted = false;
        clearDeferredSensorRefresh();
        restoreDragPreviewFromSnapshot(tab, localSnapshot);
        showNotification(t('moveFailed'), false);
      }
    })
    .catch(() => {
      if (dragSource && dragSource.tab === tab) dragSource.dropCommitted = false;
      clearDeferredSensorRefresh();
      restoreDragPreviewFromSnapshot(tab, localSnapshot);
      showNotification(t('networkErrorMove'), false);
    });
  }

  function loadTileDataAndSelect(tab, index) { selectTile(index, tab); }

  function getTopLeftConfiguredTileIndex(tab) {
    let selectedIndex = -1;
    let selectedRow = Number.MAX_SAFE_INTEGER;
    let selectedCol = Number.MAX_SAFE_INTEGER;
    document.querySelectorAll('#tab-tiles-' + tab + ' .tile').forEach(tile => {
      const index = parseInt(tile.dataset.index, 10);
      if (isNaN(index) || Number(tile.dataset.type || 0) === 0) return;
      const row = parseInt(tile.style.gridRowStart, 10);
      const col = parseInt(tile.style.gridColumnStart, 10);
      const safeRow = isNaN(row) ? Number.MAX_SAFE_INTEGER : row;
      const safeCol = isNaN(col) ? Number.MAX_SAFE_INTEGER : col;
      if (safeRow < selectedRow || (safeRow === selectedRow && safeCol < selectedCol)) {
        selectedIndex = index;
        selectedRow = safeRow;
        selectedCol = safeCol;
      }
    });
    return selectedIndex >= 0 ? selectedIndex : 0;
  }

  let screensaverDraft = null;
  let screensaverLoaded = false;
  let screensaverLoading = false;
  let screensaverSelected = { kind: 'background', index: -1 };
  let screensaverWallpaperIndex = -1;
  let screensaverSaveTimer = null;
  const screensaverTimeFontSizes = [20, 24, 28, 32, 40, 48, 56, 64, 72, 80, 96];
  const screensaverDateFontSizes = [20, 24, 28, 32, 40, 48, 56, 64, 72];

  function invalidateScreensaverEditor() {
    screensaverLoaded = false;
    screensaverLoading = false;
    screensaverDraft = null;
    screensaverWallpaperIndex = -1;
  }

  function ssClamp(value, min, max) {
    const n = Number(value);
    return Math.max(min, Math.min(max, Number.isFinite(n) ? n : min));
  }

  function ssNearestClockFont(value, dateLine = false) {
    const wanted = Number(value) || 20;
    const sizes = dateLine ? screensaverDateFontSizes : screensaverTimeFontSizes;
    return sizes.reduce((best, size) =>
      Math.abs(size - wanted) < Math.abs(best - wanted) ? size : best,
      sizes[0]);
  }

  function ssClockAlignment(value) {
    return Math.round(ssClamp(value ?? 1, 0, 2));
  }

  function ssClockAlignmentCss(value) {
    return ['left', 'center', 'right'][ssClockAlignment(value)];
  }

  function getScreensaverClockPreviewDate(d) {
    if (!d) return '';
    const weekdayNames = getClockPreviewLanguage().toLowerCase().startsWith('de')
      ? ['Sonntag', 'Montag', 'Dienstag', 'Mittwoch', 'Donnerstag', 'Freitag', 'Samstag']
      : ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
    let text = d.show_date ? getClockPreviewDate(d.date_format) : '';
    if (d.show_weekday) {
      text = weekdayNames[new Date().getDay()] + (text ? ', ' + text : '');
    }
    return text;
  }

  function ssNormalizeLoaded(data) {
    data.time_font_size = ssNearestClockFont(data.time_font_size || 48, false);
    data.date_font_size = ssNearestClockFont(data.date_font_size || 28, true);
    data.time_alignment = ssClockAlignment(data.time_alignment);
    data.date_alignment = ssClockAlignment(data.date_alignment);
    data.tile_border = data.tile_border !== false;
    data.wallpapers = Array.isArray(data.wallpapers) ? data.wallpapers : [];
    data.duration_seconds = Math.round(ssClamp(
      data.duration_seconds ?? 15, 3, 3600));
    const configured = new Map(data.wallpapers.map(item => [item.file_name, item]));
    const hadConfiguredWallpapers = data.wallpapers.length > 0;
    (data.available_wallpapers || []).forEach(name => {
      if (!configured.has(name)) {
        data.wallpapers.push({
          file_name: name, enabled: !hadConfiguredWallpapers,
          focus_x: 500, focus_y: 500, zoom: 1000
        });
      }
    });
    screensaverWallpaperIndex = data.wallpapers.findIndex(w => w.enabled);
    if (screensaverWallpaperIndex < 0 && data.wallpapers.length) screensaverWallpaperIndex = 0;
    return data;
  }

  function initScreensaverEditor() {
    if (screensaverLoaded || screensaverLoading) {
      if (screensaverLoaded) renderScreensaverEditor();
      return;
    }
    screensaverLoading = true;
    fetch('/api/screensaver').then(r => r.json()).then(config => {
      if (!config || !config.success) throw new Error('screensaver config');
      screensaverDraft = ssNormalizeLoaded(config);
      screensaverLoaded = true;
      bindScreensaverEditor();
      selectScreensaverBackground();
      renderScreensaverEditor();
    }).catch(() => showNotification(t('screensaverLoadFailed'), false))
      .finally(() => { screensaverLoading = false; });
  }

  function ssPayload() {
    const d = screensaverDraft;
    return {
      version: 2,
      use_wallpapers: !!d.use_wallpapers,
      shuffle: !!d.shuffle,
      tile_shadow: !!d.tile_shadow,
      tile_border: d.tile_border !== false,
      show_time: !!d.show_time,
      show_date: !!d.show_date,
      show_weekday: !!d.show_weekday,
      clock_shadow: !!d.clock_shadow,
      time_format: Number(d.time_format || 0),
      date_format: Number(d.date_format || 0),
      time_alignment: ssClockAlignment(d.time_alignment),
      date_alignment: ssClockAlignment(d.date_alignment),
      time_font_size: ssNearestClockFont(d.time_font_size || 48, false),
      date_font_size: ssNearestClockFont(d.date_font_size || 28, true),
      clock_x: Math.round(ssClamp(d.clock_x, 0, 1000)),
      clock_y: Math.round(ssClamp(d.clock_y, 0, 1000)),
      duration_seconds: Math.round(ssClamp(d.duration_seconds, 3, 3600)),
      preview_wallpaper: ssCurrentWallpaper()?.file_name || '',
      wallpapers: d.wallpapers.map(w => ({
        file_name: w.file_name, enabled: !!w.enabled,
        focus_x: Math.round(ssClamp(w.focus_x, 0, 1000)),
        focus_y: Math.round(ssClamp(w.focus_y, 0, 1000)),
        zoom: Math.round(ssClamp(w.zoom, 1000, 3000))
      }))
    };
  }

  function scheduleScreensaverSave() {
    if (!screensaverLoaded) return;
    window.clearTimeout(screensaverSaveTimer);
    screensaverSaveTimer = window.setTimeout(saveScreensaverNow, 650);
  }

  function saveScreensaverNow() {
    if (!screensaverLoaded) return;
    fetch('/api/screensaver', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(ssPayload())
    }).then(async response => {
      const result = await response.json().catch(() => ({}));
      if (!response.ok || !result.success) throw new Error(result.error || 'save');
      showNotification(t('screensaverSaved'));
    }).catch(() => {
      showNotification(t('screensaverSaveFailed'), false);
    });
  }

  function ssCurrentWallpaper() {
    if (!screensaverDraft || screensaverWallpaperIndex < 0) return null;
    return screensaverDraft.wallpapers[screensaverWallpaperIndex] || null;
  }

  function selectScreensaverBackground() {
    screensaverSelected = { kind: 'background', index: -1 };
    const bg = document.getElementById('screensaverBackgroundSettings');
    if (bg) bg.classList.remove('hidden');
    document.getElementById('screensaverClockSettings')?.classList.add('hidden');
    document.getElementById('screensaverClock')?.classList.remove('selected-clock');
    const settings = document.getElementById('screensaverSettings');
    settings?.querySelector('.tile-specific-settings')?.classList.add('hidden');
    document.querySelectorAll('#tab-tiles-screensaver .tile').forEach(tile => {
      tile.classList.remove('active');
      delete tile.dataset.selected;
    });
    document.getElementById('screensaverGrid')?.classList.add('selected-background');
    currentTileIndex = -1;
    currentTileTab = 'screensaver';
    renderScreensaverEditor();
  }

  function selectScreensaverClock() {
    screensaverSelected = { kind: 'clock', index: -1 };
    document.getElementById('screensaverBackgroundSettings')?.classList.add('hidden');
    document.getElementById('screensaverClockSettings')?.classList.remove('hidden');
    document.getElementById('screensaverGrid')?.classList.remove('selected-background');
    const settings = document.getElementById('screensaverSettings');
    settings?.querySelector('.tile-specific-settings')?.classList.add('hidden');
    document.querySelectorAll('#tab-tiles-screensaver .tile').forEach(tile => {
      tile.classList.remove('active');
      delete tile.dataset.selected;
    });
    currentTileIndex = -1;
    currentTileTab = 'screensaver';
    renderScreensaverEditor();
  }

  function renderScreensaverWallpapers() {
    const list = document.getElementById('screensaverWallpaperList');
    if (!list || !screensaverDraft) return;
    list.innerHTML = '';
    screensaverDraft.wallpapers.forEach((wallpaper, index) => {
      const row = document.createElement('div');
      row.className = 'screensaver-wallpaper-row' + (index === screensaverWallpaperIndex ? ' active' : '');
      const enabled = document.createElement('input');
      enabled.type = 'checkbox'; enabled.checked = !!wallpaper.enabled;
      enabled.addEventListener('change', () => {
        wallpaper.enabled = enabled.checked;
        screensaverWallpaperIndex = index;
        renderScreensaverEditor(); scheduleScreensaverSave();
      });
      const name = document.createElement('div');
      name.className = 'screensaver-wallpaper-name'; name.textContent = wallpaper.file_name;
      name.addEventListener('click', () => {
        screensaverWallpaperIndex = index;
        renderScreensaverEditor();
      });
      // Gleiche Chevron-Grafik wie der Pfeil der Select-Felder.
      const chevronSvg = dir =>
        '<svg width="12" height="8" viewBox="0 0 12 8" aria-hidden="true"><path d="' +
        (dir < 0 ? 'M1 6.5l5-5 5 5' : 'M1 1.5l5 5 5-5') +
        '" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/></svg>';
      const up = document.createElement('button');
      up.type = 'button'; up.className = 'screensaver-wallpaper-move'; up.innerHTML = chevronSvg(-1);
      up.disabled = index === 0;
      up.addEventListener('click', () => {
        if (index === 0) return;
        [screensaverDraft.wallpapers[index - 1], screensaverDraft.wallpapers[index]] =
          [screensaverDraft.wallpapers[index], screensaverDraft.wallpapers[index - 1]];
        screensaverWallpaperIndex = index - 1; renderScreensaverEditor(); scheduleScreensaverSave();
      });
      const down = document.createElement('button');
      down.type = 'button'; down.className = 'screensaver-wallpaper-move'; down.innerHTML = chevronSvg(1);
      down.disabled = index === screensaverDraft.wallpapers.length - 1;
      down.addEventListener('click', () => {
        if (down.disabled) return;
        [screensaverDraft.wallpapers[index + 1], screensaverDraft.wallpapers[index]] =
          [screensaverDraft.wallpapers[index], screensaverDraft.wallpapers[index + 1]];
        screensaverWallpaperIndex = index + 1; renderScreensaverEditor(); scheduleScreensaverSave();
      });
      row.append(enabled, name, up, down); list.appendChild(row);
    });
    if (!screensaverDraft.wallpapers.length) {
      const empty = document.createElement('div');
      empty.className = 'screensaver-save-state';
      empty.textContent = t('screensaverNoWallpapers');
      list.appendChild(empty);
    }
  }

  function renderScreensaverEditor() {
    if (!screensaverDraft) return;
    const d = screensaverDraft;
    const preview = document.getElementById('screensaverGrid');
    const image = document.getElementById('screensaverPreviewImage');
    const clock = document.getElementById('screensaverClock');
    if (!preview || !image || !clock) return;
    preview.classList.toggle('selected-background', screensaverSelected.kind === 'background');
    clock.classList.toggle('selected-clock', screensaverSelected.kind === 'clock');
    const width = preview.getBoundingClientRect().width || 800;
    const scale = width / Number(d.screen_width || 1280);
    const rootStyles = getComputedStyle(document.documentElement);
    const devicePx = (name, fallback) => {
      const value = parseFloat(rootStyles.getPropertyValue(name));
      return Number.isFinite(value) && value > 0 ? value : fallback;
    };
    const deviceClockFontPx = (raw, fallback) => {
      const requested = Number(raw || fallback);
      return devicePx('--screensaver-fs' + requested, requested);
    };
    clock.style.setProperty(
      '--screensaver-clock-shadow-2',
      (devicePx('--screensaver-shadow-2', 2) * scale) + 'px');
    clock.style.setProperty(
      '--screensaver-clock-shadow-4',
      (devicePx('--screensaver-shadow-4', 4) * scale) + 'px');
    clock.style.setProperty(
      '--screensaver-clock-shadow-6',
      (devicePx('--screensaver-shadow-6', 6) * scale) + 'px');
    const wallpaper = ssCurrentWallpaper();
    if (d.use_wallpapers && wallpaper && wallpaper.file_name) {
      const wanted = '/api/screensaver/wallpaper?name=' + encodeURIComponent(wallpaper.file_name);
      if (image.dataset.src !== wanted) { image.src = wanted; image.dataset.src = wanted; }
      image.hidden = false;
      image.style.display = 'block';
      image.style.objectPosition = (wallpaper.focus_x / 10) + '% ' + (wallpaper.focus_y / 10) + '%';
      image.style.transformOrigin = (wallpaper.focus_x / 10) + '% ' + (wallpaper.focus_y / 10) + '%';
      image.style.transform = 'scale(' + (wallpaper.zoom / 1000) + ')';
    } else {
      image.hidden = true;
      image.style.display = 'none';
      image.removeAttribute('src');
      delete image.dataset.src;
    }
    clock.style.left = (d.clock_x / 10) + '%';
    clock.style.top = (d.clock_y / 10) + '%';
    const time = document.getElementById('screensaverClockTime');
    const date = document.getElementById('screensaverClockDate');
    time.hidden = !d.show_time;
    date.hidden = !d.show_date && !d.show_weekday;
    time.style.fontSize =
      Math.max(10, deviceClockFontPx(d.time_font_size, 48) * scale) + 'px';
    date.style.fontSize =
      Math.max(8, deviceClockFontPx(d.date_font_size, 28) * scale) + 'px';
    time.textContent = getClockPreviewTime(d.time_format);
    date.textContent = getScreensaverClockPreviewDate(d);
    time.style.width = 'auto';
    date.style.width = 'auto';
    time.style.textAlign = ssClockAlignmentCss(d.time_alignment);
    date.style.textAlign = ssClockAlignmentCss(d.date_alignment);
    // Beide Zeilen erhalten die Breite der laengeren Zeile. Dadurch ist die
    // Ausrichtung im Browser dieselbe wie im kompakten LVGL-Uhr-Container.
    const clockLineWidth = Math.ceil(Math.max(
      time.hidden ? 0 : time.getBoundingClientRect().width,
      date.hidden ? 0 : date.getBoundingClientRect().width));
    if (!time.hidden && clockLineWidth) time.style.width = clockLineWidth + 'px';
    if (!date.hidden && clockLineWidth) date.style.width = clockLineWidth + 'px';
    clock.hidden = false;
    clock.classList.toggle('clock-disabled', !d.show_time && !d.show_date && !d.show_weekday);
    clock.classList.toggle('clock-shadowed', !!d.clock_shadow);
    preview.classList.toggle('tiles-shadowed', !!d.tile_shadow);
    preview.classList.toggle('tiles-bordered', d.tile_border !== false);
    document.getElementById('screensaverUseWallpapers').checked = !!d.use_wallpapers;
    document.getElementById('screensaverShuffle').checked = !!d.shuffle;
    document.getElementById('screensaverTileShadow').checked = !!d.tile_shadow;
    document.getElementById('screensaverTileBorder').checked = d.tile_border !== false;
    document.getElementById('screensaverShowTime').checked = !!d.show_time;
    document.getElementById('screensaverShowDate').checked = !!d.show_date;
    document.getElementById('screensaverShowWeekday').checked = !!d.show_weekday;
    document.getElementById('screensaverClockShadow').checked = !!d.clock_shadow;
    document.getElementById('screensaverTimeFont').value = String(d.time_font_size || 48);
    document.getElementById('screensaverDateFont').value = String(d.date_font_size || 28);
    document.getElementById('screensaverTimeAlignment').value = String(ssClockAlignment(d.time_alignment));
    document.getElementById('screensaverDateAlignment').value = String(ssClockAlignment(d.date_alignment));
    document.getElementById('screensaverTimeFormat').value = String(d.time_format || 0);
    document.getElementById('screensaverDateFormat').value = String(d.date_format || 0);
    const controls = document.getElementById('screensaverWallpaperControls');
    controls.hidden = !wallpaper;
    if (wallpaper) {
      document.getElementById('screensaverWallpaperDuration').value = d.duration_seconds;
      document.getElementById('screensaverWallpaperZoom').value = wallpaper.zoom;
      document.getElementById('screensaverFocusX').value = wallpaper.focus_x;
      document.getElementById('screensaverFocusY').value = wallpaper.focus_y;
    }
    renderScreensaverWallpapers();
  }

  function bindScreensaverEditor() {
    const preview = document.getElementById('screensaverGrid');
    const clock = document.getElementById('screensaverClock');
    const clockResize = clock?.querySelector('.screensaver-clock-resize-handle');
    if (!preview || preview.dataset.bound === '1') return;
    preview.dataset.bound = '1';
    preview.addEventListener('click', e => {
      if (!e.target.closest('.tile') && !e.target.closest('#screensaverClock')) {
        selectScreensaverBackground();
      }
    });
    let backgroundDrag = null;
    preview.addEventListener('pointerdown', e => {
      if (e.target.closest('.tile') || e.target.closest('#screensaverClock')) return;
      selectScreensaverBackground();
      const wallpaper = ssCurrentWallpaper();
      if (!wallpaper) return;
      backgroundDrag = { id: e.pointerId, x: e.clientX, y: e.clientY,
                         fx: Number(wallpaper.focus_x), fy: Number(wallpaper.focus_y) };
      preview.setPointerCapture(e.pointerId);
    });
    preview.addEventListener('pointermove', e => {
      if (!backgroundDrag || backgroundDrag.id !== e.pointerId) return;
      const wallpaper = ssCurrentWallpaper(); const rect = preview.getBoundingClientRect();
      wallpaper.focus_x = Math.round(ssClamp(backgroundDrag.fx - (e.clientX - backgroundDrag.x) * 1000 / rect.width, 0, 1000));
      wallpaper.focus_y = Math.round(ssClamp(backgroundDrag.fy - (e.clientY - backgroundDrag.y) * 1000 / rect.height, 0, 1000));
      renderScreensaverEditor();
    });
    const finishBackgroundDrag = e => {
      if (!backgroundDrag || backgroundDrag.id !== e.pointerId) return;
      backgroundDrag = null; scheduleScreensaverSave();
    };
    preview.addEventListener('pointerup', finishBackgroundDrag);
    preview.addEventListener('pointercancel', finishBackgroundDrag);
    let clockDrag = null;
    clock.addEventListener('pointerdown', e => {
      if (e.target.closest('.screensaver-clock-resize-handle')) return;
      e.stopPropagation();
      const clockRect = clock.getBoundingClientRect();
      clockDrag = {
        id: e.pointerId,
        offsetX: e.clientX - (clockRect.left + clockRect.width / 2),
        offsetY: e.clientY - (clockRect.top + clockRect.height / 2)
      };
      clock.setPointerCapture(e.pointerId);
      clock.classList.add('dragging'); selectScreensaverClock();
    });
    clock.addEventListener('pointermove', e => {
      if (!clockDrag || clockDrag.id !== e.pointerId) return;
      const rect = preview.getBoundingClientRect();
      const centerX = e.clientX - clockDrag.offsetX;
      const centerY = e.clientY - clockDrag.offsetY;
      screensaverDraft.clock_x = Math.round(ssClamp((centerX - rect.left) * 1000 / rect.width, 0, 1000));
      screensaverDraft.clock_y = Math.round(ssClamp((centerY - rect.top) * 1000 / rect.height, 0, 1000));
      renderScreensaverEditor();
    });
    const finishClockDrag = e => {
      if (!clockDrag || clockDrag.id !== e.pointerId) return;
      clockDrag = null; clock.classList.remove('dragging'); scheduleScreensaverSave();
    };
    clock.addEventListener('pointerup', finishClockDrag);
    clock.addEventListener('pointercancel', finishClockDrag);
    clock.addEventListener('click', e => {
      e.stopPropagation();
      selectScreensaverClock();
    });

    let clockResizeDrag = null;
    if (clockResize) {
      clockResize.addEventListener('pointerdown', e => {
        e.preventDefault();
        e.stopPropagation();
        selectScreensaverClock();
        const rect = clock.getBoundingClientRect();
        clockResizeDrag = {
          id: e.pointerId,
          x: e.clientX,
          y: e.clientY,
          width: Math.max(1, rect.width),
          height: Math.max(1, rect.height),
          timeFont: Number(screensaverDraft.time_font_size || 48),
          dateFont: Number(screensaverDraft.date_font_size || 28)
        };
        clockResize.setPointerCapture(e.pointerId);
      });
      clockResize.addEventListener('pointermove', e => {
        if (!clockResizeDrag || clockResizeDrag.id !== e.pointerId) return;
        const widthFactor = (clockResizeDrag.width + e.clientX - clockResizeDrag.x) /
                            clockResizeDrag.width;
        const heightFactor = (clockResizeDrag.height + e.clientY - clockResizeDrag.y) /
                             clockResizeDrag.height;
        const factor = ssClamp(Math.max(widthFactor, heightFactor), 0.35, 3.0);
        screensaverDraft.time_font_size = ssNearestClockFont(
          clockResizeDrag.timeFont * factor, false);
        screensaverDraft.date_font_size = ssNearestClockFont(
          clockResizeDrag.dateFont * factor, true);
        renderScreensaverEditor();
      });
      const finishClockResize = e => {
        if (!clockResizeDrag || clockResizeDrag.id !== e.pointerId) return;
        clockResizeDrag = null;
        scheduleScreensaverSave();
      };
      clockResize.addEventListener('pointerup', finishClockResize);
      clockResize.addEventListener('pointercancel', finishClockResize);
    }

    const bind = (id, event, fn, save = true) => {
      const element = document.getElementById(id); if (!element) return;
      element.addEventListener(event, () => { fn(element); renderScreensaverEditor(); if (save) scheduleScreensaverSave(); });
    };
    bind('screensaverUseWallpapers', 'change', el => { screensaverDraft.use_wallpapers = el.checked; });
    bind('screensaverShuffle', 'change', el => { screensaverDraft.shuffle = el.checked; });
    bind('screensaverTileShadow', 'change', el => { screensaverDraft.tile_shadow = el.checked; });
    bind('screensaverTileBorder', 'change', el => { screensaverDraft.tile_border = el.checked; });
    bind('screensaverShowTime', 'change', el => { screensaverDraft.show_time = el.checked; });
    bind('screensaverShowDate', 'change', el => { screensaverDraft.show_date = el.checked; });
    bind('screensaverShowWeekday', 'change', el => { screensaverDraft.show_weekday = el.checked; });
    bind('screensaverClockShadow', 'change', el => { screensaverDraft.clock_shadow = el.checked; });
    bind('screensaverTimeFont', 'change', el => {
      screensaverDraft.time_font_size = ssNearestClockFont(el.value, false);
    });
    bind('screensaverDateFont', 'change', el => {
      screensaverDraft.date_font_size = ssNearestClockFont(el.value, true);
    });
    bind('screensaverTimeAlignment', 'change', el => {
      screensaverDraft.time_alignment = ssClockAlignment(el.value);
    });
    bind('screensaverDateAlignment', 'change', el => {
      screensaverDraft.date_alignment = ssClockAlignment(el.value);
    });
    bind('screensaverTimeFormat', 'change', el => { screensaverDraft.time_format = Number(el.value); });
    bind('screensaverDateFormat', 'change', el => { screensaverDraft.date_format = Number(el.value); });
    bind('screensaverWallpaperDuration', 'change', el => {
      screensaverDraft.duration_seconds = Math.round(ssClamp(el.value, 3, 3600));
    });
    bind('screensaverWallpaperZoom', 'input', el => { const w = ssCurrentWallpaper(); if (w) w.zoom = Number(el.value); }, false);
    bind('screensaverWallpaperZoom', 'change', el => { const w = ssCurrentWallpaper(); if (w) w.zoom = Number(el.value); });
    bind('screensaverFocusX', 'input', el => { const w = ssCurrentWallpaper(); if (w) w.focus_x = Number(el.value); }, false);
    bind('screensaverFocusX', 'change', el => { const w = ssCurrentWallpaper(); if (w) w.focus_x = Number(el.value); });
    bind('screensaverFocusY', 'input', el => { const w = ssCurrentWallpaper(); if (w) w.focus_y = Number(el.value); }, false);
    bind('screensaverFocusY', 'change', el => { const w = ssCurrentWallpaper(); if (w) w.focus_y = Number(el.value); });

    window.addEventListener('resize', () => { if (screensaverLoaded) renderScreensaverEditor(); });
  }

  document.addEventListener('DOMContentLoaded', () => {
    toggleStaticNetworkFields();
    toggleNetworkSettings();
    initAdminSettingsSave();
    initTileTabs();
    loadSelectedTileStates();
    loadDraftsFromStorage();
    loadTileClipboard();
    // Nach einem Browser-Neuladen auf dem zuletzt geoeffneten Admin-Tab
    // bleiben. Nur wenn dieser nicht mehr existiert, auf Home zurueckfallen.
    let initialTab = '';
    try { initialTab = localStorage.getItem('activeAdminTab') || ''; } catch (e) {}
    const homeTab = tabByFolder[0] || tileTabs[0];
    if (initialTab && document.getElementById(initialTab)) {
      switchTab(initialTab);
    } else if (homeTab) {
      switchTab('tab-tiles-' + homeTab);
    } else {
      switchTab('tab-network');
    }
    loadSensorValues(true, true);
    setInterval(() => { if (!document.hidden && !fileManagerUploadBusy) loadSensorValues(false, false); }, 15000);
    tileTabs.forEach(tab => {
      enableTileDrag(tab);
      enableTileResize(tab);
    });
    fillStaticClockPreviews();
    setInterval(fillStaticClockPreviews, 30000);
    updateTileSettingsMaxHeight();
  });
  </script>
)html";

  append_tile_type_scripts(html);
}

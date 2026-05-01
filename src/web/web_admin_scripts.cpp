#include "src/web/web_admin_scripts.h"
#include "src/types/types_registry.h"
#include "src/tiles/tile_config.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

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
    if (tabName === 'tab-network') {
      window.setTimeout(() => {
        if (typeof loadFileManager === 'function' && !fileManagerLoaded) loadFileManager();
      }, 0);
    }
  }

  function toggleStaticWifiFields() {
    const toggle = document.getElementById('wifi_use_static');
    const fields = document.getElementById('wifi_static_fields');
    if (!toggle || !fields) return;
    fields.classList.toggle('is-hidden', !toggle.checked);
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
      link.download = 'ui_screenshot.bmp';
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
    const file = inputEl && inputEl.files && inputEl.files.length ? inputEl.files[0] : null;
    nameEl.textContent = file
      ? file.name
      : fileManagerText('Keine Datei ausgew\u00e4hlt', 'No file selected');
  }

  function formatFileManagerSize(entry) {
    if (!entry || entry.dir) return '-';
    let size = Number(entry.size || 0);
    if (!Number.isFinite(size) || size < 0) size = 0;
    if (size < 1024) return size + ' B';
    if (size < 1024 * 1024) return (size / 1024).toFixed(size < 10 * 1024 ? 1 : 0) + ' KB';
    return (size / (1024 * 1024)).toFixed(size < 10 * 1024 * 1024 ? 1 : 0) + ' MB';
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
      showNotification(fileManagerText('Gel\u00f6scht.', 'Deleted.'));
      loadFileManager();
    } catch (err) {
      showNotification(err?.message || fileManagerText('L\u00f6schen fehlgeschlagen.', 'Delete failed.'), false);
    }
  }

  function uploadFileManagerFile() {
    const input = document.getElementById('file_manager_upload');
    if (!input || !input.files || !input.files.length) {
      showNotification(fileManagerText('Bitte zuerst eine Datei ausw\u00e4hlen.', 'Select a file first.'), false);
      return;
    }
    const file = input.files[0];
    const formData = new FormData();
    formData.append('file', file);
    setFileManagerStatus(fileManagerText('Upload l\u00e4uft...', 'Uploading...'));

    const xhr = new XMLHttpRequest();
    xhr.open('POST', fileManagerUrl('/api/files/upload', {
      fs: fileManagerState.fs,
      path: fileManagerState.path || '/'
    }), true);
    xhr.onload = () => {
      let data = {};
      try { data = JSON.parse(xhr.responseText || '{}'); } catch (e) {}
      if (xhr.status < 200 || xhr.status >= 300 || !data.success) {
        const message = data.error || fileManagerText('Upload fehlgeschlagen.', 'Upload failed.');
        setFileManagerStatus(message, false);
        showNotification(message, false);
        return;
      }
      input.value = '';
      updateFileManagerUploadName(input);
      setFileManagerStatus(fileManagerText('Upload abgeschlossen.', 'Upload complete.'), true);
      showNotification(fileManagerText('Upload abgeschlossen.', 'Upload complete.'));
      loadFileManager();
    };
    xhr.onerror = () => {
      const message = fileManagerText('Upload fehlgeschlagen.', 'Upload failed.');
      setFileManagerStatus(message, false);
      showNotification(message, false);
    };
    xhr.send(formData);
  }

  async function uploadOtaFirmware() {
    const input = document.getElementById('ota_file');
    const button = document.getElementById('ota_upload_btn');
    const chooseBtn = document.getElementById('ota_choose_btn');
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
    input.disabled = true;
    setOtaUi(t('otaUploading') + ' 0%', 'busy', '', 0);
    showNotification(t('otaUploading'));

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
      input.disabled = false;
    };

    xhr.send(formData);
  }

  // Tile Editor State
)html";
  html += "  const GRID_COLS = " + String(GRID_COLS) + ";\n";
  html += "  const GRID_ROWS = " + String(GRID_ROWS) + ";\n";
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

  function normalizeSensorMetaPayload(payload) {
    if (!payload || typeof payload !== 'object') {
      return { values: {}, units: {}, icons: {}, names: {}, loaded: false };
    }
    const hasMeta = Object.prototype.hasOwnProperty.call(payload, 'values') ||
                    Object.prototype.hasOwnProperty.call(payload, 'units') ||
                    Object.prototype.hasOwnProperty.call(payload, 'icons') ||
                    Object.prototype.hasOwnProperty.call(payload, 'names');
    if (!hasMeta) {
      return { values: payload || {}, units: {}, icons: {}, names: {}, loaded: true };
    }
    return {
      values: payload.values || {},
      units: payload.units || {},
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

  function normalizeSnapshotLayout(snapshot, index) {
    const fallbackCol = (index >= 0) ? ((index % GRID_COLS) + 1) : 1;
    const fallbackRow = (index >= 0) ? (Math.floor(index / GRID_COLS) + 1) : 1;
    let col = clampInt(snapshot?.col, 1, GRID_COLS, fallbackCol);
    let row = clampInt(snapshot?.row, 1, GRID_ROWS, fallbackRow);
    let spanW = clampInt(snapshot?.span_w, 1, GRID_COLS, 1);
    let spanH = clampInt(snapshot?.span_h, 1, GRID_ROWS, 1);
    const maxSpanW = GRID_COLS - (col - 1);
    const maxSpanH = GRID_ROWS - (row - 1);
    if (spanW > maxSpanW) spanW = maxSpanW;
    if (spanH > maxSpanH) spanH = maxSpanH;
    return { col: col - 1, row: row - 1, span_w: spanW, span_h: spanH };
  }

  function buildTileSnapshotFromInputs(tab) {
    const prefix = tab;
    const snapshot = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
      col: document.getElementById(prefix + '_tile_col')?.value || '1',
      row: document.getElementById(prefix + '_tile_row')?.value || '1',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
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
    const layout = normalizeSnapshotLayout(snapshot, index);
    const numericFields = ['type', 'sensor_decimals', 'sensor_value_font', 'sensor_display_mode', 'sensor_gauge_min', 'sensor_gauge_max', 'switch_style', 'navigate_target', 'popup_open_mode', 'key_code', 'key_modifier'];

    tile.type = clampInt(snapshot?.type, 0, 255, Number(prev.type) || 0);
    tile.title = snapshot?.title || '';
    tile.icon_name = snapshot?.icon || '';
    tile.bg_color = hexToRgb(snapshot?.color || '#2A2A2A');
    tile.col = layout.col;
    tile.row = layout.row;
    tile.span_w = layout.span_w;
    tile.span_h = layout.span_h;

    for (const [key, value] of Object.entries(snapshot || {})) {
      if (key === '_dirty' || key === '_rev' || key === 'icon' || key === 'color' || key === 'col' || key === 'row' || key === 'span_w' || key === 'span_h' || key === 'type' || key === 'title') continue;
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
  function restoreCurrentTileSelectionUi() {
    if (currentTileIndex === -1 || !currentTileTab) return;
    document.querySelectorAll('.tile').forEach(t => t.classList.remove('active'));
    const settingsId = currentTileTab + 'Settings';
    document.getElementById(settingsId)?.classList.remove('hidden');
    const activeTile = document.getElementById(currentTileTab + '-tile-' + currentTileIndex);
    if (activeTile) activeTile.classList.add('active');
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

    const networkBtn = Array.from(nav.querySelectorAll('.tab-btn')).find(btn => btn.getAttribute('onclick')?.includes("'tab-network'"));
    if (networkBtn) nav.insertBefore(buttonEl, networkBtn);
    else nav.appendChild(buttonEl);
    networkTab.parentNode.insertBefore(tabEl, networkTab);

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
        localStorage.setItem(SELECTED_TILE_STORAGE_KEY, JSON.stringify({ tab: currentTileTab, index: currentTileIndex }));
      } else {
        localStorage.removeItem(SELECTED_TILE_STORAGE_KEY);
      }
    } catch (e) {}
  }
  function restoreSelectedTileState() {
    try {
      const raw = localStorage.getItem(SELECTED_TILE_STORAGE_KEY);
      if (!raw) return false;
      const saved = JSON.parse(raw);
      const tab = saved && typeof saved.tab === 'string' ? saved.tab : '';
      const index = Number(saved && saved.index);
      if (!tab || !Number.isInteger(index) || index < 0 || index >= TILES_PER_GRID) {
        localStorage.removeItem(SELECTED_TILE_STORAGE_KEY);
        return false;
      }
      if (!document.getElementById(tab) || !document.getElementById(tab + '-tile-' + index)) {
        localStorage.removeItem(SELECTED_TILE_STORAGE_KEY);
        return false;
      }
      selectTile(index, tab);
      return true;
    } catch (e) {
      try { localStorage.removeItem(SELECTED_TILE_STORAGE_KEY); } catch (ignored) {}
      return false;
    }
  }
  function clampInt(value, min, max, fallback) {
    const v = parseInt(value, 10);
    if (isNaN(v)) return fallback !== undefined ? fallback : min;
    if (v < min) return min;
    if (v > max) return max;
    return v;
  }

  function normalizeTileLayout(tile, index) {
    const fallbackCol = index % GRID_COLS;
    const fallbackRow = Math.floor(index / GRID_COLS);
    const col = clampInt(tile?.col, 0, GRID_COLS - 1, fallbackCol);
    const row = clampInt(tile?.row, 0, GRID_ROWS - 1, fallbackRow);
    let spanW = clampInt(tile?.span_w, 1, GRID_COLS, 1);
    let spanH = clampInt(tile?.span_h, 1, GRID_ROWS, 1);
    if (spanW > GRID_COLS - col) spanW = GRID_COLS - col;
    if (spanH > GRID_ROWS - row) spanH = GRID_ROWS - row;
    return { col, row, span_w: spanW, span_h: spanH };
  }

  function setTileGridPosition(el, col, row, spanW, spanH) {
    if (!el) return;
    el.style.gridColumn = (col + 1) + ' / span ' + spanW;
    el.style.gridRow = (row + 1) + ' / span ' + spanH;
    el.dataset.col = String(col);
    el.dataset.row = String(row);
    el.dataset.spanW = String(spanW);
    el.dataset.spanH = String(spanH);
  }

  function getTileElementLayout(tab, index) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return null;
    const col = clampInt(el.dataset.col, 0, GRID_COLS - 1, null);
    const row = clampInt(el.dataset.row, 0, GRID_ROWS - 1, null);
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
      const layout = normalizeTileLayout(tile, idx);
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
    for (let r = 0; r < GRID_ROWS; r++) {
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
    let row = clampInt(rowEl.value, 1, GRID_ROWS, 1);
    let spanW = clampInt(spanWEl.value, 1, GRID_COLS, 1);
    let spanH = clampInt(spanHEl.value, 1, GRID_ROWS, 1);

    const maxSpanW = GRID_COLS - (col - 1);
    const maxSpanH = GRID_ROWS - (row - 1);
    if (spanW > maxSpanW) spanW = maxSpanW;
    if (spanH > maxSpanH) spanH = maxSpanH;

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
    const d = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
      col: document.getElementById(prefix + '_tile_col')?.value || '1',
      row: document.getElementById(prefix + '_tile_row')?.value || '1',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
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
    document.getElementById(prefix + '_tile_color').value = d.color || '#2A2A2A';
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
    const data = {
      type: document.getElementById(prefix + '_tile_type')?.value || '0',
      title: document.getElementById(prefix + '_tile_title')?.value || '',
      icon: document.getElementById(prefix + '_tile_icon')?.value || '',
      color: document.getElementById(prefix + '_tile_color')?.value || '#2A2A2A',
      span_w: document.getElementById(prefix + '_tile_span_w')?.value || '1',
      span_h: document.getElementById(prefix + '_tile_span_h')?.value || '1'
    };
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
    const colorEl = document.getElementById(prefix + '_tile_color');
    if (colorEl) colorEl.value = data.color || '#2A2A2A';
    const spanWEl = document.getElementById(prefix + '_tile_span_w');
    if (spanWEl) spanWEl.value = data.span_w || '1';
    const spanHEl = document.getElementById(prefix + '_tile_span_h');
    if (spanHEl) spanHEl.value = data.span_h || '1';
    const meta = getTileTypeMeta(typeValue);
    callTypeHandler(meta, 'load', prefix, data);
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
    currentTileIndex = index;
    currentTileTab = tab;
    persistSelectedTileState();
    document.querySelectorAll('.tile').forEach(t => t.classList.remove('active', 'drop-target', 'dragging'));
    const tileId = tab + '-tile-' + index;
    document.getElementById(tileId)?.classList.add('active');
    const settingsId = tab + 'Settings';
    const settingsPanel = document.getElementById(settingsId);
    if (settingsPanel) {
      const tileSpecific = settingsPanel.querySelector('.tile-specific-settings');
      if (tileSpecific) tileSpecific.classList.remove('hidden');
    }
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
    bindLive(colorInput, 'input', 'tileColor', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(colInput, 'input', 'tileCol', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(rowInput, 'input', 'tileRow', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(spanWInput, 'input', 'tileSpanW', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(spanHInput, 'input', 'tileSpanH', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(typeSelect, 'change', 'tileType', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(entitySelect, 'change', 'sensorEntity', () => { maybeFillTitleFromSensor(tab); updateTilePreview(tab); updateSensorValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(weatherSelect, 'change', 'weatherEntity', () => { maybeFillTitleFromWeather(tab); updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(weatherPopupModeSelect, 'change', 'weatherPopupMode', () => { updateDraft(tab); scheduleAutoSave(tab); });
    bindLive(energySelect, 'change', 'energyEntity', () => { maybeFillTitleFromEnergy(tab); updateTilePreview(tab); updateEnergyValuePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
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
        }
      };
      settingsPanel.addEventListener('change', delegatedClockRefresh);
      settingsPanel.addEventListener('input', delegatedClockRefresh);
      settingsPanel.dataset.clockLiveBound = '1';
    }
  }

  function updateTilePreview(tab) {
    if (currentTileIndex === -1) return;
    const prefix = tab;
    const tileId = tab + '-tile-' + currentTileIndex;
    const tileElem = document.getElementById(tileId);
    if (!tileElem) return;

    const wasActive = tileElem.classList.contains('active');
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
    const iconEntity = (previewKind === 'sensor')
      ? (isEnergyType ? energyEntity : sensorEntity)
      : (previewKind === 'switch'
        ? switchEntity
        : (previewKind === 'weather'
          ? weatherEntity
          : (previewKind === 'media' ? mediaEntity : '')));
    const iconName = resolveIconName(iconInput ? iconInput.value : '', iconEntity, sensorMetaCache.icons);

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
    const tileBg = color || defaultBg;
    tileElem.style.background = tileBg;
    tileElem.style.removeProperty('--switch-knob-color');
    tileElem.style.removeProperty('--switch-on-color');
    if (type === '5' && switchStyle === '1') {
      tileElem.style.setProperty('--switch-knob-color', tileBg);
      tileElem.style.setProperty('--switch-on-color', '#3B82F6');
    }

    let html = '';

    if (iconName) {
      html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
    }

    if (title) {
      html += '<div class="tile-title" id="' + tileId + '-title">' + title + '</div>';
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

    if (previewKind === 'media') {
      html += '<div class="tile-media-title">--</div>';
      html += '<div class="tile-media-subtitle">--</div>';
      html += '<div class="tile-media-state">--</div>';
    }

    if (previewKind === 'clock') {
      const flags = getClockFlagsFromInputs(prefix);
      const clockTimeFont = document.getElementById(prefix + '_clock_time_font')?.value || '40';
      const clockDateFont = document.getElementById(prefix + '_clock_date_font')?.value || '20';
      const clockTimeFormat = document.getElementById(prefix + '_clock_time_format')?.value || '0';
      const clockDateFormat = document.getElementById(prefix + '_clock_date_format')?.value || '0';
      if (flags & 1) html += '<div class="tile-clock-time" ' + getClockPreviewTextStyle(clockTimeFont, 40, '#fff') + '>' + getClockPreviewTime(clockTimeFormat) + '</div>';
      if (flags & 2) html += '<div class="tile-clock-date" ' + getClockPreviewTextStyle(clockDateFont, 24, '#cbd5e1') + '>' + getClockPreviewDate(clockDateFormat) + '</div>';
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
    if (type === '15') updateMediaValuePreview(tab);
    updateLayoutFromInputs(tab);
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
        resetAllTypeFields(tab);
        updateTileType(tab);
        document.getElementById(prefix + '_tile_title').value = data.title || '';
        document.getElementById(prefix + '_tile_icon').value = data.icon_name || '';
        document.getElementById(prefix + '_tile_color').value = rgbToHex(data.bg_color || 0x2A2A2A);
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
          const layout = normalizeTileLayout(layoutInput, index);
          colEl.value = String(layout.col + 1);
          rowEl.value = String(layout.row + 1);
          spanWEl.value = String(layout.span_w);
          spanHEl.value = String(layout.span_h);
        }
        const meta = getTileTypeMeta(data.type || 0);
        callTypeHandler(meta, 'load', prefix, data);
        syncGaugeUi(tab);
        const tileElem = document.getElementById(tab + '-tile-' + index);
        if (tileElem) tileElem.classList.add('active');
        const draft = (drafts[tab] || {})[index];
        if (draft && draft._dirty) {
          applyDraft(tab, index);
        } else {
          if (draft && data.type === 0 && draft.type !== data.type) clearDraft(tab, index);
          updateTilePreview(tab);
        }
        setupLivePreview(tab);
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
    document.querySelectorAll('#' + prefix + 'Settings .type-fields').forEach(f => f.classList.remove('show'));
    const meta = getTileTypeMeta(typeValue);
    if (meta.fields) {
      const fieldsEl = document.getElementById(prefix + '_' + meta.fields + '_fields');
      if (fieldsEl) fieldsEl.classList.add('show');
    }
    if (meta.onSelect) {
      callTypeHandler(meta, 'onSelect', tab);
    }
    syncGaugeUi(tab);
    applySpecialTileUiState(tab);
  }

  function showNotification(message, success = true) {
    const notification = document.getElementById('notification');
    notification.textContent = message;
    notification.style.background = success ? '#10b981' : '#ef4444';
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
    document.getElementById(prefix + '_tile_color').value = '#2A2A2A';
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
    const layout = normalizeSnapshotLayout(snapshot, tileIndex);
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
    formData.append('bg_color', hexToRgb(snapshot.color || '#2A2A2A'));
    const typeValue = String(snapshot.type || '0');
    for (const [key, value] of Object.entries(snapshot)) {
      if (key === '_dirty' || key === '_rev' || key === 'type' || key === 'title' || key === 'icon' || key === 'color' || key === 'col' || key === 'row' || key === 'span_w' || key === 'span_h') continue;
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

  async function exportTilesConfig() {
    try {
      const tabs = tileTabs.slice();
      const tileRequests = tabs.map(tab => {
        const folderId = getFolderIdForTab(tab);
        if (folderId === undefined) return Promise.resolve([]);
        return fetch('/api/tiles?folder=' + encodeURIComponent(folderId))
          .then(res => res.json())
          .catch(() => []);
      });
      const tilesLists = await Promise.all(tileRequests);
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
        version: 2,
        exported_at: new Date().toISOString(),
        folders: folders,
        grids: grids
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
    fd.append('bg_color', parseBgColorValue(tile.bg_color));
    const layout = normalizeTileLayout(tile, index);
    fd.append('col', layout.col);
    fd.append('row', layout.row);
    fd.append('span_w', layout.span_w);
    fd.append('span_h', layout.span_h);

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
      const displayMode = (tile.sensor_display_mode !== undefined) ? tile.sensor_display_mode : (tile.sensor_gauge ? 1 : 0);
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
    }

    const res = await fetch('/api/tiles', { method: 'POST', body: fd });
    const data = await res.json();
    if (!data.success) {
      throw new Error('Tile speichern fehlgeschlagen');
    }
  }

  function rgbToHex(rgb) { return '#' + ('000000' + rgb.toString(16)).slice(-6); }
  function hexToRgb(hex) { return parseInt(hex.replace('#', ''), 16); }

  function renderTileFromData(tab, index, tile, sensorMeta) {
    const el = document.getElementById(tab + '-tile-' + index);
    if (!el) return;
    const metaValues = sensorMeta?.values || {};
    const metaUnits = sensorMeta?.units || {};
    const metaIcons = sensorMeta?.icons || {};
    el.dataset.index = index.toString();
    const typeValue = String(tile?.type ?? '0');
    const meta = getTileTypeMeta(typeValue);
    let cls = ['tile'];
    if (meta.css) cls.push(meta.css);
    if (typeValue === '5' && tile.switch_style === 1) cls.push('switch-toggle');
    if (typeValue === '0' && (!meta.css || meta.css !== 'empty')) cls.push('empty');
    el.className = cls.join(' ');
    el.dataset.type = typeValue;
    if (typeValue === '0') el.style.background = 'transparent';
    else {
      const bg = tile.bg_color ? rgbToHex(tile.bg_color) : (meta.defaultBg || '#353535');
      el.style.background = bg;
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
                          previewKind === 'weather' || previewKind === 'media')
        ? (tile.sensor_entity || '')
        : '';
      const iconName = resolveIconName(tile.icon_name || '', iconEntity, metaIcons);

      let html = '';

      if (iconName) {
        html += '<i class="mdi mdi-' + iconName + ' tile-icon"></i>';
      }

      if (tile.title && tile.title.length) {
        html += '<div class="tile-title" id="' + tab + '-tile-' + index + '-title">' + tile.title + '</div>';
      }

      if (previewKind === 'sensor') {
        let value = '--';
        if (tile.sensor_entity) value = formatSensorValue(metaValues[tile.sensor_entity] ?? '--', tile.sensor_decimals);
        const unit = resolveUnitValue(tile.sensor_unit || '', tile.sensor_entity || '', metaUnits);
        html += '<div class="tile-value ' + sensorValueClass + '" id="' + tab + '-tile-' + index + '-value">' + value + (unit ? '<span class="tile-unit">' + unit + '</span>' : '') + '</div>';
      }
      if (previewKind === 'media') {
        const parsed = (typeof parseMediaPreviewPayload === 'function')
          ? parseMediaPreviewPayload(tile.sensor_entity ? (metaValues[tile.sensor_entity] ?? '') : '')
          : { title: '--', subtitle: '--', state: '--' };
        html += '<div class="tile-media-title">' + (parsed.title || '--') + '</div>';
        html += '<div class="tile-media-subtitle">' + (parsed.subtitle || '--') + '</div>';
        html += '<div class="tile-media-state">' + (parsed.state || '--') + '</div>';
      }
      if (previewKind === 'clock') {
        const flags = normalizeClockFlags(tile.sensor_decimals);
        const clockTimeFont = tile.key_code || 40;
        const clockDateFont = tile.key_modifier || 20;
        const clockTimeFormat = (tile.sensor_gauge_min !== undefined) ? tile.sensor_gauge_min : 0;
        const clockDateFormat = (tile.sensor_gauge_max !== undefined) ? tile.sensor_gauge_max : 0;
        if (flags & 1) html += '<div class="tile-clock-time" ' + getClockPreviewTextStyle(clockTimeFont, 40, '#fff') + '>' + getClockPreviewTime(clockTimeFormat) + '</div>';
        if (flags & 2) html += '<div class="tile-clock-date" ' + getClockPreviewTextStyle(clockDateFont, 24, '#cbd5e1') + '>' + getClockPreviewDate(clockDateFormat) + '</div>';
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
      } else {
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

  function getTileGridMetrics(tab) {
    const grid = getTileGrid(tab);
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
    const rows = parseGridTrackSizes(style.gridTemplateRows);
    const cellW = cols.length ? cols[0] : ((rect.width - padLeft - padRight - (gapX * (GRID_COLS - 1))) / GRID_COLS);
    const cellH = rows.length ? rows[0] : ((rect.height - padTop - padBottom - (gapY * (GRID_ROWS - 1))) / GRID_ROWS);
    return { rect, gapX, gapY, padLeft, padTop, cellW, cellH };
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
    if (row < 0) row = 0;
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
    return normalizeTileLayout(tiles[index], index);
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

  function canPlaceTileLayout(tab, index, candidateLayout) {
    if (!candidateLayout) return false;
    if (candidateLayout.col < 0 || candidateLayout.row < 0) return false;
    if (candidateLayout.span_w < 1 || candidateLayout.span_h < 1) return false;
    if ((candidateLayout.col + candidateLayout.span_w) > GRID_COLS) return false;
    if ((candidateLayout.row + candidateLayout.span_h) > GRID_ROWS) return false;

    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles)) return true;

    for (let i = 0; i < tiles.length; i++) {
      if (i === index) continue;
      const tile = tiles[i];
      if (!tile || Number(tile.type || 0) === 0) continue;
      const otherLayout = getTileElementLayout(tab, i) || getTileLayoutFromData(tab, i);
      if (!otherLayout) continue;
      if (rectsOverlap(candidateLayout, otherLayout)) return false;
    }
    return true;
  }

  function manhattanDistance(colA, rowA, colB, rowB) {
    return Math.abs(colA - colB) + Math.abs(rowA - rowB);
  }

  function buildPlacementCandidates(spanW, spanH, preferredCol, preferredRow) {
    const candidates = [];
    for (let row = 0; row < GRID_ROWS; row++) {
      for (let col = 0; col < GRID_COLS; col++) {
        if ((col + spanW) > GRID_COLS || (row + spanH) > GRID_ROWS) continue;
        let distance = (row * GRID_COLS) + col;
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

  function simulateSmartReorderLayouts(tab, fromIdx, targetCol, targetRow) {
    const tiles = getTilesData(tab);
    if (!Array.isArray(tiles) || fromIdx < 0 || fromIdx >= tiles.length) return null;

    const baseLayouts = (dragSource && dragSource.tab === tab && Array.isArray(dragSource.baseLayouts))
      ? dragSource.baseLayouts
      : captureLayoutSnapshot(tab);

    const movingTile = tiles[fromIdx];
    const movingBase = baseLayouts[fromIdx] ? cloneLayout(baseLayouts[fromIdx]) : null;
    if (!movingTile || Number(movingTile.type || 0) === 0 || !movingBase) return null;
    if ((targetCol + movingBase.span_w) > GRID_COLS || (targetRow + movingBase.span_h) > GRID_ROWS) return null;

    const workingLayouts = baseLayouts.map(layout => cloneLayout(layout));
    const targetLayout = {
      col: targetCol,
      row: targetRow,
      span_w: movingBase.span_w,
      span_h: movingBase.span_h
    };

    const displacedIndices = [];
    for (let i = 0; i < tiles.length; i++) {
      if (i === fromIdx) continue;
      const tile = tiles[i];
      const layout = baseLayouts[i];
      if (!tile || Number(tile.type || 0) === 0 || !layout) continue;
      if (rectsOverlap(targetLayout, layout)) displacedIndices.push(i);
    }

    displacedIndices.sort((a, b) => {
      const layoutA = baseLayouts[a];
      const layoutB = baseLayouts[b];
      if (layoutA.row !== layoutB.row) return layoutA.row - layoutB.row;
      if (layoutA.col !== layoutB.col) return layoutA.col - layoutB.col;
      return a - b;
    });

    workingLayouts[fromIdx] = targetLayout;
    const floating = new Set(displacedIndices);

    for (let order = 0; order < displacedIndices.length; order++) {
      const displacedIndex = displacedIndices[order];
      const layout = baseLayouts[displacedIndex];
      if (!layout) return null;
      floating.delete(displacedIndex);

      const preferredCol = order === 0 ? movingBase.col : layout.col;
      const preferredRow = order === 0 ? movingBase.row : layout.row;
      const candidates = buildPlacementCandidates(layout.span_w, layout.span_h, preferredCol, preferredRow);

      let placed = false;
      for (const candidate of candidates) {
        const nextLayout = {
          col: candidate.col,
          row: candidate.row,
          span_w: layout.span_w,
          span_h: layout.span_h
        };
        let blocked = false;
        for (let i = 0; i < tiles.length; i++) {
          if (i === displacedIndex || floating.has(i)) continue;
          const tile = tiles[i];
          const otherLayout = workingLayouts[i];
          if (!tile || Number(tile.type || 0) === 0 || !otherLayout) continue;
          if (rectsOverlap(nextLayout, otherLayout)) {
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

  function updateResizePlaceholder(tab, layout, valid) {
    const placeholder = ensureResizePlaceholder(tab);
    if (!placeholder || !layout) return;
    if (valid) {
      placeholder.classList.remove('show', 'invalid');
      return;
    }
    placeholder.classList.add('show');
    placeholder.classList.add('invalid');
    setTileGridPosition(placeholder, layout.col, layout.row, layout.span_w, layout.span_h);
  }

  function buildResizeCandidate(layout, direction, clientX, clientY, tab) {
    const rawCell = getRawGridCellFromPointer(tab, clientX, clientY);
    if (!layout || !rawCell) return null;

    let spanW = layout.span_w;
    let spanH = layout.span_h;
    if (String(direction || '').includes('e')) {
      spanW = clampInt(rawCell.col - layout.col + 1, 1, GRID_COLS - layout.col, layout.span_w);
    }
    if (String(direction || '').includes('s')) {
      spanH = clampInt(rawCell.row - layout.row + 1, 1, GRID_ROWS - layout.row, layout.span_h);
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
      applyLayoutInputsFromLayout(state.tab, finalLayout, false);
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

    resizeState.lastValidLayout = cloneLayout(candidate);
    if (resizeState.tab === currentTileTab && resizeState.index === currentTileIndex) {
      applyLayoutInputsFromLayout(resizeState.tab, candidate, false);
      updateLayoutFromInputs(resizeState.tab);
    }
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
      lastValidLayout: cloneLayout(layout)
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
    const targetRow = clampInt(row, 0, GRID_ROWS - 1, sourceLayout.row);
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
    const targetRow = clampInt(cell.row, 0, GRID_ROWS - 1, sourceLayout.row);
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
    const targetRow = clampInt(cell.row, 0, GRID_ROWS - 1, sourceLayout.row);
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
        loadSensorValues(true, true);
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

  document.addEventListener('DOMContentLoaded', () => {
    toggleStaticWifiFields();
    initTileTabs();
    loadDraftsFromStorage();
    loadTileClipboard();
    loadSensorValues(true, true);
    let savedTab = null;
    try { savedTab = localStorage.getItem('activeAdminTab'); } catch (e) {}
    const defaultTab = tileTabs.length ? ('tab-tiles-' + tileTabs[0]) : 'tab-network';
    const targetTab = savedTab && document.getElementById(savedTab) ? savedTab : defaultTab;
    const targetBtn = Array.from(document.querySelectorAll('.tab-btn')).find(btn => btn.getAttribute('onclick')?.includes(targetTab)) || document.querySelector('.tab-btn');
    if (targetBtn) targetBtn.click();
    setInterval(() => { if (!document.hidden) loadSensorValues(false, false); }, 15000);
    tileTabs.forEach(tab => {
      enableTileDrag(tab);
      enableTileResize(tab);
    });
  });
  </script>
)html";

  append_tile_type_scripts(html);
}








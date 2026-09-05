
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
      const otaSize = encodeURIComponent(String(file.size || 0));
      const otaFilename = encodeURIComponent(String(file.name || ''));
      const prepRes = await fetch('/api/ota/prepare?size=' + otaSize + '&filename=' + otaFilename, {
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

    const xhr = new XMLHttpRequest();
    const otaSize = encodeURIComponent(String(file.size || 0));
    const otaFilename = encodeURIComponent(String(file.name || ''));
    xhr.open('POST', '/api/ota/upload/raw?size=' + otaSize + '&filename=' + otaFilename, true);
    xhr.setRequestHeader('Content-Type', 'application/octet-stream');
    xhr.setRequestHeader('X-HomeTiles-OTA-Filename', otaFilename);

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

    xhr.send(file);
  }

  // Tile Editor State

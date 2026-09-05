
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

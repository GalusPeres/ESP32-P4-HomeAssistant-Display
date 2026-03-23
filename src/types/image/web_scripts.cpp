#include "src/types/image/web_scripts.h"

void append_image_scripts(String& html) {
  html += R"html(
  <script>
  const imageIntervalDefault = '3600';

  function syncImageUrl(tab) {
    const prefix = tab;
    const input = document.getElementById(prefix + '_image_path');
    const urlInput = document.getElementById(prefix + '_image_url');
    if (!input || !urlInput) return;
    input.value = String(urlInput.value || '').trim();
  }

  function loadImageFields(tab, data) {
    const prefix = tab;
    const path = String((data && data.image_path) || '').trim();
    const pathEl = document.getElementById(prefix + '_image_path');
    if (pathEl) pathEl.value = path;
    const urlEl = document.getElementById(prefix + '_image_url');
    if (urlEl) urlEl.value = path;
    const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
    if (intervalEl) intervalEl.value = data.image_slideshow_sec || imageIntervalDefault;
  }

  function saveImageFields(tab, formData) {
    const prefix = tab;
    const imgUrl = document.getElementById(prefix + '_image_url');
    const imgPath = document.getElementById(prefix + '_image_path');
    const finalPath = imgUrl ? String(imgUrl.value || '').trim() : '';
    if (imgPath) imgPath.value = finalPath;
    formData.append('image_path', finalPath);
    formData.append('image_slideshow_sec', document.getElementById(prefix + '_image_slideshow_sec')?.value || imageIntervalDefault);
    formData.append('image_preview', '0');
  }

  function resetImageFields(tab) {
    const prefix = tab;
    const pathEl = document.getElementById(prefix + '_image_path');
    if (pathEl) pathEl.value = '';
    const urlEl = document.getElementById(prefix + '_image_url');
    if (urlEl) urlEl.value = '';
    const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
    if (intervalEl) intervalEl.value = imageIntervalDefault;
  }

  function onImageTypeSelected(tab) { syncImageUrl(tab); }
  function refreshImageSelect(tab, force) { void tab; void force; }

  function setImagePath(tab, value, autosave = true) {
    const prefix = tab;
    const path = String(value || '').trim();
    const input = document.getElementById(prefix + '_image_path');
    const urlInput = document.getElementById(prefix + '_image_url');
    if (input) input.value = path;
    if (urlInput) urlInput.value = path;
    if (autosave) {
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    }
  }

  function setImageUrl(tab, url, autosave = true) {
    setImagePath(tab, url, autosave);
  }
  </script>
)html";
}

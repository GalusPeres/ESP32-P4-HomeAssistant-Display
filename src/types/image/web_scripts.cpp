#include "src/types/image/web_scripts.h"

void append_image_scripts(String& html) {
  html += R"html(
  <script>
  const slideshowTokenLegacy = '__slideshow__';
  const slideshowTokenBin = '__slideshow_bin__';
  const slideshowTokenJpeg = '__slideshow_jpeg__';
  const imageUrlToken = '__url__';
  const urlIntervalDefault = '3600';
  const slideshowIntervalDefault = '10';
  let sdImageList = [];
  let sdImageListLoaded = false;

  function isImageUrl(value) {
    return /^https?:\/\//i.test(String(value || '').trim());
  }
  function normalizeImageToken(value) {
    if (!value) return '';
    if (value === slideshowTokenLegacy) return slideshowTokenBin;
    return value;
  }

  function populateImageSelect(tab, list) {
    const prefix = tab;
    const select = document.getElementById(prefix + '_image_select');
    if (!select) return;
    const current = select.value;
    const inputVal = document.getElementById(prefix + '_image_path')?.value || '';
    const urlInput = document.getElementById(prefix + '_image_url');
    const items = Array.isArray(list) ? list : [];
    select.innerHTML = '';
    const slideshowBinOpt = document.createElement('option');
    slideshowBinOpt.value = slideshowTokenBin;
    slideshowBinOpt.textContent = 'Alle .bin (Diashow)';
    select.appendChild(slideshowBinOpt);
    const slideshowJpegOpt = document.createElement('option');
    slideshowJpegOpt.value = slideshowTokenJpeg;
    slideshowJpegOpt.textContent = 'Alle JPEG (Diashow)';
    select.appendChild(slideshowJpegOpt);
    const urlOpt = document.createElement('option');
    urlOpt.value = imageUrlToken;
    urlOpt.textContent = 'URL (HTTP/HTTPS)';
    select.appendChild(urlOpt);
    items.forEach(p => {
      const opt = document.createElement('option');
      opt.value = p;
      opt.textContent = p;
      select.appendChild(opt);
    });
    const preferred = normalizeImageToken(inputVal || current || '');
    const isUrlValue = isImageUrl(preferred) || preferred === imageUrlToken;
    const valid = preferred === slideshowTokenBin || preferred === slideshowTokenJpeg || items.includes(preferred) || isUrlValue;
    if (valid) {
      select.value = isUrlValue ? imageUrlToken : preferred;
    } else if (!inputVal && !current) {
      select.value = slideshowTokenBin;
      setImagePath(tab, slideshowTokenBin, false);
    } else {
      select.value = slideshowTokenBin;
    }
    if (isUrlValue && urlInput && inputVal) urlInput.value = inputVal;
    updateImageUrlVisibility(tab, select.value, inputVal || '');
  }

  function refreshImageSelect(tab, force) {
    if (!force && sdImageListLoaded) {
      populateImageSelect(tab, sdImageList);
      return;
    }
    fetch('/api/sd_images')
      .then(res => res.json())
      .then(list => {
        sdImageList = Array.isArray(list) ? list : [];
        sdImageListLoaded = true;
        populateImageSelect(tab, sdImageList);
      })
      .catch(() => {
        sdImageListLoaded = false;
      });
  }

  function applyImageUiState(tab, path) {
    const prefix = tab;
    const select = document.getElementById(prefix + '_image_select');
    if (!select) return;
    const urlInput = document.getElementById(prefix + '_image_url');
    if (!path) {
      setImagePath(tab, slideshowTokenBin, false);
      return;
    }
    const normalized = normalizeImageToken(path);
    if (isImageUrl(normalized) || normalized === imageUrlToken) {
      select.value = imageUrlToken;
      if (urlInput) urlInput.value = normalized === imageUrlToken ? '' : normalized;
    } else {
      select.value = normalized;
      if (select.value !== normalized) select.value = slideshowTokenBin;
      if (urlInput) urlInput.value = '';
    }
    updateImageUrlVisibility(tab, select.value, path);
  }

  function setImagePath(tab, value, autosave = true) {
    const prefix = tab;
    const input = document.getElementById(prefix + '_image_path');
    if (!input) return;
    const normalized = normalizeImageToken(value || '');
    input.value = normalized;
    const urlInput = document.getElementById(prefix + '_image_url');
    if (urlInput) urlInput.value = '';
    const select = document.getElementById(prefix + '_image_select');
    if (select) {
      select.value = input.value;
      if (select.value !== input.value) select.value = slideshowTokenBin;
    }
    updateImageUrlVisibility(tab, select ? select.value : '', input.value);
    if (autosave) {
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    }
  }

  function setImageUrl(tab, url, autosave = true) {
    const prefix = tab;
    const input = document.getElementById(prefix + '_image_path');
    if (!input) return;
    const normalized = String(url || '').trim();
    input.value = normalized;
    const urlInput = document.getElementById(prefix + '_image_url');
    if (urlInput) urlInput.value = normalized;
    const select = document.getElementById(prefix + '_image_select');
    if (select) select.value = imageUrlToken;
    updateImageUrlVisibility(tab, imageUrlToken, normalized);
    if (autosave) {
      updateTilePreview(tab);
      updateDraft(tab);
      scheduleAutoSave(tab);
    }
  }

  function updateImageUrlVisibility(tab, selectedValue, currentPath) {
    const prefix = tab;
    const wrap = document.getElementById(prefix + '_image_url_fields');
    if (!wrap) return;
    const show = selectedValue === imageUrlToken || isImageUrl(currentPath || '');
    wrap.style.display = show ? 'block' : 'none';
    const label = document.getElementById(prefix + '_image_interval_label');
    if (label) {
      label.textContent = show ? 'URL Cache Intervall (Sekunden)' : 'Diashow Intervall (Sekunden)';
    }
    const intervalInput = document.getElementById(prefix + '_image_slideshow_sec');
    if (show && intervalInput) {
      const val = String(intervalInput.value || '').trim();
      if (val === '' || val === slideshowIntervalDefault) {
        intervalInput.value = urlIntervalDefault;
      }
    }
  }

  function onImageTypeSelected(tab) {
    const prefix = tab;
    const path = document.getElementById(prefix + '_image_path')?.value || '';
    refreshImageSelect(tab, false);
    applyImageUiState(tab, path);
  }

  function loadImageFields(tab, data) {
    const prefix = tab;
    const pathEl = document.getElementById(prefix + '_image_path');
    if (pathEl) pathEl.value = data.image_path || '';
    const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
    if (intervalEl) intervalEl.value = data.image_slideshow_sec || '10';
    applyImageUiState(tab, data.image_path || '');
    refreshImageSelect(tab, false);
  }

  function saveImageFields(tab, formData) {
    const prefix = tab;
    const imgSelect = document.getElementById(prefix + '_image_select');
    const imgUrl = document.getElementById(prefix + '_image_url');
    const imgPath = document.getElementById(prefix + '_image_path');
    let finalPath = imgPath ? imgPath.value : '';
    if (imgSelect && imgSelect.value === imageUrlToken && imgUrl && imgUrl.value.trim().length > 0) {
      finalPath = imgUrl.value.trim();
      if (imgPath) imgPath.value = finalPath;
    }
    formData.append('image_path', finalPath);
    formData.append('image_slideshow_sec', document.getElementById(prefix + '_image_slideshow_sec')?.value || '10');
  }

  function resetImageFields(tab) {
    const prefix = tab;
    const pathEl = document.getElementById(prefix + '_image_path');
    if (pathEl) pathEl.value = '';
    const intervalEl = document.getElementById(prefix + '_image_slideshow_sec');
    if (intervalEl) intervalEl.value = '10';
    const urlEl = document.getElementById(prefix + '_image_url');
    if (urlEl) urlEl.value = '';
    updateImageUrlVisibility(tab, '', '');
    applyImageUiState(tab, '');
  }
  </script>
)html";
}


function loadAnimationFields(tab, data) {
    const el = document.getElementById(tab + '_animation_file');
    if (el) {
      // The chosen file name is stored in scene_alias (see firmware web_handler).
      const val = (data && (data.animation_file !== undefined ? data.animation_file
                           : data.scene_alias)) || '';
      el.value = val;
      if (el.value !== val) el.value = '';  // saved file gone from SD -> none
    }
    // Speed (fps) is stored in image_slideshow_sec for this tile type.
    const fps = document.getElementById(tab + '_animation_fps');
    if (fps) {
      let v = data && data.image_slideshow_sec ? parseInt(data.image_slideshow_sec, 10) : 10;
      if (!(v >= 1 && v <= 30)) v = 10;
      fps.value = v;
      const lbl = document.getElementById(tab + '_animation_fps_val');
      if (lbl) lbl.textContent = v;
    }
    const fit = document.getElementById(tab + '_animation_fit');
    if (fit) {
      let v = data && data.animation_fit !== undefined
        ? parseInt(data.animation_fit, 10)
        : (data && data.sensor_display_mode !== undefined ? parseInt(data.sensor_display_mode, 10) : 0);
      if (!(v >= 0 && v <= 2)) v = 0;
      fit.value = String(v);
    }
    const zoom = document.getElementById(tab + '_animation_zoom');
    if (zoom) {
      let v = data && data.animation_zoom !== undefined
        ? parseInt(data.animation_zoom, 10)
        : (data && data.sensor_gauge_max !== undefined ? parseInt(data.sensor_gauge_max, 10) : 100);
      if (!(v >= 25 && v <= 300)) v = 100;
      zoom.value = v;
      const lbl = document.getElementById(tab + '_animation_zoom_val');
      if (lbl) lbl.textContent = v;
    }
  }

  function saveAnimationFields(tab, formData) {
    const el = document.getElementById(tab + '_animation_file');
    formData.append('animation_file', el ? (el.value || '') : '');
    const fps = document.getElementById(tab + '_animation_fps');
    formData.append('animation_fps', fps ? (fps.value || '10') : '10');
    const fit = document.getElementById(tab + '_animation_fit');
    formData.append('animation_fit', fit ? (fit.value || '0') : '0');
    const zoom = document.getElementById(tab + '_animation_zoom');
    formData.append('animation_zoom', zoom ? (zoom.value || '100') : '100');
  }

  function resetAnimationFields(tab) {
    const el = document.getElementById(tab + '_animation_file');
    if (el) el.value = '';
    const fps = document.getElementById(tab + '_animation_fps');
    if (fps) fps.value = 10;
    const lbl = document.getElementById(tab + '_animation_fps_val');
    if (lbl) lbl.textContent = '10';
    const fit = document.getElementById(tab + '_animation_fit');
    if (fit) fit.value = '0';
    const zoom = document.getElementById(tab + '_animation_zoom');
    if (zoom) zoom.value = 100;
    const zoomLbl = document.getElementById(tab + '_animation_zoom_val');
    if (zoomLbl) zoomLbl.textContent = '100';
  }

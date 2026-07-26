#include "src/types/camera/web_scripts.h"

void append_camera_scripts(String& html) {
  html += R"html(
  <script>
  function loadCameraFields(tab, data) {
    const el = document.getElementById(tab + '_camera_entity');
    if (el) el.value = data.sensor_entity || data.camera_entity || '';
    maybeFillTitleFromEntity(tab, '_camera_entity');
  }
  function saveCameraFields(tab, formData) {
    const entity =
      document.getElementById(tab + '_camera_entity')?.value || '';
    formData.append('camera_entity', entity);
    formData.append('sensor_entity', entity);
  }
  function resetCameraFields(tab) {
    const el = document.getElementById(tab + '_camera_entity');
    if (el) el.value = '';
  }
  </script>
)html";
}

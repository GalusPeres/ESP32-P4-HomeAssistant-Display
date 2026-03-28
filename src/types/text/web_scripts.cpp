#include "src/types/text/web_scripts.h"

void append_text_scripts(String& html) {
  html += R"html(
  <script>
  function normalizeTextValueFont(value) {
    const v = String(value || '0');
    return (['1','2','3','4'].includes(v)) ? v : '0';
  }

  function loadTextFields(tab, data) {
    const prefix = tab;
    const textEl = document.getElementById(prefix + '_text_value');
    const fontEl = document.getElementById(prefix + '_text_value_font');
    if (!textEl) return;
    if (data && data.text_value !== undefined) {
      textEl.value = data.text_value || '';
    } else if (data && data.scene_alias !== undefined) {
      textEl.value = data.scene_alias || '';
    } else if (data && data.key_macro !== undefined) {
      textEl.value = data.key_macro || '';
    } else {
      textEl.value = '';
    }
    if (fontEl) {
      if (data && data.text_value_font !== undefined) {
        fontEl.value = normalizeTextValueFont(data.text_value_font);
      } else if (data && data.sensor_value_font !== undefined) {
        fontEl.value = normalizeTextValueFont(data.sensor_value_font);
      } else {
        fontEl.value = '0';
      }
    }
  }

  function saveTextFields(tab, formData) {
    const prefix = tab;
    formData.append('text_value', document.getElementById(prefix + '_text_value')?.value || '');
    formData.append('text_value_font', document.getElementById(prefix + '_text_value_font')?.value || '0');
  }

  function resetTextFields(tab) {
    const prefix = tab;
    const textEl = document.getElementById(prefix + '_text_value');
    if (textEl) textEl.value = '';
    const fontEl = document.getElementById(prefix + '_text_value_font');
    if (fontEl) fontEl.value = '0';
  }
  </script>
)html";
}

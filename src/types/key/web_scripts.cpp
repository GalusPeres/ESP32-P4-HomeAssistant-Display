#include "src/types/key/web_scripts.h"

void append_key_scripts(String& html) {
  html += R"html(
  <script>
  function loadKeyFields(tab, data) {
    const prefix = tab;
    const keyEl = document.getElementById(prefix + '_key_macro');
    if (keyEl) keyEl.value = data.key_macro || '';
  }

  function saveKeyFields(tab, formData) {
    const prefix = tab;
    formData.append('key_macro', document.getElementById(prefix + '_key_macro')?.value || '');
  }

  function resetKeyFields(tab) {
    const prefix = tab;
    const keyEl = document.getElementById(prefix + '_key_macro');
    if (keyEl) keyEl.value = '';
  }
  </script>
)html";
}

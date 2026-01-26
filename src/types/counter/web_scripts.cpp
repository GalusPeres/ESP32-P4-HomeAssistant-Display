#include "src/types/counter/web_scripts.h"

void append_counter_scripts(String& html) {
  html += R"html(
  <script>
  function loadCounterFields(tab, data) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_counter_value');
    if (!el) return;
    if (data && data.counter_value !== undefined) {
      el.value = data.counter_value;
    } else if (data && data.scene_alias !== undefined) {
      el.value = data.scene_alias || '0';
    } else {
      el.value = '0';
    }
  }

  function saveCounterFields(tab, formData) {
    const prefix = tab;
    formData.append('counter_value', document.getElementById(prefix + '_counter_value')?.value || '0');
  }

  function resetCounterFields(tab) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_counter_value');
    if (el) el.value = '0';
  }
  </script>
)html";
}

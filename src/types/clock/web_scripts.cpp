#include "src/types/clock/web_scripts.h"

void append_clock_scripts(String& html) {
  html += R"html(
  <script>
  function getClockPreviewTime() {
    const now = new Date();
    const hh = String(now.getHours()).padStart(2, '0');
    const mm = String(now.getMinutes()).padStart(2, '0');
    return hh + ':' + mm;
  }
  </script>
)html";
}

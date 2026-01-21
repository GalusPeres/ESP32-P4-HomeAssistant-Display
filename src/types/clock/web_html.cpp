#include "src/types/clock/web_html.h"

void append_clock_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Clock Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_clock_fields" class="type-fields">
              <div style="font-size:12px;color:#64748b;margin-bottom:6px;">Keine Uhr-Einstellungen.</div>
            </div>
)html";
}

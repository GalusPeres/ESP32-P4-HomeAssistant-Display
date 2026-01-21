#include "src/types/clock/web_html.h"

void append_clock_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Clock Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_clock_fields" class="type-fields">
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_clock_show_time" checked>
                Uhrzeit anzeigen
              </label>
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_clock_show_date">
                Datum anzeigen
              </label>
            </div>
)html";
}

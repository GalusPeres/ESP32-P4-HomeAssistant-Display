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
              <label>Uhrzeit Schriftgroesse</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_time_font">
                <option value="48">48 (Default)</option>
                <option value="40">40</option>
                <option value="32">32</option>
                <option value="24">24</option>
                <option value="20">20</option>
              </select>
              <label>Datum Schriftgroesse</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_date_font">
                <option value="24">24 (Default)</option>
                <option value="20">20</option>
                <option value="32">32</option>
                <option value="40">40</option>
                <option value="48">48</option>
              </select>
            </div>
)html";
}


#include "src/types/text/web_html.h"

void append_text_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Text Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_text_fields" class="type-fields">
              <label>Text</label>
              <textarea id=")html";
  html += tab_id;
  html += R"html(_text_value" rows="3" placeholder="Text fuer die Kachel"></textarea>
              <label>Text-Groesse</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_text_value_font">
                <option value="0">40 (Default)</option>
                <option value="1">20</option>
                <option value="2">24</option>
                <option value="3">28</option>
                <option value="4">32</option>
              </select>
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Max 31 Zeichen gespeichert.</div>
            </div>
)html";
}

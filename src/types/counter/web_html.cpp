#include "src/types/counter/web_html.h"

void append_counter_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Counter Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_counter_fields" class="type-fields">
              <label>Startwert</label>
              <input id=")html";
  html += tab_id;
  html += R"html(_counter_value" type="number" value="0" min="0" step="1" placeholder="0">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">Tap: +1, Long-Press: Reset auf 0</div>
            </div>
)html";
}

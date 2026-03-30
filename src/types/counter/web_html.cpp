#include "src/types/counter/web_html.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_counter_fields_html(String& html, const String& tab_id) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <!-- Counter Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_counter_fields" class="type-fields">
              <label>)html";
  html += tr.counter_start_value;
  html += R"html(</label>
              <input id=")html";
  html += tab_id;
  html += R"html(_counter_value" type="number" value="0" min="0" step="1" placeholder="0">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">)html";
  html += tr.counter_hint;
  html += R"html(</div>
            </div>
)html";
}

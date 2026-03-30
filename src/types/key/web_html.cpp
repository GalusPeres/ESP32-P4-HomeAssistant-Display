#include "src/types/key/web_html.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_key_fields_html(String& html, const String& tab_id) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <!-- Key Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_key_fields" class="type-fields">
              <label>)html";
  html += tr.macro_label;
  html += R"html(</label>
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_key_macro" placeholder="z.B. ctrl+g">
              <div style="font-size:11px;color:#64748b;margin-top:4px;">)html";
  html += tr.macro_examples;
  html += R"html(</div>
            </div>
)html";
}

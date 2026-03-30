#include "src/types/navigate/web_html.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_navigate_fields_html(String& html, const String& tab_id, const String& navigateOptionsHtml) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <!-- Navigate Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_navigate_fields" class="type-fields">
              <label>)html";
  html += tr.target_folder;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_navigate_target">
                <option value="0">)html";
  html += tr.new_folder;
  html += R"html(</option>
)html";
  html += navigateOptionsHtml;
  html += R"html(
              </select>
              <div id=")html";
  html += tab_id;
  html += R"html(_navigate_note" style="font-size:11px;color:#64748b;margin-top:6px;"></div>
            </div>
)html";
}

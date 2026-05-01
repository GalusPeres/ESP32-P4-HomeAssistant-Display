#include "src/types/media/web_html.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin_utils.h"

void append_media_fields_html(String& html,
                              const String& tab_id,
                              const std::vector<String>& mediaOptions) {
  const auto& tr = i18n::strings(configManager.getConfig().language);

  html += R"html(
            <!-- Media Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_media_fields" class="type-fields">
              <label>)html";
  html += tr.media_entity;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_media_entity">
                <option value="">)html";
  html += tr.no_selection;
  html += R"html(</option>
)html";

  for (const auto& opt : mediaOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String name = haBridgeConfig.findSensorName(opt);
    if (!name.length()) {
      name = humanizeIdentifier(opt, true);
    }
    String label = name + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
            </div>
)html";
}

#include "src/types/camera/web_html.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin_utils.h"

void append_camera_fields_html(
    String& html,
    const String& tab_id,
    const std::vector<String>& camera_options) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <div id=")html";
  html += tab_id;
  html += R"html(_camera_fields" class="type-fields">
              <label>)html";
  appendHtmlEscaped(html, tr.camera_entity);
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_camera_entity">
                <option value="">)html";
  appendHtmlEscaped(html, tr.no_selection);
  html += R"html(</option>
)html";
  for (const auto& entity : camera_options) {
    html += "<option value=\"";
    appendHtmlEscaped(html, entity);
    html += "\">";
    String name = haBridgeConfig.findSensorName(entity);
    if (!name.length()) name = humanizeIdentifier(entity, true);
    appendHtmlEscaped(html, name + " - " + entity);
    html += "</option>";
  }
  html += R"html(
              </select>
            </div>
)html";
}

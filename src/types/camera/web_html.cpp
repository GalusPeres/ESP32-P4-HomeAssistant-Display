#include "src/types/camera/web_html.h"

#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin_utils.h"

void append_camera_fields_html(
    String& html,
    const String& tab_id,
    const std::vector<String>& camera_options) {
  html += R"html(
            <div id=")html";
  html += tab_id;
  html += R"html(_camera_fields" class="type-fields">
              <label>Kamera</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_camera_entity">
                <option value="">Keine Auswahl</option>
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

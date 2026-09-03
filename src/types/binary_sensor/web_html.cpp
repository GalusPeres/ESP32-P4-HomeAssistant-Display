#include "src/types/binary_sensor/web_html.h"

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/network/ha_bridge_config.h"
#include "src/web/web_admin_utils.h"

void append_binary_sensor_fields_html(
    String& html, const String& tab_id,
    const std::vector<String>& binary_sensor_options) {
  const char* language = configManager.getConfig().language;
  const auto& tr = i18n::strings(language);

  html += "<div id=\"";
  html += tab_id;
  html += "_binary_sensor_fields\" class=\"type-fields\"><label>";
  html += i18n::binary_sensor_label(language, 1);
  html += "</label><select id=\"";
  html += tab_id;
  html += "_binary_sensor_entity\"><option value=\"\">";
  html += tr.no_selection;
  html += "</option>";
  for (const String& entity : binary_sensor_options) {
    html += "<option value=\"";
    appendHtmlEscaped(html, entity);
    html += "\">";
    String name = haBridgeConfig.findSensorName(entity);
    if (!name.length()) name = humanizeIdentifier(entity, true);
    appendHtmlEscaped(html, name + " - " + entity);
    html += "</option>";
  }
  html += "</select>";

  if (tab_id != "screensaver") {
    html += "<label>";
    html += tr.popup_open;
    html += "</label><select id=\"";
    html += tab_id;
    html += "_binary_sensor_popup_open_mode\"><option value=\"1\">";
    html += tr.short_press;
    html += "</option><option value=\"0\">";
    html += tr.long_press;
    html += "</option></select>";
  }
  html += "</div>\n";
}

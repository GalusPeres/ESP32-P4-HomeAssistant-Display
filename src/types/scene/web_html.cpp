#include "src/types/scene/web_html.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_scene_fields_html(String& html, const String& tab_id, const std::vector<SceneOption>& sceneOptions) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <!-- Scene Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_scene_fields" class="type-fields">
              <label>)html";
  html += tr.scene_label;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_scene_alias">
                <option value="">)html";
  html += tr.no_selection;
  html += R"html(</option>
)html";

  for (const auto& opt : sceneOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt.alias);
    html += "\">";
    String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
            </div>
)html";
}

#include "src/types/clock/web_html.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_clock_fields_html(String& html, const String& tab_id) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <!-- Clock Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_clock_fields" class="type-fields">
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_clock_show_time" checked>
                )html";
  html += tr.show_time;
  html += R"html(
              </label>
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_clock_show_date">
                )html";
  html += tr.show_date;
  html += R"html(
              </label>
              <label>)html";
  html += tr.time_font_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_time_font">
                <option value="48">48 (Default)</option>
                <option value="40">40</option>
                <option value="32">32</option>
                <option value="24">24</option>
                <option value="20">20</option>
              </select>
              <label>)html";
  html += tr.date_font_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_date_font">
                <option value="24">24 (Default)</option>
                <option value="20">20</option>
                <option value="32">32</option>
                <option value="40">40</option>
                <option value="48">48</option>
              </select>
            </div>
)html";
}


#include "src/types/clock/web_html.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_clock_fields_html(String& html, const String& tab_id) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  const bool is_de = i18n::normalize_language_code(configManager.getConfig().language)[0] == 'd';
  const char* time_format_label = is_de ? "Zeitformat" : "Time format";
  const char* date_format_label = is_de ? "Datumsformat" : "Date format";
  const char* auto_label = is_de ? "Auto (Sprache)" : "Auto (language)";
  const char* time_24_label = is_de ? "24 Stunden" : "24-hour";
  const char* time_12_label = is_de ? "12 Stunden" : "12-hour";
  html += R"html(
            <!-- Clock Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_clock_fields" class="type-fields">
              <div class="clock-toggle-row">
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
              </div>
              <label>)html";
  html += tr.time_font_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_time_font">
                <option value="40">40 (Default)</option>
                <option value="20">20</option>
                <option value="24">24</option>
                <option value="28">28</option>
                <option value="32">32</option>
                <option value="48">48</option>
              </select>
              <label>)html";
  html += tr.date_font_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_date_font">
                <option value="20">20 (Default)</option>
                <option value="24">24</option>
                <option value="28">28</option>
                <option value="32">32</option>
                <option value="40">40</option>
                <option value="48">48</option>
              </select>
              <label>)html";
  html += time_format_label;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_time_format">
                <option value="0">)html";
  html += auto_label;
  html += R"html(</option>
                <option value="1">)html";
  html += time_24_label;
  html += R"html(</option>
                <option value="2">)html";
  html += time_12_label;
  html += R"html(</option>
              </select>
              <label>)html";
  html += date_format_label;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_date_format">
                <option value="0">)html";
  html += auto_label;
  html += R"html(</option>
                <option value="1">DD.MM.YYYY</option>
                <option value="2">MM/DD/YYYY</option>
                <option value="3">YYYY/MM/DD</option>
              </select>
            </div>
)html";
}


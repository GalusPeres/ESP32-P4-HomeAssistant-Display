#include "src/types/sensor/web_html.h"

void append_sensor_fields_html(String& html, const String& tab_id, const std::vector<String>& sensorOptions) {
  html += R"html(
            <!-- Sensor Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_sensor_fields" class="type-fields">
              <label>Sensor Entity</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_sensor_entity">
                <option value="">Keine Auswahl</option>
)html";

  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>Einheit</label>
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_sensor_unit" placeholder="z.B. °C">
                <label>Nachkommastellen (leer = Originalwert)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_decimals" min="0" max="6" step="1" placeholder="z.B. 1">
                <label>Wert-Groesse</label>
                <select id=")html";
  html += tab_id;
  html += R"html(_sensor_value_font">
                  <option value="0">40 (Default)</option>
                  <option value="1">20</option>
                  <option value="2">24</option>
                  <option value="3">28</option>
                  <option value="4">32</option>
                </select>
                <label>Anzeige-Modus</label>
                <select id=")html";
  html += tab_id;
  html += R"html(_sensor_display_mode">
                  <option value="0">Keine</option>
                  <option value="1">Gauge</option>
                  <option value="2">Graph</option>
                </select>
                <div id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_fields" class="gauge-fields hidden">
                  <label>Gauge Min</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_min" step="1" placeholder="z.B. 0">
                  <label>Gauge Max</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_max" step="1" placeholder="z.B. 100">
                  <label>Bogengrad (90-359)</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_arc" min="90" max="359" step="1" placeholder="100">
                  <label>Gauge Groesse (100-800 px)</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_size" min="100" max="800" step="1" placeholder="350">
                  <label>Y-Offset (-100 bis 200)</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_gauge_y_offset" min="-100" max="200" step="1" placeholder="12">
                </div>
                <div id=")html";
  html += tab_id;
  html += R"html(_sensor_graph_fields" class="graph-fields hidden">
                  <label>Graph Hoehe (20-200 px)</label>
                  <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_graph_height" min="20" max="200" step="1" placeholder="60">
                </div>
                <label>Popup oeffnen</label>
                <select id=")html";
  html += tab_id;
  html += R"html(_sensor_popup_open_mode">
                  <option value="0">Long Press</option>
                  <option value="1">Short Press</option>
                </select>
                <label>Wert Y-Offset (-100 bis 200)</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_sensor_value_y_offset" min="-100" max="200" step="1" placeholder="0">
            </div>
)html";
}

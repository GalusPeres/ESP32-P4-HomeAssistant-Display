#include "src/types/radar/web_html.h"

void append_radar_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Radar Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_radar_fields" class="type-fields">
              <label>Radar Gebiet</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_radar_preset">
                <option value="bayern">Bayern</option>
                <option value="germany">Deutschland</option>
                <option value="south">Sueddeutschland</option>
                <option value="custom">Custom BBOX</option>
              </select>
              <label>Custom BBOX</label>
              <input type="text" id=")html";
  html += tab_id;
  html += R"html(_radar_bbox" placeholder="47.0,8.9,50.8,14.0">
              <small class="field-hint">Format: minLat,minLon,maxLat,maxLon</small>
              <label>Frames</label>
              <input type="number" min="2" max="8" step="1" id=")html";
  html += tab_id;
  html += R"html(_radar_frame_count" value="6">
              <label>Frame Dauer (ms)</label>
              <input type="number" min="150" max="3000" step="50" id=")html";
  html += tab_id;
  html += R"html(_radar_frame_delay_ms" value="450">
              <label>Refresh (Sekunden)</label>
              <input type="number" min="60" max="3600" step="30" id=")html";
  html += tab_id;
  html += R"html(_radar_refresh_sec" value="300">
              <label>Popup oeffnen</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_radar_popup_open_mode">
                <option value="0">Long Press</option>
                <option value="1">Short Press</option>
              </select>
            </div>
)html";
}

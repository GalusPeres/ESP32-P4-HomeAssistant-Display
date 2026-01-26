#include "src/types/image/web_html.h"

void append_image_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Image Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_image_fields" class="type-fields">
              <label>Bildauswahl (.bin/.jpg von SD)</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_image_select">
              </select>
              <div id=")html";
  html += tab_id;
  html += R"html(_image_url_fields" style="display:none;">
                <label>Bild-URL (HTTP/HTTPS)</label>
                <input type="url" id=")html";
  html += tab_id;
  html += R"html(_image_url" placeholder="https://example.com/bild.jpg">
              </div>
              <label id=")html";
  html += tab_id;
  html += R"html(_image_interval_label">Diashow Intervall (Sekunden)</label>
              <input type="number" min="1" max="3600" step="1" id=")html";
  html += tab_id;
  html += R"html(_image_slideshow_sec" value="10">
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_image_preview">
                Kachelvorschau anzeigen (nur Display)
              </label>
              <input type="hidden" id=")html";
  html += tab_id;
  html += R"html(_image_path">
            </div>
)html";
}

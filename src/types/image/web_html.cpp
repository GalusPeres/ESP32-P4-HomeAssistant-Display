#include "src/types/image/web_html.h"

void append_image_fields_html(String& html, const String& tab_id) {
  html += R"html(
            <!-- Image Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_image_fields" class="type-fields">
              <label>Bild-URL (JPG / PNG / GIF)</label>
              <input type="url" id=")html";
  html += tab_id;
  html += R"html(_image_url" placeholder="https://www.dwd.de/DWD/wetter/radar/radfilm_bay_akt.gif">
              <label>Cache Intervall (Sekunden)</label>
              <input type="number" min="1" max="3600" step="1" id=")html";
  html += tab_id;
  html += R"html(_image_slideshow_sec" value="3600">
              <input type="hidden" id=")html";
  html += tab_id;
  html += R"html(_image_path">
            </div>
)html";
}

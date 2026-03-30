#include "src/types/navigate/web_html.h"
void append_navigate_fields_html(String& html, const String& tab_id, const String& navigateOptionsHtml) {
  (void)navigateOptionsHtml;
  html += R"html(
            <!-- Navigate Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_navigate_fields" class="type-fields"></div>
)html";
}

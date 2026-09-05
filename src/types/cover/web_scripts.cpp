#include "src/types/cover/web_scripts.h"

#include "src/core/config/config_manager.h"
#include "src/core/i18n/i18n.h"

namespace {

void append_cover_js_string(String& html, const String& value) {
  html += "'";
  for (size_t index = 0; index < value.length(); ++index) {
    const char c = value[index];
    switch (c) {
      case '\\': html += "\\\\"; break;
      case '\'': html += "\\'"; break;
      case '\r': break;
      case '\n': html += "\\n"; break;
      case '<': html += "\\x3c"; break;
      default: html += c; break;
    }
  }
  html += "'";
}

}  // namespace

void append_cover_scripts(String& html) {
  html += R"html(
  <script>
)html";

  const char* language = configManager.getConfig().language;
  html += "  const COVER_I18N = Object.freeze({\n";
  auto append_i18n = [&](const char* key, const String& value) {
    html += "    ";
    html += key;
    html += ": ";
    append_cover_js_string(html, value);
    html += ",\n";
  };
  append_i18n("open", i18n::cover_state_label(language, "open"));
  append_i18n("opening", i18n::cover_state_label(language, "opening"));
  append_i18n("closed", i18n::cover_state_label(language, "closed"));
  append_i18n("closing", i18n::cover_state_label(language, "closing"));
  append_i18n(
      "unavailable", i18n::cover_state_label(language, "unavailable"));
  append_i18n("unknown", i18n::cover_state_label(language, "unknown"));
  html += "  });\n  </script>\n";
}

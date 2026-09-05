#include "src/types/navigate/web_scripts.h"
#include "src/core/config/config_manager.h"
#include "src/core/i18n/i18n.h"

namespace {

void appendJsString(String& output, const char* value) {
  output += "'";
  for (const char* p = value ? value : ""; *p; ++p) {
    if (*p == '\\' || *p == '\'') output += '\\';
    if (*p == '\r') continue;
    if (*p == '\n') output += "\\n";
    else output += *p;
  }
  output += "'";
}

}  // namespace

void append_navigate_scripts(String& html) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += "<script>const NAVIGATE_I18N={folderPinSaved:";
  appendJsString(html, tr.folder_pin_saved);
  html += ",folderPinSaveFailed:";
  appendJsString(html, tr.folder_pin_save_failed);
  html += ",folderPinCreateFirst:";
  appendJsString(html, tr.folder_pin_create_first);
  html += "};</script>\n";
}

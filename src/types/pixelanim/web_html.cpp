#include "src/types/pixelanim/web_html.h"

#include <FS.h>
#include <algorithm>
#include <vector>

#include "src/core/config/config_manager.h"
#include "src/core/i18n/i18n.h"
#include "src/devices/device.h"
#include "src/web/server/web_admin_utils.h"

namespace {

// Basename of a directory entry (the custom SD FS may hand back a full path).
String base_name(const String& raw) {
  int slash = raw.lastIndexOf('/');
  return (slash >= 0) ? raw.substring(slash + 1) : raw;
}

bool ends_with_panim(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".panim");
}

// Scans /animations and returns the .panim file names (basename only), sorted.
// Mirrors the working file-browser pattern in web_admin_files.cpp: open the
// directory and iterate openNextFile() directly -- do NOT gate on the opened
// dir's isDirectory(), which returns false on the custom WaveshareSDMMCFS.
std::vector<String> list_animations() {
  std::vector<String> names;
  // Animations live on the physical SD card (Device::sdFS == WaveshareSDMMC),
  // NOT in internal LittleFS (Device::storageFS). That is the filesystem the
  // file manager browses too.
  if (!Device::sdReady()) return names;

  fs::File root = Device::sdFS().open("/animations");
  if (!root) return names;

  for (fs::File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    if (!entry.isDirectory()) {
      const char* raw = entry.name();
      String name = base_name(raw ? String(raw) : String());
      if (name.length() && ends_with_panim(name)) names.push_back(name);
    }
  }

  std::sort(names.begin(), names.end(), [](const String& a, const String& b) {
    return a.compareTo(b) < 0;
  });
  return names;
}

}  // namespace

void append_pixelanim_fields_html(String& html, const String& tab_id) {
  const auto& tr = i18n::strings(configManager.getConfig().language);

  html += R"html(
            <!-- Animation Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_animation_fields" class="type-fields">
              <label>Animation</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_animation_file">
                <option value="">)html";
  html += tr.no_selection;
  html += R"html(</option>
)html";

  std::vector<String> anims = list_animations();
  for (const auto& name : anims) {
    html += "<option value=\"";
    appendHtmlEscaped(html, name);
    html += "\">";
    appendHtmlEscaped(html, name);
    html += "</option>";
  }

  html += R"html(
              </select>
              <div style="font-size:11px;color:#8a8a8a;margin-top:4px;">)html";
  if (anims.empty()) {
    html += "Keine Animationen gefunden &mdash; .panim-Dateien nach /animations auf die SD-Karte legen.";
  } else {
    html += ".panim-Dateien liegen im SD-Ordner /animations.";
  }
  html += R"html(</div>
              <label style="margin-top:10px;">Geschwindigkeit (<span id=")html";
  html += tab_id;
  html += R"html(_animation_fps_val">10</span> fps)</label>
              <input type="range" min="1" max="30" step="1" value="10" style="width:100%;" id=")html";
  html += tab_id;
  html += R"html(_animation_fps" oninput="document.getElementById(')html";
  html += tab_id;
  html += R"html(_animation_fps_val').textContent=this.value">
              <label style="margin-top:10px;">Darstellung</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_animation_fit">
                <option value="0">Einpassen</option>
                <option value="1">Randlos füllen</option>
                <option value="2">Strecken</option>
              </select>
              <label style="margin-top:10px;">Zoom (<span id=")html";
  html += tab_id;
  html += R"html(_animation_zoom_val">100</span> %)</label>
              <input type="range" min="25" max="300" step="5" value="100" style="width:100%;" id=")html";
  html += tab_id;
  html += R"html(_animation_zoom" oninput="document.getElementById(')html";
  html += tab_id;
  html += R"html(_animation_zoom_val').textContent=this.value">
            </div>
)html";
}

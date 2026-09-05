#include "src/web/server/web_admin.h"

#include "src/io/hardware_io.h"
#include "src/network/mqtt/mqtt_handlers.h"
#include "src/network/network_manager.h"
#include "src/web/server/web_admin_utils.h"

namespace {

void append_error_json(String& out, const String& error) {
  out = "{\"success\":false,\"error\":\"";
  for (size_t i = 0; i < error.length(); ++i) {
    const char c = error.charAt(i);
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': break;
      default: out += c; break;
    }
  }
  out += "\"}";
}

}  // namespace

void WebAdminServer::handleGetHardwareIo() {
  webAdminMarkActivity();
  String json = hardwareIo.toJson(true);
  if (!json.endsWith("}")) {
    server.send(500, "application/json", "{\"success\":false}");
    return;
  }
  json.remove(json.length() - 1);
  json += ",\"success\":true}";
  sendChunkedResponse(server, 200, "application/json", json);
}

void WebAdminServer::handleSaveHardwareIo() {
  webAdminMarkActivity();
  String error;
  if (!hardwareIo.replaceFromJson(server.arg("plain"), error)) {
    String json;
    append_error_json(json, error);
    server.send(400, "application/json", json);
    return;
  }

  // The retained device announcement is the source of dynamic HA entities.
  // Refresh it immediately while connected; otherwise the next MQTT connection
  // publishes it automatically.
  networkManager.publishBridgeConfig();
  // Renaming can change the local entity ID. Defer the more expensive route
  // scan until the Web Admin quiet window has elapsed.
  mqttRequestDynamicSlotsReload();
  server.send(200, "application/json", "{\"success\":true}");
}

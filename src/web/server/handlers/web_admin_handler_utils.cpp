#include "src/web/server/handlers/web_admin_handler_utils.h"

namespace web_admin_handlers {

bool endsWithIgnoreCase(const String& value, const char* suffix) {
  if (!suffix) return false;
  String v = value;
  v.toLowerCase();
  String s = suffix;
  s.toLowerCase();
  return v.endsWith(s);
}

void appendJsonEscaped(String& out, const String& value) {
  for (size_t i = 0; i < value.length(); ++i) {
    char c = value.charAt(i);
    if (c == '\"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else {
      out += c;
    }
  }
}

void sendJsonError(WebServer& server, int code, const String& error) {
  String json = "{\"success\":false,\"error\":\"";
  appendJsonEscaped(json, error);
  json += "\"}";
  server.send(code, "application/json", json);
}

}  // namespace web_admin_handlers

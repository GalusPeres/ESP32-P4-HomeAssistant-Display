#include "src/web/web_admin.h"
#include <WiFi.h>

WebAdminServer webAdminServer;

WebAdminServer::WebAdminServer() : server(80), running(false) {}

bool WebAdminServer::start() {
  if (running) {
    Serial.println("[WebAdmin] Server laeuft bereits");
    return true;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WebAdmin] Start abgebrochen - kein WiFi");
    return false;
  }

  server.on("/", [this]() { this->handleRoot(); });
  server.on("/mqtt", HTTP_POST, [this]() { this->handleSaveMQTT(); });
  server.on("/status", [this]() { this->handleStatus(); });
  server.on("/bridge_refresh", HTTP_POST, [this]() { this->handleBridgeRefresh(); });
  server.on("/bridge", HTTP_POST, [this]() { this->handleSaveBridge(); });
  server.on("/game_controls", HTTP_POST, [this]() { this->handleSaveGameControls(); });
  server.on("/restart", HTTP_POST, [this]() { this->handleRestart(); });
  server.on("/api/status", [this]() { this->handleStatus(); });
  server.on("/api/tiles", HTTP_GET, [this]() { this->handleGetTiles(); });
  server.on("/api/tiles", HTTP_POST, [this]() { this->handleSaveTiles(); });
  server.on("/api/tiles/reorder", HTTP_POST, [this]() { this->handleReorderTiles(); });
  server.on("/api/sensor_values", HTTP_GET, [this]() { this->handleGetSensorValues(); });

  server.begin();
  running = true;
  IPAddress ip = WiFi.localIP();
  Serial.printf("[WebAdmin] erreichbar unter http://%s\n", ip.toString().c_str());
  return true;
}

void WebAdminServer::stop() {
  if (!running) return;
  server.stop();
  running = false;
  Serial.println("[WebAdmin] Server gestoppt");
}

void WebAdminServer::handle() {
  if (!running) return;
  server.handleClient();
}

void WebAdminServer::handleRoot() {
  server.send(200, "text/html", getAdminPage());
}

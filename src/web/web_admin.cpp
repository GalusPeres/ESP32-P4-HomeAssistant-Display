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
  server.on("/api/folders", HTTP_GET, [this]() { this->handleGetFolders(); });
  server.on("/api/folders/tab", HTTP_GET, [this]() { this->handleGetFolderTab(); });
  server.on("/api/folders/delete", HTTP_POST, [this]() { this->handleDeleteFolder(); });
  server.on("/api/sensor_values", HTTP_GET, [this]() { this->handleGetSensorValues(); });
  server.on("/api/sd_images", HTTP_GET, [this]() { this->handleGetSdImages(); });
  server.on("/api/sd_icons", HTTP_GET, [this]() { this->handleGetSdIcons(); });
  server.on("/api/screenshot", HTTP_POST, [this]() { this->handleCreateScreenshot(); });
  server.on("/api/screenshot/download", HTTP_GET, [this]() { this->handleDownloadScreenshot(); });
  server.on("/api/ota/upload", HTTP_POST,
    [this]() { this->handleOtaUploadDone(); },
    [this]() { this->handleOtaUpdate(); });
  server.on("/api/ota/install", HTTP_POST, [this]() { this->handleStartOtaInstall(); });
  server.on("/api/ota/status", HTTP_GET, [this]() { this->handleGetOtaStatus(); });
  server.on("/api/upload_icon", HTTP_POST,
    [this]() { this->handleUploadIconDone(); },
    [this]() { this->handleUploadIcon(); });
  server.on("/api/files/list", HTTP_GET, [this]() { this->handleFileManagerList(); });
  server.on("/api/files/download", HTTP_GET, [this]() { this->handleFileManagerDownload(); });
  server.on("/api/files/delete", HTTP_POST, [this]() { this->handleFileManagerDelete(); });
  server.on("/api/files/rename", HTTP_POST, [this]() { this->handleFileManagerRename(); });
  server.on("/api/files/mkdir", HTTP_POST, [this]() { this->handleFileManagerMkdir(); });
  server.on("/api/files/upload", HTTP_POST,
    [this]() { this->handleFileManagerUploadDone(); },
    [this]() { this->handleFileManagerUpload(); });

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
  webAdminServiceOta();
}

void WebAdminServer::handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.send(200, "text/html; charset=utf-8", getAdminPage());
}

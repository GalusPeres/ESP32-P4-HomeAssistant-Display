#ifndef WEB_ADMIN_H
#define WEB_ADMIN_H

#include <Arduino.h>
#include <WebServer.h>
#include "src/core/config_manager.h"
#include "src/core/github_update.h"
#include "src/network/ha_bridge_config.h"

typedef GithubUpdate::CheckResult (*web_github_check_callback_t)();
typedef void (*web_github_install_callback_t)(const char* tag);

// Webinterface für MQTT-Konfiguration im normalen Netzwerk
// Läuft wenn das Gerät bereits mit WiFi verbunden ist
//
// Refactored into modular structure:
// - web_admin.cpp/h: Core server class (start/stop/handle)
// - web_admin_utils.cpp/h: Utility functions (parsing, escaping, etc.)
// - web_admin_handlers.cpp/h: HTTP request handlers
// - web_admin_html.cpp/h: HTML page generation with tab navigation

class WebAdminServer {
public:
  WebAdminServer();

  // Startet Webserver im normalen WiFi
  bool start();

  // Stoppt Webserver
  void stop();

  // Muss regelmäßig in loop() aufgerufen werden
  void handle();

  // Prüft ob Server läuft
  bool isRunning() const { return running; }

  void setGithubUpdateCallbacks(web_github_check_callback_t check_cb,
                                web_github_install_callback_t install_cb);
  void setGithubUpdateInstallFailed(const char* error);

  // Request Handler (implemented in web_admin_handlers.cpp)
  void handleRoot();
  void handleSaveMQTT();
  void handleSaveBridge();
  void handleBridgeRefresh();
  void handleStatus();
  void handleRestart();
  void handleGetTiles();
  void handleSaveTiles();
  void handleReorderTiles();
  void handleGetFolders();
  void handleGetFolderTab();
  void handleDeleteFolder();
  void handleGetSensorValues();
  void handleGetEntityOptions();
  void handleGetScreensaver();
  void handleSaveScreensaver();
  void handleGetHardwareIo();
  void handleSaveHardwareIo();
  void handleSaveTileBorders();
  void handleSaveSettingsTileVisibility();
  void handleGetScreensaverWallpaper();
  void handleGetSdImages();
  void handleGetSdIcons();
  void handleCreateScreenshot();
  void handleDownloadScreenshot();
  void handlePrepareOtaUpload();
  void handleOtaUpdate();
  void handleOtaRawUpdate();
  void handleOtaUploadDone();
  void handleStartOtaInstall();
  void handleGetOtaStatus();
  void handleGithubUpdateCheck();
  void handleGithubUpdateInstall();
  void handleGetGithubUpdateStatus();
  void handleUploadIcon();
  void handleUploadIconDone();
  void handleFileManagerList();
  void handleFileManagerDownload();
  void handleFileManagerDelete();
  void handleFileManagerRename();
  void handleFileManagerMkdir();
  void handleFileManagerUpload();
  void handleFileManagerUploadDone();
  void handleCoreDumpDownload();
  void handleCoreDumpErase();
  void handleCrashLogDownload();
  void handleSdDiagnosticsDownload();

  // HTML-Seiten (implemented in web_admin_html.cpp)
  String getAdminPage();
  String getSuccessPage();
  String getBridgeSuccessPage();
  String getStatusJSON();

  // Public access to server for handler methods
  WebServer server;

private:
  bool running;
  bool routes_registered;
  web_github_check_callback_t github_check_callback;
  web_github_install_callback_t github_install_callback;
  GithubUpdate::CheckResult last_github_check;
  bool github_check_valid;
  bool github_install_requested;
  String github_install_error;
};

// Globale Instanz
extern WebAdminServer webAdminServer;
bool webAdminOtaInProgress();
void webAdminServiceOta();
void webAdminMarkActivity();
bool webAdminRecentlyActive(uint32_t quiet_ms = 3000);

#endif // WEB_ADMIN_H

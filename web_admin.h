#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "config_manager.h"
#include "ha_bridge_config.h"

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

  // Request Handler (implemented in web_admin_handlers.cpp)
  void handleRoot();
  void handleSaveMQTT();
  void handleSaveBridge();
  void handleBridgeRefresh();
  void handleSaveGameControls();
  void handleStatus();
  void handleRestart();

  // HTML-Seiten (implemented in web_admin_html.cpp)
  String getAdminPage();
  String getSuccessPage();
  String getBridgeSuccessPage();
  String getStatusJSON();

  // Public access to server for handler methods
  WebServer server;

private:
  bool running;
};

// Globale Instanz
extern WebAdminServer webAdminServer;

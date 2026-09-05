#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "src/core/config/config_manager.h"

// Wi-Fi setup through a device-specific access point and captive portal.

const char* webConfigApSsid();
const char* webConfigApPassword();

class WebConfigServer {
public:
  WebConfigServer();

  // Start the hotspot and web server.
  bool start();

  // Stop the hotspot and web server.
  void stop();

  // Service DNS and HTTP regularly from loop().
  void handle();

  // Report whether configuration was saved.
  bool hasNewConfig() const { return config_saved; }

  // Clear the saved flag.
  void resetConfigFlag() { config_saved = false; }

  // Report whether the server is running.
  bool isRunning() const { return running; }

private:
  WebServer server;
  DNSServer dnsServer;
  bool running;
  bool config_saved;
  bool routes_registered;

  // Request Handler
  void handleRoot();
  void handleSave();
  void handleCaptivePortal();
  void handleNotFound();

  // HTML-Seiten
  String getConfigPage();
  String getSuccessPage();
};

// Shared instance.
extern WebConfigServer webConfigServer;

#endif // WEB_CONFIG_H

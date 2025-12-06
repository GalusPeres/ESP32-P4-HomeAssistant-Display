#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "src/core/config_manager.h"

// Webinterface für WiFi/MQTT-Konfiguration im Hotspot-Modus
// Erstellt einen Access Point "Tab5_Config" und bietet ein Webinterface zur Konfiguration

class WebConfigServer {
public:
  WebConfigServer();

  // Startet Hotspot und Webserver
  bool start();

  // Stoppt Hotspot und Webserver
  void stop();

  // Muss regelmäßig in loop() aufgerufen werden
  void handle();

  // Prüft ob Konfiguration gespeichert wurde
  bool hasNewConfig() const { return config_saved; }

  // Setzt Flag zurück
  void resetConfigFlag() { config_saved = false; }

  // Prüft ob Server läuft
  bool isRunning() const { return running; }

private:
  WebServer server;
  DNSServer dnsServer;
  bool running;
  bool config_saved;

  // Request Handler
  void handleRoot();
  void handleSave();
  void handleNotFound();

  // HTML-Seiten
  String getConfigPage();
  String getSuccessPage();
};

// Globale Instanz
extern WebConfigServer webConfigServer;

#endif // WEB_CONFIG_H

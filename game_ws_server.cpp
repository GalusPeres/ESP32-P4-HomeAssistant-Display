#include "game_ws_server.h"
#include <ArduinoJson.h>

// Globale Instanz
GameWSServer gameWSServer;

// Statische Referenz für Callback
static GameWSServer* g_instance = nullptr;

void GameWSServer::init(uint16_t port) {
  if (running) {
    Serial.println("[GameWS] Bereits initialisiert");
    return;
  }

  Serial.printf("[GameWS] Starte WebSocket Server auf Port %u...\n", port);

  ws = new WebSocketsServer(port);
  g_instance = this;

  ws->begin();
  ws->onEvent(onWebSocketEvent);

  running = true;

  Serial.printf("[GameWS] WebSocket Server läuft auf Port %u\n", port);
  Serial.println("[GameWS] Clients können sich verbinden: ws://<tab5-ip>:8081");
}

void GameWSServer::handle() {
  if (ws && running) {
    ws->loop();
  }
}

void GameWSServer::broadcastButtonPress(uint8_t slot, const char* name, uint8_t key_code, uint8_t modifier) {
  if (!ws || !running) return;

  // JSON erstellen
  JsonDocument doc;
  doc["type"] = "button_press";
  doc["slot"] = slot;
  doc["name"] = name;
  doc["key"] = key_code;
  doc["modifier"] = modifier;
  doc["timestamp"] = millis();

  String json;
  serializeJson(doc, json);

  // An alle verbundenen Clients broadcasten
  ws->broadcastTXT(json);

  Serial.printf("[GameWS] Broadcast: %s\n", json.c_str());
}

uint8_t GameWSServer::getClientCount() const {
  if (!ws || !running) return 0;
  return ws->connectedClients();
}

void GameWSServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[GameWS] Client #%u disconnected\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = g_instance->ws->remoteIP(num);
      Serial.printf("[GameWS] Client #%u connected from %s\n", num, ip.toString().c_str());

      // Willkommensnachricht senden
      JsonDocument doc;
      doc["type"] = "connected";
      doc["message"] = "Tab5 Game Controls WebSocket";
      doc["version"] = "1.0";

      String json;
      serializeJson(doc, json);
      g_instance->ws->sendTXT(num, json);
      break;
    }

    case WStype_TEXT:
      Serial.printf("[GameWS] Client #%u sent: %s\n", num, payload);
      // Hier könnten wir später Befehle vom Client empfangen (z.B. Button-Config)
      break;

    case WStype_ERROR:
      Serial.printf("[GameWS] Client #%u error\n", num);
      break;

    default:
      break;
  }
}

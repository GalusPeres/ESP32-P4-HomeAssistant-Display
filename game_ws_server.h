#pragma once
#include <Arduino.h>
#include <WebSocketsServer.h>

// WebSocket Server für Game Controls
// Sendet Button-Events an verbundene Clients (z.B. Electron App)

class GameWSServer {
public:
  void init(uint16_t port = 8081);
  void handle();

  // Sendet Button-Event an alle verbundenen Clients
  void broadcastButtonPress(uint8_t slot, const char* name, uint8_t key_code, uint8_t modifier);

  bool isRunning() const { return running; }
  uint8_t getClientCount() const;

private:
  WebSocketsServer* ws = nullptr;
  bool running = false;

  static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
};

// Globale Instanz
extern GameWSServer gameWSServer;

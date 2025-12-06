#pragma once
#include <Arduino.h>

// Forward declaration to avoid circular dependency
class WebAdminServer;

// HTTP request handler methods for WebAdminServer class
// These are implemented as methods of WebAdminServer and handle:
// - MQTT configuration saving
// - Home bridge configuration saving
// - Game controls configuration saving
// - Bridge refresh requests
// - Status queries
// - Device restart

// Note: All handler implementations are in web_admin_handlers.cpp
// and are methods of the WebAdminServer class defined in web_admin.h

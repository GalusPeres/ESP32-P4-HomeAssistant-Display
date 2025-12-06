#pragma once
#include <Arduino.h>

// Forward declaration to avoid circular dependency
class WebAdminServer;

// HTML generation methods for WebAdminServer class
// These are implemented as methods of WebAdminServer and generate:
// - Main admin page with tab navigation (Network, Home, Game Controls)
// - Success pages for various operations
// - Status JSON response

// Note: All HTML generation implementations are in web_admin_html.cpp
// and are methods of the WebAdminServer class defined in web_admin.h

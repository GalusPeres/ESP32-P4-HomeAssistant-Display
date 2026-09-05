#ifndef WEB_ADMIN_FONTS_H
#define WEB_ADMIN_FONTS_H

#include <Arduino.h>
#include <WebServer.h>

// Inter 4.1 is bundled so the admin UI uses the same typeface on Windows,
// iOS, and the physical display without depending on an internet CDN.
void appendWebFontFaceStyles(String& html);
void sendWebFontRegular(WebServer& server);
void sendWebFontSemibold(WebServer& server);

#endif  // WEB_ADMIN_FONTS_H

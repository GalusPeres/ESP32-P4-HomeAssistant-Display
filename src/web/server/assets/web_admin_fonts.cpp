#include "src/web/server/assets/web_admin_fonts.h"

namespace {

// Latin subset of Inter 4.1. The matching OFL-1.1 license is stored in
// src/fonts/Inter-LICENSE.txt.
static const uint8_t kInterRegularWoff2[] PROGMEM = {
#include "src/web/generated/inter_4_1_regular_woff2.inc"
};

static const uint8_t kInterSemiboldWoff2[] PROGMEM = {
#include "src/web/generated/inter_4_1_semibold_woff2.inc"
};

void sendFont(WebServer& server, const uint8_t* data, size_t size) {
  server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  server.send_P(200, PSTR("font/woff2"),
                reinterpret_cast<PGM_P>(data), size);
}

}  // namespace

void appendWebFontFaceStyles(String& html) {
  html += R"html(
  <style>
    @font-face {
      font-family:'HomeTiles Inter';
      src:url('/assets/inter-4.1-regular.woff2') format('woff2');
      font-style:normal;
      font-weight:400;
      font-display:swap;
    }
    @font-face {
      font-family:'HomeTiles Inter';
      src:url('/assets/inter-4.1-regular.woff2') format('woff2');
      font-style:normal;
      font-weight:500;
      font-display:swap;
    }
    @font-face {
      font-family:'HomeTiles Inter';
      src:url('/assets/inter-4.1-semibold.woff2') format('woff2');
      font-style:normal;
      font-weight:600;
      font-display:swap;
    }
    @font-face {
      font-family:'HomeTiles Inter';
      src:url('/assets/inter-4.1-semibold.woff2') format('woff2');
      font-style:normal;
      font-weight:700;
      font-display:swap;
    }
  </style>
)html";
}

void sendWebFontRegular(WebServer& server) {
  sendFont(server, kInterRegularWoff2, sizeof(kInterRegularWoff2));
}

void sendWebFontSemibold(WebServer& server) {
  sendFont(server, kInterSemiboldWoff2, sizeof(kInterSemiboldWoff2));
}

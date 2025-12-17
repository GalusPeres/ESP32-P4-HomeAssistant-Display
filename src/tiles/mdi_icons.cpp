#include "mdi_icons.h"
#include <Arduino.h>

// ============================================================
// ICON MAPPING: Name -> Unicode Codepoint
// ============================================================
// Format: {"icon-name", 0xF01C9},
//
// 🔴 PLATZHALTER: HIER LISTE EINFÜGEN! 🔴
// Ersetze diesen Kommentar mit der kompletten Liste aus icons.txt
// ============================================================

struct IconMapping {
  const char* name;
  uint32_t codepoint;
};

static const IconMapping iconMap[] PROGMEM = {
  // ============================================================
  // 🔴 LISTE HIER EINFÜGEN (von icons.txt) 🔴
  // WICHTIG: Muss alphabetisch sortiert sein für Binary Search!
  // ============================================================
  {"home", 0xF02DC},           // Beispiel (alphabetisch sortiert)
  {"lightbulb", 0xF0335},      // Beispiel
  {"thermometer", 0xF0500},    // Beispiel
  // ... HIER KOMMEN ALLE ICONS HIN (sortiert!) ...
  // ============================================================
};

static const size_t ICON_COUNT = sizeof(iconMap) / sizeof(iconMap[0]);

// Binary Search durch iconMap (sortiert nach Name)
static int32_t findIconIndex(const char* name) {
  if (!name || !*name) return -1;

  int32_t left = 0;
  int32_t right = ICON_COUNT - 1;

  while (left <= right) {
    int32_t mid = left + (right - left) / 2;

    // Name aus PROGMEM lesen
    char buffer[64];
    strncpy_P(buffer, iconMap[mid].name, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    int cmp = strcmp(name, buffer);

    if (cmp == 0) return mid;
    else if (cmp < 0) right = mid - 1;
    else left = mid + 1;
  }

  return -1; // Nicht gefunden
}

// Gibt den Codepoint für einen Icon-Namen zurück
uint32_t getMdiCodepoint(const String& iconName) {
  if (iconName.length() == 0) {
    Serial.println("[MDI] Icon name is empty");
    return 0;
  }

  String searchName = iconName;
  searchName.toLowerCase();
  searchName.trim();

  // Bereinigung von HA-Prefixen (mdi:flash -> flash)
  if (searchName.startsWith("mdi:")) {
    searchName.remove(0, 4);
  } else if (searchName.startsWith("mdi-")) {
    searchName.remove(0, 4);
  }

  Serial.printf("[MDI] Looking up icon: '%s' (from '%s', count=%d)\n",
                searchName.c_str(), iconName.c_str(), ICON_COUNT);

  int32_t index = findIconIndex(searchName.c_str());

  if (index >= 0) {
    uint32_t codepoint = pgm_read_dword(&iconMap[index].codepoint);
    Serial.printf("[MDI] Found icon at index %d, codepoint=0x%X\n", index, codepoint);
    return codepoint;
  }

  // Fallback: Fragezeichen-Icon
  Serial.printf("[MDI] Icon '%s' not found, using fallback\n", searchName.c_str());
  return 0xF02D8;
}

// Gibt ein String mit dem Unicode-Zeichen zurück (für lv_label_set_text)
String getMdiChar(const String& iconName) {
  uint32_t codepoint = getMdiCodepoint(iconName);
  if (codepoint == 0) return "";

  // UTF-8 Encoding für Unicode Codepoint
  String result = "";

  if (codepoint <= 0x7F) {
    result += (char)codepoint;
  } else if (codepoint <= 0x7FF) {
    result += (char)(0xC0 | (codepoint >> 6));
    result += (char)(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0xFFFF) {
    result += (char)(0xE0 | (codepoint >> 12));
    result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
    result += (char)(0x80 | (codepoint & 0x3F));
  } else if (codepoint <= 0x10FFFF) {
    result += (char)(0xF0 | (codepoint >> 18));
    result += (char)(0x80 | ((codepoint >> 12) & 0x3F));
    result += (char)(0x80 | ((codepoint >> 6) & 0x3F));
    result += (char)(0x80 | (codepoint & 0x3F));
  }

  return result;
}

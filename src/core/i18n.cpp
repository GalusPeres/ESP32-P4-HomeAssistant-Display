#include "src/core/i18n.h"

namespace i18n {

static const Strings kStringsDe = {
    "de",
    "de",
    "Sprache",
    "Zeitzone",
    "Zeitformat",
    "Datumsformat",
    "Auto (Sprache)",
    "Auto (Lokalisierung)",
    "24 Stunden",
    "12 Stunden",
    "English",
    "Deutsch",

    "Home",
    "Ordner ",

    "Waveshare Admin",
    "Waveshare Admin-Panel",
    "Konfiguration & Übersicht",
    "Klicke auf eine Kachel, um sie zu bearbeiten. Wähle den Typ (Sensor/Wetter/Szene/Key/Ordner/Settings/Switch/Bild/Uhr/Text) und passe die Einstellungen an.",
    "Ordner / Tab löschen",
    "Kachel Einstellungen",
    "Typ",
    "Titel",
    "Kachel-Titel",
    "Icon (MDI)",
    "z.B. home, thermometer, lightbulb",
    "Icon-Liste anzeigen",
    "Farbe",
    "Spalte",
    "Zeile",
    "Breite (Zellen)",
    "Höhe (Zellen)",
    "Änderungen werden automatisch gespeichert.",
    "Kopieren",
    "Einfügen",
    "Löschen",
    "Import / Export (alle Ordner & Kacheln)",
    "Export",
    "Import",
    "Import überschreibt alle Kacheln der vorhandenen Ordner.",
    "WiFi",
    "MQTT",
    "Lokalisierung",
    "Screenshot",
    "Firmware Update",
    "Screenshot erstellen & herunterladen",
    "Speichert /ui_screenshot.bmp auf der microSD-Karte. Die vorhandene Datei wird überschrieben.",
    "Firmware-Datei",
    "Aktuelle Firmware",
    "Update",
    "Hier nur die update.bin hochladen. Die factory.bin ist nur für den ersten Flash gedacht.",
    "Datei auswählen",
    "Keine Datei ausgewählt",

    "WiFi Status",
    "Verbunden",
    "Getrennt",
    "Offline",
    "Status: AP (PW: %s)",
    "WLAN",
    "SSID",
    "IP",
    "Passwort",
    "Statische IP",
    "Gateway",
    "Subnetzmaske",
    "DNS-Server",
    "Leer lassen für DHCP",
    "MQTT nicht konfiguriert",
    "AP aktivieren",
    "AP beenden",
    "Ja",
    "Nein",

    "MQTT Host / IP",
    "Port",
    "Benutzername",
    "Passwort",
    "MQTT Client ID",
    "leer = automatisch",
    "Leer lassen = automatisch aus der MAC-Adresse erzeugen.",
    "Geräte-Topic Basis",
    "Home Assistant Prefix",
    "Uhrzeit Schriftgröße",
    "Datum Schriftgröße",
    "Speichern",
    "Gerät wirklich neu starten?",
    "Gerät neu starten",
    "MQTT-Konfiguration gespeichert",
    "Das Gerät verbindet sich neu ...",
    "Bridge-Konfiguration gespeichert",
    "Die Daten wurden per MQTT übertragen.",
    "Speichern fehlgeschlagen",

    "Waveshare WiFi-Konfiguration",
    "WiFi-Konfiguration",
    "Schritt 1: Mit WLAN verbinden",
    "WiFi-Verbindung",
    "SSID (Netzwerkname)",
    "Mein WiFi",
    "Passwort",
    "Passwort",
    "Leer lassen für offenes Netzwerk",
    "Hinweis:",
    "Nach erfolgreicher WLAN-Verbindung kannst du über das Webinterface im normalen Netzwerk die MQTT-Einstellungen konfigurieren.",
    "Speichern & Verbinden",
    "Erfolgreich gespeichert!",
    "Die Konfiguration wurde erfolgreich gespeichert.<br>Das Gerät wird jetzt neu gestartet und versucht sich mit dem WiFi zu verbinden.",
    "Du wirst in 10 Sekunden automatisch weitergeleitet.<br>Falls die Verbindung nicht klappt, aktiviere den Hotspot-Modus erneut über die Einstellungen.",
    "Fehler: WiFi SSID ist erforderlich!",

    "Display",
    "Helligkeit",
    "Farbton",
    "Sättigung",
    "Sleep",
    "nach",
    "Nie",
    "Touch",
    "GT911 / kein IMU",

    "Leer",
    "Sensor",
    "Wetter",
    "Szene",
    "Key",
    "Ordner",
    "Schalter",
    "Uhr",
    "Text",
    "Counter",
    "Settings",
    "Zurück",

    "Keine Auswahl",
    "Sensor Entity",
    "Einheit",
    "Nachkommastellen (leer = Originalwert)",
    "Wert-Größe",
    "Anzeige-Modus",
    "Keine",
    "Gauge",
    "Graph",
    "Gauge Min",
    "Gauge Max",
    "Bogengrad (90-359)",
    "Gauge Größe (100-800 px)",
    "Y-Offset (-100 bis 200)",
    "Graph Höhe (20-200 px)",
    "Popup öffnen",
    "Short Press",
    "Long Press",
    "Wert Y-Offset (-100 bis 200)",
    "Weather Entity",
    "Schalter/Licht",
    "Anzeige",
    "Icon Button",
    "LVGL Switch",
    "Uhrzeit anzeigen",
    "Datum anzeigen",
    "Uhrzeit Schriftgröße",
    "Datum Schriftgröße",
    "Text",
    "Text für die Kachel",
    "Text-Größe",
    "Max 31 Zeichen gespeichert.",
    "Ziel-Ordner",
    "Neuer Ordner",
    "Szene",
    "Makro",
    "Beispiele: g, ctrl+g, ctrl+shift+a",
    "Startwert",
    "Tap: +1, Long-Press: Reset auf 0",

    "Bitte zuerst eine Kachel wählen",
    "Kachel kopiert",
    "Keine kopierte Kachel vorhanden",
    "Kachel eingefügt",
    "Settings-Kachel (fest)",
    "Zurück-Kachel (fest)",
    "Diese Kachel kann nicht gelöscht werden",
    "Dieser Ordner kann nicht gelöscht werden",
    "Ordner \"{name}\" wirklich löschen?\n\nAlle Kacheln in diesem Ordner werden gelöscht und die Ordner-Kachel im übergeordneten Ordner wird entfernt.",
    "Ordner gelöscht",
    "Fehler beim Löschen",
    "Ordner nicht gefunden",
    "Kachel gespeichert & Display aktualisiert!",
    "Unbekannt",
    "Netzwerkfehler",
    "Netzwerkfehler beim Speichern",
    "Export erstellt!",
    "Export fehlgeschlagen",
    "Import-JSON ungültig",
    "Import fehlgeschlagen",
    "Import läuft...",
    "Import abgeschlossen!",
    "Kachel passt dort nicht hin",
    "Keine sinnvolle Anordnung gefunden",
    "Kacheln verschoben & gespeichert!",
    "Fehler beim Verschieben",
    "Netzwerkfehler beim Verschieben",
    "Screenshot wird erstellt...",
    "Screenshot gespeichert & Download gestartet!",
    "Screenshot fehlgeschlagen",
    "Bitte zuerst eine update.bin auswählen",
    "Firmware wird aktualisiert...",
    "Update wird installiert...",
    "Warte auf Neustart...",
    "Update erfolgreich installiert. Das Gerät startet jetzt neu.",
    "Firmware-Update fehlgeschlagen"};

static const Strings kStringsEn = {
    "en",
    "en",
    "Language",
    "Time zone",
    "Time format",
    "Date format",
    "Auto (language)",
    "Auto (localization)",
    "24-hour",
    "12-hour",
    "English",
    "Deutsch",

    "Home",
    "Folder ",

    "Waveshare Admin",
    "Waveshare Admin Panel",
    "Configuration & Overview",
    "Click a tile to edit it. Choose the type (Sensor/Weather/Scene/Key/Folder/Settings/Switch/Image/Clock/Text) and adjust its settings.",
    "Delete Folder / Tab",
    "Tile Settings",
    "Type",
    "Title",
    "Tile title",
    "Icon (MDI)",
    "e.g. home, thermometer, lightbulb",
    "Show icon list",
    "Color",
    "Column",
    "Row",
    "Width (cells)",
    "Height (cells)",
    "Changes are saved automatically.",
    "Copy",
    "Paste",
    "Delete",
    "Import / Export (all folders & tiles)",
    "Export",
    "Import",
    "Import overwrites all tiles in existing folders.",
    "WiFi",
    "MQTT",
    "Localization",
    "Screenshot",
    "Firmware Update",
    "Create & Download Screenshot",
    "Saves /ui_screenshot.bmp to the microSD card. The existing file is overwritten.",
    "Firmware file",
    "Current firmware",
    "Update",
    "Upload only the update.bin here. The factory.bin is only for the first flash.",
    "Choose file",
    "No file selected",

    "WiFi Status",
    "Connected",
    "Disconnected",
    "Offline",
    "Status: AP (PW: %s)",
    "WiFi",
    "SSID",
    "IP",
    "Password",
    "Static IP",
    "Gateway",
    "Subnet mask",
    "DNS server",
    "Leave empty for DHCP",
    "MQTT not configured",
    "Enable AP",
    "Disable AP",
    "Yes",
    "No",

    "MQTT Host / IP",
    "Port",
    "Username",
    "Password",
    "MQTT Client ID",
    "empty = automatic",
    "Leave empty to generate it automatically from the MAC address.",
    "Device topic base",
    "Home Assistant prefix",
    "Time font size",
    "Date font size",
    "Save",
    "Restart device now?",
    "Restart Device",
    "MQTT configuration saved",
    "The device is reconnecting ...",
    "Bridge configuration saved",
    "Data was sent via MQTT.",
    "Save failed",

    "Waveshare WiFi Configuration",
    "WiFi Configuration",
    "Step 1: Connect to WiFi",
    "WiFi Connection",
    "SSID (network name)",
    "My WiFi",
    "Password",
    "Password",
    "Leave empty for an open network",
    "Note:",
    "After the WiFi connection works, you can configure MQTT later through the web interface on your normal network.",
    "Save & Connect",
    "Saved successfully!",
    "The configuration was saved successfully.<br>The device will now restart and try to connect to WiFi.",
    "You will be redirected automatically in 10 seconds.<br>If the connection fails, enable hotspot mode again from the settings.",
    "Error: WiFi SSID is required!",

    "Display",
    "Brightness",
    "Hue",
    "Saturation",
    "Sleep",
    "after",
    "Never",
    "Touch",
    "GT911 / no IMU",

    "Empty",
    "Sensor",
    "Weather",
    "Scene",
    "Key",
    "Folder",
    "Switch",
    "Clock",
    "Text",
    "Counter",
    "Settings",
    "Back",

    "No selection",
    "Sensor Entity",
    "Unit",
    "Decimals (empty = original value)",
    "Value size",
    "Display mode",
    "None",
    "Gauge",
    "Graph",
    "Gauge Min",
    "Gauge Max",
    "Arc degree (90-359)",
    "Gauge size (100-800 px)",
    "Y offset (-100 to 200)",
    "Graph height (20-200 px)",
    "Open popup",
    "Short Press",
    "Long Press",
    "Value Y offset (-100 to 200)",
    "Weather Entity",
    "Switch / Light",
    "Display",
    "Icon Button",
    "LVGL Switch",
    "Show time",
    "Show date",
    "Time font size",
    "Date font size",
    "Text",
    "Text for the tile",
    "Text size",
    "Max 31 characters are stored.",
    "Target folder",
    "New folder",
    "Scene",
    "Macro",
    "Examples: g, ctrl+g, ctrl+shift+a",
    "Start value",
    "Tap: +1, long press: reset to 0",

    "Please select a tile first",
    "Tile copied",
    "No copied tile available",
    "Tile pasted",
    "Settings tile (fixed)",
    "Back tile (fixed)",
    "This tile cannot be deleted",
    "This folder cannot be deleted",
    "Delete folder \"{name}\"?\n\nAll tiles in this folder will be deleted and the folder tile in the parent folder will be removed.",
    "Folder deleted",
    "Delete failed",
    "Folder not found",
    "Tile saved & display updated!",
    "Unknown",
    "Network error",
    "Network error while saving",
    "Export created!",
    "Export failed",
    "Invalid import JSON",
    "Import failed",
    "Import in progress...",
    "Import complete!",
    "Tile does not fit there",
    "No valid arrangement found",
    "Tiles moved & saved!",
    "Move failed",
    "Network error while moving",
    "Creating screenshot...",
    "Screenshot saved & download started!",
    "Screenshot failed",
    "Please select an update.bin first",
    "Updating firmware...",
    "Installing update...",
    "Waiting for restart...",
    "Update installed successfully. The device is restarting now.",
    "Firmware update failed"};

const char* normalize_language_code(const char* language_code) {
  if (!language_code || !language_code[0]) return kStringsEn.code;
  if ((language_code[0] == 'd' || language_code[0] == 'D') &&
      (language_code[1] == 'e' || language_code[1] == 'E') &&
      language_code[2] == '\0') {
    return kStringsDe.code;
  }
  return kStringsEn.code;
}

const Strings& strings(const char* language_code) {
  return (normalize_language_code(language_code)[0] == 'd') ? kStringsDe : kStringsEn;
}

String build_language_options_html(const char* selected_code) {
  const char* normalized = normalize_language_code(selected_code);
  String html;
  html.reserve(128);
  html += "<option value=\"en\"";
  if (strcmp(normalized, "en") == 0) html += " selected";
  html += ">";
  html += "English";
  html += "</option>";
  html += "<option value=\"de\"";
  if (strcmp(normalized, "de") == 0) html += " selected";
  html += ">";
  html += "Deutsch";
  html += "</option>";
  return html;
}

static bool parse_iso_date_internal(const String& iso, int& y, int& m, int& d) {
  if (iso.length() < 10) return false;
  if (iso.charAt(4) != '-' || iso.charAt(7) != '-') return false;
  y = iso.substring(0, 4).toInt();
  m = iso.substring(5, 7).toInt();
  d = iso.substring(8, 10).toInt();
  return (y > 0 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}

String weather_condition_label(const char* language_code, const String& condition) {
  String key = condition;
  key.trim();
  key.toLowerCase();
  if (!key.length()) return "--";

  const bool is_de = normalize_language_code(language_code)[0] == 'd';

  if (key == "clear-night") return is_de ? "Klare Nacht" : "Clear night";
  if (key == "cloudy") return is_de ? "Bewölkt" : "Cloudy";
  if (key == "exceptional") return is_de ? "Ausnahme" : "Exceptional";
  if (key == "fog") return is_de ? "Nebel" : "Fog";
  if (key == "hail") return is_de ? "Hagel" : "Hail";
  if (key == "lightning") return is_de ? "Gewitter" : "Lightning";
  if (key == "lightning-rainy") return is_de ? "Gewitterregen" : "Lightning rain";
  if (key == "partlycloudy") return is_de ? "Teilw. bewölkt" : "Partly cloudy";
  if (key == "pouring") return is_de ? "Starkregen" : "Pouring";
  if (key == "rainy") return is_de ? "Regen" : "Rain";
  if (key == "snowy") return is_de ? "Schnee" : "Snow";
  if (key == "snowy-rainy") return is_de ? "Schneeregen" : "Sleet";
  if (key == "sunny") return is_de ? "Sonnig" : "Sunny";
  if (key == "windy") return is_de ? "Windig" : "Windy";
  if (key == "windy-variant") return is_de ? "Böig" : "Windy";

  String text = condition;
  text.replace("-", " ");
  text.replace("_", " ");
  text.trim();
  return text.length() ? text : "--";
}

String weather_weekday_short(const char* language_code, const String& iso) {
  int y = 0, m = 0, d = 0;
  if (!parse_iso_date_internal(iso, y, m, d)) return "";

  int mm = m;
  int yy = y;
  if (mm < 3) {
    mm += 12;
    yy -= 1;
  }
  int K = yy % 100;
  int J = yy / 100;
  int h = (d + (13 * (mm + 1)) / 5 + K + (K / 4) + (J / 4) + (5 * J)) % 7;
  int dow = (h + 6) % 7;
  if (dow < 0 || dow > 6) return "";

  static const char* kDaysDe[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
  static const char* kDaysEn[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const bool is_de = normalize_language_code(language_code)[0] == 'd';
  return String(is_de ? kDaysDe[dow] : kDaysEn[dow]);
}

const char* weather_tomorrow_label(const char* language_code) {
  return (normalize_language_code(language_code)[0] == 'd') ? "Morgen" : "Tomorrow";
}

}  // namespace i18n

#ifndef GITHUB_UPDATE_H
#define GITHUB_UPDATE_H

#include <Arduino.h>

// Update-ueber-GitHub: Versions-Check ueber den /releases/latest-Redirect
// (Location-Header genuegt - kein JSON, keine API, kein Rate-Limit) und
// OTA-Install per HTTPS-Download des Release-Assets in die Update-Partition.
//
// Namenskonvention der GitHub-Releases (MUSS beim Anlegen eingehalten werden):
//   Tag:   vX.Y.Z (gleiches Format wie FW_VERSION in version.txt)
//   Asset: hometiles_<tag>_<geraete-key>.bin
//   z.B.   hometiles_v0.3.2_waveshare_touch_lcd_8.bin
//   (bis v0.2.9 hiess das Schema esp32-p4-homeassistant-display-*; ein Geraet
//   auf <= v0.2.9 faellt automatisch auf diesen alten Namen zurueck, wenn der
//   aktuelle Asset-Name auf dem Release fehlt -- siehe install() in
//   github_update.cpp. Neue Releases muessen den alten Namen NICHT mehr
//   mitliefern, seit v0.3.1 keine Geraete mehr auf <= v0.2.9 unterwegs sind.)
// Die Geraete-Keys sind: waveshare_touch_lcd_7, waveshare_touch_lcd_8,
// waveshare_touch_lcd_10_1, waveshare_4b, m5stacks_tab5 und
// guition_jc8012p4a1.
//
// Repo wurde von ESP32-P4-HomeAssistant-Display auf HomeTiles umbenannt.
// GitHub redirected die alte URL dauerhaft, d.h. Geraete mit einer aelteren
// Firmware (die noch die alte URL einkompiliert haben) funktionieren weiter.
// Die alte URL darf trotzdem NIE wieder als eigenes Repo angelegt werden --
// das wuerde den Redirect kaputt machen.
namespace GithubUpdate {

constexpr const char* kRepoUrl =
    "https://github.com/GalusPeres/HomeTiles";

struct CheckResult {
  bool ok = false;                // Anfrage erfolgreich beantwortet
  bool update_available = false;  // latest_tag ist neuer als FW_VERSION
  bool tls_alloc_failed = false;  // Handshake scheiterte an internem RAM
  char latest_tag[24] = {};       // z.B. "v0.3.0"
};

// Blockiert 1-3 Sekunden (TLS-Handshake) - nur vom Loop-Task aufrufen,
// nie direkt aus einem LVGL-Event-Callback (Pending-Flag-Muster wie beim
// Hotspot-Toggle).
CheckResult checkLatest();

// Fortschritt in Bytes; total bleibt 0, solange die Groesse unbekannt ist.
// Der Callback ist auch der LVGL-Pump-Takt des Aufrufers.
typedef void (*ProgressFn)(size_t written, size_t total);

// Laedt das Asset des laufenden Geraeteprofils und schreibt es in die
// OTA-Partition. Blockiert minutenlang; der Aufrufer legt vorher MQTT und
// Web-Admin still (TLS braucht ~45KB internes RAM) und startet bei true
// selbst neu. Bei false steht der Grund in error_out.
bool install(const char* tag, ProgressFn progress, String& error_out);

// Forensik des letzten install()-Laufs (fehlgeschlagene Ranges, Versuche,
// Speicher-/WLAN-Lage), mehrzeilig. Der Aufrufer haengt sie vor dem sicheren
// Neustart an den Absturzbericht an, da der Neustart selbst keinen Core-Dump
// hinterlaesst. Leer, solange kein Install lief.
String lastInstallDiag();

// True, wenn der letzte install()-Fehlschlag NACH der Validierung von Asset
// und Firmware-Metadaten passierte (Transportabriss statt inhaltlichem
// Fehler). Nur dann lohnt ein automatischer neuer Versuch nach dem Neustart;
// bei "device mismatch" o.ae. wuerde er nur eine Reboot-Schleife erzeugen.
bool lastInstallRetryable();

}  // namespace GithubUpdate

#endif  // GITHUB_UPDATE_H

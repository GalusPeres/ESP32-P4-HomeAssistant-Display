#pragma once

#include <Arduino.h>

#include "src/tiles/tile_config.h"

// Globaler Screensaver-Modus. Die Konfiguration liegt unabhaengig von den
// Clock-Tiles in screensaver_config; ein Tap auf jede Clock-Tile oeffnet
// denselben Modus.
void show_image_screensaver();
void hide_image_screensaver();
bool is_image_screensaver_visible();
void image_screensaver_brightness_changed();

// Laedt das erste konfigurierte Bild verzoegert in den PSRAM-Cache.
void preload_image_screensaver();

// Im Hauptloop aufrufen. Startet den Screensaver nach der in den Display-
// Einstellungen konfigurierten Inaktivitaetszeit.
void service_image_screensaver_auto(uint32_t last_activity_ms);

// Nach einer Speicherung aus dem Admin-Panel. Eine offene Ansicht uebernimmt
// Bild- und Uhr-Aenderungen im naechsten LVGL-Tick, ohne das Overlay zu
// schliessen. preview_wallpaper ist nur die aktuell im Editor ausgewaehlte
// Vorschau und wird nicht dauerhaft gespeichert.
void image_screensaver_config_changed(
    const String& preview_wallpaper = String());

// Getrennter Live-Refresh fuer Kachel-Saves/Drag&Drop. Er baut nur das kleine
// Screensaver-Grid neu und fasst Bild und Uhr nicht an.
void image_screensaver_tiles_changed();

// Szenen in der unteren Reihe verwenden dieselbe bereits vorhandene Publish-
// Funktion wie normale Tiles.
void image_screensaver_set_scene_callback(void (*callback)(const char*));

// Tile-Renderer braucht bei seinem getrennten Screensaver-Widget-Kontext die
// passende Slot-Konfiguration (keine MQTT-Sonderbehandlung).
const Tile* image_screensaver_get_slot_tile(uint8_t index);

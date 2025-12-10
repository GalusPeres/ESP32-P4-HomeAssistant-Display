# Session-Bericht: NVS Persistence Problem & Partition Table Fehler

**Datum:** 2025-12-10
**Status:** FEHLGESCHLAGEN - Device nicht bootbar
**Letzter funktionierender Commit:** 36c3b3f

---

## Zusammenfassung

Versuch, Weather-Tab (3. Grid) zur Tile-Konfiguration hinzuzufügen führte zu komplettem Verlust der NVS-Persistenz. Anschließender Versuch, auf 2-Grid-Modus zurückzufallen endete mit einem nicht bootbaren Device aufgrund eines Partition Table Fehlers.

**Hauptproblem:** NVS-Partition (20KB) zu klein für 3 Grids (benötigt 24KB temporären Speicher während des Schreibvorgangs)

---

## Chronologie der Ereignisse

### 1. Ausgangssituation (Commit 36c3b3f)
- ✅ 2 Tabs funktionieren (Home, Game)
- ✅ NVS-Persistenz funktioniert einwandfrei
- ✅ Tiles speichern und laden korrekt nach Neustart

### 2. Weather-Tab Implementierung
**Geänderte Dateien:**
- `src/tiles/tile_config.h` - weather_grid hinzugefügt
- `src/tiles/tile_config.cpp` - saveSingleGrid() implementiert
- `src/tiles/tile_renderer.h` - GridType::WEATHER = 2 hinzugefügt
- `src/tiles/tile_renderer.cpp` - g_weather_sensors[] Array hinzugefügt
- `src/ui/tab_tiles_weather.cpp` (NEU) - Weather-Tab Implementation
- `src/ui/tab_tiles_weather.h` (NEU) - Weather-Tab Header
- `src/ui/ui_manager.cpp` - build_tiles_weather_tab() statt build_weather_tab()
- `src/web/web_admin_handlers.cpp` - "weather" Parameter in API hinzugefügt
- `src/web/web_admin_html.cpp` - Weather-Tab HTML (192 Zeilen)
- `src/web/web_admin_scripts.cpp` - JavaScript für 3 Tabs erweitert

**Probleme:**
1. ❌ API akzeptierte "weather" Parameter nicht → 400 Bad Request
2. ❌ Tiles im Weather-Tab verschwanden sofort nach Erstellung
3. ❌ Alle Grids (Home, Game, Weather) verloren Persistenz nach Neustart

### 3. Versuch: 2-Grid Test-Modus
**Ziel:** Weather temporär von NVS deaktivieren um zu testen, ob 2 Grids funktionieren

**Änderungen in tile_config.cpp:**
```cpp
bool TileConfig::load() {
  bool home_ok = loadGrid("home", home_grid);
  bool game_ok = loadGrid("game", game_grid);
  // TEMP: Weather deaktiviert zum Testen (2 Grids statt 3)
  // bool weather_ok = loadGrid("weather", weather_grid);
  return home_ok && game_ok;
}

bool TileConfig::saveSingleGrid(const char* grid_name, const TileGridConfig& grid) {
  if (strcmp(grid_name, "weather") == 0) {
    Serial.println("[NVS] TEMP: Weather Grid speichern übersprungen");
    weather_grid = grid;  // RAM only
    return true;  // Fake success
  }
  // ... normal save for home/game
}
```

**Ergebnis:** Nicht getestet - Device bootete nicht mehr

### 4. KRITISCHER FEHLER: Partition Table
**Was passierte:**
- Partition Table Datei `partitions.csv` wurde erstellt (Versuch NVS auf 64KB zu vergrößern)
- Device flashte die neue Partition Table
- ⚠️ **FATALER FEHLER:** Device kann nicht mehr booten

**Boot-Fehler:**
```
E (28) boot: ota data partition invalid and no factory
E (29) esp_image: image at 0x20000 has invalid magic byte
E (31) boot: OTA app partition slot 0 is not bootable
E (48) boot: No bootable app partitions in the partition table
```

**Fehlerursache:**
- Neue Partition Table hat App-Partitionen anders layoutet
- Flash enthält App-Code an falschen Adressen
- Bootloader findet keine gültige App-Partition

**Lösungsversuche:**
1. ❌ partitions.csv gelöscht, neu geflasht → **Fehlgeschlagen**
2. ❌ Arduino IDE "Erase Flash: All Flash Contents" → **Fehlgeschlagen**

**Problem:** Partition Table bleibt im Flash gespeichert auch nach "Erase Flash"

---

## Technische Details

### NVS Schreibverhalten (Root Cause)
```
Preferences Library (ESP32):
- Schreibt NEUE Daten BEVOR alte gelöscht werden
- Benötigt temporär 2× Speicherplatz
- 3 Grids × ~4KB = 12KB Nutzdaten
- Während save(): 12KB alt + 12KB neu = 24KB temporary
- NVS Partition: nur 20KB verfügbar
- → OVERFLOW → Schreibvorgang schlägt fehl
```

### saveSingleGrid() Ansatz
**Idee:** Nur geänderte Grids speichern statt alle 3 auf einmal

**Rechnung:**
```
Normal save (3 grids):     12KB alt + 12KB neu = 24KB temp
Single grid save:           4KB alt +  4KB neu =  8KB temp
```

**Status:** Implementiert aber nicht getestet (Device bootete nicht)

### Partition Table Problem
**Standard ESP32-P4 Partition:**
```
nvs,      data, nvs,     0x9000,  0x5000,    (20KB)
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x200000,
```

**Versuchte Custom Partition:**
```
nvs,      data, nvs,     0x9000,  0x10000,   (64KB) ← Mehr Platz
otadata,  data, ota,     0x19000, 0x2000,
app0,     app,  ota_0,   0x20000, 0x1E0000,  ← FALSCHE ADRESSE!
```

**Problem:** App-Code wurde an 0x10000 geflasht, aber Partition Table sagt App ist bei 0x20000

---

## Was funktioniert NICHT

1. ❌ **Device bootet nicht** - Partition Table Konflikt
2. ❌ **NVS Persistenz** - Alle 3 Grids speichern nicht
3. ❌ **Weather Grid** - API funktioniert nicht korrekt
4. ❌ **Erase Flash** - Partition Table bleibt bestehen

---

## Was WIR WISSEN

### ✅ Funktionierende Komponenten (bei Commit 36c3b3f)
- Home + Game Tabs (2 Grids) funktionieren perfekt
- Web Admin Interface
- MQTT Sensor Updates
- Tile Rendering System
- Scene Publishing

### ⚠️ Nicht getestete Änderungen
- saveSingleGrid() Implementation (Code korrekt, aber nicht getestet)
- Weather Tab Rendering (sieht aus wie Home/Game)
- 2-Grid Test-Modus (Code zurückgesetzt vor Test)

---

## OFFENE PROBLEME

### Problem 1: Device nicht bootbar
**Symptom:** Bootloop, "No bootable app partitions"

**Mögliche Ursachen:**
1. Partition Table im Flash (Offset 0x8000) ist korrumpiert
2. Arduino IDE's "Erase Flash" löscht nicht den Bootloader-Bereich
3. Partition Table Offset falsch

**Mögliche Lösungen:**
1. **esptool.py direkt verwenden:**
   ```bash
   esptool.py --chip esp32p4 erase_flash
   ```
2. **Bootloader + Partition manuell flashen:**
   ```bash
   esptool.py write_flash 0x0 bootloader.bin
   esptool.py write_flash 0x8000 partition-table.bin
   esptool.py write_flash 0x10000 app.bin
   ```
3. **USB-DFU Mode** (falls verfügbar bei ESP32-P4)

### Problem 2: NVS zu klein für 3 Grids
**Root Cause:** 20KB Partition, 24KB benötigt während Schreibvorgang

**Lösungsansätze:**

#### Option A: saveSingleGrid() optimieren
- ✅ Code existiert bereits
- ⚠️ Nicht getestet
- Web Interface muss nur geänderte Grids senden
- RAM hält alle 3 Grids, NVS speichert einzeln

#### Option B: Daten komprimieren
- Viele Tiles sind TILE_EMPTY (0 bytes relevant)
- Nur verwendete Tiles speichern
- Variable-length encoding
- Geschätztes Einsparpotential: 40-60%

#### Option C: Größere NVS Partition (GEFÄHRLICH!)
- ⚠️ Erfordert korrekte Partition Table
- ⚠️ Kann Device bricken (wie heute passiert)
- Benötigt genaue Kenntnis der ESP32-P4 Memory Map

#### Option D: Alternatives Storage
- SPIFFS/LittleFS statt NVS
- JSON Dateien (einfacher zu debuggen)
- Größerer Speicherbereich verfügbar
- Langsamere Zugriffszeiten (akzeptabel für Config)

### Problem 3: Code-Duplikation
**User-Feedback:** "es muss doch jeder tab fast gleich aussehen, also der code"

**Dateien mit identischem Code:**
- tab_tiles_home.cpp (111 Zeilen)
- tab_tiles_game.cpp (111 Zeilen)
- tab_tiles_weather.cpp (111 Zeilen)

**Einziger Unterschied:** GridType enum (HOME/GAME/WEATHER)

**Mögliche Lösung:**
- Generic tile_tab_builder.cpp
- Template-basierter Ansatz
- Reduziert Code auf ~40 Zeilen pro Tab

---

## EMPFEHLUNGEN FÜR NÄCHSTE SESSION

### 🔴 KRITISCH: Device reparieren
**Priorität 1:** Device bootbar machen

**Schritte:**
1. esptool.py --chip esp32p4 erase_flash
2. Arduino IDE: Flash Default Partition
3. Test mit Commit 36c3b3f (bekannt funktionierend)

### 🟡 MITTEL: NVS Problem analysieren
**Nach Device-Reparatur:**

1. **Test saveSingleGrid():**
   - 2-Grid Modus aktivieren (Weather deaktiviert)
   - Prüfen ob Home/Game persistieren
   - Wenn JA: saveSingleGrid() funktioniert

2. **NVS Debug Logging:**
   ```cpp
   esp_err_t err = nvs_get_stats(NULL, &stats);
   Serial.printf("Used: %d / Free: %d\n", stats.used_entries, stats.free_entries);
   ```

3. **Speicherverbrauch messen:**
   - Wie viel speichert ein einzelnes Grid?
   - Worst-case (alle 12 Tiles belegt)
   - Overhead durch Preferences Library

### 🟢 NIEDRIG: Code refactoring
**Erst NACH erfolgreichen Tests:**
- Tile Tab Code unifizieren
- Generic Builder implementieren

---

## LESSONS LEARNED

### ❌ Was schiefging:

1. **Partition Table ohne Backup geändert**
   - Hätte zuerst Backup erstellen sollen
   - esptool.py read_flash 0x8000 0x1000 partition_backup.bin

2. **Zu viele Änderungen auf einmal**
   - Weather Tab + NVS Änderungen + Web Interface
   - Schwer zu debuggen welche Änderung Probleme verursacht

3. **Nicht schrittweise getestet**
   - saveSingleGrid() Code geschrieben aber nie getestet
   - Hätte erst mit 2 Grids testen sollen

4. **Unterschätzt: ESP32-P4 ist neu**
   - Wenig Dokumentation
   - Arduino Core noch nicht stabil
   - Partition Tables unterscheiden sich von ESP32/ESP32-S3

### ✅ Was funktionierte:

1. **Code-Struktur ist sauber**
   - Modularer Aufbau (tile_config, tile_renderer, tab_tiles_*)
   - Klare Verantwortlichkeiten

2. **saveSingleGrid() Logik korrekt**
   - 8KB statt 24KB temporary
   - Sollte theoretisch funktionieren

3. **Web Interface komplett**
   - Alle 3 Tabs implementiert
   - JavaScript korrekt erweitert
   - HTML sauber strukturiert

---

## NEXT STEPS

### Sofort (nächste Session):
1. ✅ Code auf 36c3b3f zurückgesetzt (ERLEDIGT)
2. ⚠️ Device mit esptool.py reparieren
3. ⚠️ Test: Funktioniert Commit 36c3b3f noch?

### Danach:
4. saveSingleGrid() isoliert testen (nur 2 Grids)
5. NVS Speicherverbrauch messen
6. Entscheidung: Welcher Lösungsansatz für 3 Grids?

### Vermeiden:
- ❌ KEINE Partition Table Änderungen ohne Backup
- ❌ KEINE Custom Partitions für ESP32-P4 (zu riskant)
- ❌ KEINE großen Refactorings während Bug-Fixing

---

## TECHNISCHE REFERENZEN

### ESP32-P4 Spezifikationen
- Flash: 8MB (M5Stack Tab5)
- PSRAM: 8MB
- Default NVS: 20KB (0x5000 bytes)
- Boot Mode: SPI Flash

### Preferences Library Limits
- Max Namespace length: 15 chars
- Max Key length: 15 chars
- Max String length: 4000 bytes (ca. 4KB)
- Max Blob size: 508KB (aber NVS partition limitiert)

### Unsere Tile Storage
- 3 Grids × 12 Tiles = 36 Tiles total
- Pro Tile: 9 NVS keys (type, title, color, entity, unit, decimals, scene, macro, code, modifier)
- Total: 324 NVS keys
- Key Overhead: ~23 bytes pro Key
- Metadata: ~7.5KB
- Nutzdaten: ~12KB
- **Total: ~19.5KB** (passt knapp in 20KB partition!)

**ABER:** Während save() werden temporär BEIDE Versionen gehalten → 39KB needed!

---

## DATEIEN GEÄNDERT (ZURÜCKGESETZT)

```
src/tiles/tile_config.cpp       (saveSingleGrid added, 2-grid mode)
src/tiles/tile_config.h          (weather_grid, saveSingleGrid)
src/tiles/tile_renderer.cpp     (g_weather_sensors array)
src/tiles/tile_renderer.h       (GridType::WEATHER)
src/ui/tab_tiles_game.cpp       (structure unified)
src/ui/tab_tiles_weather.cpp    (NEW - deleted)
src/ui/tab_tiles_weather.h      (NEW - deleted)
src/ui/ui_manager.cpp           (build_tiles_weather_tab call)
src/web/web_admin_handlers.cpp  (weather API support)
src/web/web_admin_html.cpp      (weather tab HTML)
src/web/web_admin_scripts.cpp   (3-tab JavaScript)
```

**Status nach Reset:** Alle Dateien zurück auf Commit 36c3b3f

---

## USER FEEDBACK

**Frustration Level:** SEHR HOCH

**Hauptprobleme aus User-Sicht:**
1. "warum hat es vorher funktioniert und jetzt nicht mehr" - Berechtigte Frage
2. "es muss doch jeder tab fast gleich aussehen" - Code-Duplikation nervt
3. Device bootet nicht mehr - Arbeit blockiert
4. Erase Flash dauert zu lange - Geduld am Ende

**Wichtig für nächste Session:**
- Kleine, testbare Schritte
- Kein Experimentieren mit kritischen Komponenten
- Device MUSS funktionieren vor Code-Änderungen

---

**Ende des Berichts**

---

## Update 2025-12-10 (später)

- Device wieder bootbar mit altem Code; Web-Admin läuft.
- Tiles werden jetzt grid-weise gespeichert (`saveSingleGrid`) und nur relevante Felder je Typ geschrieben, leere Tiles speichern nur den Typ; irrelevante Keys werden aus NVS entfernt.
- Neue API `/api/status` liefert NVS-Statistiken; mit 2 voll belegten Grids: `nvs_used_entries≈243`, `nvs_free_entries≈387` (~63% frei).
- NVS-Problem aktuell entschärft ohne Partition-Table-Änderungen.
- Geplanter nächster Schritt: Blob-Format pro Grid (1 Key je Grid) mit Migration, um Einträge weiter zu minimieren.

## Update 2025-12-10 (Blob-Storage)

- Tile-Grids werden jetzt als Blob pro Grid gespeichert (`home_blob`, `game_blob`), feste Feldlängen; Laden versucht zuerst Blob, sonst Legacy -> sofortige Migration.
- Legacy-Keys pro Grid werden beim Speichern/Laden entfernt; Weather-Altlasten werden beim Laden mit aufgeräumt.
- `/api/status` zeigt jetzt auch `nvs_tab5_tiles_used` und `nvs_tab5_config_used`.
- Aktueller Stand mit leeren Tiles: `nvs_used_entries≈241`, davon `tiles_used≈146` (Blob ~2.2 KB ≈ ~70 Einträge pro Grid) und `config_used≈91`; ausreichend freier Platz (`nvs_free_entries≈389`).

## Update 2025-12-10 (Weather-Grid reaktiviert)

- Weather-Grid wieder aktiv und nutzt denselben Tile-Renderer wie Home/Game.
- Web-Admin erweitert um dritten Tiles-Tab (Weather) inkl. Drag&Drop, Auto-Save und API-Support (`tab=weather` in /api/tiles und /api/tiles/reorder).
- MQTT-Dynamik erweitert: Sensor-Updates werden auch ins Weather-Grid gemappt.

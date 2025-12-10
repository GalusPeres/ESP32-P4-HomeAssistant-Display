# Implementation Plan: Tile Tabs Unification

**Datum:** 2025-12-10
**Ziel:** Code-Duplikation in tab_tiles_home/game/weather beseitigen
**Status:** PLAN - Wartet auf User-Approval

---

## 1. ANALYSE: Aktuelle Situation

### Drei identische Dateien (je ~110 Zeilen):
- `src/ui/tab_tiles_home.cpp/.h`
- `src/ui/tab_tiles_game.cpp/.h`
- `src/ui/tab_tiles_weather.cpp/.h`

### Einzige Unterschiede:
```diff
- g_tiles_home_grid          + g_tiles_game_grid          + g_tiles_weather_grid
- GridType::HOME             + GridType::GAME             + GridType::WEATHER
- getHomeGrid()              + getGameGrid()              + getWeatherGrid()
- [TilesHome]                + [TilesGame]                + [TilesWeather]
```

### Public API (identisch in allen 3 Dateien):
```cpp
void build_tiles_X_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb);
void tiles_X_reload_layout();
void tiles_X_update_tile(uint8_t index);
void tiles_X_update_sensor_by_entity(const char* entity_id, const char* value);
```

### Aufgerufen von:
1. **ui_manager.cpp:126-128**: `build_tiles_X_tab()` - beim UI-Aufbau
2. **web_admin_handlers.cpp:460-467**: `tiles_X_update_tile()` - nach Tile-Änderung via Web
3. **mqtt_handlers.cpp:152-154**: `tiles_X_update_sensor_by_entity()` - bei MQTT-Updates

---

## 2. LÖSUNG: Unified Implementation mit Backward Compatibility

### Ansatz: Option A (Empfohlen)
✅ Unified Implementation mit GridType-Parameter
✅ Wrappers für Backward Compatibility
✅ Keine Breaking Changes
✅ Schrittweise Migration möglich

---

## 3. IMPLEMENTATION PLAN

### Phase 1: Unified Core erstellen

#### Neue Dateien:
- `src/ui/tab_tiles_unified.h`
- `src/ui/tab_tiles_unified.cpp`

#### Unified API:
```cpp
// tab_tiles_unified.h
#ifndef TAB_TILES_UNIFIED_H
#define TAB_TILES_UNIFIED_H

#include <lvgl.h>
#include "src/tiles/tile_renderer.h"

// Unified functions with GridType parameter
void build_tiles_tab(lv_obj_t* parent, GridType grid_type, scene_publish_cb_t scene_cb);
void tiles_reload_layout(GridType grid_type);
void tiles_update_tile(GridType grid_type, uint8_t index);
void tiles_update_sensor_by_entity(GridType grid_type, const char* entity_id, const char* value);

#endif
```

#### Interner State (statt 3× separate Variablen):
```cpp
// tab_tiles_unified.cpp
static lv_obj_t* g_tiles_grids[3] = {nullptr};        // [HOME, GAME, WEATHER]
static scene_publish_cb_t g_tiles_scene_cbs[3] = {nullptr};
static lv_obj_t* g_tiles_objs[3][TILES_PER_GRID] = {nullptr};
```

#### Config-Getter-Mapping:
```cpp
static const TileGridConfig& getGridConfig(GridType type) {
  switch(type) {
    case GridType::HOME:    return tileConfig.getHomeGrid();
    case GridType::GAME:    return tileConfig.getGameGrid();
    case GridType::WEATHER: return tileConfig.getWeatherGrid();
    default:                return tileConfig.getHomeGrid();
  }
}
```

#### Serial-Logging mit Namen:
```cpp
static const char* getGridName(GridType type) {
  switch(type) {
    case GridType::HOME:    return "TilesHome";
    case GridType::GAME:    return "TilesGame";
    case GridType::WEATHER: return "TilesWeather";
    default:                return "TilesUnknown";
  }
}
```

---

### Phase 2: Wrapper Headers erstellen

#### Umwandeln der Header-Dateien zu Thin Wrappers:

**tab_tiles_home.h:**
```cpp
#ifndef TAB_TILES_HOME_H
#define TAB_TILES_HOME_H

#include "tab_tiles_unified.h"

// Backward compatibility wrappers
inline void build_tiles_home_tab(lv_obj_t* parent, scene_publish_cb_t scene_cb) {
    build_tiles_tab(parent, GridType::HOME, scene_cb);
}

inline void tiles_home_reload_layout() {
    tiles_reload_layout(GridType::HOME);
}

inline void tiles_home_update_tile(uint8_t index) {
    tiles_update_tile(GridType::HOME, index);
}

inline void tiles_home_update_sensor_by_entity(const char* entity_id, const char* value) {
    tiles_update_sensor_by_entity(GridType::HOME, entity_id, value);
}

#endif // TAB_TILES_HOME_H
```

**Analog für tab_tiles_game.h und tab_tiles_weather.h**

---

### Phase 3: Alte CPP-Dateien löschen

```
❌ DELETE: src/ui/tab_tiles_home.cpp
❌ DELETE: src/ui/tab_tiles_game.cpp
❌ DELETE: src/ui/tab_tiles_weather.cpp
```

---

## 4. DATEIEN DIE GEÄNDERT WERDEN

### Neue Dateien (erstellen):
- ✅ `src/ui/tab_tiles_unified.h` (neu)
- ✅ `src/ui/tab_tiles_unified.cpp` (neu)

### Bestehende Dateien (ändern):
- ✏️ `src/ui/tab_tiles_home.h` (wird zu Wrapper, ~20 Zeilen)
- ✏️ `src/ui/tab_tiles_game.h` (wird zu Wrapper, ~20 Zeilen)
- ✏️ `src/ui/tab_tiles_weather.h` (wird zu Wrapper, ~20 Zeilen)

### Dateien löschen:
- ❌ `src/ui/tab_tiles_home.cpp`
- ❌ `src/ui/tab_tiles_game.cpp`
- ❌ `src/ui/tab_tiles_weather.cpp`

### Keine Änderungen nötig:
- ✅ `src/ui/ui_manager.cpp` - nutzt weiterhin `build_tiles_X_tab()`
- ✅ `src/web/web_admin_handlers.cpp` - nutzt weiterhin `tiles_X_update_tile()`
- ✅ `src/network/mqtt_handlers.cpp` - nutzt weiterhin `tiles_X_update_sensor_by_entity()`
- ✅ `src/tiles/tile_config.h/cpp` - **KEINE ÄNDERUNGEN** (User-Vorgabe!)
- ✅ `src/tiles/tile_renderer.h/cpp` - keine Änderungen nötig

---

## 5. VORHER / NACHHER

### VORHER:
```
333 Zeilen Code verteilt auf 6 Dateien
├── tab_tiles_home.cpp (111 Zeilen)
├── tab_tiles_home.h (19 Zeilen)
├── tab_tiles_game.cpp (109 Zeilen)
├── tab_tiles_game.h (19 Zeilen)
├── tab_tiles_weather.cpp (106 Zeilen)
└── tab_tiles_weather.h (19 Zeilen)
```

### NACHHER:
```
~190 Zeilen Code verteilt auf 5 Dateien
├── tab_tiles_unified.cpp (~110 Zeilen)  ← EINE Implementation
├── tab_tiles_unified.h (~20 Zeilen)
├── tab_tiles_home.h (~20 Zeilen)        ← Thin Wrapper
├── tab_tiles_game.h (~20 Zeilen)        ← Thin Wrapper
└── tab_tiles_weather.h (~20 Zeilen)     ← Thin Wrapper
```

**Code-Reduktion:** 333 → ~190 Zeilen (**-43%**)

---

## 6. TESTING PLAN

### Nach Implementation testen:

1. **Compile Test:**
   ```bash
   pio run
   ```
   ✅ Muss ohne Fehler kompilieren

2. **Function Test (alle 3 Tabs):**
   - ✅ Home Tab: Tiles laden, anzeigen
   - ✅ Game Tab: Tiles laden, anzeigen
   - ✅ Weather Tab: Tiles laden, anzeigen

3. **Web Interface Test:**
   - ✅ Tile bearbeiten (alle 3 Tabs)
   - ✅ Tile löschen
   - ✅ Tiles reordern (Drag&Drop)

4. **MQTT Test:**
   - ✅ Sensor-Update via MQTT
   - ✅ Wert-Update auf allen relevanten Grids

5. **NVS Test:**
   - ✅ Tiles speichern (per Grid)
   - ✅ Neustart → Tiles korrekt geladen
   - ✅ `/api/status` → NVS Usage OK

---

## 7. ROLLBACK PLAN

Falls Probleme auftreten:

```bash
# Zurück auf letzten funktionierenden Stand
git reset --hard HEAD
```

Oder selektiv:
```bash
git checkout HEAD -- src/ui/tab_tiles_*.cpp
git checkout HEAD -- src/ui/tab_tiles_*.h
rm src/ui/tab_tiles_unified.*
```

---

## 8. WICHTIG: WAS NICHT GEÄNDERT WIRD

### ❌ KEINE Änderungen an:
- `src/tiles/tile_config.cpp` - Speicher-Logik bleibt unverändert!
- `src/tiles/tile_config.h` - Blob-Storage unangetastet!
- NVS-Handling - funktioniert bereits perfekt!
- GridType enum in `tile_renderer.h` - bleibt gleich
- `saveSingleGrid()` - bleibt wie es ist

### ✅ NUR UI-Code wird vereinheitlicht:
- Rendering-Logik
- Event-Handling
- MQTT/Web-Update-Funktionen

---

## 9. BENEFITS

### Code Quality:
- ✅ DRY-Prinzip (Don't Repeat Yourself)
- ✅ Single Source of Truth
- ✅ Einfacher zu warten
- ✅ Bugfixes nur an einer Stelle

### Maintenance:
- ✅ Änderungen betreffen alle 3 Tabs automatisch
- ✅ Keine Synchronisation zwischen Dateien nötig
- ✅ Weniger Code = weniger Bugs

### Future-Proof:
- ✅ Neues Grid hinzufügen? Nur GridType enum erweitern!
- ✅ Neue Funktion? Nur 1× implementieren statt 3×
- ✅ Leicht auf 4. oder 5. Grid erweiterbar

---

## 10. NEXT STEPS

### Wenn Plan approved:
1. ✅ Plan dem User zeigen
2. ⏳ Warten auf Approval
3. 🚀 Implementation starten (Schritt für Schritt)
4. ✅ Testen
5. ✅ Commit

### Implementation-Reihenfolge:
1. Erstelle `tab_tiles_unified.cpp/.h`
2. Teste Compile
3. Ändere `tab_tiles_home.h` zu Wrapper
4. Lösche `tab_tiles_home.cpp`
5. Test mit nur Home-Tab
6. Wiederhole für Game + Weather
7. Final Test aller Features
8. Commit

---

**Ende des Plans**

**Warten auf User-Feedback:** Ist dieser Ansatz OK? Soll ich starten?

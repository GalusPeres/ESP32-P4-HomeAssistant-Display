# Anleitung: Neuen Kachel-Typ hinzufügen

Diese Anleitung beschreibt **alle notwendigen Schritte** um einen neuen Tile-Typ zum Tab5 LVGL System hinzuzufügen.

## Übersicht

**Anzahl betroffener Dateien:** 8
**Geschätzte Zeit:** 30-60 Minuten
**Schwierigkeit:** Mittel

---

## ⚠️ Wichtige Konzepte

### Element-Pool Pattern
Um **keine Änderungen** an der `Tile`/`PackedTile` Struktur vornehmen zu müssen (vermeidet NVS-Migration):

- **Wiederverwendung** bestehender Felder für neue Zwecke
- **Semantik** ändert sich basierend auf `type` Feld
- **Beispiel:** Navigation nutzt `sensor_decimals` als `target_tab`

### Verfügbare Felder im Element-Pool

| Feld | Typ | Original-Verwendung | Wiederverwendbar für |
|------|-----|---------------------|---------------------|
| `sensor_decimals` | `uint8_t` | Nachkommastellen (0-9, 0xFF=keine) | Enum-Werte, Indizes (0-255) |
| `key_code` | `uint8_t` | USB HID Scancode | Alternative Enum-Werte |
| `key_modifier` | `uint8_t` | Modifier bits (Ctrl/Shift/Alt) | Bit-Flags (8 Flags möglich) |
| `sensor_entity` | `String` | HA Entity ID | Generischer String-Wert 1 |
| `sensor_unit` | `String` | Einheit | Generischer String-Wert 2 |
| `scene_alias` | `String` | HA Scene Alias | Generischer String-Wert 3 |
| `key_macro` | `String` | Makro-String | Generischer String-Wert 4 |

---

## 📋 Checkliste: Neuen Typ hinzufügen

### ✅ Schritt 1: Backend - Enum & Rendering

#### 1.1 `src/tiles/tile_config.h`
**Was:** Neuen Typ zur Enum hinzufügen

```cpp
enum TileType : uint8_t {
  TILE_EMPTY = 0,
  TILE_SENSOR = 1,
  TILE_SCENE = 2,
  TILE_KEY = 3,
  TILE_NAVIGATE = 4,
  TILE_MEIN_NEUER_TYP = 5  // ← Hier hinzufügen
};
```

**Zeile:** ~13
**Wichtig:** Fortlaufende Nummerierung beibehalten

---

#### 1.2 `src/tiles/tile_renderer.h`
**Was:** Render-Funktion deklarieren

```cpp
// Typ-spezifische Render-Funktionen
lv_obj_t* render_sensor_tile(...);
lv_obj_t* render_scene_tile(...);
lv_obj_t* render_key_tile(...);
lv_obj_t* render_navigate_tile(...);
lv_obj_t* render_mein_neuer_tile(...);  // ← Hier hinzufügen
```

**Zeile:** ~26
**Signatur:** Orientiere dich an ähnlichen Typen (Scene/Key für Buttons, Sensor für komplexere Layouts)

---

#### 1.3 `src/tiles/tile_renderer.cpp`

**1.3.1** Includes hinzufügen (falls nötig)
```cpp
#include "src/ui/ui_manager.h"  // Wenn du switchToTab() oder andere UI-Funktionen brauchst
```
**Zeile:** ~6

**1.3.2** Switch-Case erweitern
```cpp
lv_obj_t* render_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type, scene_publish_cb_t scene_cb) {
  switch (tile.type) {
    case TILE_SENSOR:
      return render_sensor_tile(parent, col, row, tile, index, grid_type);
    // ... andere Cases ...
    case TILE_MEIN_NEUER_TYP:
      return render_mein_neuer_tile(parent, col, row, tile, index);
    default:
      return render_empty_tile(parent, col, row);
  }
}
```
**Zeile:** ~217

**1.3.3** Event-Data Struct definieren (falls Events benötigt)
```cpp
struct MeinNeuerEventData {
  uint8_t parameter1;
  String parameter2;
};
```
**Zeile:** Vor der render-Funktion

**1.3.4** Render-Funktion implementieren
```cpp
lv_obj_t* render_mein_neuer_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  // Button/Container erstellen
  lv_obj_t* btn = lv_button_create(parent);

  // Styling (Radius, Border, Farbe)
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Grid-Position
  lv_obj_set_grid_cell(btn,
      LV_GRID_ALIGN_STRETCH, col, 1,
      LV_GRID_ALIGN_STRETCH, row, 1);

  // Icon (optional)
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    String iconChar = getMdiChar(tile.icon_name);
    if (iconChar.length() > 0) {
      lv_obj_t* icon_lbl = lv_label_create(btn);
      set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
      lv_label_set_text(icon_lbl, iconChar.c_str());
      lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);
    }
  }

  // Title (optional)
  if (tile.title.length() > 0) {
    lv_obj_t* title_lbl = lv_label_create(btn);
    set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
    lv_label_set_text(title_lbl, tile.title.c_str());
    lv_obj_align(title_lbl, LV_ALIGN_CENTER, 0, 35);
  }

  // Event-Handler (falls benötigt)
  if (/* Bedingung */) {
    MeinNeuerEventData* event_data = new MeinNeuerEventData{ /* ... */ };
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
      if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
      MeinNeuerEventData* data = static_cast<MeinNeuerEventData*>(lv_event_get_user_data(e));
      if (data) {
        // Aktion ausführen
      }
    }, LV_EVENT_CLICKED, event_data);
  }

  return btn;
}
```
**Zeile:** Am Ende der Datei, vor `render_empty_tile()`

---

#### 1.4 `src/ui/ui_manager.h` (falls benötigt)
**Was:** Funktionen public machen, die von tiles aufgerufen werden

```cpp
public:
  void meineFunktion();  // Von private nach public verschieben
```

**Wichtig:** Nur wenn dein Tile UI-Manager Funktionen aufruft!

---

### ✅ Schritt 2: Frontend - HTML Formular

#### 2.1 `src/web/web_admin_html.cpp`

**2.1.1** Dropdown-Option hinzufügen
```cpp
<option value="0">Leer</option>
<option value="1">Sensor</option>
<option value="2">Szene</option>
<option value="3">Key</option>
<option value="4">Navigation</option>
<option value="5">Mein Neuer Typ</option>  // ← Hier
```
**Zeile:** ~193 (Typ-Dropdown)

**2.1.2** Formular-Felder hinzufügen
```cpp
<!-- Mein Neuer Typ Fields -->
<div id=")html";
  html += tab_id;
  html += R"html(_mein_neuer_fields" class="type-fields">
  <label>Parameter 1</label>
  <input type="text" id=")html";
  html += tab_id;
  html += R"html(_mein_neuer_param1" placeholder="Wert eingeben">

  <label>Parameter 2</label>
  <select id=")html";
  html += tab_id;
  html += R"html(_mein_neuer_param2">
    <option value="0">Option 1</option>
    <option value="1">Option 2</option>
  </select>
</div>
```
**Zeile:** ~310 (nach Navigate-Fields)

**2.1.3** Grid-Rendering Case hinzufügen
```cpp
} else if (tile.type == TILE_MEIN_NEUER_TYP) {
  cssClass += " mein-neuer-typ";
  if (tile.bg_color != 0) {
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)tile.bg_color);
    tileStyle = "background:";
    tileStyle += colorHex;
  } else {
    tileStyle = "background:#353535";
  }
}
```
**Zeile:** ~110 (in Grid-Rendering Schleife)

---

### ✅ Schritt 3: Frontend - JavaScript

#### 3.1 `src/web/web_admin_scripts.cpp`

**3.1.1** `updateTileType()` - Felder anzeigen
```javascript
function updateTileType(tab) {
  const prefix = tab;
  const typeValue = document.getElementById(prefix + '_tile_type').value;
  document.querySelectorAll('#' + prefix + 'Settings .type-fields').forEach(f => f.classList.remove('show'));

  if (typeValue === '1') document.getElementById(prefix + '_sensor_fields').classList.add('show');
  // ... andere Cases ...
  else if (typeValue === '5') document.getElementById(prefix + '_mein_neuer_fields').classList.add('show');
}
```
**Zeile:** ~469

**3.1.2** `loadTileData()` - Daten laden
```javascript
} else if (data.type === 5) {
  document.getElementById(prefix + '_mein_neuer_param1').value = data.sensor_entity || '';
  document.getElementById(prefix + '_mein_neuer_param2').value = (data.sensor_decimals !== undefined) ? data.sensor_decimals : '0';
}
```
**Zeile:** ~447

**3.1.3** `saveTile()` - Daten speichern
```javascript
} else if (typeValue === '5') {
  formData.append('mein_neuer_param1', document.getElementById(prefix + '_mein_neuer_param1').value);
  formData.append('mein_neuer_param2', document.getElementById(prefix + '_mein_neuer_param2').value);
}
```
**Zeile:** ~527

**3.1.4** `renderTileFromData()` - CSS-Klasse setzen
```javascript
let cls = ['tile'];
if (tile.type === 1) cls.push('sensor');
else if (tile.type === 2) cls.push('scene');
else if (tile.type === 3) cls.push('key');
else if (tile.type === 4) cls.push('navigate');
else if (tile.type === 5) cls.push('mein-neuer-typ');  // ← Hier
else cls.push('empty');
```
**Zeile:** ~550

**3.1.5** `setupLivePreview()` - Auto-Save aktivieren

**a)** Fields-Array erweitern:
```javascript
const fields = [
  '_tile_title','_tile_color','_tile_type','_sensor_entity','_sensor_unit',
  '_sensor_decimals','_scene_alias','_key_macro','_navigate_target',
  '_mein_neuer_param1', '_mein_neuer_param2'  // ← Hier
];
```
**Zeile:** ~276

**b)** Element-Referenzen hinzufügen:
```javascript
const meinNeuerParam1 = document.getElementById(prefix + '_mein_neuer_param1');
const meinNeuerParam2 = document.getElementById(prefix + '_mein_neuer_param2');
```
**Zeile:** ~292

**c)** Event-Listener hinzufügen:
```javascript
if (meinNeuerParam1) meinNeuerParam1.addEventListener('input', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
if (meinNeuerParam2) meinNeuerParam2.addEventListener('change', () => { updateTilePreview(tab); updateDraft(tab); scheduleAutoSave(tab); });
```
**Zeile:** ~303

**3.1.6** `updateTilePreview()` - Live-Vorschau

**a)** Type-Detection erweitern:
```javascript
const typeWas = tileElem.classList.contains('sensor')   ? '1' :
                tileElem.classList.contains('scene')    ? '2' :
                tileElem.classList.contains('key')      ? '3' :
                tileElem.classList.contains('navigate') ? '4' :
                tileElem.classList.contains('mein-neuer-typ') ? '5' : '0';
```
**Zeile:** ~360

**b)** CSS-Klasse setzen:
```javascript
} else if (type === '5') {
  tileElem.classList.add('mein-neuer-typ');
  tileElem.style.background = color || '#353535';
}
```
**Zeile:** ~390

---

### ✅ Schritt 4: Backend - API Handler

#### 4.1 `src/web/web_admin_handlers.cpp`
**Was:** Parameter parsen und in Element-Pool speichern

```cpp
} else if (type == TILE_MEIN_NEUER_TYP) {
  // Element-Pool: sensor_entity = param1, sensor_decimals = param2
  tile.sensor_entity = server.hasArg("mein_neuer_param1") ? server.arg("mein_neuer_param1") : "";
  tile.sensor_decimals = server.hasArg("mein_neuer_param2") ? server.arg("mein_neuer_param2").toInt() : 0;
} else {
  tile.sensor_decimals = 0xFF;
}
```
**Zeile:** ~459

**Wichtig:** Kommentar schreiben welches Feld wofür verwendet wird!

---

### ✅ Schritt 5: CSS Styling

#### 5.1 `src/web/web_admin_styles.cpp`

**5.1.1** Flexbox Centering (für Button-Tiles)
```css
.tile.scene,
.tile.key,
.tile.navigate,
.tile.mein-neuer-typ { display:flex; flex-direction:column; align-items:center; justify-content:center; }
```
**Zeile:** ~113

**5.1.2** Title Styling
```css
.tile.scene .tile-title,
.tile.key .tile-title,
.tile.navigate .tile-title,
.tile.mein-neuer-typ .tile-title { text-align:center; align-self:auto; width:100%; }
```
**Zeile:** ~161

**5.1.3** Icon/Title Margins
```css
/* Scene/Key/Navigate: Icon oben-mittig (flexbox zentriert automatisch) */
.tile.scene .tile-icon,
.tile.key .tile-icon,
.tile.navigate .tile-icon,
.tile.mein-neuer-typ .tile-icon {
  margin-bottom:4px;
}
.tile.scene .tile-title,
.tile.key .tile-title,
.tile.navigate .tile-title,
.tile.mein-neuer-typ .tile-title {
  margin-top:4px;
}
```
**Zeile:** ~197

**Alternative:** Eigene CSS-Klassen für komplett abweichendes Layout (wie Sensor-Tiles)

---

## 🧪 Testing-Checkliste

Nach der Implementation:

- [ ] Kompilierung erfolgreich
- [ ] Typ erscheint im Dropdown
- [ ] Formular-Felder werden angezeigt beim Typ-Wechsel
- [ ] Live-Preview zeigt korrekt zentriertes Layout
- [ ] Auto-Save funktioniert (Werte bleiben nach Reload erhalten)
- [ ] Backend speichert Werte korrekt (Serial Monitor checken)
- [ ] Grid zeigt Tile mit korrektem Styling
- [ ] Tile wird auf Display korrekt gerendert
- [ ] Event-Handler funktioniert (falls vorhanden)
- [ ] Icon und Titel werden zentriert angezeigt

---

## 🐛 Häufige Fehler

### ❌ CSS vergessen
**Symptom:** Tile ist linksbündig statt zentriert
**Lösung:** Alle 3 CSS-Regeln hinzufügen (Centering, Title, Icon/Margins)

### ❌ Auto-Save vergessen
**Symptom:** Dropdown/Input-Werte werden nicht gespeichert
**Lösung:** Fields-Array, Element-Referenz UND Event-Listener hinzufügen

### ❌ Live-Preview vergessen
**Symptom:** Layout springt von links nach zentriert beim Tippen
**Lösung:** Type-Detection UND CSS-Klassen-Zuweisung in updateTilePreview()

### ❌ Element-Pool falsch dokumentiert
**Symptom:** Verwirrung welches Feld wofür genutzt wird
**Lösung:** **IMMER** Kommentar schreiben: `// Element-Pool: sensor_decimals = target_tab`

### ❌ renderTileFromData() vergessen
**Symptom:** Grid zeigt tile als "empty" nach Speichern
**Lösung:** CSS-Klasse in renderTileFromData() hinzufügen

---

## 📊 Zusammenfassung

**8 Dateien ändern:**
1. ✅ tile_config.h (Enum)
2. ✅ tile_renderer.h (Deklaration)
3. ✅ tile_renderer.cpp (Rendering)
4. ✅ ui_manager.h (optional - API)
5. ✅ web_admin_html.cpp (HTML)
6. ✅ web_admin_scripts.cpp (JavaScript - 6 Funktionen!)
7. ✅ web_admin_handlers.cpp (Backend)
8. ✅ web_admin_styles.cpp (CSS - 3 Regeln)

**Geschätzte Zeilen Code:** 150-250 (je nach Komplexität)

---

## 💡 Tipps

- **Kopiere bestehenden Code** von ähnlichem Typ (Scene für Buttons, Sensor für komplexe Layouts)
- **Search & Replace** mit einheitlicher Namenskonvention (`mein_neuer` überall gleich!)
- **Teste nach jedem Schritt** (Backend → HTML → JavaScript → CSS)
- **Nutze Element-Pool** statt neue Felder hinzuzufügen
- **Dokumentiere Element-Pool Nutzung** mit Kommentaren

---

## 📝 Beispiel: Navigation-Tile (TILE_NAVIGATE)

### Element-Pool Nutzung
- **titel** → `title` (Standard-Feld)
- **icon** → `icon_name` (Standard-Feld)
- **farbe** → `bg_color` (Standard-Feld)
- **target_tab** → `sensor_decimals` (0=Tab0, 1=Tab1, 2=Tab2) ← **Wiederverwendet**

### Geänderte Dateien
1. tile_config.h - `TILE_NAVIGATE = 4`
2. tile_renderer.h - `render_navigate_tile()` Deklaration
3. tile_renderer.cpp - 90 Zeilen Rendering-Code
4. ui_manager.h - `switchToTab()` public gemacht
5. web_admin_html.cpp - Dropdown + Formular + Grid-Rendering
6. web_admin_scripts.cpp - 6 Funktionen erweitert
7. web_admin_handlers.cpp - Backend-Handler
8. web_admin_styles.cpp - 3 CSS-Regeln

**Resultat:** Funktionierender Navigation-Button der zwischen Tabs wechselt, ohne Änderung der Tile-Struktur!


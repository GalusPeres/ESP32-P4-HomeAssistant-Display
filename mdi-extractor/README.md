# MDI Icon Extractor

Extrahiert Material Design Icons Codepoints für ESP32/Arduino Projekte.

## Installation

```bash
npm install
```

## Verwendung

```bash
npm run extract
```

Das Programm:
1. Liest `@mdi/font` SCSS Variables
2. Extrahiert alle Icon-Namen und Codepoints
3. Filtert Home Assistant relevante Icons
4. Gibt C++ Map Einträge aus
5. Speichert Ergebnis in `icons.txt`

## Output

```cpp
{"home", 0xF02DC},
{"lightbulb", 0xF0335},
...
```

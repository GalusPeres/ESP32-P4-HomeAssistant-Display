# Tab5 Game Controls - Electron App

Stream Deck-style keyboard controller for M5Stack Tab5.

## Installation

1. **Node.js installieren** (falls nicht vorhanden):
   - Download: https://nodejs.org/
   - Version 18 oder höher

2. **Dependencies installieren:**
   ```bash
   cd electron-app
   npm install
   ```

3. **App starten:**
   ```bash
   npm start
   ```

## Konfiguration

1. **Tab5 IP-Adresse einstellen:**
   - In der App unter "Tab5 IP" eingeben
   - Standard: `192.168.1.50`
   - Tab5 IP findest du im Tab5 Web-Interface

2. **Connect klicken**
   - App verbindet sich zum Tab5 WebSocket Server (Port 8081)
   - Status zeigt "Connected" wenn verbunden

## Verwendung

1. **Tab5 Setup:**
   - Flash den Tab5 Code mit WebSocket Server
   - Konfiguriere Buttons im Web-Interface: `http://<tab5-ip>/admin`

2. **Electron App:**
   - Starte die App
   - Verbinde zu Tab5
   - App läuft im System Tray weiter

3. **Button drücken:**
   - Drücke Button auf Tab5
   - App empfängt Event und simuliert Tastendruck
   - Funktioniert in jedem Programm (z.B. Star Citizen)

## Features

- ✅ WebSocket Client zu Tab5
- ✅ Tastensimulation (Ctrl, Shift, Alt + Taste)
- ✅ System Tray Integration
- ✅ Auto-Reconnect bei Verbindungsabbruch
- ✅ Activity Log
- ✅ Live Status Anzeige

## Tastenkürzel Mapping

Die App verwendet die Scan Codes vom Tab5 und mapped sie zu echten Tasten:
- `0x04-0x1D` → A-Z
- `0x1E-0x27` → 1-0
- `0x2C` → Space
- `0x28` → Enter

Modifier:
- `0x01` → Ctrl
- `0x02` → Shift
- `0x04` → Alt

## Troubleshooting

**"Cannot connect to Tab5":**
- Prüfe Tab5 IP-Adresse
- Prüfe ob Tab5 im gleichen Netzwerk
- Prüfe ob WebSocket Server auf Tab5 läuft (Port 8081)

**"robotjs not found":**
- `npm install robotjs` manuell ausführen
- Bei Windows: Visual Studio Build Tools erforderlich

**Tastendruck funktioniert nicht:**
- App muss als Administrator laufen (Windows)
- Prüfe Log für Fehlermeldungen

## Build für Verteilung

```bash
npm run build
```

Erstellt ausführbare `.exe` Datei in `dist/` Ordner.

## Lizenz

MIT

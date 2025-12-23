const { app, BrowserWindow, ipcMain, Tray, Menu } = require('electron');
const path = require('path');
const WebSocket = require('ws');
const Store = require('electron-store');

// keysender für schnelle Tastensimulation
const { Hardware } = require('keysender');
const kb = new Hardware(null); // null = kein spezifisches Fenster, globale Eingabe

// Settings Store
const store = new Store({
  defaults: {
    autostart: true,  // Standard: Autostart aktiviert
    tab5_ip: '192.168.2.235'
  }
});

let mainWindow = null;
let tray = null;
let ws = null;
let reconnectTimer = null;
let pingInterval = null;
let connectionAttempts = 0;

// Tab5 WebSocket Verbindung
let TAB5_IP = store.get('tab5_ip'); // Aus Settings laden
const TAB5_PORT = 8081;

// Scan Code zu Robot Key Mapping
// Based on USB HID Usage Tables
const SCANCODE_MAP = {
  // Letters
  0x04: 'a', 0x05: 'b', 0x06: 'c', 0x07: 'd', 0x08: 'e', 0x09: 'f',
  0x0A: 'g', 0x0B: 'h', 0x0C: 'i', 0x0D: 'j', 0x0E: 'k', 0x0F: 'l',
  0x10: 'm', 0x11: 'n', 0x12: 'o', 0x13: 'p', 0x14: 'q', 0x15: 'r',
  0x16: 's', 0x17: 't', 0x18: 'u', 0x19: 'v', 0x1A: 'w', 0x1B: 'x',
  0x1C: 'y', 0x1D: 'z',

  // Numbers (Main Keyboard)
  0x1E: '1', 0x1F: '2', 0x20: '3', 0x21: '4', 0x22: '5',
  0x23: '6', 0x24: '7', 0x25: '8', 0x26: '9', 0x27: '0',

  // Special Keys
  0x28: 'enter', 0x29: 'escape', 0x2A: 'backspace', 0x2B: 'tab', 0x2C: 'space',
  0x2D: '-', 0x2E: '=', 0x2F: '[', 0x30: ']',
  0x31: '\\', 0x33: ';', 0x34: '\'', 0x35: '`',
  0x36: ',', 0x37: '.', 0x38: '/', 0x39: 'capsLock',

  // Function Keys
  0x3A: 'f1', 0x3B: 'f2', 0x3C: 'f3', 0x3D: 'f4', 0x3E: 'f5', 0x3F: 'f6',
  0x40: 'f7', 0x41: 'f8', 0x42: 'f9', 0x43: 'f10', 0x44: 'f11', 0x45: 'f12',

  // System Keys
  0x46: 'printScreen', 0x47: 'scrollLock', 0x48: 'pause', 0x49: 'insert',
  0x4A: 'home', 0x4B: 'pageUp', 0x4C: 'delete', 0x4D: 'end', 0x4E: 'pageDown',
  0x4F: 'right', 0x50: 'left', 0x51: 'down', 0x52: 'up',

  // Numpad
  0x53: 'numLock', 0x54: 'divide', 0x55: 'multiply', 0x56: 'subtract', 0x57: 'add',
  0x58: 'enter', 0x59: 'num1', 0x5A: 'num2', 0x5B: 'num3', 0x5C: 'num4', 0x5D: 'num5',
  0x5E: 'num6', 0x5F: 'num7', 0x60: 'num8', 0x61: 'num9', 0x62: 'num0', 0x63: 'decimal',

  // Media & Extra
  0x7F: 'audioMute', 0x80: 'audioVolUp', 0x81: 'audioVolDown',
  0xE0: 'controlLeft', 0xE1: 'shiftLeft', 0xE2: 'altLeft', 0xE3: 'metaLeft',
  0xE4: 'controlRight', 0xE5: 'shiftRight', 0xE6: 'altRight', 0xE7: 'metaRight'
};

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 500,
    height: 750,  // Etwas höher für Autostart-Checkbox
    show: false, // Nicht sofort anzeigen
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    }
  });

  // Menüleiste entfernen
  Menu.setApplicationMenu(null);

  mainWindow.loadFile('index.html');

  // Fenster anzeigen wenn bereit (oder im Tray lassen)
  mainWindow.once('ready-to-show', () => {
    // Nur anzeigen wenn kein Tray Icon existiert
    if (!tray) {
      mainWindow.show();
    }
  });

  mainWindow.on('close', (event) => {
    if (!app.isQuitting) {
      event.preventDefault();
      mainWindow.hide();
    }
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });
}

function createTray() {
  const fs = require('fs');
  const iconPath = path.join(__dirname, 'icon.png');

  if (!fs.existsSync(iconPath)) {
    console.log('⚠️ icon.png not found - skipping tray icon');
    return;
  }

  tray = new Tray(iconPath);

  const contextMenu = Menu.buildFromTemplate([
    {
      label: 'Show Window',
      click: () => {
        mainWindow.show();
        mainWindow.focus();
      }
    },
    { type: 'separator' },
    {
      label: 'Quit',
      click: () => {
        app.isQuitting = true;
        app.quit();
      }
    }
  ]);

  tray.setContextMenu(contextMenu);
  tray.setToolTip('Tab5 Game Controls - Click to open');

  // Doppelklick auf Tray Icon öffnet Fenster
  tray.on('double-click', () => {
    mainWindow.show();
    mainWindow.focus();
  });
}

function connectToTab5() {
  const url = `ws://${TAB5_IP}:${TAB5_PORT}`;

  connectionAttempts++;
  log(`Connecting to Tab5: ${url} (attempt ${connectionAttempts})`);

  ws = new WebSocket(url, {
    perMessageDeflate: false,
    handshakeTimeout: 5000
  });

  ws.on('open', () => {
    log('✅ Connected to Tab5!');
    updateStatus('connected');
    connectionAttempts = 0;

    // Reconnect Timer clearen
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }

    // Ping alle 15 Sekunden senden
    pingInterval = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.ping();
      }
    }, 15000);
  });

  ws.on('message', (data) => {
    try {
      const msg = JSON.parse(data.toString());
      handleTab5Message(msg);
    } catch (err) {
      log(`Error parsing message: ${err.message}`);
    }
  });

  ws.on('pong', () => {
    // Server antwortet auf Ping - Verbindung ist aktiv
  });

  ws.on('close', (code, reason) => {
    log(`❌ Disconnected from Tab5 (code: ${code})`);
    updateStatus('disconnected');

    // Cleanup
    if (pingInterval) {
      clearInterval(pingInterval);
      pingInterval = null;
    }
    ws = null;

    // Auto-Reconnect mit exponential backoff (max 30 Sekunden)
    const delay = Math.min(5000 * Math.pow(1.5, Math.min(connectionAttempts, 5)), 30000);
    log(`Reconnecting in ${Math.round(delay / 1000)} seconds...`);

    reconnectTimer = setTimeout(() => {
      connectToTab5();
    }, delay);
  });

  ws.on('error', (err) => {
    log(`WebSocket error: ${err.message}`);
  });
}

function handleTab5Message(msg) {
  log(`📩 Received: ${JSON.stringify(msg)}`);

  if (msg.type === 'button_press') {
    simulateKeyPress(msg.key, msg.modifier);

    // An Renderer senden für UI Update
    if (mainWindow) {
      mainWindow.webContents.send('button-pressed', msg);
    }
  }
}

async function simulateKeyPress(scancode, modifier) {
  const key = SCANCODE_MAP[scancode];

  if (!key) {
    log(`⚠️ Unknown scan code: 0x${scancode.toString(16)}`);
    return;
  }

  const modifiers = [];
  if (modifier & 0x01) modifiers.push('control');
  if (modifier & 0x02) modifiers.push('shift');
  if (modifier & 0x04) modifiers.push('alt');
  if (modifier & 0x08) modifiers.push('command'); // Win/Meta

  log(`⌨️ Processing: ${modifiers.join('+')} ${key} (Code: 0x${scancode.toString(16)})`);

  try {
    // keysender Hardware API
    if (modifiers.length > 0) {
      // Mit Modifiern
      const keyCombo = [...modifiers, key];
      log(`   -> Sending combo: [${keyCombo.join(', ')}]`);
      await kb.keyboard.sendKey(keyCombo);
    } else {
      // Ohne Modifier
      log(`   -> Sending single: ${key}`);
      await kb.keyboard.sendKey(key);
    }
    log(`✅ Success`);
  } catch (err) {
    log(`❌ ERROR Sending Key: ${err.message}`);
    // Prevent App Crash by catching all errors here
  }
}

function log(message) {
  const timestamp = new Date().toLocaleTimeString();
  console.log(`[${timestamp}] ${message}`);

  if (mainWindow) {
    mainWindow.webContents.send('log', { timestamp, message });
  }
}

function updateStatus(status) {
  if (mainWindow) {
    mainWindow.webContents.send('status', status);
  }
}

// IPC Handlers
ipcMain.on('connect', () => {
  if (!ws || ws.readyState !== WebSocket.OPEN) {
    connectToTab5();
  }
});

ipcMain.on('disconnect', () => {
  if (ws) {
    // Cleanup Timers
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    if (pingInterval) {
      clearInterval(pingInterval);
      pingInterval = null;
    }

    // Verbindung schließen
    ws.close();
    ws = null;
    updateStatus('disconnected');
    log('Manually disconnected');
  }
});

ipcMain.on('get-tab5-ip', (event) => {
  event.returnValue = store.get('tab5_ip');
});

ipcMain.on('set-tab5-ip', (event, ip) => {
  TAB5_IP = ip;
  store.set('tab5_ip', ip);  // In Config speichern
  log(`Tab5 IP changed to: ${ip}`);
});

// Autostart Funktionen
function setAutostart(enabled) {
  app.setLoginItemSettings({
    openAtLogin: enabled,
    openAsHidden: true,  // Im Tray starten
    args: ['--hidden']
  });
  store.set('autostart', enabled);
  log(`Autostart ${enabled ? 'enabled' : 'disabled'}`);
}

function getAutostart() {
  return store.get('autostart', true);
}

// IPC Handler für Autostart
ipcMain.on('get-autostart', (event) => {
  event.returnValue = getAutostart();
});

ipcMain.on('set-autostart', (event, enabled) => {
  setAutostart(enabled);
});

// App Lifecycle
app.whenReady().then(() => {
  // Autostart beim ersten Start aktivieren
  const autostartEnabled = getAutostart();
  setAutostart(autostartEnabled);

  createTray();    // Tray zuerst erstellen
  createWindow();  // Dann Window (startet minimiert wenn Tray existiert)

  // Auto-Connect nach 2 Sekunden
  setTimeout(() => {
    connectToTab5();
  }, 2000);
});

app.on('window-all-closed', () => {
  // Nichts tun - App läuft im Tray weiter
});

app.on('activate', () => {
  if (mainWindow === null) {
    createWindow();
  } else {
    mainWindow.show();
  }
});

app.on('before-quit', () => {
  if (ws) {
    ws.close();
  }
});

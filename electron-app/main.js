const { app, BrowserWindow, ipcMain, Tray, Menu } = require('electron');
const path = require('path');
const WebSocket = require('ws');

// keysender für schnelle Tastensimulation
const { Hardware } = require('keysender');
const kb = new Hardware(null); // null = kein spezifisches Fenster, globale Eingabe

let mainWindow = null;
let tray = null;
let ws = null;
let reconnectTimer = null;
let pingInterval = null;
let connectionAttempts = 0;

// Tab5 WebSocket Verbindung
let TAB5_IP = '192.168.2.235'; // Änderbar über UI
const TAB5_PORT = 8081;

// Scan Code zu Robot Key Mapping
const SCANCODE_MAP = {
  0x04: 'a', 0x05: 'b', 0x06: 'c', 0x07: 'd', 0x08: 'e', 0x09: 'f',
  0x0A: 'g', 0x0B: 'h', 0x0C: 'i', 0x0D: 'j', 0x0E: 'k', 0x0F: 'l',
  0x10: 'm', 0x11: 'n', 0x12: 'o', 0x13: 'p', 0x14: 'q', 0x15: 'r',
  0x16: 's', 0x17: 't', 0x18: 'u', 0x19: 'v', 0x1A: 'w', 0x1B: 'x',
  0x1C: 'y', 0x1D: 'z',
  0x1E: '1', 0x1F: '2', 0x20: '3', 0x21: '4', 0x22: '5',
  0x23: '6', 0x24: '7', 0x25: '8', 0x26: '9', 0x27: '0',
  0x28: 'enter', 0x29: 'escape', 0x2A: 'backspace', 0x2B: 'tab', 0x2C: 'space'
};

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 500,
    height: 700,
    show: false, // Nicht sofort anzeigen
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false
    },
    icon: path.join(__dirname, 'icon.png')
  });

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
  // Tray Icon optional - nur wenn icon.png existiert
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

  log(`⌨️ Simulating: ${modifiers.join('+')} ${key}`);

  try {
    // keysender Hardware API - braucht ein Array mit allen Tasten
    if (modifiers.length > 0) {
      // Mit Modifiern: ['alt', 'n'] oder ['ctrl', 'shift', 'a']
      const keyCombo = [...modifiers, key];
      await kb.keyboard.sendKey(keyCombo);
      log(`✅ Key pressed: ${keyCombo.join('+')}`);
    } else {
      // Ohne Modifier
      await kb.keyboard.sendKey(key);
      log(`✅ Key pressed: ${key}`);
    }
  } catch (err) {
    log(`❌ Error: ${err.message}`);
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

ipcMain.on('set-tab5-ip', (event, ip) => {
  TAB5_IP = ip;
  // TODO: In Config speichern
  log(`Tab5 IP changed to: ${ip}`);
});

// App Lifecycle
app.whenReady().then(() => {
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

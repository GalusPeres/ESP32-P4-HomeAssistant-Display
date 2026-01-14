const { app, BrowserWindow, ipcMain, Tray, Menu } = require('electron');
const path = require('path');
const os = require('os');
const si = require('systeminformation');
const WebSocket = require('ws');
const Store = require('electron-store');

// keysender für schnelle Tastensimulation
const { Hardware } = require('keysender');
const kb = new Hardware(null); // null = kein spezifisches Fenster, globale Eingabe

// Settings Store
const store = new Store({
  defaults: {
    autostart: true,  // Standard: Autostart aktiviert
    tab5_ip: '192.168.2.235',
    metrics: {
      cpu_load: true,
      cpu_temp: false,
      gpu_temp: false,
      gpu_load: false,
      mem_used: true,
      mem_pct: true,
      uptime: true
    },
    simconnect: {
      enabled: false,
      host: '127.0.0.1',
      port: 13375
    }
  }
});

let mainWindow = null;
let tray = null;
let ws = null;
let reconnectTimer = null;
let pingInterval = null;
let connectionAttempts = 0;
let metricsInterval = null;
let lastCpuSample = null;
let metricsInFlight = false;
let simWs = null;
let simReconnectTimer = null;
let simConnected = false;
let lastSimSensors = [];

const startHidden = process.argv.includes('--hidden');

// Tab5 WebSocket Verbindung
let TAB5_IP = store.get('tab5_ip'); // Aus Settings laden
const TAB5_PORT = 8081;
const METRICS_INTERVAL_MS = 5000;
const DEFAULT_METRICS = {
  cpu_load: true,
  cpu_temp: false,
  gpu_temp: false,
  gpu_load: false,
  mem_used: true,
  mem_pct: true,
  uptime: true
};
const DEFAULT_SIMCONNECT = {
  enabled: false,
  host: '127.0.0.1',
  port: 13375
};
const SIMCONNECT_RECONNECT_MS = 2000;
const lastMetricValues = {};

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

function normalizeMetricsConfig(raw) {
  const cfg = { ...DEFAULT_METRICS };
  if (!raw || typeof raw !== 'object') return cfg;
  for (const key of Object.keys(cfg)) {
    if (raw[key] !== undefined) {
      cfg[key] = !!raw[key];
    }
  }
  return cfg;
}

function getMetricsConfig() {
  return normalizeMetricsConfig(store.get('metrics', {}));
}

function setMetricsConfig(raw) {
  const cfg = normalizeMetricsConfig(raw);
  store.set('metrics', cfg);
  return cfg;
}

function normalizeSimConfig(raw) {
  const cfg = { ...DEFAULT_SIMCONNECT };
  if (!raw || typeof raw !== 'object') return cfg;
  if (raw.enabled !== undefined) cfg.enabled = !!raw.enabled;
  if (raw.host) cfg.host = String(raw.host);
  if (raw.port !== undefined) {
    const parsed = Number(raw.port);
    if (Number.isFinite(parsed) && parsed > 0) {
      cfg.port = Math.floor(parsed);
    }
  }
  return cfg;
}

function getSimConfig() {
  return normalizeSimConfig(store.get('simconnect', {}));
}

function setSimConfig(raw) {
  const cfg = normalizeSimConfig(raw);
  store.set('simconnect', cfg);
  return cfg;
}

function getMetricValue(entityId, value) {
  if (value === null || value === undefined) {
    if (Object.prototype.hasOwnProperty.call(lastMetricValues, entityId)) {
      return lastMetricValues[entityId];
    }
    return '--';
  }
  lastMetricValues[entityId] = value;
  return value;
}

function updateMetricsPreview(sensors) {
  if (!mainWindow || !mainWindow.webContents) return;
  const payload = Array.isArray(sensors) ? sensors : [];
  mainWindow.webContents.send('metrics-update', payload);
}

function updateSimPreview(sensors) {
  if (!mainWindow || !mainWindow.webContents) return;
  const payload = Array.isArray(sensors) ? sensors : [];
  mainWindow.webContents.send('sim-update', payload);
}

function normalizeIncomingSensors(rawSensors) {
  if (!Array.isArray(rawSensors)) return [];
  return rawSensors
    .map((sensor) => {
      if (!sensor || typeof sensor !== 'object') return null;
      const entityId = sensor.entity_id || sensor.entityId;
      if (!entityId) return null;
      const name = sensor.name || entityId;
      const unit = sensor.unit || '';
      const value = getMetricValue(entityId, sensor.value);
      return { entity_id: entityId, name, unit, value };
    })
    .filter(Boolean);
}

function normalizeTemperature(value) {
  if (!Number.isFinite(value)) return null;
  if (value <= 0) return null;
  return value;
}

function normalizePercent(value) {
  if (!Number.isFinite(value)) return null;
  return Math.max(0, Math.min(100, value));
}

function getCpuTempValue(cpuTemp) {
  if (!cpuTemp) return null;
  const main = normalizeTemperature(cpuTemp.main);
  if (main !== null) return main;
  if (Array.isArray(cpuTemp.cores) && cpuTemp.cores.length) {
    const values = cpuTemp.cores
      .map((v) => normalizeTemperature(v))
      .filter((v) => v !== null);
    if (values.length) {
      const sum = values.reduce((acc, v) => acc + v, 0);
      return sum / values.length;
    }
  }
  return null;
}

function pickGpuController(graphics) {
  if (!graphics || !Array.isArray(graphics.controllers)) return null;
  return graphics.controllers.find((c) => {
    const temp = normalizeTemperature(c.temperatureGpu);
    const load = normalizePercent(Number(c.utilizationGpu));
    return temp !== null || load !== null;
  }) || null;
}

function getCpuUsagePercent() {
  const cpus = os.cpus();
  let idle = 0;
  let total = 0;

  for (const cpu of cpus) {
    idle += cpu.times.idle;
    total += cpu.times.user + cpu.times.nice + cpu.times.sys + cpu.times.idle + cpu.times.irq;
  }

  if (!lastCpuSample) {
    lastCpuSample = { idle, total };
    return null;
  }

  const idleDiff = idle - lastCpuSample.idle;
  const totalDiff = total - lastCpuSample.total;
  lastCpuSample = { idle, total };

  if (totalDiff <= 0) return null;

  let usage = (1 - idleDiff / totalDiff) * 100;
  if (!Number.isFinite(usage)) return null;
  usage = Math.max(0, Math.min(100, usage));
  return usage;
}

async function buildPcMetricsPayload() {
  const config = getMetricsConfig();
  const sensors = [];
  const tasks = [];
  let cpuTemp = null;
  let graphics = null;

  if (config.cpu_temp) {
    tasks.push(
      si.cpuTemperature()
        .then((data) => { cpuTemp = data; })
        .catch(() => {})
    );
  }

  if (config.gpu_temp || config.gpu_load) {
    tasks.push(
      si.graphics()
        .then((data) => { graphics = data; })
        .catch(() => {})
    );
  }

  if (config.cpu_load) {
    const cpuLoad = getCpuUsagePercent();
    const value = cpuLoad !== null ? Number(cpuLoad.toFixed(1)) : null;
    sensors.push({
      entity_id: 'pc.cpu_load',
      name: 'CPU Load',
      unit: '%',
      value: getMetricValue('pc.cpu_load', value)
    });
  }

  if (config.mem_used || config.mem_pct) {
    const totalMem = os.totalmem();
    const freeMem = os.freemem();
    const usedMem = totalMem - freeMem;
    const usedGb = usedMem / (1024 ** 3);
    const usedPct = totalMem > 0 ? (usedMem / totalMem) * 100 : 0;

    if (config.mem_used) {
      sensors.push({
        entity_id: 'pc.mem_used',
        name: 'RAM Used',
        unit: 'GB',
        value: getMetricValue('pc.mem_used', Number(usedGb.toFixed(2)))
      });
    }

    if (config.mem_pct) {
      sensors.push({
        entity_id: 'pc.mem_pct',
        name: 'RAM Usage',
        unit: '%',
        value: getMetricValue('pc.mem_pct', Number(usedPct.toFixed(1)))
      });
    }
  }

  if (config.uptime) {
    sensors.push({
      entity_id: 'pc.uptime',
      name: 'PC Uptime',
      unit: 's',
      value: getMetricValue('pc.uptime', Math.floor(os.uptime()))
    });
  }

  if (tasks.length) {
    await Promise.all(tasks);
  }

  if (config.cpu_temp) {
    const temp = getCpuTempValue(cpuTemp);
    const value = temp !== null ? Number(temp.toFixed(1)) : null;
    sensors.push({
      entity_id: 'pc.cpu_temp',
      name: 'CPU Temp',
      unit: 'C',
      value: getMetricValue('pc.cpu_temp', value)
    });
  }

  if (config.gpu_temp || config.gpu_load) {
    const gpu = pickGpuController(graphics);
    const gpuTemp = gpu ? normalizeTemperature(gpu.temperatureGpu) : null;
    const gpuLoad = gpu ? normalizePercent(Number(gpu.utilizationGpu)) : null;

    if (config.gpu_temp) {
      const value = gpuTemp !== null ? Number(gpuTemp.toFixed(1)) : null;
      sensors.push({
        entity_id: 'pc.gpu_temp',
        name: 'GPU Temp',
        unit: 'C',
        value: getMetricValue('pc.gpu_temp', value)
      });
    }

    if (config.gpu_load) {
      const value = gpuLoad !== null ? Number(gpuLoad.toFixed(1)) : null;
      sensors.push({
        entity_id: 'pc.gpu_load',
        name: 'GPU Load',
        unit: '%',
        value: getMetricValue('pc.gpu_load', value)
      });
    }
  }

  updateMetricsPreview(sensors);
  if (!sensors.length) return null;
  return { type: 'pc_metrics', sensors };
}

function sendSensorsToTab5(sensors) {
  if (!Array.isArray(sensors) || sensors.length === 0) return;
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ type: 'pc_metrics', sensors }));
  }
}

async function sendPcMetrics() {
  if (metricsInFlight) return;
  metricsInFlight = true;
  try {
    const payload = await buildPcMetricsPayload();
    if (!payload) return;
    sendSensorsToTab5(payload.sensors);
  } catch (err) {
    log(`Metrics send error: ${err.message}`);
  } finally {
    metricsInFlight = false;
  }
}

function startMetricsLoop() {
  stopMetricsLoop();
  lastCpuSample = null;
  getCpuUsagePercent();
  metricsInterval = setInterval(sendPcMetrics, METRICS_INTERVAL_MS);
  setTimeout(sendPcMetrics, 1000);
}

function stopMetricsLoop() {
  if (metricsInterval) {
    clearInterval(metricsInterval);
    metricsInterval = null;
  }
}

function updateSimStatus(status) {
  if (mainWindow) {
    mainWindow.webContents.send('sim-status', status);
  }
}

function handleSimBridgeMessage(data) {
  let msg = null;
  try {
    msg = JSON.parse(data.toString());
  } catch (err) {
    log(`SimConnect parse error: ${err.message}`);
    return;
  }
  if (!msg || !Array.isArray(msg.sensors)) return;
  const sensors = normalizeIncomingSensors(msg.sensors);
  if (!sensors.length) return;
  lastSimSensors = sensors;
  updateSimPreview(sensors);
  sendSensorsToTab5(sensors);
}

function connectSimBridge() {
  const cfg = getSimConfig();
  if (!cfg.enabled) {
    updateSimStatus('disconnected');
    return;
  }
  if (simWs && (simWs.readyState === WebSocket.OPEN || simWs.readyState === WebSocket.CONNECTING)) {
    return;
  }

  const url = `ws://${cfg.host}:${cfg.port}`;
  log(`Connecting SimConnect bridge: ${url}`);

  simWs = new WebSocket(url, {
    perMessageDeflate: false,
    handshakeTimeout: 5000
  });

  simWs.on('open', () => {
    simConnected = true;
    updateSimStatus('connected');
    log('SimConnect bridge connected');
    if (simReconnectTimer) {
      clearTimeout(simReconnectTimer);
      simReconnectTimer = null;
    }
  });

  simWs.on('message', (data) => {
    handleSimBridgeMessage(data);
  });

  simWs.on('close', () => {
    simConnected = false;
    updateSimStatus('disconnected');
    log('SimConnect bridge disconnected');
    if (!getSimConfig().enabled) return;
    if (simReconnectTimer) return;
    simReconnectTimer = setTimeout(() => {
      simReconnectTimer = null;
      connectSimBridge();
    }, SIMCONNECT_RECONNECT_MS);
  });

  simWs.on('error', (err) => {
    log(`SimConnect bridge error: ${err.message}`);
  });
}

function disconnectSimBridge() {
  if (simReconnectTimer) {
    clearTimeout(simReconnectTimer);
    simReconnectTimer = null;
  }
  if (simWs) {
    simWs.close();
    simWs = null;
  }
  simConnected = false;
  updateSimStatus('disconnected');
}

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
    if (startHidden) return;
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

    startMetricsLoop();
    if (lastSimSensors.length) {
      sendSensorsToTab5(lastSimSensors);
    }
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
    stopMetricsLoop();
    stopMetricsLoop();
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

ipcMain.on('get-metrics-config', (event) => {
  event.returnValue = getMetricsConfig();
});

ipcMain.on('set-metrics-config', (event, config) => {
  const next = setMetricsConfig(config);
  log(`Metrics config updated: ${JSON.stringify(next)}`);
});

ipcMain.on('get-sim-config', (event) => {
  event.returnValue = getSimConfig();
});

ipcMain.on('set-sim-config', (event, config) => {
  const next = setSimConfig(config);
  log(`SimConnect config updated: ${JSON.stringify(next)}`);
  disconnectSimBridge();
  if (next.enabled) {
    connectSimBridge();
  }
});

// Autostart Funktionen
function setAutostart(enabled) {
  const loginItem = {
    openAtLogin: enabled,
    openAsHidden: true,
    args: ['--hidden']
  };

  if (!app.isPackaged) {
    loginItem.path = process.execPath;
    loginItem.args = [app.getAppPath(), '--hidden'];
  }

  app.setLoginItemSettings(loginItem);
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
  startMetricsLoop();
  if (getSimConfig().enabled) {
    connectSimBridge();
  }

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
  stopMetricsLoop();
  disconnectSimBridge();
  if (ws) {
    ws.close();
  }
});

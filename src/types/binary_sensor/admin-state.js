
  const BINARY_SENSOR_ICON_PAIRS = Object.freeze({
    '': Object.freeze(['radiobox-blank', 'checkbox-marked-circle']),
    battery: Object.freeze(['battery', 'battery-outline']),
    battery_charging: Object.freeze(['battery', 'battery-charging']),
    carbon_monoxide: Object.freeze(['smoke-detector', 'smoke-detector-alert']),
    cold: Object.freeze(['thermometer', 'snowflake']),
    connectivity: Object.freeze(['close-network-outline', 'check-network-outline']),
    door: Object.freeze(['door-closed', 'door-open']),
    garage_door: Object.freeze(['garage', 'garage-open']),
    gas: Object.freeze(['check-circle', 'alert-circle']),
    heat: Object.freeze(['thermometer', 'fire']),
    light: Object.freeze(['brightness-5', 'brightness-7']),
    lock: Object.freeze(['lock', 'lock-open']),
    moisture: Object.freeze(['water-off', 'water']),
    motion: Object.freeze(['motion-sensor-off', 'motion-sensor']),
    moving: Object.freeze(['octagon', 'arrow-right']),
    occupancy: Object.freeze(['home-outline', 'home']),
    opening: Object.freeze(['square', 'square-outline']),
    plug: Object.freeze(['power-plug-off', 'power-plug']),
    power: Object.freeze(['power-plug-off', 'power-plug']),
    presence: Object.freeze(['home-outline', 'home']),
    problem: Object.freeze(['check-circle', 'alert-circle']),
    running: Object.freeze(['stop', 'play']),
    safety: Object.freeze(['check-circle', 'alert-circle']),
    smoke: Object.freeze(['smoke-detector-variant', 'smoke-detector-variant-alert']),
    sound: Object.freeze(['music-note-off', 'music-note']),
    tamper: Object.freeze(['check-circle', 'alert-circle']),
    update: Object.freeze(['package', 'package-up']),
    vibration: Object.freeze(['crop-portrait', 'vibrate']),
    window: Object.freeze(['window-closed', 'window-open'])
  });
  function parseBinarySensorPreviewPayload(value) {
    const out = {
      valid: false,
      state: '',
      available: false,
      deviceClass: '',
      icon: ''
    };
    if (value === undefined || value === null) return out;

    let source = value;
    if (typeof source === 'string') {
      const text = source.trim();
      if (!text.length) return out;
      if (text.startsWith('{')) {
        try {
          source = JSON.parse(text);
        } catch (error) {
          return out;
        }
      } else {
        source = { state: text };
      }
    }
    if (!source || typeof source !== 'object') return out;

    const attrs = source.attributes && typeof source.attributes === 'object'
      ? source.attributes : source;
    const state = String(source.state ?? attrs.state ?? '').trim().toLowerCase();
    if (!['on', 'off', 'unknown', 'unavailable'].includes(state)) return out;

    out.valid = true;
    out.state = state;
    out.available = typeof source.available === 'boolean'
      ? source.available : state !== 'unavailable';
    if (state === 'unavailable') out.available = false;
    out.deviceClass = String(
      source.device_class ?? attrs.device_class ?? '').trim().toLowerCase();
    out.icon = normalizeMdiIconName(source.icon ?? attrs.icon ?? '');
    return out;
  }

  function binarySensorPreviewIconPair(deviceClass) {
    return BINARY_SENSOR_ICON_PAIRS[String(deviceClass || '').toLowerCase()] ||
      BINARY_SENSOR_ICON_PAIRS[''];
  }

  function resolveBinarySensorPreviewIcon(
      rawIcon, entityId, state, metaIcons) {
    if (isExplicitlyDisabledValue(rawIcon)) return '';
    const configured = normalizeMdiIconName(rawIcon);
    if (configured) return configured;

    const stateIcon = normalizeMdiIconName(state?.icon || '');
    if (stateIcon) return stateIcon;
    const entityIcon = entityId && metaIcons
      ? normalizeMdiIconName(metaIcons[entityId]) : '';
    if (entityIcon) return entityIcon;

    const pair = binarySensorPreviewIconPair(state?.deviceClass);
    const active = state?.valid === true && state?.available === true &&
      state?.state === 'on';
    return pair[active ? 1 : 0];
  }

  function binarySensorPreviewColor(state) {
    return state?.valid === true && state?.available === true &&
      state?.state === 'on' ? '#ffc107' : '#9e9e9e';
  }

  function binarySensorPreviewStateText(state) {
    if (!state?.valid) return '--';
    const translations = typeof BINARY_SENSOR_I18N === 'object'
      ? BINARY_SENSOR_I18N : {};
    if (state.available === false || state.state === 'unavailable') {
      return translations.unavailable || '--';
    }
    if (state.state === 'unknown') return translations.unknown || '--';
    const states = translations.states || {};
    const pair = states[state.deviceClass] || states[''] || {};
    return pair[state.state] || translations.unknown || '--';
  }

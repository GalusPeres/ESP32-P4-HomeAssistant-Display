function t(key) {
    return Object.prototype.hasOwnProperty.call(APP_I18N, key) ? APP_I18N[key] : key;
  }
  function tf(key, replacements) {
    let out = t(key);
    if (!replacements) return out;
    Object.keys(replacements).forEach(name => {
      out = out.replaceAll('{' + name + '}', String(replacements[name]));
    });
    return out;
  }
  let APP_LOCALE = document.documentElement.lang || 'en';
  function formatLocalizedNumber(value, decimals = 0, trimTrailingZeros = false) {
    const numeric = Number(String(value ?? '').trim().replace(',', '.'));
    if (!Number.isFinite(numeric)) return '--';
    const digits = Math.max(0, Math.min(6, Number.parseInt(decimals, 10) || 0));
    return new Intl.NumberFormat(APP_LOCALE, {
      useGrouping: false,
      minimumFractionDigits: trimTrailingZeros ? 0 : digits,
      maximumFractionDigits: digits
    }).format(numeric);
  }
  function localizeNumericText(value) {
    const text = String(value ?? '').trim();
    if (!text.length) return text;
    const normalized = text.replace(',', '.');
    if (!/^[+-]?(?:\d+(?:\.\d*)?|\.\d+)$/.test(normalized)) return text;
    const fraction = normalized.includes('.') ? normalized.split('.')[1].length : 0;
    return formatLocalizedNumber(Number(normalized), fraction, false);
  }

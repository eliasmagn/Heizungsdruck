const $ = (id) => document.getElementById(id);
const stateName = {
  0: 'UNKNOWN',
  1: 'SENSOR_FAULT',
  2: 'PRESSURE_LOW',
  3: 'OK',
  4: 'PRESSURE_HIGH',
};

let configCache = null;
let historyCache = [];

function toast(msg, isError = false) {
  const t = $('toast');
  t.textContent = msg;
  t.classList.remove('hidden');
  t.classList.toggle('error', isError);
  setTimeout(() => t.classList.add('hidden'), 2200);
}

async function apiJson(url) {
  const r = await fetch(url);
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}

async function apiText(url, method = 'GET', body = null) {
  const opts = { method, headers: {} };
  if (body !== null) {
    opts.headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(body);
  }
  const r = await fetch(url, opts);
  const txt = await r.text();
  if (!r.ok) throw new Error(txt || `${r.status}`);
  return txt;
}

function switchTab(tabId) {
  document.querySelectorAll('.tab-btn').forEach((b) => b.classList.toggle('active', b.dataset.tab === tabId));
  document.querySelectorAll('.tab').forEach((t) => t.classList.toggle('active', t.id === tabId));
}

document.querySelectorAll('.tab-btn').forEach((btn) => {
  btn.addEventListener('click', () => switchTab(btn.dataset.tab));
});

function updateLive(status) {
  const bar = Number(status.pressureBar || 0);
  $('barValue').textContent = bar.toFixed(2);
  $('adcValue').textContent = String(status.filteredAdc ?? '--');
  $('adcRawValue').textContent = String(status.rawAdc ?? '--');
  $('voltageValue').textContent = `${Number(status.voltage || 0).toFixed(3)} V`;
  $('uptimeValue').textContent = `${Math.floor(Number(status.uptimeSec || 0) / 60)} min`;
  $('calibLiveAdc').textContent = String(status.filteredAdc ?? '--');

  const fill = Math.max(0, Math.min(100, (bar / 3) * 100));
  $('pressureFill').style.width = `${fill}%`;

  const state = stateName[Number(status.state)] || `STATE_${status.state}`;
  $('statusLine').textContent = `Status ${state}, Fault ${status.fault}, gültig=${Boolean(status.valid)}`;
  $('wifiPill').textContent = `WiFi: ${status.wifi ? 'ONLINE' : 'AP/OFFLINE'}`;
  $('mqttPill').textContent = `MQTT: ${status.mqtt ? 'ONLINE' : 'OFFLINE'}`;
  $('statePill').textContent = `State: ${state}`;
}

function drawHistory(entries) {
  const c = $('historyCanvas');
  const ctx = c.getContext('2d');
  ctx.fillStyle = '#030712';
  ctx.fillRect(0, 0, c.width, c.height);

  ctx.strokeStyle = '#223047';
  for (let i = 0; i <= 6; i += 1) {
    const y = (i * c.height) / 6;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(c.width, y);
    ctx.stroke();
  }

  if (!entries || entries.length < 2) return;

  const maxBar = Math.max(3, ...entries.map((e) => Number(e.bar || 0)));
  ctx.strokeStyle = '#22d3ee';
  ctx.lineWidth = 2;
  ctx.beginPath();
  entries.forEach((entry, i) => {
    const x = i * (c.width / (entries.length - 1));
    const y = c.height - (Number(entry.bar || 0) / maxBar) * c.height;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

async function refreshHistory() {
  const payload = await apiJson('/api/history');
  historyCache = payload.entries || [];
  drawHistory(historyCache);
}

function calibrationRows(points) {
  const rows = $('calibrationRows');
  rows.innerHTML = '';
  points.forEach((point, index) => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td>${index}</td>
      <td><input data-i="${index}" data-k="bar" type="number" step="0.5" value="${Number(point.bar || 0).toFixed(1)}"></td>
      <td><input data-i="${index}" data-k="adc" type="number" value="${point.adc ?? 0}"></td>
      <td><input data-i="${index}" data-k="valid" type="checkbox" ${point.valid ? 'checked' : ''}></td>
    `;
    rows.appendChild(tr);
  });

  rows.querySelectorAll('input').forEach((input) => {
    input.addEventListener('change', () => {
      const idx = Number(input.dataset.i);
      const key = input.dataset.k;
      if (key === 'valid') configCache.calib.points[idx][key] = input.checked;
      else configCache.calib.points[idx][key] = Number(input.value || 0);
    });
  });
}

function fillConfig(cfg) {
  configCache = cfg;

  $('sensorAdcPin').value = cfg.sensor.adcPin;
  $('sensorSampleCount').value = cfg.sensor.sampleCount;
  $('sensorInterval').value = cfg.sensor.updateIntervalMs;
  $('sensorDisconnectAdc').value = cfg.sensor.disconnectAdc;
  $('sensorShortGndAdc').value = cfg.sensor.shortGndAdc;
  $('sensorShortVccAdc').value = cfg.sensor.shortVccAdc;
  $('sensorMaxJumpBar').value = cfg.sensor.maxJumpBar;

  $('wifiSsid').value = cfg.network.wifiSsid || '';
  $('wifiPassword').value = cfg.network.wifiPassword || '';
  $('apSsid').value = cfg.network.apSsid || '';
  $('apPassword').value = cfg.network.apPassword || '';
  $('hostname').value = cfg.network.hostname || '';

  $('mqttEnabled').checked = Boolean(cfg.mqtt.enabled);
  $('mqttHost').value = cfg.mqtt.host || '';
  $('mqttPort').value = cfg.mqtt.port || 1883;
  $('mqttUser').value = cfg.mqtt.username || '';
  $('mqttPassword').value = cfg.mqtt.password || '';
  $('mqttTopicBase').value = cfg.mqtt.topicBase || 'heizungsdruck';
  $('mqttPublishInterval').value = cfg.mqtt.publishIntervalMs || 10000;

  $('alarmLow').value = cfg.alarm.lowBar;
  $('alarmHigh').value = cfg.alarm.highBar;
  $('alarmHysteresis').value = cfg.alarm.hysteresisBar;
  $('alarmRepeat').value = cfg.alarm.repeatMinutes;
  $('telegramToken').value = cfg.alarm.telegramBotToken || '';
  $('telegramChat').value = cfg.alarm.telegramChatId || '';
  $('webhookUrl').value = cfg.alarm.emailWebhookUrl || '';

  $('wgEnabled').checked = Boolean(cfg.wireguard.enabled);
  $('wgStatusUrl').value = cfg.wireguard.statusUrl || '';
  $('wgEnableUrl').value = cfg.wireguard.enableUrl || '';
  $('wgDisableUrl').value = cfg.wireguard.disableUrl || '';
  $('wgAuthToken').value = cfg.wireguard.authToken || '';

  $('adcLow').value = cfg.calib.adcLow;
  $('adcHigh').value = cfg.calib.adcHigh;
  $('barLow').value = cfg.calib.barLow;
  $('barHigh').value = cfg.calib.barHigh;
  $('offsetBar').value = cfg.calib.offsetBar;
  calibrationRows(cfg.calib.points || []);
}

async function refreshConfig() {
  const cfg = await apiJson('/api/config');
  fillConfig(cfg);
}

$('refreshHistory').onclick = async () => {
  try {
    await refreshHistory();
    toast('Verlauf geladen');
  } catch (e) {
    toast(e.message, true);
  }
};

$('exportJson').onclick = () => {
  const blob = new Blob([JSON.stringify({ entries: historyCache }, null, 2)], { type: 'application/json' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'heizungsdruck-history.json';
  a.click();
};

$('exportCsv').onclick = () => {
  const lines = ['ts,bar,state,valid', ...historyCache.map((e) => `${e.ts},${e.bar},${e.state},${e.valid}`)];
  const blob = new Blob([lines.join('\n')], { type: 'text/csv' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = 'heizungsdruck-history.csv';
  a.click();
};

$('capturePoint').onclick = async () => {
  try {
    await apiText('/api/calibration/capture', 'POST', { bar: Number($('captureBar').value || 0) });
    await refreshConfig();
    toast('Kalibrierpunkt aufgenommen');
  } catch (e) {
    toast(e.message, true);
  }
};

$('saveCalibration').onclick = async () => {
  try {
    configCache.calib.adcLow = Number($('adcLow').value || 0);
    configCache.calib.adcHigh = Number($('adcHigh').value || 0);
    configCache.calib.barLow = Number($('barLow').value || 0);
    configCache.calib.barHigh = Number($('barHigh').value || 10);
    configCache.calib.offsetBar = Number($('offsetBar').value || 0);
    await apiText('/api/config/calibration', 'POST', { calib: configCache.calib });
    toast('Kalibrierung gespeichert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('clearCalibration').onclick = async () => {
  try {
    await apiText('/api/calibration/clear', 'POST', {});
    await refreshConfig();
    toast('Kalibrierung gelöscht');
  } catch (e) {
    toast(e.message, true);
  }
};

$('saveSensor').onclick = async () => {
  try {
    await apiText('/api/config/sensor', 'POST', {
      adcPin: Number($('sensorAdcPin').value || 34),
      sampleCount: Number($('sensorSampleCount').value || 9),
      updateIntervalMs: Number($('sensorInterval').value || 100),
      disconnectAdc: Number($('sensorDisconnectAdc').value || 80),
      shortGndAdc: Number($('sensorShortGndAdc').value || 20),
      shortVccAdc: Number($('sensorShortVccAdc').value || 4070),
      maxJumpBar: Number($('sensorMaxJumpBar').value || 0.7),
    });
    toast('Sensor gespeichert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('saveNetwork').onclick = async () => {
  try {
    await apiText('/api/config/network', 'POST', {
      wifiSsid: $('wifiSsid').value,
      wifiPassword: $('wifiPassword').value,
      apSsid: $('apSsid').value,
      apPassword: $('apPassword').value,
      hostname: $('hostname').value,
    });
    toast('Netzwerk gespeichert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('saveMqtt').onclick = async () => {
  try {
    await apiText('/api/config/mqtt', 'POST', {
      enabled: $('mqttEnabled').checked,
      host: $('mqttHost').value,
      port: Number($('mqttPort').value || 1883),
      username: $('mqttUser').value,
      password: $('mqttPassword').value,
      topicBase: $('mqttTopicBase').value,
      publishIntervalMs: Number($('mqttPublishInterval').value || 10000),
    });
    toast('MQTT gespeichert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('saveAlarm').onclick = async () => {
  try {
    await apiText('/api/config/alarm', 'POST', {
      lowBar: Number($('alarmLow').value || 1),
      highBar: Number($('alarmHigh').value || 2.2),
      hysteresisBar: Number($('alarmHysteresis').value || 0.1),
      repeatMinutes: Number($('alarmRepeat').value || 30),
      telegramBotToken: $('telegramToken').value,
      telegramChatId: $('telegramChat').value,
      emailWebhookUrl: $('webhookUrl').value,
    });
    toast('Alarm gespeichert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('saveWireguard').onclick = async () => {
  try {
    await apiText('/api/config/wireguard', 'POST', {
      enabled: $('wgEnabled').checked,
      statusUrl: $('wgStatusUrl').value,
      enableUrl: $('wgEnableUrl').value,
      disableUrl: $('wgDisableUrl').value,
      authToken: $('wgAuthToken').value,
    });
    toast('WireGuard gespeichert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('wgStatus').onclick = async () => {
  try {
    $('wgOutput').textContent = JSON.stringify(await apiJson('/api/wireguard/status'), null, 2);
  } catch (e) {
    $('wgOutput').textContent = e.message;
  }
};

$('wgEnable').onclick = async () => {
  try {
    $('wgOutput').textContent = await apiText('/api/wireguard/enable', 'POST', {});
  } catch (e) {
    $('wgOutput').textContent = e.message;
  }
};

$('wgDisable').onclick = async () => {
  try {
    $('wgOutput').textContent = await apiText('/api/wireguard/disable', 'POST', {});
  } catch (e) {
    $('wgOutput').textContent = e.message;
  }
};

$('testTelegram').onclick = async () => {
  try {
    toast(await apiText('/api/test/telegram', 'POST', {}));
  } catch (e) {
    toast(e.message, true);
  }
};

$('testWebhook').onclick = async () => {
  try {
    toast(await apiText('/api/test/webhook', 'POST', {}));
  } catch (e) {
    toast(e.message, true);
  }
};

$('rebootDevice').onclick = async () => {
  try {
    await apiText('/api/reboot', 'POST', {});
    toast('Neustart ausgelöst');
  } catch (e) {
    toast(e.message, true);
  }
};

$('refreshConfig').onclick = async () => {
  try {
    await refreshConfig();
    toast('Config geladen');
  } catch (e) {
    toast(e.message, true);
  }
};

$('exportConfig').onclick = async () => {
  try {
    $('configImport').value = JSON.stringify(await apiJson('/api/config/export'), null, 2);
    toast('Config exportiert');
  } catch (e) {
    toast(e.message, true);
  }
};

$('importConfig').onclick = async () => {
  try {
    const payload = JSON.parse($('configImport').value || '{}');
    await apiText('/api/config/import', 'POST', payload);
    await refreshConfig();
    toast('Config importiert');
  } catch (e) {
    toast(e.message, true);
  }
};

async function refreshDiag() {
  try {
    const diag = await apiJson('/api/diag');
    $('diagOutput').textContent = JSON.stringify(diag, null, 2);
  } catch (e) {
    $('diagOutput').textContent = `Fehler: ${e.message}`;
  }
}

async function pollStatus() {
  try {
    updateLive(await apiJson('/api/status'));
  } catch (e) {
    $('statusLine').textContent = `Keine Verbindung: ${e.message}`;
    $('wifiPill').textContent = 'WiFi: OFFLINE';
    $('mqttPill').textContent = 'MQTT: OFFLINE';
  }
}

(async function init() {
  await Promise.all([refreshConfig(), refreshHistory(), pollStatus(), refreshDiag()]);
  setInterval(pollStatus, 1000);
  setInterval(refreshHistory, 5000);
  setInterval(refreshDiag, 3000);
})();

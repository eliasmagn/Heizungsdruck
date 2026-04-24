const tabs = document.querySelectorAll('.tab');
document.querySelectorAll('nav button').forEach(btn => {
  btn.addEventListener('click', () => {
    tabs.forEach(t => t.classList.remove('active'));
    document.getElementById(btn.dataset.tab).classList.add('active');
  });
});

const HISTORY_KEY = 'heizungsdruck_history_v1';
const CALIB_KEY = 'heizungsdruck_calib_v1';
let historyData = JSON.parse(localStorage.getItem(HISTORY_KEY) || '[]');
let calibPoints = JSON.parse(localStorage.getItem(CALIB_KEY) || '[]');
let lastStatus = null;

const $ = id => document.getElementById(id);
const showToast = msg => {
  const t = $('toast');
  t.textContent = msg;
  t.classList.remove('hidden');
  setTimeout(() => t.classList.add('hidden'), 1800);
};

async function jget(url){ return await (await fetch(url)).json(); }
async function post(url, body){
  const r = await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body||{})});
  return {ok:r.ok, text:await r.text()};
}

function persist() {
  localStorage.setItem(HISTORY_KEY, JSON.stringify(historyData));
  localStorage.setItem(CALIB_KEY, JSON.stringify(calibPoints));
}

function drawHistory() {
  const c = $('historyCanvas');
  const ctx = c.getContext('2d');
  ctx.fillStyle = '#020617'; ctx.fillRect(0,0,c.width,c.height);
  if (historyData.length < 2) return;
  const maxBar = Math.max(3, ...historyData.map(e => e.bar || 0));
  ctx.strokeStyle = '#22d3ee';
  ctx.beginPath();
  historyData.forEach((e,i) => {
    const x = i * (c.width / (historyData.length - 1));
    const y = c.height - ((e.bar || 0)/maxBar*c.height);
    if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
  });
  ctx.stroke();
}

function renderCalibTable() {
  const body = $('calibRows');
  body.innerHTML = '';
  calibPoints.forEach((p, i) => {
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${i+1}</td><td><input data-i='${i}' data-k='bar' type='number' step='0.1' value='${p.bar}'></td>` +
                   `<td><input data-i='${i}' data-k='adc' type='number' value='${p.adc}'></td>` +
                   `<td><button data-del='${i}'>X</button></td>`;
    body.appendChild(tr);
  });

  body.querySelectorAll('input').forEach(el => el.addEventListener('change', () => {
    const i = Number(el.dataset.i);
    calibPoints[i][el.dataset.k] = Number(el.value || 0);
    persist();
  }));
  body.querySelectorAll('button[data-del]').forEach(el => el.addEventListener('click', () => {
    calibPoints.splice(Number(el.dataset.del), 1);
    persist();
    renderCalibTable();
  }));
}

function appendHistoryFromStatus(s) {
  if (!s) return;
  historyData.push({ts: Date.now(), bar: Number(s.pressureBar || 0), adc: Number(s.filteredAdc || 0), valid: !!s.valid});
  if (historyData.length > 10000) historyData.shift();
  persist();
  drawHistory();
}

async function refreshStatus() {
  try {
    const s = await jget('/api/status');
    lastStatus = s;
    $('barValue').textContent = (Number(s.pressureBar || 0)).toFixed(2);
    $('adcValue').textContent = String(s.filteredAdc ?? '----');
    $('voltValue').textContent = `${(Number(s.voltage || 0)).toFixed(2)} V`;
    $('statusLine').textContent = `valid=${!!s.valid} fault=${s.fault} state=${s.state}`;
    $('calibLiveAdc').textContent = String(s.filteredAdc ?? '----');
    appendHistoryFromStatus(s);
  } catch {
    $('statusLine').textContent = 'Keine Verbindung zum ESP';
  }
}

async function loadSensorConfig() {
  try {
    const c = await jget('/api/config');
    $('sensorAdcPin').value = c.sensor?.adcPin ?? 34;
    $('sensorSampleCount').value = c.sensor?.sampleCount ?? 9;
    $('sensorInterval').value = c.sensor?.updateIntervalMs ?? 100;
  } catch {}
}

$('saveSensor').onclick = async () => {
  const body = {
    adcPin: Number($('sensorAdcPin').value || 34),
    sampleCount: Number($('sensorSampleCount').value || 9),
    updateIntervalMs: Number($('sensorInterval').value || 100)
  };
  const r = await post('/api/config/sensor', body);
  showToast(r.ok ? 'Sensor gespeichert' : `Fehler: ${r.text}`);
};

$('captureCalib').onclick = () => {
  if (!lastStatus) return showToast('Kein Livewert vorhanden');
  calibPoints.push({bar: Number($('calibBarInput').value || 0), adc: Number(lastStatus.filteredAdc || 0)});
  persist();
  renderCalibTable();
  showToast('Kalibrierpunkt aufgenommen');
};

$('resetCalib').onclick = () => {
  calibPoints = [];
  persist();
  renderCalibTable();
  showToast('Kalibrierpunkte gelöscht');
};

$('sendCalib').onclick = async () => {
  const points = Array.from({length:21}, (_,i)=>({bar:i*0.5, adc:0, valid:false}));
  calibPoints.forEach(p => {
    const idx = Math.round(Number(p.bar) * 2);
    if (idx >= 0 && idx < points.length) {
      points[idx] = {bar: idx*0.5, adc: Number(p.adc||0), valid:true};
    }
  });
  const r = await post('/api/config/calibration', {calib:{points}});
  showToast(r.ok ? 'Kalibrierung an ESP gesendet' : `Fehler: ${r.text}`);
};

$('clearHistory').onclick = () => { historyData = []; persist(); drawHistory(); showToast('History gelöscht'); };
$('exportJson').onclick = () => {
  const blob = new Blob([JSON.stringify(historyData,null,2)], {type:'application/json'});
  const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = 'history.json'; a.click();
};
$('exportCsv').onclick = () => {
  const lines = ['ts,bar,adc,valid', ...historyData.map(e => `${e.ts},${e.bar},${e.adc},${e.valid}`)];
  const blob = new Blob([lines.join('\n')], {type:'text/csv'});
  const a = document.createElement('a'); a.href = URL.createObjectURL(blob); a.download = 'history.csv'; a.click();
};

renderCalibTable();
drawHistory();
loadSensorConfig();
refreshStatus();
setInterval(refreshStatus, 1000);

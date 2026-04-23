const tabs = document.querySelectorAll('section.tab');
document.querySelectorAll('nav button').forEach(b => b.addEventListener('click', () => {
  tabs.forEach(t => t.classList.remove('active'));
  document.getElementById(b.dataset.tab).classList.add('active');
}));

const fmt = n => Number(n || 0).toFixed(2);

async function jget(url){return await (await fetch(url)).json();}
async function post(url, body){
  const r = await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):''});
  return await r.text();
}

async function refreshStatus(){
  const s = await jget('/api/status');
  document.getElementById('pressure').textContent = `${fmt(s.pressureBar)} bar`;
  document.getElementById('state').textContent = `State ${s.state} | Fault ${s.fault} | WiFi ${s.wifi} MQTT ${s.mqtt}`;
  document.getElementById('currentAdc').textContent = s.filteredAdc;
}

async function refreshHistory(){
  const h = await jget('/api/history');
  const c = document.getElementById('history');
  const x = c.getContext('2d');
  x.fillStyle='#0b1220'; x.fillRect(0,0,c.width,c.height);
  const e = h.entries || []; if(e.length<2) return;
  for(let i=1;i<e.length;i++){
    const px1=(i-1)*(c.width/(e.length-1)); const py1=c.height-((e[i-1].bar||0)/3*c.height);
    const px2=i*(c.width/(e.length-1)); const py2=c.height-((e[i].bar||0)/3*c.height);
    const state = e[i].state;
    x.strokeStyle = (state===1||state===2||state===4)?'#f43f5e':'#22d3ee';
    x.beginPath(); x.moveTo(px1,py1); x.lineTo(px2,py2); x.stroke();
  }
}

async function loadConfig(){
  const c = await jget('/api/config');
  wifiSsid.value=c.network.wifiSsid||''; wifiPassword.value=c.network.wifiPassword||'';
  apSsid.value=c.network.apSsid||''; apPassword.value=c.network.apPassword||'';
  mqttHost.value=c.mqtt.host||''; mqttPort.value=c.mqtt.port||1883;
  mqttUser.value=c.mqtt.username||''; mqttPass.value=c.mqtt.password||'';
  mqttTopic.value=c.mqtt.topicBase||''; mqttInterval.value=c.mqtt.publishIntervalMs||10000;
  wgEnabled.checked=!!c.wireguard?.enabled; wgStatusUrl.value=c.wireguard?.statusUrl||'';
  wgEnableUrl.value=c.wireguard?.enableUrl||''; wgDisableUrl.value=c.wireguard?.disableUrl||'';
  wgAuthToken.value=c.wireguard?.authToken||'';
  lowBar.value=c.alarm.lowBar; highBar.value=c.alarm.highBar; hysteresis.value=c.alarm.hysteresisBar;
  repeatMinutes.value=c.alarm.repeatMinutes||30; tgToken.value=c.alarm.telegramBotToken||'';
  tgChat.value=c.alarm.telegramChatId||''; webhook.value=c.alarm.emailWebhookUrl||'';

  const rows = document.getElementById('pointRows'); rows.innerHTML='';
  (c.calib.points||[]).forEach((p, idx)=>{
    const tr=document.createElement('tr');
    tr.innerHTML=`<td>${fmt(p.bar)}</td><td><input data-adc=\"${idx}\" type=\"number\" value=\"${p.adc}\"></td><td><input data-valid=\"${idx}\" type=\"checkbox\" ${p.valid?'checked':''}></td>`;
    rows.appendChild(tr);
  });
}

saveNetwork.onclick = async ()=> saveResult.textContent = await post('/api/config/network',{wifiSsid:wifiSsid.value,wifiPassword:wifiPassword.value,apSsid:apSsid.value,apPassword:apPassword.value});
saveMqtt.onclick = async ()=> saveResult.textContent = await post('/api/config/mqtt',{enabled:!!mqttHost.value,host:mqttHost.value,port:+mqttPort.value,username:mqttUser.value,password:mqttPass.value,topicBase:mqttTopic.value,publishIntervalMs:+mqttInterval.value});
saveWg.onclick = async ()=> saveResult.textContent = await post('/api/config/wireguard',{enabled:wgEnabled.checked,statusUrl:wgStatusUrl.value,enableUrl:wgEnableUrl.value,disableUrl:wgDisableUrl.value,authToken:wgAuthToken.value});
saveAlarm.onclick = async ()=> saveResult.textContent = await post('/api/config/alarm',{lowBar:+lowBar.value,highBar:+highBar.value,hysteresisBar:+hysteresis.value,repeatMinutes:+repeatMinutes.value,telegramBotToken:tgToken.value,telegramChatId:tgChat.value,emailWebhookUrl:webhook.value});
capturePoint.onclick = async ()=> {calibResult.textContent = await post('/api/calibration/capture',{bar:+captureBar.value}); await loadConfig();};
clearPoints.onclick = async ()=> {calibResult.textContent = await post('/api/calibration/clear'); await loadConfig();};
saveCalibTable.onclick = async ()=> {
  const cfg = await jget('/api/config');
  document.querySelectorAll('input[data-adc]').forEach((el)=>{
    const i = Number(el.dataset.adc);
    cfg.calib.points[i].adc = Number(el.value || 0);
  });
  document.querySelectorAll('input[data-valid]').forEach((el)=>{
    const i = Number(el.dataset.valid);
    cfg.calib.points[i].valid = !!el.checked;
  });
  calibResult.textContent = await post('/api/config/calibration', {calib:{points: cfg.calib.points}});
  await loadConfig();
};
testTelegram.onclick = async ()=> diagOut.textContent = await post('/api/test/telegram');
testWebhook.onclick = async ()=> diagOut.textContent = await post('/api/test/webhook');
wgStatus.onclick = async ()=> diagOut.textContent = JSON.stringify(await jget('/api/wireguard/status'),null,2);
wgEnable.onclick = async ()=> diagOut.textContent = await post('/api/wireguard/enable');
wgDisable.onclick = async ()=> diagOut.textContent = await post('/api/wireguard/disable');
reboot.onclick = async ()=> diagOut.textContent = await post('/api/reboot');
exportConfig.onclick = async ()=> {configJson.value = JSON.stringify(await jget('/api/config/export'), null, 2);};
importConfig.onclick = async ()=> {
  const r = await fetch('/api/config/import',{method:'POST',headers:{'Content-Type':'application/json'},body:configJson.value});
  saveResult.textContent = await r.text();
  await loadConfig();
};

async function pollDiag(){diagOut.textContent = JSON.stringify(await jget('/api/diag'),null,2);}

setInterval(refreshStatus,1500);
setInterval(refreshHistory,3000);
setInterval(pollDiag,3000);
refreshStatus(); refreshHistory(); pollDiag(); loadConfig();

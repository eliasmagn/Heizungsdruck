#include "WebUI.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "JsonCodec.h"

WebUI::WebUI(uint16_t port) : server_(port) {}

String WebUI::pageTemplate(const char *title, const char *body, const char *script) {
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>" + String(title) + "</title>";
  html += "<style>body{font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:1rem}"
          ".card{background:#1e293b;padding:1rem;border-radius:10px;margin-bottom:1rem}"
          "input,select,textarea{width:100%;padding:8px;margin:5px 0;background:#0b1220;color:#e2e8f0;border:1px solid #334155}"
          "button{padding:8px 12px;background:#0284c7;color:#fff;border:0;border-radius:6px;margin-right:0.5rem}"
          "table{border-collapse:collapse;width:100%}th,td{border:1px solid #334155;padding:6px;text-align:left}"
          "nav a{color:#7dd3fc;margin-right:12px}</style></head><body><nav>"
          "<a href='/'>Dashboard</a><a href='/history'>History</a><a href='/settings'>Settings</a><a href='/calibration'>Calibration</a><a href='/diag'>Diagnostics</a>"
          "</nav>";
  html += body;
  html += "<script>" + String(script) + "</script></body></html>";
  return html;
}

void WebUI::begin() {
  LittleFS.begin(true);
  setupRoutes();
  server_.begin();
}

void WebUI::loop() { server_.handleClient(); }

void WebUI::updateLiveData(const PressureReading &reading, PressureState state, bool wifiConnected, bool mqttConnected,
                           uint32_t uptimeSec) {
  lastReading_ = reading;
  lastState_ = state;
  wifiConnected_ = wifiConnected;
  mqttConnected_ = mqttConnected;
  uptimeSec_ = uptimeSec;
}

void WebUI::attachConfig(AppConfig *cfg, bool (*saveFn)(const AppConfig &)) {
  cfg_ = cfg;
  saveConfig_ = saveFn;
}

void WebUI::attachHistory(PressureHistory *history) { history_ = history; }

String WebUI::statusJson() const {
  JsonDocument doc;
  doc["pressureBar"] = lastReading_.pressureBar;
  doc["rawAdc"] = lastReading_.rawAdc;
  doc["filteredAdc"] = lastReading_.filteredAdc;
  doc["voltage"] = lastReading_.voltage;
  doc["valid"] = lastReading_.valid;
  doc["fault"] = static_cast<int>(lastReading_.fault);
  doc["state"] = static_cast<int>(lastState_);
  doc["wifi"] = wifiConnected_;
  doc["mqtt"] = mqttConnected_;
  doc["uptimeSec"] = uptimeSec_;
  String out;
  serializeJson(doc, out);
  return out;
}

String WebUI::historyJson() const {
  JsonDocument doc;
  JsonArray arr = doc["entries"].to<JsonArray>();
  if (history_ != nullptr) {
    for (const auto &e : history_->entries()) {
      JsonObject item = arr.add<JsonObject>();
      item["ts"] = e.ts;
      item["bar"] = e.bar;
      item["state"] = e.state;
      item["valid"] = e.valid;
    }
  }
  String out;
  serializeJson(doc, out);
  return out;
}

String WebUI::diagnosticsJson() const {
  JsonDocument doc;
  deserializeJson(doc, statusJson());
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["flashChipSize"] = ESP.getFlashChipSize();
  doc["sketchSize"] = ESP.getSketchSize();
  doc["wifiRssi"] = WiFi.RSSI();
  doc["wifiIp"] = WiFi.localIP().toString();
  doc["apIp"] = WiFi.softAPIP().toString();
  String out;
  serializeJson(doc, out);
  return out;
}

String WebUI::configJson() const {
  if (!cfg_) return "{}";
  return String(configToJson(*cfg_).c_str());
}

bool WebUI::saveUpdatedConfig(const AppConfig &candidate, String &errorOut) {
  if (!cfg_ || !saveConfig_) {
    errorOut = "config unavailable";
    return false;
  }
  std::string err;
  if (!candidate.validate(err)) {
    errorOut = String("validation failed: ") + err.c_str();
    return false;
  }
  if (!saveConfig_(candidate)) {
    errorOut = "persist failed";
    return false;
  }
  *cfg_ = candidate;
  return true;
}

bool WebUI::parseFloatArg(const String &value, float &out) {
  if (value.isEmpty()) return false;
  char *end = nullptr;
  out = strtof(value.c_str(), &end);
  return end != value.c_str();
}

void WebUI::setupRoutes() {
  if (LittleFS.exists("/index.html")) {
    server_.serveStatic("/", LittleFS, "/index.html");
  }
  if (LittleFS.exists("/app.js")) {
    server_.serveStatic("/app.js", LittleFS, "/app.js");
  }
  if (LittleFS.exists("/style.css")) {
    server_.serveStatic("/style.css", LittleFS, "/style.css");
  }

  const char *dashBody =
      "<div class='card'><h2>Druck</h2><h1 id='p'>--.- bar</h1><div id='s'></div><small id='meta'></small></div>"
      "<div class='card'><h3>Debug</h3><pre id='dbg'></pre></div>";
  const char *dashScript =
      "async function upd(){const r=await fetch('/api/status');const j=await r.json();"
      "document.getElementById('p').textContent=(j.pressureBar??0).toFixed(2)+' bar';"
      "document.getElementById('s').textContent='State '+j.state+' | Fault '+j.fault;"
      "document.getElementById('meta').textContent='WiFi:'+j.wifi+' MQTT:'+j.mqtt+' Uptime:'+j.uptimeSec+'s';"
      "document.getElementById('dbg').textContent=JSON.stringify(j,null,2);}"
      "setInterval(upd,1500);upd();";

  server_.on("/", HTTP_GET, [this, dashBody, dashScript]() {
    server_.send(200, "text/html", pageTemplate("Dashboard", dashBody, dashScript));
  });

  const char *historyBody = "<div class='card'><h2>History</h2><canvas id='c' width='720' height='220'></canvas></div>";
  const char *historyScript =
      "async function plot(){const r=await fetch('/api/history');const d=await r.json();const c=document.getElementById('c');"
      "const x=c.getContext('2d');x.fillStyle='#0b1220';x.fillRect(0,0,c.width,c.height);"
      "const e=d.entries||[];if(e.length<2)return;"
      "x.strokeStyle='#1e293b';for(let i=0;i<=6;i++){const y=i*c.height/6;x.beginPath();x.moveTo(0,y);x.lineTo(c.width,y);x.stroke();}"
      "x.strokeStyle='#22d3ee';x.beginPath();"
      "for(let i=0;i<e.length;i++){const px=i*(c.width/(e.length-1));const py=c.height-((e[i].bar||0)/3.0*c.height);if(i===0)x.moveTo(px,py);else x.lineTo(px,py);}x.stroke();}"
      "setInterval(plot,3000);plot();";
  server_.on("/history", HTTP_GET, [this, historyBody, historyScript]() {
    server_.send(200, "text/html", pageTemplate("History", historyBody, historyScript));
  });

  const char *settingsBody =
      "<div class='card'><h2>Network</h2><label>WiFi SSID<input id='wifiSsid'></label><label>WiFi Password<input id='wifiPassword' type='password'></label>"
      "<label>AP SSID<input id='apSsid'></label><label>AP Password<input id='apPassword'></label><button onclick='saveNetwork()'>Save Network</button></div>"
      "<div class='card'><h2>MQTT</h2><label>Host<input id='mqttHost'></label><label>Port<input id='mqttPort' type='number'></label>"
      "<label>User<input id='mqttUser'></label><label>Password<input id='mqttPass' type='password'></label><label>Topic Prefix<input id='mqttTopic'></label>"
      "<label>Publish Interval ms<input id='mqttInterval' type='number'></label><button onclick='saveMqtt()'>Save MQTT</button></div>"
      "<div class='card'><h2>Alarm</h2><label>Low bar<input id='low' type='number' step='0.01'></label><label>High bar<input id='high' type='number' step='0.01'></label>"
      "<label>Hysteresis<input id='hyst' type='number' step='0.01'></label><label>Repeat minutes<input id='repeat' type='number'></label>"
      "<label>Telegram Bot Token<input id='tgToken'></label><label>Telegram Chat ID<input id='tgChat'></label><label>Email Webhook URL<input id='webhook'></label>"
      "<button onclick='saveAlarm()'>Save Alarm</button></div><pre id='result'></pre>";
  const char *settingsScript =
      "async function cfg(){return await (await fetch('/api/config')).json();}"
      "async function load(){const j=await cfg();"
      "wifiSsid.value=j.network.wifiSsid||'';wifiPassword.value=j.network.wifiPassword||'';apSsid.value=j.network.apSsid||'';apPassword.value=j.network.apPassword||'';"
      "mqttHost.value=j.mqtt.host||'';mqttPort.value=j.mqtt.port||1883;mqttUser.value=j.mqtt.username||'';mqttPass.value=j.mqtt.password||'';mqttTopic.value=j.mqtt.topicBase||'';mqttInterval.value=j.mqtt.publishIntervalMs||10000;"
      "low.value=j.alarm.lowBar;high.value=j.alarm.highBar;hyst.value=j.alarm.hysteresisBar;repeat.value=j.alarm.repeatMinutes||30;tgToken.value=j.alarm.telegramBotToken||'';tgChat.value=j.alarm.telegramChatId||'';webhook.value=j.alarm.emailWebhookUrl||'';}"
      "async function post(url,body){const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});result.textContent=await r.text();}"
      "async function saveNetwork(){await post('/api/config/network',{wifiSsid:wifiSsid.value,wifiPassword:wifiPassword.value,apSsid:apSsid.value,apPassword:apPassword.value});}"
      "async function saveMqtt(){await post('/api/config/mqtt',{host:mqttHost.value,port:+mqttPort.value,username:mqttUser.value,password:mqttPass.value,topicBase:mqttTopic.value,publishIntervalMs:+mqttInterval.value,enabled:!!mqttHost.value});}"
      "async function saveAlarm(){await post('/api/config/alarm',{lowBar:+low.value,highBar:+high.value,hysteresisBar:+hyst.value,repeatMinutes:+repeat.value,telegramBotToken:tgToken.value,telegramChatId:tgChat.value,emailWebhookUrl:webhook.value});}"
      "load();";
  server_.on("/settings", HTTP_GET, [this, settingsBody, settingsScript]() {
    server_.send(200, "text/html", pageTemplate("Settings", settingsBody, settingsScript));
  });

  const char *calibBody =
      "<div class='card'><h2>Kalibrierung 0.0 bis 10.0 bar</h2><p>Aktueller ADC: <b id='adc'>-</b></p>"
      "<label>Bar für Capture<input id='captureBar' type='number' step='0.5' min='0' max='10' value='1.0'></label>"
      "<button onclick='capture()'>Punkt speichern</button><button onclick='clearAll()'>Alle löschen</button>"
      "<table><thead><tr><th>Bar</th><th>ADC</th><th>Valid</th><th>Aktion</th></tr></thead><tbody id='rows'></tbody></table></div><pre id='msg'></pre>";
  const char *calibScript =
      "let config=null;"
      "function barToIndex(bar){return Math.round(bar*2);}"
      "async function load(){const s=await (await fetch('/api/status')).json();adc.textContent=s.filteredAdc;config=await (await fetch('/api/config')).json();render();}"
      "function render(){rows.innerHTML='';(config.calib.points||[]).forEach((p,i)=>{const tr=document.createElement('tr');"
      "tr.innerHTML=`<td>${p.bar.toFixed(1)}</td><td><input type='number' value='${p.adc||0}' onchange='updateAdc(${i},this.value)'></td><td>${p.valid?'ja':'nein'}</td><td><button onclick='toggle(${i})'>${p.valid?'deaktivieren':'aktivieren'}</button></td>`;rows.appendChild(tr);});}"
      "function updateAdc(i,v){config.calib.points[i].adc=+v;}"
      "function toggle(i){config.calib.points[i].valid=!config.calib.points[i].valid;render();}"
      "async function capture(){const bar=+captureBar.value;const idx=barToIndex(bar);const r=await fetch('/api/calibration/capture',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({bar})});msg.textContent=await r.text();await load();}"
      "async function clearAll(){const r=await fetch('/api/calibration/clear',{method:'POST'});msg.textContent=await r.text();await load();}"
      "setInterval(load,2500);load();";
  server_.on("/calibration", HTTP_GET, [this, calibBody, calibScript]() {
    server_.send(200, "text/html", pageTemplate("Calibration", calibBody, calibScript));
  });

  const char *diagBody =
      "<div class='card'><h2>Diagnostics</h2><button onclick='sendTelegram()'>Test Telegram</button><button onclick='sendWebhook()'>Test Webhook</button><button onclick='restart()'>Restart</button><pre id='diag'></pre></div>";
  const char *diagScript =
      "async function upd(){diag.textContent=JSON.stringify(await (await fetch('/api/diag')).json(),null,2);}"
      "async function sendTelegram(){await fetch('/api/test/telegram',{method:'POST'});upd();}"
      "async function sendWebhook(){await fetch('/api/test/webhook',{method:'POST'});upd();}"
      "async function restart(){await fetch('/api/reboot',{method:'POST'});}setInterval(upd,2000);upd();";
  server_.on("/diag", HTTP_GET, [this, diagBody, diagScript]() {
    server_.send(200, "text/html", pageTemplate("Diagnostics", diagBody, diagScript));
  });

  server_.on("/api/status", HTTP_GET, [this]() { server_.send(200, "application/json", statusJson()); });
  server_.on("/api/history", HTTP_GET, [this]() { server_.send(200, "application/json", historyJson()); });
  server_.on("/api/diag", HTTP_GET, [this]() { server_.send(200, "application/json", diagnosticsJson()); });
  server_.on("/api/config", HTTP_GET, [this]() { server_.send(200, "application/json", configJson()); });
  server_.on("/api/config/export", HTTP_GET, [this]() { server_.send(200, "application/json", configJson()); });

  server_.on("/api/config", HTTP_POST, [this]() {
    AppConfig candidate = cfg_ ? *cfg_ : defaultConfig();
    std::string err;
    if (!configFromJson(server_.arg("plain").c_str(), candidate, err)) {
      return server_.send(400, "text/plain", String("invalid json/config: ") + err.c_str());
    }
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "saved");
  });

  server_.on("/api/config/network", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    if (!doc["wifiSsid"].isNull()) candidate.network.wifiSsid = doc["wifiSsid"].as<const char *>();
    if (!doc["wifiPassword"].isNull()) candidate.network.wifiPassword = doc["wifiPassword"].as<const char *>();
    if (!doc["apSsid"].isNull()) candidate.network.apSsid = doc["apSsid"].as<const char *>();
    if (!doc["apPassword"].isNull()) candidate.network.apPassword = doc["apPassword"].as<const char *>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "network saved");
  });

  server_.on("/api/config/import", HTTP_POST, [this]() {
    AppConfig candidate = cfg_ ? *cfg_ : defaultConfig();
    std::string err;
    if (!configFromJson(server_.arg("plain").c_str(), candidate, err)) {
      return server_.send(400, "text/plain", String("invalid json/config: ") + err.c_str());
    }
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "imported");
  });

  server_.on("/api/config/mqtt", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    if (!doc["enabled"].isNull()) candidate.mqtt.enabled = doc["enabled"].as<bool>();
    if (!doc["host"].isNull()) candidate.mqtt.host = doc["host"].as<const char *>();
    if (!doc["port"].isNull()) candidate.mqtt.port = doc["port"].as<uint16_t>();
    if (!doc["username"].isNull()) candidate.mqtt.username = doc["username"].as<const char *>();
    if (!doc["password"].isNull()) candidate.mqtt.password = doc["password"].as<const char *>();
    if (!doc["topicBase"].isNull()) candidate.mqtt.topicBase = doc["topicBase"].as<const char *>();
    if (!doc["publishIntervalMs"].isNull()) candidate.mqtt.publishIntervalMs = doc["publishIntervalMs"].as<uint32_t>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "mqtt saved");
  });

  server_.on("/api/config/wireguard", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    if (!doc["enabled"].isNull()) candidate.wireguard.enabled = doc["enabled"].as<bool>();
    if (!doc["statusUrl"].isNull()) candidate.wireguard.statusUrl = doc["statusUrl"].as<const char *>();
    if (!doc["enableUrl"].isNull()) candidate.wireguard.enableUrl = doc["enableUrl"].as<const char *>();
    if (!doc["disableUrl"].isNull()) candidate.wireguard.disableUrl = doc["disableUrl"].as<const char *>();
    if (!doc["authToken"].isNull()) candidate.wireguard.authToken = doc["authToken"].as<const char *>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "wireguard saved");
  });

  server_.on("/api/config/alarm", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    if (!doc["lowBar"].isNull()) candidate.alarm.lowBar = doc["lowBar"].as<float>();
    if (!doc["highBar"].isNull()) candidate.alarm.highBar = doc["highBar"].as<float>();
    if (!doc["hysteresisBar"].isNull()) candidate.alarm.hysteresisBar = doc["hysteresisBar"].as<float>();
    if (!doc["repeatMinutes"].isNull()) candidate.alarm.repeatMinutes = doc["repeatMinutes"].as<uint16_t>();
    if (!doc["telegramBotToken"].isNull()) candidate.alarm.telegramBotToken = doc["telegramBotToken"].as<const char *>();
    if (!doc["telegramChatId"].isNull()) candidate.alarm.telegramChatId = doc["telegramChatId"].as<const char *>();
    if (!doc["emailWebhookUrl"].isNull()) candidate.alarm.emailWebhookUrl = doc["emailWebhookUrl"].as<const char *>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "alarm saved");
  });

  server_.on("/api/config/calibration", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    std::string err;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    JsonVariantConst cv = doc["calib"];
    if (cv.isNull()) cv = doc;
    if (!cv["adcLow"].isNull()) candidate.calib.adcLow = cv["adcLow"].as<int>();
    if (!cv["adcHigh"].isNull()) candidate.calib.adcHigh = cv["adcHigh"].as<int>();
    if (!cv["barLow"].isNull()) candidate.calib.barLow = cv["barLow"].as<float>();
    if (!cv["barHigh"].isNull()) candidate.calib.barHigh = cv["barHigh"].as<float>();
    if (!cv["offsetBar"].isNull()) candidate.calib.offsetBar = cv["offsetBar"].as<float>();
    JsonArrayConst points = cv["points"].as<JsonArrayConst>();
    if (!points.isNull()) {
      size_t idx = 0;
      for (JsonObjectConst point : points) {
        if (idx >= candidate.calib.points.size()) break;
        if (!point["bar"].isNull()) candidate.calib.points[idx].bar = point["bar"].as<float>();
        if (!point["adc"].isNull()) candidate.calib.points[idx].adc = point["adc"].as<int>();
        if (!point["valid"].isNull()) candidate.calib.points[idx].valid = point["valid"].as<bool>();
        ++idx;
      }
    }
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "calibration saved");
  });

  server_.on("/api/calibration/capture", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    const float bar = doc["bar"].as<float>();
    if (bar < 0.0f || bar > 10.0f) return server_.send(400, "text/plain", "bar must be 0..10");
    const int idx = static_cast<int>(bar * 2.0f + 0.5f);
    if (idx < 0 || idx >= static_cast<int>(candidate.calib.points.size())) {
      return server_.send(400, "text/plain", "bar index out of range");
    }
    candidate.calib.points[idx].bar = static_cast<float>(idx) * 0.5f;
    candidate.calib.points[idx].adc = lastReading_.filteredAdc;
    candidate.calib.points[idx].valid = true;
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "captured");
  });

  server_.on("/api/calibration/clear", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    for (size_t i = 0; i < candidate.calib.points.size(); ++i) {
      candidate.calib.points[i].bar = static_cast<float>(i) * 0.5f;
      candidate.calib.points[i].adc = 0;
      candidate.calib.points[i].valid = false;
    }
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "cleared");
  });

  server_.on("/api/test/telegram", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    if (cfg_->alarm.telegramBotToken.empty() || cfg_->alarm.telegramChatId.empty()) {
      return server_.send(400, "text/plain", "telegram config missing");
    }
    HTTPClient http;
    const String url = "https://api.telegram.org/bot" + String(cfg_->alarm.telegramBotToken.c_str()) + "/sendMessage";
    JsonDocument payload;
    payload["chat_id"] = cfg_->alarm.telegramChatId.c_str();
    payload["text"] = "Heizungsdruck test alarm.";
    String body;
    serializeJson(payload, body);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    const int code = http.POST(body);
    String resp = http.getString();
    http.end();
    if (code < 200 || code >= 300) {
      return server_.send(502, "text/plain", String("telegram failed: ") + code + " " + resp);
    }
    server_.send(200, "text/plain", "telegram test sent");
  });

  server_.on("/api/test/webhook", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    if (cfg_->alarm.emailWebhookUrl.empty()) {
      return server_.send(400, "text/plain", "webhook url missing");
    }
    HTTPClient http;
    http.begin(cfg_->alarm.emailWebhookUrl.c_str());
    http.addHeader("Content-Type", "application/json");
    JsonDocument payload;
    payload["device"] = cfg_->network.hostname.c_str();
    payload["pressureBar"] = lastReading_.pressureBar;
    payload["state"] = static_cast<int>(lastState_);
    payload["kind"] = "test";
    String body;
    serializeJson(payload, body);
    const int code = http.POST(body);
    String resp = http.getString();
    http.end();
    if (code < 200 || code >= 300) {
      return server_.send(502, "text/plain", String("webhook failed: ") + code + " " + resp);
    }
    server_.send(200, "text/plain", "webhook test sent");
  });

  server_.on("/api/wireguard/status", HTTP_GET, [this]() {
    if (!cfg_ || !cfg_->wireguard.enabled) return server_.send(400, "text/plain", "wireguard disabled");
    if (cfg_->wireguard.statusUrl.empty()) return server_.send(400, "text/plain", "wireguard statusUrl missing");
    HTTPClient http;
    http.begin(cfg_->wireguard.statusUrl.c_str());
    if (!cfg_->wireguard.authToken.empty()) {
      http.addHeader("Authorization", ("Bearer " + String(cfg_->wireguard.authToken.c_str())));
    }
    const int code = http.GET();
    const String resp = http.getString();
    http.end();
    if (code < 200 || code >= 300) return server_.send(502, "text/plain", String("wireguard status failed: ") + code);
    server_.send(200, "application/json", resp);
  });

  server_.on("/api/wireguard/enable", HTTP_POST, [this]() {
    if (!cfg_ || !cfg_->wireguard.enabled) return server_.send(400, "text/plain", "wireguard disabled");
    if (cfg_->wireguard.enableUrl.empty()) return server_.send(400, "text/plain", "wireguard enableUrl missing");
    HTTPClient http;
    http.begin(cfg_->wireguard.enableUrl.c_str());
    if (!cfg_->wireguard.authToken.empty()) {
      http.addHeader("Authorization", ("Bearer " + String(cfg_->wireguard.authToken.c_str())));
    }
    const int code = http.POST("");
    const String resp = http.getString();
    http.end();
    if (code < 200 || code >= 300) return server_.send(502, "text/plain", String("wireguard enable failed: ") + code);
    server_.send(200, "text/plain", resp.isEmpty() ? "enabled" : resp);
  });

  server_.on("/api/wireguard/disable", HTTP_POST, [this]() {
    if (!cfg_ || !cfg_->wireguard.enabled) return server_.send(400, "text/plain", "wireguard disabled");
    if (cfg_->wireguard.disableUrl.empty()) return server_.send(400, "text/plain", "wireguard disableUrl missing");
    HTTPClient http;
    http.begin(cfg_->wireguard.disableUrl.c_str());
    if (!cfg_->wireguard.authToken.empty()) {
      http.addHeader("Authorization", ("Bearer " + String(cfg_->wireguard.authToken.c_str())));
    }
    const int code = http.POST("");
    const String resp = http.getString();
    http.end();
    if (code < 200 || code >= 300) return server_.send(502, "text/plain", String("wireguard disable failed: ") + code);
    server_.send(200, "text/plain", resp.isEmpty() ? "disabled" : resp);
  });

  server_.on("/api/reboot", HTTP_POST, [this]() {
    server_.send(200, "text/plain", "restarting");
    delay(100);
    ESP.restart();
  });
}

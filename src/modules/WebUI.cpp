#include "WebUI.h"

#include <ArduinoJson.h>

WebUI::WebUI(uint16_t port) : server_(port) {}

String WebUI::pageTemplate(const char *title, const char *body, const char *script) {
  String html = "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>" + String(title) + "</title>";
  html += "<style>body{font-family:Arial,sans-serif;background:#0f172a;color:#e2e8f0;margin:0;padding:1rem}"
          ".card{background:#1e293b;padding:1rem;border-radius:10px;margin-bottom:1rem}"
          "input{width:100%;padding:8px;margin:5px 0;background:#0b1220;color:#e2e8f0;border:1px solid #334155}"
          "button{padding:8px 12px;background:#0284c7;color:#fff;border:0;border-radius:6px}"
          "nav a{color:#7dd3fc;margin-right:12px}</style></head><body><nav>"
          "<a href='/'>Dashboard</a><a href='/history'>History</a><a href='/settings'>Settings</a><a href='/diag'>Diagnostics</a>"
          "</nav>";
  html += body;
  html += "<script>" + String(script) + "</script></body></html>";
  return html;
}

void WebUI::begin() {
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

String WebUI::diagnosticsJson() const { return statusJson(); }

void WebUI::setupRoutes() {
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

  const char *historyBody = "<div class='card'><h2>History</h2><canvas id='c' width='360' height='140'></canvas></div>";
  const char *historyScript =
      "async function plot(){const r=await fetch('/api/history');const d=await r.json();const c=document.getElementById('c');"
      "const x=c.getContext('2d');x.fillStyle='#0b1220';x.fillRect(0,0,c.width,c.height);"
      "const e=d.entries||[];if(e.length<2)return;x.strokeStyle='#22d3ee';x.beginPath();"
      "for(let i=0;i<e.length;i++){const px=i*(c.width/(e.length-1));const py=c.height-((e[i].bar||0)/3.0*c.height);if(i===0)x.moveTo(px,py);else x.lineTo(px,py);}x.stroke();}"
      "setInterval(plot,3000);plot();";
  server_.on("/history", HTTP_GET, [this, historyBody, historyScript]() {
    server_.send(200, "text/html", pageTemplate("History", historyBody, historyScript));
  });

  const char *settingsBody =
      "<div class='card'><h2>Settings</h2><label>Low bar<input id='low' type='number' step='0.01'></label>"
      "<label>High bar<input id='high' type='number' step='0.01'></label>"
      "<label>Offset bar<input id='offset' type='number' step='0.01'></label>"
      "<button onclick='save()'>Save</button><pre id='result'></pre></div>";
  const char *settingsScript =
      "async function load(){const j=await (await fetch('/api/config')).json();low.value=j.alarm.lowBar;high.value=j.alarm.highBar;offset.value=j.calib.offsetBar;}"
      "async function save(){const body={alarm:{lowBar:+low.value,highBar:+high.value},calib:{offsetBar:+offset.value}};"
      "const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});"
      "result.textContent=await r.text();}load();";
  server_.on("/settings", HTTP_GET, [this, settingsBody, settingsScript]() {
    server_.send(200, "text/html", pageTemplate("Settings", settingsBody, settingsScript));
  });

  const char *diagBody =
      "<div class='card'><h2>Diagnostics</h2><button onclick='restart()'>Restart</button><pre id='diag'></pre></div>";
  const char *diagScript =
      "async function upd(){diag.textContent=JSON.stringify(await (await fetch('/api/diag')).json(),null,2);}"
      "async function restart(){await fetch('/api/restart',{method:'POST'});}setInterval(upd,2000);upd();";
  server_.on("/diag", HTTP_GET, [this, diagBody, diagScript]() {
    server_.send(200, "text/html", pageTemplate("Diagnostics", diagBody, diagScript));
  });

  server_.on("/api/status", HTTP_GET, [this]() { server_.send(200, "application/json", statusJson()); });
  server_.on("/api/history", HTTP_GET, [this]() { server_.send(200, "application/json", historyJson()); });
  server_.on("/api/diag", HTTP_GET, [this]() { server_.send(200, "application/json", diagnosticsJson()); });

  server_.on("/api/config", HTTP_GET, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    JsonDocument doc;
    doc["alarm"]["lowBar"] = cfg_->alarm.lowBar;
    doc["alarm"]["highBar"] = cfg_->alarm.highBar;
    doc["calib"]["offsetBar"] = cfg_->calib.offsetBar;
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
  });

  server_.on("/api/config", HTTP_POST, [this]() {
    if (!cfg_ || !saveConfig_) return server_.send(500, "text/plain", "config unavailable");
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) {
      return server_.send(400, "text/plain", "invalid json");
    }

    AppConfig candidate = *cfg_;
    if (!doc["alarm"]["lowBar"].isNull()) candidate.alarm.lowBar = doc["alarm"]["lowBar"].as<float>();
    if (!doc["alarm"]["highBar"].isNull()) candidate.alarm.highBar = doc["alarm"]["highBar"].as<float>();
    if (!doc["calib"]["offsetBar"].isNull()) candidate.calib.offsetBar = doc["calib"]["offsetBar"].as<float>();

    std::string err;
    if (!candidate.validate(err)) {
      return server_.send(400, "text/plain", String("validation failed: ") + err.c_str());
    }

    *cfg_ = candidate;
    if (!saveConfig_(*cfg_)) {
      return server_.send(500, "text/plain", "persist failed");
    }
    server_.send(200, "text/plain", "saved");
  });

  server_.on("/api/restart", HTTP_POST, [this]() {
    server_.send(200, "text/plain", "restarting");
    delay(100);
    ESP.restart();
  });
}

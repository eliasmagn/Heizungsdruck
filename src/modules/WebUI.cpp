#include "WebUI.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "JsonCodec.h"

WebUI::WebUI(uint16_t port) : server_(port) {}

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

void WebUI::attachWireGuardManager(WireGuardManager *wireguard) { wireguard_ = wireguard; }

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
  if (wireguard_ != nullptr) {
    const WireGuardStatus wg = wireguard_->status();
    JsonObject w = doc["wireguard"].to<JsonObject>();
    w["enabled"] = wg.enabled;
    w["configured"] = wg.configured;
    w["online"] = wg.online;
    w["localAddress"] = wg.localAddress.c_str();
    w["peerEndpoint"] = wg.peerEndpoint.c_str();
    w["peerPort"] = wg.peerPort;
    w["lastHandshake"] = wg.lastHandshake;
    w["lastError"] = wg.lastError.c_str();
  }
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
  if (wireguard_ != nullptr) {
    const WireGuardStatus wg = wireguard_->status();
    JsonObject w = doc["wireguard"].to<JsonObject>();
    w["enabled"] = wg.enabled;
    w["configured"] = wg.configured;
    w["online"] = wg.online;
    w["localAddress"] = wg.localAddress.c_str();
    w["peerEndpoint"] = wg.peerEndpoint.c_str();
    w["peerPort"] = wg.peerPort;
    w["lastHandshake"] = wg.lastHandshake;
    w["lastError"] = wg.lastError.c_str();
  }
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

void WebUI::setupRoutes() {
  auto serveSpaIndex = [this]() {
    if (!LittleFS.exists("/index.html")) return server_.send(500, "text/plain", "LittleFS index.html missing");
    auto file = LittleFS.open("/index.html", "r");
    if (!file) return server_.send(500, "text/plain", "LittleFS index.html open failed");
    server_.streamFile(file, "text/html");
    file.close();
  };

  server_.on("/", HTTP_GET, serveSpaIndex);
  server_.serveStatic("/app.js", LittleFS, "/app.js", "max-age=60");
  server_.serveStatic("/style.css", LittleFS, "/style.css", "max-age=60");
  server_.serveStatic("/assets", LittleFS, "/assets", "max-age=86400");

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
    if (!doc["hostname"].isNull()) candidate.network.hostname = doc["hostname"].as<const char *>();
    if (!doc["wifiTxPowerDbm"].isNull()) candidate.network.wifiTxPowerDbm = doc["wifiTxPowerDbm"].as<float>();
    if (!doc["wifi11bMode"].isNull()) candidate.network.wifi11bMode = doc["wifi11bMode"].as<bool>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "network saved");
  });

  server_.on("/api/wifi/scan", HTTP_GET, [this]() {
    const int found = WiFi.scanNetworks(false, true);
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (int i = 0; i < found; ++i) {
      JsonObject item = arr.add<JsonObject>();
      item["ssid"] = WiFi.SSID(i);
      item["rssi"] = WiFi.RSSI(i);
      item["channel"] = WiFi.channel(i);
      item["open"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    }
    WiFi.scanDelete();
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
  });

  server_.on("/api/config/sensor", HTTP_POST, [this]() {
    if (!cfg_) return server_.send(500, "text/plain", "config unavailable");
    AppConfig candidate = *cfg_;
    JsonDocument doc;
    if (deserializeJson(doc, server_.arg("plain"))) return server_.send(400, "text/plain", "invalid json");
    if (!doc["adcPin"].isNull()) candidate.sensor.adcPin = doc["adcPin"].as<uint8_t>();
    if (!doc["sampleCount"].isNull()) candidate.sensor.sampleCount = doc["sampleCount"].as<uint16_t>();
    if (!doc["updateIntervalMs"].isNull()) candidate.sensor.updateIntervalMs = doc["updateIntervalMs"].as<uint32_t>();
    if (!doc["disconnectAdc"].isNull()) candidate.sensor.disconnectAdc = doc["disconnectAdc"].as<int>();
    if (!doc["shortGndAdc"].isNull()) candidate.sensor.shortGndAdc = doc["shortGndAdc"].as<int>();
    if (!doc["shortVccAdc"].isNull()) candidate.sensor.shortVccAdc = doc["shortVccAdc"].as<int>();
    if (!doc["maxJumpBar"].isNull()) candidate.sensor.maxJumpBar = doc["maxJumpBar"].as<float>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "sensor saved");
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
    if (!doc["localAddress"].isNull()) candidate.wireguard.localAddress = doc["localAddress"].as<const char *>();
    if (!doc["netmask"].isNull()) candidate.wireguard.netmask = doc["netmask"].as<const char *>();
    if (!doc["privateKey"].isNull()) candidate.wireguard.privateKey = doc["privateKey"].as<const char *>();
    if (!doc["peerEndpoint"].isNull()) candidate.wireguard.peerEndpoint = doc["peerEndpoint"].as<const char *>();
    if (!doc["peerPort"].isNull()) candidate.wireguard.peerPort = doc["peerPort"].as<uint16_t>();
    if (!doc["peerPublicKey"].isNull()) candidate.wireguard.peerPublicKey = doc["peerPublicKey"].as<const char *>();
    if (!doc["presharedKey"].isNull()) candidate.wireguard.presharedKey = doc["presharedKey"].as<const char *>();
    if (!doc["allowedIp1"].isNull()) candidate.wireguard.allowedIp1 = doc["allowedIp1"].as<const char *>();
    if (!doc["allowedIp2"].isNull()) candidate.wireguard.allowedIp2 = doc["allowedIp2"].as<const char *>();
    if (!doc["keepAliveSeconds"].isNull()) candidate.wireguard.keepAliveSeconds = doc["keepAliveSeconds"].as<uint16_t>();
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    if (wireguard_ != nullptr) {
      if (candidate.wireguard.enabled) {
        wireguard_->enable(candidate.wireguard);
      } else {
        wireguard_->disable();
      }
    }
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
    if (wireguard_ == nullptr) return server_.send(500, "text/plain", "wireguard manager unavailable");
    const WireGuardStatus wg = wireguard_->status();
    JsonDocument doc;
    doc["enabled"] = wg.enabled;
    doc["configured"] = wg.configured;
    doc["online"] = wg.online;
    doc["localAddress"] = wg.localAddress.c_str();
    doc["peerEndpoint"] = wg.peerEndpoint.c_str();
    doc["peerPort"] = wg.peerPort;
    doc["lastHandshake"] = wg.lastHandshake;
    doc["lastError"] = wg.lastError.c_str();
    String out;
    serializeJson(doc, out);
    server_.send(200, "application/json", out);
  });

  server_.on("/api/wireguard/enable", HTTP_POST, [this]() {
    if (!cfg_ || wireguard_ == nullptr) return server_.send(500, "text/plain", "wireguard unavailable");
    if (!wireguard_->enable(cfg_->wireguard)) {
      const WireGuardStatus wg = wireguard_->status();
      return server_.send(400, "text/plain", String("wireguard enable failed: ") + wg.lastError.c_str());
    }
    server_.send(200, "text/plain", "enabled");
  });

  server_.on("/api/wireguard/disable", HTTP_POST, [this]() {
    if (wireguard_ == nullptr) return server_.send(500, "text/plain", "wireguard manager unavailable");
    wireguard_->disable();
    server_.send(200, "text/plain", "disabled");
  });

  server_.on("/api/reboot", HTTP_POST, [this]() {
    server_.send(200, "text/plain", "restarting");
    delay(100);
    ESP.restart();
  });

  server_.onNotFound([this, serveSpaIndex]() {
    if (server_.uri().startsWith("/api/")) return server_.send(404, "text/plain", "not found");
    serveSpaIndex();
  });
}

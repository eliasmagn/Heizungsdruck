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

void WebUI::setupRoutes() {
  if (LittleFS.exists("/index.html")) {
    server_.on("/", HTTP_GET, [this]() {
      auto file = LittleFS.open("/index.html", "r");
      if (!file) return server_.send(500, "text/plain", "LittleFS index.html open failed");
      server_.streamFile(file, "text/html");
      file.close();
    });
  } else {
    server_.on("/", HTTP_GET, [this]() { server_.send(500, "text/plain", "LittleFS index.html missing"); });
  }

  if (LittleFS.exists("/app.js")) {
    server_.serveStatic("/app.js", LittleFS, "/app.js", "max-age=60");
  }
  if (LittleFS.exists("/style.css")) {
    server_.serveStatic("/style.css", LittleFS, "/style.css", "max-age=60");
  }
  if (LittleFS.exists("/assets")) {
    server_.serveStatic("/assets", LittleFS, "/assets", "max-age=86400");
  }

  for (const char *legacyRoute : {"/history", "/settings", "/calibration", "/diag"}) {
    server_.on(legacyRoute, HTTP_GET, [this]() {
      server_.sendHeader("Location", "/");
      server_.send(302, "text/plain", "Moved to /");
    });
  }

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
    String outErr;
    if (!saveUpdatedConfig(candidate, outErr)) return server_.send(400, "text/plain", outErr);
    server_.send(200, "text/plain", "network saved");
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

  server_.onNotFound([this]() { server_.send(404, "text/plain", "not found"); });
}

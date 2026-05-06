#include "MqttManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
namespace {
constexpr uint16_t kMqttBufferSize = 1024;
}

MqttManager::MqttManager() : client_(wifiClient_) {}
void MqttManager::setWireGuardStateProvider(bool (*isOnlineFn)()) { wireGuardOnlineFn_ = isOnlineFn; }

void MqttManager::begin(const AppConfig &cfg) {
  cfg_ = cfg;
  client_.setServer(cfg_.mqtt.host.c_str(), cfg_.mqtt.port);
  client_.setBufferSize(kMqttBufferSize);
  Serial.printf("[MQTT] begin host=%s port=%u topicBase=%s buffer=%u enabled=%d\n", cfg_.mqtt.host.c_str(), cfg_.mqtt.port,
                cfg_.mqtt.topicBase.c_str(), kMqttBufferSize, cfg_.mqtt.enabled);
  client_.setCallback([](char *topic, uint8_t *payload, unsigned int length) {
    (void)payload;
    (void)length;
    const String t(topic);
    if (t.endsWith("/cmd/restart")) {
      delay(100);
      ESP.restart();
    }
  });
}

void MqttManager::reconnect(uint32_t nowMs) {
  if (!cfg_.mqtt.enabled) return;
  if (cfg_.mqtt.requireWireguard && wireGuardOnlineFn_ != nullptr && !wireGuardOnlineFn_()) {
    if (nowMs - lastReconnectTryMs_ >= 5000) {
      lastReconnectTryMs_ = nowMs;
      lastError_ = "wireguard requested but offline (fallback active)";
      Serial.println("[MQTT] requireWireguard=1 but tunnel offline; fallback via standard routing remains active");
    }
  }
  if (client_.connected()) return;
  if (nowMs - lastReconnectTryMs_ < 5000) return;
  lastReconnectTryMs_ = nowMs;

  const bool ok = client_.connect(cfg_.mqtt.clientId.c_str(), cfg_.mqtt.username.c_str(), cfg_.mqtt.password.c_str(),
                                  (cfg_.mqtt.topicBase + "/status").c_str(), 1, true, "offline");
  lastClientState_ = client_.state();
  if (!ok) {
    lastError_ = "connect failed";
    Serial.printf("[MQTT] connect failed state=%d host=%s port=%u clientId=%s\n", lastClientState_, cfg_.mqtt.host.c_str(),
                  cfg_.mqtt.port, cfg_.mqtt.clientId.c_str());
    return;
  }
  Serial.printf("[MQTT] connected state=%d\n", lastClientState_);
  const String statusTopic = cfg_.mqtt.topicBase + "/status";
  const String restartTopic = cfg_.mqtt.topicBase + "/cmd/restart";
  const bool onlineOk = client_.publish(statusTopic.c_str(), "online", true);
  const bool subOk = client_.subscribe(restartTopic.c_str());
  Serial.printf("[MQTT] publish online topic=%s ok=%d | subscribe %s ok=%d\n", statusTopic.c_str(), onlineOk,
                restartTopic.c_str(), subOk);
}

String MqttManager::stateToString(PressureState s) const {
  switch (s) {
    case PressureState::SENSOR_FAULT: return "SENSOR_FAULT";
    case PressureState::PRESSURE_LOW: return "PRESSURE_LOW";
    case PressureState::OK: return "PRESSURE_OK";
    case PressureState::PRESSURE_HIGH: return "PRESSURE_HIGH";
    default: return "PRESSURE_UNKNOWN";
  }
}

void MqttManager::loop(uint32_t nowMs) {
  reconnect(nowMs);
  client_.loop();
  const int st = client_.state();
  if (st != lastClientState_) {
    lastClientState_ = st;
    Serial.printf("[MQTT] state changed -> %d connected=%d\n", st, client_.connected());
  }
}

void MqttManager::publishReading(const PressureReading &reading, PressureState state, bool wifiConnected,
                                 uint32_t uptimeSec) {
  if (!cfg_.mqtt.enabled || !client_.connected()) return;
  if (cfg_.mqtt.requireWireguard && wireGuardOnlineFn_ != nullptr && !wireGuardOnlineFn_()) {
    lastError_ = "wireguard requested but offline (publishing via standard routing)";
    Serial.println("[MQTT] requireWireguard=1 but tunnel offline; publish continues via standard routing");
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastPublishMs_ < cfg_.mqtt.publishIntervalMs) return;
  lastPublishMs_ = nowMs;

  JsonDocument doc;
  doc["pressureBar"] = reading.pressureBar;
  doc["rawAdc"] = reading.rawAdc;
  doc["filteredAdc"] = reading.filteredAdc;
  doc["valid"] = reading.valid;
  doc["fault"] = static_cast<int>(reading.fault);
  doc["state"] = stateToString(state);
  doc["wifiConnected"] = wifiConnected;
  doc["uptimeSec"] = uptimeSec;

  String payload;
  serializeJson(doc, payload);
  const String telemetryTopic = cfg_.mqtt.topicBase + "/telemetry";
  const String stateTopic = cfg_.mqtt.topicBase + "/state";
  lastPublishTopic_ = telemetryTopic;
  lastPublishPayloadLen_ = payload.length();
  if (payload.length() >= client_.getBufferSize()) {
    Serial.printf("[MQTT] telemetry payload too large len=%u buffer=%u topic=%s\n", payload.length(), client_.getBufferSize(),
                  telemetryTopic.c_str());
  }
  const bool telOk = client_.publish(telemetryTopic.c_str(), payload.c_str(), false);
  const bool stateOk = client_.publish(stateTopic.c_str(), stateToString(state).c_str(), true);
  if (!telOk || !stateOk) {
    lastError_ = "publish failed";
    Serial.printf("[MQTT] publish failed telemetryOk=%d stateOk=%d topic=%s payloadLen=%u clientState=%d connected=%d\n",
                  telOk, stateOk, telemetryTopic.c_str(), payload.length(), client_.state(), client_.connected());
    return;
  }
  Serial.printf("[MQTT] publish ok topic=%s payloadLen=%u stateTopic=%s\n", telemetryTopic.c_str(), payload.length(),
                stateTopic.c_str());
}

String MqttManager::diagnosticsJson() const {
  JsonDocument doc;
  doc["connected"] = client_.connected();
  doc["clientState"] = client_.state();
  doc["lastError"] = lastError_;
  doc["lastPublishTopic"] = lastPublishTopic_;
  doc["lastPublishPayloadLen"] = lastPublishPayloadLen_;
  doc["bufferSize"] = client_.getBufferSize();
  doc["requireWireguard"] = cfg_.mqtt.requireWireguard;
  doc["wireguardOnline"] = wireGuardOnlineFn_ != nullptr ? wireGuardOnlineFn_() : false;
  String out;
  serializeJson(doc, out);
  return out;
}

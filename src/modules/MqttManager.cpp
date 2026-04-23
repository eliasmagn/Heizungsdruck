#include "MqttManager.h"

#include <ArduinoJson.h>

MqttManager::MqttManager() : client_(wifiClient_) {}

void MqttManager::begin(const AppConfig &cfg) {
  cfg_ = cfg;
  client_.setServer(cfg_.mqtt.host.c_str(), cfg_.mqtt.port);
}

void MqttManager::reconnect(uint32_t nowMs) {
  if (!cfg_.mqtt.enabled) return;
  if (client_.connected()) return;
  if (nowMs - lastReconnectTryMs_ < 5000) return;
  lastReconnectTryMs_ = nowMs;

  client_.connect(cfg_.mqtt.clientId.c_str(), cfg_.mqtt.username.c_str(), cfg_.mqtt.password.c_str(),
                  (cfg_.mqtt.topicBase + "/status").c_str(), 1, true, "offline");

  if (client_.connected()) {
    client_.publish((cfg_.mqtt.topicBase + "/status").c_str(), "online", true);
    client_.subscribe((cfg_.mqtt.topicBase + "/cmd/restart").c_str());
  }
}

String MqttManager::stateToString(PressureState s) const {
  switch (s) {
    case PressureState::SENSOR_FAULT: return "SENSOR_FAULT";
    case PressureState::LOW: return "PRESSURE_LOW";
    case PressureState::OK: return "PRESSURE_OK";
    case PressureState::HIGH: return "PRESSURE_HIGH";
    default: return "PRESSURE_UNKNOWN";
  }
}

void MqttManager::loop(uint32_t nowMs) {
  reconnect(nowMs);
  client_.loop();
}

void MqttManager::publishReading(const PressureReading &reading, PressureState state, bool wifiConnected,
                                 uint32_t uptimeSec) {
  if (!cfg_.mqtt.enabled || !client_.connected()) return;

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
  client_.publish((cfg_.mqtt.topicBase + "/telemetry").c_str(), payload.c_str(), false);
  client_.publish((cfg_.mqtt.topicBase + "/state").c_str(), stateToString(state).c_str(), true);
}

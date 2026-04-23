#pragma once

#include <WiFiClient.h>
#include <PubSubClient.h>

#include "AppConfig.h"
#include "PressureTypes.h"

class MqttManager {
 public:
  MqttManager();
  void begin(const AppConfig &cfg);
  void loop(uint32_t nowMs);
  void publishReading(const PressureReading &reading, PressureState state, bool wifiConnected, uint32_t uptimeSec);
  bool connected() const { return client_.connected(); }

 private:
  void reconnect(uint32_t nowMs);
  String stateToString(PressureState s) const;

  AppConfig cfg_;
  WiFiClient wifiClient_;
  PubSubClient client_;
  uint32_t lastReconnectTryMs_{0};
  uint32_t lastPublishMs_{0};
};

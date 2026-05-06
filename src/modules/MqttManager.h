#pragma once

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "AppConfig.h"
#include "PressureTypes.h"

class MqttManager {
 public:
  MqttManager();
  void begin(const AppConfig &cfg);
  void setWireGuardStateProvider(bool (*isOnlineFn)());
  void loop(uint32_t nowMs);
  void publishReading(const PressureReading &reading, PressureState state, bool wifiConnected, uint32_t uptimeSec);
  bool publishTestMessage(const String &note);
  bool connected() { return client_.connected(); }
  String diagnosticsJson();

 private:
  void reconnect(uint32_t nowMs);
  String stateToString(PressureState s) const;
  bool publishDiscoveryEntity(const String &component, const String &objectId, const String &name,
                              const JsonDocument &payload);
  void publishHomeAssistantDiscovery();
  String topicState() const;
  String topicTelemetry() const;
  String topicStatus() const;

  AppConfig cfg_;
  WiFiClient wifiClient_;
  PubSubClient client_;
  uint32_t lastReconnectTryMs_{0};
  uint32_t lastPublishMs_{0};
  int lastClientState_{0};
  String lastError_;
  String lastPublishTopic_;
  size_t lastPublishPayloadLen_{0};
  bool lastPublishOk_{false};
  uint32_t publishFailCount_{0};
  uint32_t publishOkCount_{0};
  bool discoveryPublished_{false};
  bool (*wireGuardOnlineFn_)() = nullptr;
};

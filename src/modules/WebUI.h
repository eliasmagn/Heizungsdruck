#pragma once

#include "platform_caps.h"
#if HAS_ASYNC_WEBUI
#include <ESPAsyncWebServer.h>
#endif

#include "AlarmManager.h"
#include "AppConfig.h"
#include "MqttManager.h"
#include "PressureHistory.h"
#include "PressureTypes.h"
#include "WireGuardManager.h"

class WebUI {
 public:
  explicit WebUI(uint16_t port = 80);

  void begin();
  void loop();

  void updateLiveData(const PressureReading &reading, PressureState state, bool wifiConnected, bool mqttConnected,
                      uint32_t uptimeSec);
  void attachConfig(AppConfig *cfg, bool (*saveFn)(const AppConfig &));
  void attachHistory(PressureHistory *history);
  void attachWireGuardManager(WireGuardManager *wireguard);
  void attachAlarmManager(AlarmManager *alarmManager);
  void attachMqttManager(MqttManager *mqttManager);

 private:
#if HAS_ASYNC_WEBUI
  String statusJson() const;
  String historyJson() const;
  String diagnosticsJson() const;
  String configJson() const;
  bool saveUpdatedConfig(const AppConfig &candidate, String &errorOut);
  void setupRoutes();
  void handleDeferredActions();

  AsyncWebServer server_;
  volatile bool pendingReboot_{false};
  volatile bool pendingTelegramTest_{false};
  volatile bool pendingWebhookTest_{false};
  volatile bool pendingMqttTest_{false};
#endif
  PressureReading lastReading_;
  PressureState lastState_{PressureState::UNKNOWN};
  bool wifiConnected_{false};
  bool mqttConnected_{false};
  uint32_t uptimeSec_{0};

  AppConfig *cfg_{nullptr};
  bool (*saveConfig_)(const AppConfig &) = nullptr;
  PressureHistory *history_{nullptr};
  WireGuardManager *wireguard_{nullptr};
  AlarmManager *alarmManager_{nullptr};
  MqttManager *mqttManager_{nullptr};
};

#pragma once

#include <WebServer.h>

#include "AppConfig.h"
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

 private:
  String statusJson() const;
  String historyJson() const;
  String diagnosticsJson() const;
  String configJson() const;
  bool saveUpdatedConfig(const AppConfig &candidate, String &errorOut);
  void setupRoutes();

  WebServer server_;
  PressureReading lastReading_;
  PressureState lastState_{PressureState::UNKNOWN};
  bool wifiConnected_{false};
  bool mqttConnected_{false};
  uint32_t uptimeSec_{0};

  AppConfig *cfg_{nullptr};
  bool (*saveConfig_)(const AppConfig &) = nullptr;
  PressureHistory *history_{nullptr};
  WireGuardManager *wireguard_{nullptr};
};

#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "AppConfig.h"
#include "PressureTypes.h"

class AlarmManager {
 public:
  AlarmManager() = default;

  void begin(const AppConfig &cfg);
  void updateConfig(const AppConfig &cfg);
  void loop(uint32_t nowMs, const PressureReading &reading, PressureState state);

 private:
  bool isAlarmState(PressureState state) const;
  void sendTelegram(const String &text);
  void sendWebhook(const PressureReading &reading, PressureState state, const String &event);

  AppConfig cfg_{};
  PressureState lastState_{PressureState::UNKNOWN};
  uint32_t lastAlertMs_{0};
  bool firstAlarmSent_{false};
};

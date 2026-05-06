#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "AppConfig.h"
#include "PressureTypes.h"

struct AlarmDispatchResult {
  bool ok;
  int httpStatus;
  String detail;
  AlarmDispatchResult() : ok(false), httpStatus(0), detail("") {}
  AlarmDispatchResult(bool okIn, int statusIn, const String &detailIn) : ok(okIn), httpStatus(statusIn), detail(detailIn) {}
};

class AlarmManager {
 public:
  AlarmManager() = default;
  void begin(const AppConfig &cfg);
  void updateConfig(const AppConfig &cfg);
  void loop(uint32_t nowMs, const PressureReading &reading, PressureState state);
  void attachConfigSaver(bool (*saveFn)(const AppConfig &));
  AlarmDispatchResult sendTelegramMessage(const String &text) const;
  AlarmDispatchResult sendWebhookTest(const PressureReading &reading, PressureState state, const String &event) const;
  AlarmDispatchResult lastTelegramResult() const { return lastTelegramResult_; }

 private:
  bool isAlarmState(PressureState state) const;
  void pollTelegramCommands(uint32_t nowMs);
  void handleTelegramCommand(const String &cmd);

  AppConfig cfg_{};
  PressureReading lastReading_{};
  PressureState lastState_{PressureState::UNKNOWN};
  uint32_t lastAlertMs_{0};
  bool firstAlarmSent_{false};
  uint32_t lastTelegramPollMs_{0};
  int64_t telegramUpdateOffset_{0};
  bool (*saveConfig_)(const AppConfig &) = nullptr;
  mutable AlarmDispatchResult lastTelegramResult_{};
};

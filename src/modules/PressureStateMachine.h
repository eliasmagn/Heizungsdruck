#pragma once

#include "AppConfig.h"
#include "PressureTypes.h"

class PressureStateMachine {
 public:
  explicit PressureStateMachine(const AppConfig &cfg);

  PressureState update(const PressureReading &reading);
  PressureState state() const { return state_; }
  void updateConfig(const AppConfig &cfg) { cfg_ = cfg; }

 private:
  AppConfig cfg_;
  PressureState state_{PressureState::UNKNOWN};
};

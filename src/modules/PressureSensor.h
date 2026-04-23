#pragma once

#include <vector>

#include "AppConfig.h"
#include "PressureMath.h"
#include "PressureTypes.h"

class PressureSensor {
 public:
  explicit PressureSensor(const AppConfig &cfg);

  void begin();
  PressureReading sample(uint32_t nowMs);
  void updateConfig(const AppConfig &cfg);

 private:
  bool classifyFault(const PressureReading &candidate, SensorFault &faultOut) const;

  AppConfig cfg_;
  PressureMath math_;
  float lastValidPressure_{0.0f};
  bool hasLastValid_{false};
};

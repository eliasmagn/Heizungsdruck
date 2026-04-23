#pragma once

#include <vector>
#include "AppConfig.h"

class PressureMath {
 public:
  explicit PressureMath(const AppConfig &cfg);

  int robustFilter(const std::vector<int> &samples) const;
  float adcToBar(int adc) const;
  float adcToVoltage(int adc) const;

 private:
  AppConfig cfg_;
};

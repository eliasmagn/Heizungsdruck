#include "PressureMath.h"

#include <algorithm>

PressureMath::PressureMath(const AppConfig &cfg) : cfg_(cfg) {}

int PressureMath::robustFilter(const std::vector<int> &samples) const {
  if (samples.empty()) return 0;

  std::vector<int> sorted = samples;
  std::sort(sorted.begin(), sorted.end());

  // median plus trimmed mean hybrid (lean and robust)
  const int median = sorted[sorted.size() / 2];

  if (sorted.size() < 5) return median;

  int sum = 0;
  int count = 0;
  for (size_t i = 1; i + 1 < sorted.size(); ++i) {
    sum += sorted[i];
    count++;
  }
  const int trimmedMean = count > 0 ? (sum / count) : median;
  return (median + trimmedMean) / 2;
}

float PressureMath::adcToBar(int adc) const {
  const auto &c = cfg_.calib;
  const float spanAdc = static_cast<float>(c.adcHigh - c.adcLow);
  const float spanBar = c.barHigh - c.barLow;
  if (spanAdc <= 1.0f) return 0.0f;

  float norm = static_cast<float>(adc - c.adcLow) / spanAdc;
  float bar = c.barLow + norm * spanBar + c.offsetBar;
  if (bar < -0.5f) bar = -0.5f;
  if (bar > 15.0f) bar = 15.0f;
  return bar;
}

float PressureMath::adcToVoltage(int adc) const {
  return (static_cast<float>(adc) / static_cast<float>(cfg_.sensor.adcMax)) * cfg_.sensor.adcVref;
}

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
  int first = -1;
  int second = -1;
  int prev = -1;
  int next = -1;
  for (size_t i = 0; i < cfg_.calib.points.size(); ++i) {
    if (!cfg_.calib.points[i].valid) continue;
    if (first < 0) first = static_cast<int>(i);
    second = static_cast<int>(i);
    const int pAdc = cfg_.calib.points[i].adc;
    if (pAdc <= adc) prev = static_cast<int>(i);
    if (pAdc >= adc && next < 0) next = static_cast<int>(i);
  }

  if (first >= 0 && second >= 0 && first != second) {
    const int loIdx = (prev >= 0) ? prev : first;
    const int hiIdx = (next >= 0) ? next : second;
    const auto &lo = cfg_.calib.points[loIdx];
    const auto &hi = cfg_.calib.points[hiIdx];
    if (hi.adc != lo.adc) {
      const float norm = static_cast<float>(adc - lo.adc) / static_cast<float>(hi.adc - lo.adc);
      float bar = lo.bar + norm * (hi.bar - lo.bar) + cfg_.calib.offsetBar;
      if (bar < -0.5f) bar = -0.5f;
      if (bar > 15.0f) bar = 15.0f;
      return bar;
    }
  }

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

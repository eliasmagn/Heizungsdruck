#include "PressureMath.h"

#include <algorithm>

PressureMath::PressureMath(const AppConfig &cfg) : cfg_(cfg) {}

int PressureMath::robustFilter(const std::vector<int> &samples) const {
  if (samples.empty()) return 0;
  std::vector<int> sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  const int median = sorted[sorted.size() / 2];
  if (sorted.size() < 5) return median;
  int sum = 0;
  int count = 0;
  for (size_t i = 1; i + 1 < sorted.size(); ++i) { sum += sorted[i]; count++; }
  const int trimmedMean = count > 0 ? (sum / count) : median;
  return (median + trimmedMean) / 2;
}

float PressureMath::adcToBar(int adc) const {
  std::vector<CalibrationConfig::Point> points;
  for (const auto &p : cfg_.calib.points) if (p.valid) points.push_back(p);
  if (points.size() >= 2) {
    std::sort(points.begin(), points.end(), [](const CalibrationConfig::Point &a, const CalibrationConfig::Point &b) { return a.adc < b.adc; });
    const auto *lo = &points.front();
    const auto *hi = &points.back();
    for (size_t i = 1; i < points.size(); ++i) {
      if (adc <= points[i].adc) { lo = &points[i - 1]; hi = &points[i]; break; }
    }
    if (hi->adc != lo->adc) {
      const float norm = static_cast<float>(adc - lo->adc) / static_cast<float>(hi->adc - lo->adc);
      float bar = lo->bar + norm * (hi->bar - lo->bar) + cfg_.calib.offsetBar;
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

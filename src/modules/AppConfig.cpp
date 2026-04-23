#include "AppConfig.h"

namespace {
void initCalibrationPoints(CalibrationConfig &calib) {
  for (size_t i = 0; i < calib.points.size(); ++i) {
    calib.points[i].bar = static_cast<float>(i) * 0.5f;
    calib.points[i].adc = 0;
    calib.points[i].valid = false;
  }
}
}  // namespace

AppConfig defaultConfig() {
  AppConfig cfg{};
  initCalibrationPoints(cfg.calib);
  return cfg;
}

bool AppConfig::validate(std::string &error) const {
  if (sensor.sampleCount < 3 || sensor.sampleCount > 31 || sensor.sampleCount % 2 == 0) {
    error = "sampleCount must be odd and in range 3..31";
    return false;
  }
  if (sensor.updateIntervalMs < 200 || sensor.updateIntervalMs > 10000) {
    error = "updateIntervalMs must be in range 200..10000";
    return false;
  }
  if (calib.adcLow >= calib.adcHigh) {
    error = "adcLow must be < adcHigh";
    return false;
  }
  if (calib.barLow >= calib.barHigh) {
    error = "barLow must be < barHigh";
    return false;
  }
  if (alarm.lowBar >= alarm.highBar) {
    error = "lowBar must be < highBar";
    return false;
  }
  if (alarm.repeatMinutes < 1 || alarm.repeatMinutes > 1440) {
    error = "repeatMinutes must be in range 1..1440";
    return false;
  }
  if (mqtt.publishIntervalMs < 1000 || mqtt.publishIntervalMs > 60000) {
    error = "publishIntervalMs must be in range 1000..60000";
    return false;
  }
  if (mqtt.topicBase.empty()) {
    error = "topicBase must not be empty";
    return false;
  }
  if (wireguard.enabled) {
    if (wireguard.statusUrl.empty() || wireguard.enableUrl.empty() || wireguard.disableUrl.empty()) {
      error = "wireguard urls must not be empty when enabled";
      return false;
    }
  }
  int validPointCount = 0;
  int lastAdc = -1;
  for (const auto &p : calib.points) {
    if (!p.valid) continue;
    if (p.adc < 0 || p.adc > sensor.adcMax) {
      error = "calibration point adc out of range";
      return false;
    }
    if (lastAdc >= 0 && p.adc < lastAdc) {
      error = "calibration points must be monotonic by bar index";
      return false;
    }
    lastAdc = p.adc;
    ++validPointCount;
  }
  if (validPointCount == 1) {
    error = "at least two calibration points required when using point calibration";
    return false;
  }

  error.clear();
  return true;
}

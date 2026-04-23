#include "AppConfig.h"

AppConfig defaultConfig() { return AppConfig{}; }

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
  if (mqtt.publishIntervalMs < 1000 || mqtt.publishIntervalMs > 60000) {
    error = "publishIntervalMs must be in range 1000..60000";
    return false;
  }
  if (mqtt.topicBase.empty()) {
    error = "topicBase must not be empty";
    return false;
  }
  error.clear();
  return true;
}

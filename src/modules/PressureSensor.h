#pragma once

#include <vector>
#include <map>
#include <OneWire.h>
#include <DallasTemperature.h>

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
  std::map<std::string,int> channelLastRaw_;
  std::map<std::string,int> channelLastFiltered_;
  OneWire oneWire_{4};
  DallasTemperature dallas_{&oneWire_};
  uint32_t lastTempReadMs_{0};
  float lastTempC_{0.0f};
  bool lastTempValid_{false};

 public:
  const std::map<std::string,int>& channelRaw() const { return channelLastRaw_; }
  const std::map<std::string,int>& channelFiltered() const { return channelLastFiltered_; }
};

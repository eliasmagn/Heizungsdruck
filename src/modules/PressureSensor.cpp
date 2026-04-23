#include "PressureSensor.h"

#include <Arduino.h>

PressureSensor::PressureSensor(const AppConfig &cfg) : cfg_(cfg), math_(cfg) {}

void PressureSensor::begin() {
  analogReadResolution(12);
  pinMode(cfg_.sensor.adcPin, INPUT);
}

void PressureSensor::updateConfig(const AppConfig &cfg) {
  cfg_ = cfg;
  math_ = PressureMath(cfg_);
}

bool PressureSensor::classifyFault(const PressureReading &candidate, SensorFault &faultOut) const {
  if (candidate.rawAdc <= cfg_.sensor.shortGndAdc) {
    faultOut = SensorFault::SHORT_GND;
    return true;
  }
  if (candidate.rawAdc >= cfg_.sensor.shortVccAdc) {
    faultOut = SensorFault::SHORT_VCC;
    return true;
  }
  if (candidate.rawAdc <= cfg_.sensor.disconnectAdc) {
    faultOut = SensorFault::DISCONNECTED;
    return true;
  }
  if (hasLastValid_ && fabsf(candidate.pressureBar - lastValidPressure_) > cfg_.sensor.maxJumpBar) {
    faultOut = SensorFault::IMPLAUSIBLE_JUMP;
    return true;
  }
  return false;
}

PressureReading PressureSensor::sample(uint32_t nowMs) {
  std::vector<int> samples;
  samples.reserve(cfg_.sensor.sampleCount);

  for (uint16_t i = 0; i < cfg_.sensor.sampleCount; ++i) {
    samples.push_back(analogRead(cfg_.sensor.adcPin));
    delayMicroseconds(500);
  }

  PressureReading r;
  r.timestampMs = nowMs;
  r.rawAdc = samples[samples.size() / 2];
  r.filteredAdc = math_.robustFilter(samples);
  r.voltage = math_.adcToVoltage(r.filteredAdc);
  r.pressureBar = math_.adcToBar(r.filteredAdc);
  r.valid = true;

  SensorFault fault = SensorFault::NONE;
  if (classifyFault(r, fault)) {
    r.valid = false;
    r.fault = fault;
    if (hasLastValid_) {
      r.pressureBar = lastValidPressure_;
    }
  } else {
    r.fault = SensorFault::NONE;
    lastValidPressure_ = r.pressureBar;
    hasLastValid_ = true;
  }

  return r;
}

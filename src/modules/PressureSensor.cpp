#include "PressureSensor.h"

#include <Arduino.h>

PressureSensor::PressureSensor(const AppConfig &cfg) : cfg_(cfg), math_(cfg) {}

void PressureSensor::begin() {
  analogReadResolution(12);
  for (const auto &ch : cfg_.sensor.analogChannels) pinMode(ch.adcPin, INPUT);
  if (cfg_.sensor.temperature.enabled) {
    oneWire_ = OneWire(cfg_.sensor.temperature.oneWirePin);
    dallas_ = DallasTemperature(&oneWire_);
    dallas_.begin();
  }
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
  std::vector<int> pressureSamples;
  pressureSamples.reserve(cfg_.sensor.sampleCount);
  channelLastRaw_.clear();
  channelLastFiltered_.clear();
  uint8_t pressurePin = cfg_.sensor.adcPin;
  for (const auto &ch : cfg_.sensor.analogChannels) if (ch.pressureSource) { pressurePin = ch.adcPin; break; }
  for (const auto &ch : cfg_.sensor.analogChannels) {
    std::vector<int> samples; samples.reserve(cfg_.sensor.sampleCount);
    for (uint16_t i=0;i<cfg_.sensor.sampleCount;++i){ samples.push_back(analogRead(ch.adcPin)); }
    channelLastRaw_[ch.id]=samples[samples.size()/2];
    channelLastFiltered_[ch.id]=math_.robustFilter(samples);
    if (ch.adcPin==pressurePin) pressureSamples = samples;
  }

  PressureReading r;
  r.timestampMs = nowMs;
  r.rawAdc = pressureSamples.empty()?0:pressureSamples[pressureSamples.size() / 2];
  r.filteredAdc = math_.robustFilter(pressureSamples);
  r.voltage = math_.adcToVoltage(r.filteredAdc);
  r.pressureBar = math_.adcToBar(r.filteredAdc);
  r.valid = true;
  if (cfg_.sensor.temperature.enabled && nowMs - lastTempReadMs_ >= cfg_.sensor.temperature.updateIntervalMs) {
    dallas_.requestTemperatures();
    float t = dallas_.getTempCByIndex(0);
    lastTempReadMs_ = nowMs;
    if (t > -100.0f && t < 150.0f) { lastTempC_ = t; lastTempValid_ = true; } else { lastTempValid_ = false; }
  }
  r.temperatureC = lastTempC_;
  r.temperatureValid = lastTempValid_;

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

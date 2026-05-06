#include "PressureSensor.h"

#include <Arduino.h>

#if __has_include("driver/adc.h") && __has_include("esp_adc/adc_continuous.h")
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "soc/soc_caps.h"
#define HAS_ADC_DMA 1
#else
#define HAS_ADC_DMA 0
#endif

namespace {
#if HAS_ADC_DMA
bool adcUnitChannelFromPin(uint8_t pin, adc_unit_t &unitOut, adc_channel_t &channelOut) {
  int ch = digitalPinToAnalogChannel(pin);
  if (ch < 0) return false;
  if (ch <= ADC_CHANNEL_7) {
    unitOut = ADC_UNIT_1;
    channelOut = static_cast<adc_channel_t>(ch);
    return true;
  }
#if SOC_ADC_PERIPH_NUM >= 2
  unitOut = ADC_UNIT_2;
  channelOut = static_cast<adc_channel_t>(ch - 10);
  return true;
#else
  return false;
#endif
}

std::vector<int> readAdcSamplesDma(uint8_t pin, uint16_t sampleCount) {
  std::vector<int> out;
  out.reserve(sampleCount);

  adc_unit_t unit = ADC_UNIT_1;
  adc_channel_t channel = ADC_CHANNEL_0;
  if (!adcUnitChannelFromPin(pin, unit, channel)) return out;

  adc_continuous_handle_t handle = nullptr;
  adc_continuous_handle_cfg_t handleCfg{};
  handleCfg.max_store_buf_size = 1024;
  handleCfg.conv_frame_size = 256;
  if (adc_continuous_new_handle(&handleCfg, &handle) != ESP_OK) return out;

  adc_digi_pattern_config_t pattern{};
  pattern.atten = ADC_ATTEN_DB_11;
  pattern.channel = channel;
  pattern.unit = unit;
  pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  adc_continuous_config_t digCfg{};
  digCfg.pattern_num = 1;
  digCfg.adc_pattern = &pattern;
  digCfg.sample_freq_hz = 20000;
  digCfg.conv_mode = (unit == ADC_UNIT_1) ? ADC_CONV_SINGLE_UNIT_1 : ADC_CONV_SINGLE_UNIT_2;
  digCfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
  if (adc_continuous_config(handle, &digCfg) != ESP_OK) {
    adc_continuous_deinit(handle);
    return out;
  }

  if (adc_continuous_start(handle) != ESP_OK) {
    adc_continuous_deinit(handle);
    return out;
  }

  uint8_t raw[256];
  while (out.size() < sampleCount) {
    uint32_t bytesRead = 0;
    esp_err_t err = adc_continuous_read(handle, raw, sizeof(raw), &bytesRead, 20);
    if (err != ESP_OK) continue;
    for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= bytesRead; i += SOC_ADC_DIGI_RESULT_BYTES) {
      auto *res = reinterpret_cast<adc_digi_output_data_t *>(&raw[i]);
      out.push_back(static_cast<int>(res->type1.data));
      if (out.size() >= sampleCount) break;
    }
  }

  adc_continuous_stop(handle);
  adc_continuous_deinit(handle);
  return out;
}
#endif
}  // namespace

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
    #if HAS_ADC_DMA
    samples = readAdcSamplesDma(ch.adcPin, cfg_.sensor.sampleCount);
    #endif
    if (samples.empty()) {
      for (uint16_t i=0;i<cfg_.sensor.sampleCount;++i){ samples.push_back(analogRead(ch.adcPin)); }
    }
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
  r.channelRaw = channelLastRaw_;
  r.channelFiltered = channelLastFiltered_;

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

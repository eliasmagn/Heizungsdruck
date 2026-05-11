#include "PressureSensor.h"

#include <Arduino.h>
#include "../platform_caps.h"
#include <math.h>
#include <algorithm>

#if __has_include("driver/adc.h") && __has_include("esp_adc/adc_continuous.h")
#include "driver/adc.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "soc/soc_caps.h"
#define HAS_ADC_DMA HAS_ADC_CONTINUOUS
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


static float readNtcC(int adc, const NtcConfig &ntc, int adcMax) {
  if (adc <= 0 || adc >= adcMax) return 0.0f;
  const float rNtc = ntc.seriesResistorOhm * (static_cast<float>(adc) / static_cast<float>(adcMax - adc));
  const float t0 = ntc.nominalTempC + 273.15f;
  const float invT = (1.0f / t0) + (logf(rNtc / ntc.nominalResistorOhm) / ntc.beta);
  return (1.0f / invT) - 273.15f + ntc.offsetC;
}
PressureSensor::PressureSensor(const AppConfig &cfg) : cfg_(cfg), math_(cfg) {}

void PressureSensor::initForCurrentConfig() {
#if defined(ESP32)
  analogReadResolution(12);
#endif
  for (const auto &ch : cfg_.sensor.analogChannels) pinMode(ch.adcPin, INPUT);
  if (cfg_.sensor.temperature.enabled && cfg_.sensor.temperature.mode == TemperatureMode::DS18B20) {
    oneWire_ = OneWire(cfg_.sensor.temperature.oneWirePin);
    dallas_ = DallasTemperature(&oneWire_);
    dallas_.begin();
  }
  lastTempReadMs_ = 0;
  lastTempValid_ = false;
}

void PressureSensor::begin() { initForCurrentConfig(); }

void PressureSensor::updateConfig(const AppConfig &cfg) {
  cfg_ = cfg;
  math_ = PressureMath(cfg_);
  initForCurrentConfig();
  hasLastValid_ = false;
  channelLastRaw_.clear();
  channelLastFiltered_.clear();
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
  auto readChannelSamples = [&](uint8_t pin) {
    std::vector<int> samples;
    samples.reserve(cfg_.sensor.sampleCount);
  #if HAS_ADC_DMA
    samples = readAdcSamplesDma(pin, cfg_.sensor.sampleCount);
  #endif
    if (samples.empty()) {
      for (uint16_t i = 0; i < cfg_.sensor.sampleCount; ++i) {
        samples.push_back(analogRead(pin));
      }
    }
    return samples;
  };

  std::vector<int> pressureSamples;
  pressureSamples.reserve(cfg_.sensor.sampleCount);
  channelLastRaw_.clear();
  channelLastFiltered_.clear();
  uint8_t pressurePin = cfg_.sensor.adcPin;
  std::string pressureChannelId = "pressure_main";
  String noiseId="";
  bool applyNoiseCompensation = false;
  for (const auto &ch : cfg_.sensor.analogChannels) {
    if (ch.pressureSource || ch.role==AnalogChannelRole::PRESSURE) { pressurePin = ch.adcPin; pressureChannelId = ch.id; break; }
  }
  bool pressureUseGlobalNoiseRef = true;
  for (const auto &ch : cfg_.sensor.analogChannels) {
    if (ch.id == pressureChannelId) {
      pressureUseGlobalNoiseRef = ch.useGlobalNoiseRef;
      break;
    }
  }
  for (const auto &ch : cfg_.sensor.analogChannels) {
    if (ch.role==AnalogChannelRole::NOISE_REF) { noiseId = ch.id.c_str(); break; }
  }
  applyNoiseCompensation = pressureUseGlobalNoiseRef && noiseId.length() > 0;
  for (const auto &ch : cfg_.sensor.analogChannels) {
    std::vector<int> samples = readChannelSamples(ch.adcPin);
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
  r.compensatedAdc = r.filteredAdc;
  r.valid = true;
  if (cfg_.sensor.temperature.enabled && nowMs - lastTempReadMs_ >= cfg_.sensor.temperature.updateIntervalMs) {
    lastTempReadMs_ = nowMs;
    if (cfg_.sensor.temperature.mode == TemperatureMode::DS18B20) {
      dallas_.requestTemperatures();
      float t = dallas_.getTempCByIndex(0);
      if (t > -100.0f && t < 150.0f) { lastTempC_ = t; lastTempValid_ = true; } else { lastTempValid_ = false; }
    } else if (cfg_.sensor.temperature.mode == TemperatureMode::NTC) {
      std::vector<int> ntcSamples = readChannelSamples(cfg_.sensor.temperature.ntc.adcPin);
      const int ntcFilteredAdc = math_.robustFilter(ntcSamples);
      lastTempC_ = readNtcC(ntcFilteredAdc, cfg_.sensor.temperature.ntc, cfg_.sensor.adcMax);
      lastTempValid_ = (lastTempC_ > -50.0f && lastTempC_ < 150.0f);
    } else { lastTempValid_ = false; }
  }
  r.temperatureC = lastTempC_;
  r.temperatureValid = lastTempValid_;
  r.channelRaw = channelLastRaw_;
  r.channelFiltered = channelLastFiltered_;
  if (applyNoiseCompensation && channelLastRaw_.count(noiseId.c_str())) {
    r.hasNoiseRef = true;
    r.noiseRawAdc = channelLastRaw_[noiseId.c_str()];
    r.noiseFilteredAdc = channelLastFiltered_[noiseId.c_str()];
    for (const auto &it: channelLastFiltered_) {
      int comp = it.second - r.noiseFilteredAdc;
      r.channelCompensated[it.first] = comp;
      if (it.first == pressureChannelId) r.compensatedAdc = comp;
    }
  }

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

#include "AppConfig.h"

#include <algorithm>

namespace {
std::string trim(const std::string &value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

bool isAllowedDeviceIdChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
}

std::string normalizeDeviceId(std::string deviceId) {
  std::transform(deviceId.begin(), deviceId.end(), deviceId.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
  return trim(deviceId);
}

void initCalibrationPoints(CalibrationConfig &calib) { calib.points.clear(); }

void ensureAnalogChannels(SensorConfig &sensor) {
  if (sensor.analogChannels.empty()) {
    AnalogChannelConfig ch;
    ch.id = "pressure_main";
    ch.adcPin = sensor.adcPin;
    ch.pressureSource = true;
    ch.role = AnalogChannelRole::PRESSURE;
    sensor.analogChannels.push_back(ch);
  }
}

}  // namespace

AppConfig defaultConfig() {
  AppConfig cfg{};
#if defined(ESP8266)
  cfg.sensor.adcPin = A0;
  cfg.sensor.adcMax = 1023;
  cfg.sensor.shortVccAdc = 1010;
  cfg.sensor.temperature.enabled = false;
  cfg.sensor.temperature.mode = TemperatureMode::NONE;
  cfg.sensor.slim.sharedAdcFrontend = SlimSharedAdcFrontend::NONE;
  cfg.sensor.slim.bootSensorSelection = SlimBootSensorSelection::PRESSURE;
  cfg.sensor.temperature.ntc.adcPin = A0;
  cfg.sensor.analogChannels.clear();
  AnalogChannelConfig pressure;
  pressure.id = "pressure_main";
  pressure.adcPin = A0;
  pressure.pressureSource = true;
  pressure.role = AnalogChannelRole::PRESSURE;
  cfg.sensor.analogChannels.push_back(pressure);
#else
  cfg.sensor.adcPin = 34;
  cfg.sensor.adcMax = 4095;
  cfg.sensor.temperature.ntc.adcPin = 35;
#endif
  initCalibrationPoints(cfg.calib);
  ensureAnalogChannels(cfg.sensor);
  return cfg;
}

bool AppConfig::validate(std::string &error) const {
  const bool sharedFrontendConfigured = sensor.slim.sharedAdcFrontend != SlimSharedAdcFrontend::NONE;
  if (sharedFrontendConfigured) {
    error = "sharedAdcFrontend is currently persisted for future use, but not supported in runtime yet";
    return false;
  }
  if (!sharedFrontendConfigured && sensor.slim.bootSensorSelection != SlimBootSensorSelection::PRESSURE) {
    error = "bootSensorSelection=temperature requires sharedAdcFrontend";
    return false;
  }

  if (sensor.sampleCount < 3 || sensor.sampleCount > 31 || sensor.sampleCount % 2 == 0) {
    error = "sampleCount must be odd and in range 3..31";
    return false;
  }
  if (sensor.updateIntervalMs < 20 || sensor.updateIntervalMs > 10000) {
    error = "updateIntervalMs must be in range 20..10000";
    return false;
  }
  if (sensor.analogChannels.empty()) { error = "at least one analog channel required"; return false; }

  if (sensor.temperature.enabled && sensor.temperature.mode == TemperatureMode::NTC) {
    if (sensor.temperature.ntc.seriesResistorOhm <= 0.0f || sensor.temperature.ntc.nominalResistorOhm <= 0.0f ||
        sensor.temperature.ntc.beta <= 0.0f) {
      error = "NTC parameters must be > 0";
      return false;
    }
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
  const std::string normalizedId = normalizeDeviceId(deviceId);
  if (normalizedId.empty()) {
    error = "deviceId must not be empty";
    return false;
  }
  if (normalizedId.size() > 32) {
    error = "deviceId max length is 32";
    return false;
  }
  for (const char c : normalizedId) {
    if (!isAllowedDeviceIdChar(c)) {
      error = "deviceId allows only [a-z0-9_-]";
      return false;
    }
  }
  if (network.hostname.empty()) {
    error = "hostname must not be empty";
    return false;
  }
  if (mqtt.clientId.empty()) {
    error = "mqtt clientId must not be empty";
    return false;
  }
  if (network.wifiTxPowerDbm < 2.0f || network.wifiTxPowerDbm > 20.5f) {
    error = "wifiTxPowerDbm must be in range 2.0..20.5";
    return false;
  }
  if (wireguard.enabled) {
    if (wireguard.localAddress.empty()) {
      error = "wireguard localAddress must not be empty when enabled";
      return false;
    }
    if (wireguard.privateKey.empty()) {
      error = "wireguard privateKey must not be empty when enabled";
      return false;
    }
    if (wireguard.peerEndpoint.empty()) {
      error = "wireguard peerEndpoint must not be empty when enabled";
      return false;
    }
    if (wireguard.peerPort == 0) {
      error = "wireguard peerPort must not be 0 when enabled";
      return false;
    }
    if (wireguard.peerPublicKey.empty()) {
      error = "wireguard peerPublicKey must not be empty when enabled";
      return false;
    }
    if (wireguard.allowedIp1.empty() && wireguard.allowedIp2.empty()) {
      error = "wireguard requires at least one allowed IP when enabled";
      return false;
    }
  }
  if (calib.points.size() > CalibrationConfig::kMaxPointCount) {
    error = "too many calibration points";
    return false;
  }
  int validPointCount = 0;
  int pressureSourceCount = 0;
  int noiseRefCount = 0;
  bool hasAnyUseGlobalNoiseRef = false;
  bool pressureChannelUsesGlobalNoiseRef = false;
  std::vector<std::string> ids;
  std::vector<uint8_t> pins;
  for (const auto &ch : sensor.analogChannels) {
    if (trim(ch.id).empty()) { error = "analog channel id must not be empty"; return false; }
    if (std::find(ids.begin(), ids.end(), ch.id) != ids.end()) { error = "analog channel ids must be unique"; return false; }
    ids.push_back(ch.id);
    if (std::find(pins.begin(), pins.end(), ch.adcPin) != pins.end() && !sharedFrontendConfigured) {
      error = "duplicate adcPin across analogChannels requires sharedAdcFrontend";
      return false;
    }
    if (ch.role == AnalogChannelRole::TEMPERATURE_NTC && ch.adcPin != sensor.temperature.ntc.adcPin) {
      error = "TEMPERATURE_NTC channel must use sensor.temperature.ntc.adcPin";
      return false;
    }
    if (ch.role == AnalogChannelRole::NOISE_REF && ch.pressureSource) {
      error = "NOISE_REF channel cannot be pressureSource";
      return false;
    }
    pins.push_back(ch.adcPin);
    if (ch.pressureSource || ch.role == AnalogChannelRole::PRESSURE) {
      pressureSourceCount++;
      if (ch.useGlobalNoiseRef) pressureChannelUsesGlobalNoiseRef = true;
    }
    if (ch.role == AnalogChannelRole::NOISE_REF) noiseRefCount++;
    if (ch.useGlobalNoiseRef) hasAnyUseGlobalNoiseRef = true;
  }
  if (pressureSourceCount != 1) {
    error = "exactly one analog channel must be pressureSource/PRESSURE";
    return false;
  }
  if (noiseRefCount > 1) {
    error = "at most one NOISE_REF channel is allowed";
    return false;
  }
  if (hasAnyUseGlobalNoiseRef && noiseRefCount == 0) {
    error = "useGlobalNoiseRef requires exactly one NOISE_REF channel";
    return false;
  }
  if (hasAnyUseGlobalNoiseRef && !pressureChannelUsesGlobalNoiseRef) {
    error = "useGlobalNoiseRef must be set on pressure source channel";
    return false;
  }
  if (sensor.temperature.enabled && sensor.temperature.mode == TemperatureMode::NTC) {
    int pressurePin = -1;
    for (const auto &ch : sensor.analogChannels) {
      if (ch.pressureSource || ch.role == AnalogChannelRole::PRESSURE) {
        pressurePin = ch.adcPin;
        break;
      }
    }
    if (pressurePin >= 0 && pressurePin == sensor.temperature.ntc.adcPin && !sharedFrontendConfigured) {
      error = "pressure+NTC on same ADC pin requires sharedAdcFrontend";
      return false;
    }
  }
#if defined(ESP8266)
  if (sensor.analogChannels.size() > 1) {
    error = "ESP8266 supports only one analog channel (A0) in slim profile";
    return false;
  }
  for (const auto &ch : sensor.analogChannels) {
    if (ch.adcPin != A0) {
      error = "ESP8266 analog channel pin must be A0";
      return false;
    }
    if (ch.role == AnalogChannelRole::NOISE_REF) {
      error = "ESP8266 slim profile does not support dedicated noise_ref channel by default";
      return false;
    }
  }
#endif
  for (const auto &p : calib.points) {
    if (!p.valid) continue;
    if (p.adc < 0 || p.adc > sensor.adcMax) {
      error = "calibration point adc out of range";
      return false;
    }
    ++validPointCount;
  }
  if (validPointCount == 1) {
    error = "at least two calibration points required when using point calibration";
    return false;
  }

  error.clear();
  return true;
}

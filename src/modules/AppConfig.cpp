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
  initCalibrationPoints(cfg.calib);
  ensureAnalogChannels(cfg.sensor);
  return cfg;
}

bool AppConfig::validate(std::string &error) const {
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
  bool hasPressureSource=false;
  for (const auto &ch : sensor.analogChannels) {
    if (ch.pressureSource) hasPressureSource=true;
  }
  if (!hasPressureSource) { error = "one analog channel must be pressureSource"; return false; }
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

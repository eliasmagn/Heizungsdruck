#pragma once

#include <stdint.h>
#include <string>
#include <vector>

enum class AnalogChannelRole : uint8_t { PRESSURE = 0, NOISE_REF = 1, AUX = 2, TEMPERATURE_NTC = 3 };

struct AnalogChannelConfig {
  std::string id{"pressure_main"};
  uint8_t adcPin{34};
  AnalogChannelRole role{AnalogChannelRole::AUX};
  bool pressureSource{false};
  bool useGlobalNoiseRef{true};
};

enum class TemperatureMode : uint8_t { NONE = 0, NTC = 1, DS18B20 = 2 };

enum class SlimSharedAdcFrontend : uint8_t { NONE = 0, ADS1115 = 1, ADS1015 = 2, TLA2024 = 3, CD4051_MUX = 4, TCA9548A = 5 };
enum class SlimBootSensorSelection : uint8_t { PRESSURE = 0, TEMPERATURE = 1 };

struct SlimProfileConfig {
  SlimSharedAdcFrontend sharedAdcFrontend{SlimSharedAdcFrontend::NONE};
  SlimBootSensorSelection bootSensorSelection{SlimBootSensorSelection::PRESSURE};
};

struct NtcConfig {
  uint8_t adcPin{35};
  float seriesResistorOhm{10000.0f};
  float nominalResistorOhm{10000.0f};
  float beta{3950.0f};
  float nominalTempC{25.0f};
  float offsetC{0.0f};
  std::string sensorId{"ntc_main"};
};

struct TemperatureConfig {
  bool enabled{true};
  TemperatureMode mode{TemperatureMode::NTC};
  uint8_t oneWirePin{4};
  uint32_t updateIntervalMs{2000};
  NtcConfig ntc;
};

struct SensorConfig {
  uint8_t adcPin{34};
  uint16_t sampleCount{9};
  uint32_t updateIntervalMs{100};
  float adcVref{3.3f};
  int adcMax{4095};
  int disconnectAdc{80};
  int shortGndAdc{20};
  int shortVccAdc{4070};
  float maxJumpBar{0.7f};
  std::vector<AnalogChannelConfig> analogChannels;
  TemperatureConfig temperature;
  SlimProfileConfig slim;
};

struct CalibrationConfig {
  int adcLow{400};
  int adcHigh{3800};
  float barLow{0.0f};
  float barHigh{10.0f};
  float offsetBar{0.0f};
  static constexpr size_t kMaxPointCount = 20;
  struct Point {
    float bar{0.0f};
    int adc{0};
    bool valid{false};
  };
  std::vector<Point> points;
};

struct AlarmConfig { float lowBar{1.0f}; float highBar{2.2f}; float hysteresisBar{0.1f}; uint16_t repeatMinutes{30}; std::string telegramBotToken; std::string telegramChatId; std::string emailWebhookUrl; };
struct MqttConfig { bool enabled{false}; std::string host{"192.168.1.50"}; uint16_t port{1883}; std::string username; std::string password; std::string clientId{"heizungsdruck"}; std::string topicBase{"heizungsdruck"}; uint32_t publishIntervalMs{10000}; bool requireWireguard{false}; };
struct NetworkConfig { std::string wifiSsid; std::string wifiPassword; std::string apSsid{"Heizungsdruck-Setup"}; std::string apPassword; std::string hostname{"heizungsdruck"}; float wifiTxPowerDbm{8.5f}; bool wifi11bMode{true}; };
struct WireGuardConfig { bool enabled{false}; std::string localAddress; std::string netmask{"255.255.255.0"}; std::string privateKey; std::string peerEndpoint; uint16_t peerPort{0}; std::string peerPublicKey; std::string presharedKey; std::string allowedIp1; std::string allowedIp2; uint16_t keepAliveSeconds{0}; };

struct AppConfig {
  SensorConfig sensor;
  CalibrationConfig calib;
  AlarmConfig alarm;
  MqttConfig mqtt;
  NetworkConfig network;
  WireGuardConfig wireguard;
  std::string deviceId{"kreis1"};
  bool validate(std::string &error) const;
};
AppConfig defaultConfig();

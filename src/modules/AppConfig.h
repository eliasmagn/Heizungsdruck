#pragma once

#include <stdint.h>
#include <string>

struct SensorConfig {
  uint8_t adcPin{34};
  uint16_t sampleCount{9};
  uint32_t updateIntervalMs{1000};
  float adcVref{3.3f};
  int adcMax{4095};
  int disconnectAdc{80};
  int shortGndAdc{20};
  int shortVccAdc{4070};
  float maxJumpBar{0.7f};
};

struct CalibrationConfig {
  int adcLow{400};
  int adcHigh{3800};
  float barLow{0.0f};
  float barHigh{10.0f};
  float offsetBar{0.0f};
};

struct AlarmConfig {
  float lowBar{1.0f};
  float highBar{2.2f};
  float hysteresisBar{0.1f};
};

struct MqttConfig {
  bool enabled{false};
  std::string host{"192.168.1.50"};
  uint16_t port{1883};
  std::string username;
  std::string password;
  std::string clientId{"heizungsdruck"};
  std::string topicBase{"heizungsdruck"};
  uint32_t publishIntervalMs{10000};
};

struct NetworkConfig {
  std::string hostname{"heizungsdruck"};
};

struct AppConfig {
  SensorConfig sensor;
  CalibrationConfig calib;
  AlarmConfig alarm;
  MqttConfig mqtt;
  NetworkConfig network;

  bool validate(std::string &error) const;
};

AppConfig defaultConfig();

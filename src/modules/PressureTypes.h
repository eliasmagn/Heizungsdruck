#pragma once

#include <stdint.h>
#include <map>
#include <string>

enum class PressureState : uint8_t {
  UNKNOWN = 0,
  SENSOR_FAULT = 1,
  PRESSURE_LOW = 2,
  OK = 3,
  PRESSURE_HIGH = 4,
};

enum class SensorFault : uint8_t {
  NONE = 0,
  DISCONNECTED = 1,
  SHORT_GND = 2,
  SHORT_VCC = 3,
  IMPLAUSIBLE_JUMP = 4,
};

struct PressureReading {
  uint32_t timestampMs{0};
  int rawAdc{0};
  int filteredAdc{0};
  float voltage{0.0f};
  float pressureBar{0.0f};
  bool valid{false};
  SensorFault fault{SensorFault::NONE};
  float temperatureC{0.0f};
  bool temperatureValid{false};
  float pressureDrift1h{0.0f};
  float pressureDrift24h{0.0f};
  bool pressureDrift1hValid{false};
  bool pressureDrift24hValid{false};
  float temperatureDrift1h{0.0f};
  float temperatureDrift24h{0.0f};
  bool temperatureDrift1hValid{false};
  bool temperatureDrift24hValid{false};
  std::map<std::string, int> channelRaw;
  std::map<std::string, int> channelFiltered;
};

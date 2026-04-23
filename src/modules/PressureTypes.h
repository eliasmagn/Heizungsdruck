#pragma once

#include <stdint.h>

enum class PressureState : uint8_t {
  UNKNOWN = 0,
  SENSOR_FAULT = 1,
  LOW = 2,
  OK = 3,
  HIGH = 4,
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
};

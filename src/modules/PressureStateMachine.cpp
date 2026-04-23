#include "PressureStateMachine.h"

PressureStateMachine::PressureStateMachine(const AppConfig &cfg) : cfg_(cfg) {}

PressureState PressureStateMachine::update(const PressureReading &reading) {
  if (!reading.valid || reading.fault != SensorFault::NONE) {
    state_ = PressureState::SENSOR_FAULT;
    return state_;
  }

  const float low = cfg_.alarm.lowBar;
  const float high = cfg_.alarm.highBar;
  const float h = cfg_.alarm.hysteresisBar;

  switch (state_) {
    case PressureState::LOW:
      if (reading.pressureBar >= low + h) state_ = PressureState::OK;
      break;
    case PressureState::HIGH:
      if (reading.pressureBar <= high - h) state_ = PressureState::OK;
      break;
    case PressureState::OK:
      if (reading.pressureBar < low - h) state_ = PressureState::LOW;
      else if (reading.pressureBar > high + h) state_ = PressureState::HIGH;
      break;
    case PressureState::UNKNOWN:
    case PressureState::SENSOR_FAULT:
    default:
      if (reading.pressureBar < low) state_ = PressureState::LOW;
      else if (reading.pressureBar > high) state_ = PressureState::HIGH;
      else state_ = PressureState::OK;
      break;
  }

  return state_;
}

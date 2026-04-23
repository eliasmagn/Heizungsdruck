#include "PressureHistory.h"

PressureHistory::PressureHistory(size_t capacity) : capacity_(capacity) { ring_.reserve(capacity); }

void PressureHistory::add(const PressureReading &reading, PressureState state) {
  if (ring_.size() >= capacity_) {
    ring_.erase(ring_.begin());
  }
  ring_.push_back({reading.timestampMs, reading.pressureBar, static_cast<uint8_t>(state), reading.valid});
}

std::vector<PressureHistory::Entry> PressureHistory::entries() const { return ring_; }

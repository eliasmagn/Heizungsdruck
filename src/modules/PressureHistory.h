#pragma once

#include <vector>

#include "PressureTypes.h"

class PressureHistory {
 public:
  explicit PressureHistory(size_t capacity = 180);
  void add(const PressureReading &reading, PressureState state);
  void applyDrift(PressureReading &reading, uint32_t nowMs);

  struct Entry {
    uint32_t ts;
    float bar;
    uint8_t state;
    bool valid;
  };

  std::vector<Entry> entries() const;

 private:
  struct DriftSnapshot {
    uint32_t ts;
    float pressureBar;
    bool pressureValid;
    float temperatureC;
    bool temperatureValid;
  };

  const DriftSnapshot *findSnapshotBefore(uint32_t targetMs) const;

  static const uint32_t kDriftSnapshotIntervalMs = 5UL * 60UL * 1000UL;
  static const size_t kDriftCapacity = 289;  // 24h @ 5min + current edge
  size_t capacity_;
  std::vector<Entry> ring_;
  std::vector<DriftSnapshot> driftRing_;
  uint32_t lastDriftSnapshotMs_{0};
};

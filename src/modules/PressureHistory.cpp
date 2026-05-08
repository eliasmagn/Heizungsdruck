#include "PressureHistory.h"

PressureHistory::PressureHistory(size_t capacity) : capacity_(capacity) { ring_.reserve(capacity); }

void PressureHistory::add(const PressureReading &reading, PressureState state) {
  if (ring_.size() >= capacity_) {
    ring_.erase(ring_.begin());
  }
  ring_.push_back({reading.timestampMs, reading.pressureBar, static_cast<uint8_t>(state), reading.valid});

  if (lastDriftSnapshotMs_ != 0 && (reading.timestampMs - lastDriftSnapshotMs_) < kDriftSnapshotIntervalMs) return;
  lastDriftSnapshotMs_ = reading.timestampMs;
  if (driftRing_.size() >= kDriftCapacity) driftRing_.erase(driftRing_.begin());
  driftRing_.push_back({reading.timestampMs, reading.pressureBar, reading.valid});
}

std::vector<PressureHistory::Entry> PressureHistory::entries() const { return ring_; }

const PressureHistory::DriftSnapshot *PressureHistory::findSnapshotBefore(uint32_t targetMs) const {
  for (std::vector<DriftSnapshot>::const_reverse_iterator it = driftRing_.rbegin(); it != driftRing_.rend(); ++it) {
    if (it->ts <= targetMs) return &(*it);
  }
  return nullptr;
}

void PressureHistory::applyDrift(PressureReading &reading, uint32_t nowMs) {
  const DriftSnapshot *s1h = findSnapshotBefore(nowMs - 60UL * 60UL * 1000UL);
  if (s1h != nullptr && s1h->pressureValid && reading.valid) {
    reading.pressureDrift1h = reading.pressureBar - s1h->pressureBar;
    reading.pressureDrift1hValid = true;
  }
  const DriftSnapshot *s24h = findSnapshotBefore(nowMs - 24UL * 60UL * 60UL * 1000UL);
  if (s24h != nullptr && s24h->pressureValid && reading.valid) {
    reading.pressureDrift24h = reading.pressureBar - s24h->pressureBar;
    reading.pressureDrift24hValid = true;
  }
}

#pragma once

#include <vector>

#include "PressureTypes.h"

class PressureHistory {
 public:
  explicit PressureHistory(size_t capacity = 180);
  void add(const PressureReading &reading, PressureState state);

  struct Entry {
    uint32_t ts;
    float bar;
    uint8_t state;
    bool valid;
  };

  std::vector<Entry> entries() const;

 private:
  size_t capacity_;
  std::vector<Entry> ring_;
};

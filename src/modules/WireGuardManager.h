#pragma once

#include <stdint.h>
#include <string>

#include "AppConfig.h"

struct WireGuardStatus {
  bool enabled{false};
  bool configured{false};
  bool online{false};
  std::string localAddress;
  std::string peerEndpoint;
  uint16_t peerPort{0};
  uint32_t lastHandshake{0};
  std::string lastError;
};

class WireGuardManager {
 public:
  void begin(const WireGuardConfig &cfg);
  void loop(uint32_t nowSec);
  bool enable(const WireGuardConfig &cfg);
  void disable();
  WireGuardStatus status() const;

 private:
  bool applyConfig(const WireGuardConfig &cfg);
  bool configLooksUsable(const WireGuardConfig &cfg, std::string &error) const;

  bool configured_{false};
  bool enabled_{false};
  bool online_{false};
  uint32_t lastHandshake_{0};
  std::string lastError_;
  std::string localAddress_;
  std::string peerEndpoint_;
  uint16_t peerPort_{0};
};

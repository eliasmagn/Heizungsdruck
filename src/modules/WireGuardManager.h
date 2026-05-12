#pragma once

#include <stdint.h>
#include <string>

#include "AppConfig.h"

struct WireGuardStatus {
  bool enabled{false};
  bool configured{false};
  bool heuristicOnline{false};
  bool online{false};
  std::string localAddress;
  std::string peerEndpoint;
  uint16_t peerPort{0};
  uint32_t lastHandshake{0};
  bool handshakeSupported{false};
  std::string lastError;
  std::string lastInfo;
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
  void maybeRetryConfigure(uint32_t nowSec);

  bool configured_{false};
  bool enabled_{false};
  bool heuristicOnline_{false};
  uint32_t lastRetrySec_{0};
  bool hasRetryConfig_{false};
  WireGuardConfig retryConfig_{};
  uint32_t lastHandshake_{0};
  std::string lastError_;
  std::string lastInfo_;
  std::string localAddress_;
  std::string peerEndpoint_;
  uint16_t peerPort_{0};
};

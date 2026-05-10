#include "WireGuardManager.h"

#include <Arduino.h>
#include "../platform_caps.h"

#if HAS_WIREGUARD
#include <WiFi.h>
#include <WireGuard-ESP32.h>
#endif

namespace {
#if HAS_WIREGUARD
WireGuard gWireGuard;

bool parseIp(const std::string &raw, IPAddress &out) {
  return out.fromString(raw.c_str());
}

bool parseCidrAddress(const std::string &raw, std::string &ipOut, uint8_t &prefixOut) {
  const size_t slashPos = raw.find('/');
  if (slashPos == std::string::npos) return false;
  ipOut = raw.substr(0, slashPos);
  const std::string prefixRaw = raw.substr(slashPos + 1);
  if (prefixRaw.empty()) return false;
  const int prefix = atoi(prefixRaw.c_str());
  if (prefix < 0 || prefix > 32) return false;
  prefixOut = static_cast<uint8_t>(prefix);
  return true;
}
#endif
}  // namespace

void WireGuardManager::begin(const WireGuardConfig &cfg) {
  if (!cfg.enabled) {
    disable();
    return;
  }
  enable(cfg);
}

void WireGuardManager::loop(uint32_t nowSec) {
#if !HAS_WIREGUARD
  (void)nowSec;
  online_ = false;
  return;
#else
  if (!enabled_) {
    online_ = false;
    return;
  }

  const wl_status_t wifiStatus = WiFi.status();
  online_ = configured_ && (wifiStatus == WL_CONNECTED);
  if (!online_) {
    lastError_ = "WireGuard transport offline (WiFi disconnected or tunnel not configured)";
  }
#endif
}

bool WireGuardManager::enable(const WireGuardConfig &cfg) {
#if !HAS_WIREGUARD
  (void)cfg;
  configured_ = false;
  enabled_ = false;
  online_ = false;
  lastError_ = "WireGuard not supported on this profile";
  return false;
#else
  if (!applyConfig(cfg)) {
    enabled_ = false;
    online_ = false;
    return false;
  }
  enabled_ = true;
  online_ = WiFi.status() == WL_CONNECTED;
  if (online_) {
    lastHandshake_ = millis() / 1000;
  }
  return true;
#endif
}

void WireGuardManager::disable() {
#if HAS_WIREGUARD
  gWireGuard.end();
#endif
  enabled_ = false;
  online_ = false;
}

WireGuardStatus WireGuardManager::status() const {
  WireGuardStatus s;
  s.enabled = enabled_;
  s.configured = configured_;
  s.online = online_;
  s.localAddress = localAddress_;
  s.peerEndpoint = peerEndpoint_;
  s.peerPort = peerPort_;
  s.lastHandshake = lastHandshake_;
  s.lastError = lastError_;
  return s;
}

bool WireGuardManager::configLooksUsable(const WireGuardConfig &cfg, std::string &error) const {
  if (cfg.localAddress.empty()) {
    error = "localAddress missing";
    return false;
  }
  if (cfg.privateKey.empty()) {
    error = "privateKey missing";
    return false;
  }
  if (cfg.peerEndpoint.empty()) {
    error = "peerEndpoint missing";
    return false;
  }
  if (cfg.peerPort == 0) {
    error = "peerPort missing";
    return false;
  }
  if (cfg.peerPublicKey.empty()) {
    error = "peerPublicKey missing";
    return false;
  }
  if (cfg.allowedIp1.empty() && cfg.allowedIp2.empty()) {
    error = "at least one allowed IP required";
    return false;
  }
  error.clear();
  return true;
}

bool WireGuardManager::applyConfig(const WireGuardConfig &cfg) {
#if !HAS_WIREGUARD
  (void)cfg;
  configured_ = false;
  lastError_ = "WireGuard not supported on this profile";
  return false;
#else
  std::string error;
  if (!configLooksUsable(cfg, error)) {
    configured_ = false;
    lastError_ = error;
    return false;
  }

  std::string localAddressRaw = cfg.localAddress;
  std::string localAddressIpOnly;
  uint8_t cidrPrefix = 0;
  if (parseCidrAddress(cfg.localAddress, localAddressIpOnly, cidrPrefix)) {
    localAddressRaw = localAddressIpOnly;
  }

  IPAddress localAddress;
  if (!parseIp(localAddressRaw, localAddress)) {
    configured_ = false;
    lastError_ = "localAddress invalid (use IP or CIDR, e.g. 10.66.0.2 or 10.66.0.2/24)";
    return false;
  }

  gWireGuard.end();
  const bool ok = gWireGuard.begin(localAddress, cfg.privateKey.c_str(), cfg.peerEndpoint.c_str(),
                                   cfg.peerPublicKey.c_str(), cfg.peerPort);
  configured_ = ok;
  if (!ok) {
    lastError_ = "WireGuard begin failed";
    return false;
  }

  localAddress_ = localAddressRaw;
  peerEndpoint_ = cfg.peerEndpoint;
  peerPort_ = cfg.peerPort;
  // The WireGuard-ESP32 API used here does not expose explicit setters for
  // netmask/presharedKey/allowed IP list/keepalive; these fields are still
  // validated and persisted for future backend support.
  if (cidrPrefix > 0) {
    lastError_ = "CIDR accepted; runtime tunnel health currently inferred from WiFi link state only";
  } else {
    lastError_ = "runtime tunnel health currently inferred from WiFi link state only";
  }
  return true;
#endif
}

#include "WireGuardManager.h"

#include <Arduino.h>
#include <WiFi.h>

#include <WireGuard-ESP32.h>

namespace {
WireGuard gWireGuard;

bool parseIp(const std::string &raw, IPAddress &out) {
  return out.fromString(raw.c_str());
}
}  // namespace

void WireGuardManager::begin(const WireGuardConfig &cfg) {
  if (!cfg.enabled) {
    disable();
    return;
  }
  enable(cfg);
}

void WireGuardManager::loop(uint32_t nowSec) {
  if (!enabled_) {
    online_ = false;
    return;
  }

  const wl_status_t wifiStatus = WiFi.status();
  online_ = (wifiStatus == WL_CONNECTED);
  if (online_) {
    lastHandshake_ = nowSec;
  }
}

bool WireGuardManager::enable(const WireGuardConfig &cfg) {
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
}

void WireGuardManager::disable() {
  gWireGuard.end();
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
  std::string error;
  if (!configLooksUsable(cfg, error)) {
    configured_ = false;
    lastError_ = error;
    return false;
  }

  IPAddress localAddress;
  if (!parseIp(cfg.localAddress, localAddress)) {
    configured_ = false;
    lastError_ = "localAddress invalid";
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

  localAddress_ = cfg.localAddress;
  peerEndpoint_ = cfg.peerEndpoint;
  peerPort_ = cfg.peerPort;
  lastError_.clear();
  return true;
}

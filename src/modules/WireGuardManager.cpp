#include "WireGuardManager.h"

#include <Arduino.h>
#include <time.h>
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

bool ensureClockSynced(uint32_t timeoutMs, std::string &errorOut) {
  time_t now = time(nullptr);
  if (now > 1700000000) return true;  // already synced (after ~2023-11)

  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  const uint32_t started = millis();
  while (millis() - started < timeoutMs) {
    delay(100);
    now = time(nullptr);
    if (now > 1700000000) return true;
  }
  errorOut = "NTP time sync timeout (required by WireGuard)";
  return false;
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
  heuristicOnline_ = false;
  lastHandshake_ = 0;
  return;
#else
  (void)nowSec;
  if (!enabled_) {
    heuristicOnline_ = false;
    lastHandshake_ = 0;
    return;
  }

  maybeRetryConfigure(nowSec);

  const wl_status_t wifiStatus = WiFi.status();
  heuristicOnline_ = configured_ && (wifiStatus == WL_CONNECTED);
  if (!heuristicOnline_) {
    lastHandshake_ = 0;
    lastInfo_ = "Heuristic offline: WiFi disconnected or WireGuard not configured";
    return;
  }
  // Library does not provide real handshake metrics.
  lastHandshake_ = 0;
  lastInfo_ = "Heuristic online: configured + WiFi connected; backend has no handshake timestamp";
#endif
}

bool WireGuardManager::enable(const WireGuardConfig &cfg) {
#if !HAS_WIREGUARD
  (void)cfg;
  configured_ = false;
  enabled_ = false;
  heuristicOnline_ = false;
  lastError_ = "WireGuard not supported on this profile";
  lastInfo_.clear();
  return false;
#else
  retryConfig_ = cfg;
  hasRetryConfig_ = true;
  lastRetrySec_ = 0;
  enabled_ = true;
  if (!applyConfig(cfg)) {
    heuristicOnline_ = false;
    lastInfo_ = "WireGuard start deferred; auto-retry every 5 minutes while enabled";
    return false;
  }
  heuristicOnline_ = configured_ && (WiFi.status() == WL_CONNECTED);
  lastHandshake_ = 0;
  lastInfo_ = "WireGuard configuration applied";
  return true;
#endif
}

void WireGuardManager::disable() {
#if HAS_WIREGUARD
  gWireGuard.end();
#endif
  enabled_ = false;
  heuristicOnline_ = false;
  configured_ = false;
  hasRetryConfig_ = false;
  lastRetrySec_ = 0;
  lastHandshake_ = 0;
  lastError_.clear();
  lastInfo_ = "WireGuard disabled";
  localAddress_.clear();
  peerEndpoint_.clear();
  peerPort_ = 0;
}

void WireGuardManager::maybeRetryConfigure(uint32_t nowSec) {
#if !HAS_WIREGUARD
  (void)nowSec;
  return;
#else
  if (!enabled_) return;
  if (configured_) return;
  if (!hasRetryConfig_) return;
  if (lastRetrySec_ != 0 && (nowSec - lastRetrySec_) < 300) return;
  lastRetrySec_ = nowSec;
  if (applyConfig(retryConfig_)) {
    configured_ = true;
    heuristicOnline_ = WiFi.status() == WL_CONNECTED;
    lastInfo_ = "WireGuard auto-retry succeeded";
    lastError_.clear();
    return;
  }
  lastInfo_ = "WireGuard auto-retry pending (every 5 minutes while enabled)";
#endif
}

WireGuardStatus WireGuardManager::status() const {
  WireGuardStatus s;
  s.enabled = enabled_;
  s.configured = configured_;
  s.heuristicOnline = heuristicOnline_;
  s.online = heuristicOnline_;
  s.localAddress = localAddress_;
  s.peerEndpoint = peerEndpoint_;
  s.peerPort = peerPort_;
  s.lastHandshake = lastHandshake_;
  s.handshakeSupported = false;
  s.lastError = lastError_;
  s.lastInfo = lastInfo_;
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
  error.clear();
  return true;
}

bool WireGuardManager::applyConfig(const WireGuardConfig &cfg) {
#if !HAS_WIREGUARD
  (void)cfg;
  configured_ = false;
  lastError_ = "WireGuard not supported on this profile";
  lastInfo_.clear();
  return false;
#else
  std::string error;
  if (!configLooksUsable(cfg, error)) {
    configured_ = false;
    lastError_ = error;
    return false;
  }

  if (!ensureClockSynced(8000, error)) {
    configured_ = false;
    lastError_ = error;
    lastInfo_.clear();
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
    lastInfo_.clear();
    localAddress_.clear();
    peerEndpoint_.clear();
    peerPort_ = 0;
    return false;
  }

  localAddress_ = localAddressRaw;
  peerEndpoint_ = cfg.peerEndpoint;
  peerPort_ = cfg.peerPort;
  const bool hasReservedFields =
      !cfg.netmask.empty() || !cfg.presharedKey.empty() || !cfg.allowedIp1.empty() || !cfg.allowedIp2.empty() || cfg.keepAliveSeconds > 0;
  lastError_.clear();
  if (cidrPrefix > 0) {
    lastInfo_ = "CIDR host parsed; backend applies host IP only";
  } else {
    lastInfo_ = "Backend applies localAddress/privateKey/peerEndpoint/peerPort/peerPublicKey";
  }
  if (hasReservedFields) {
    lastInfo_ += "; netmask/presharedKey/allowedIp1/allowedIp2/keepAliveSeconds are persisted but currently ignored by this backend";
  }
  return true;
#endif
}

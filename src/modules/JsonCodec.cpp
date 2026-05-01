#include "JsonCodec.h"

#include <ArduinoJson.h>

std::string configToJson(const AppConfig &cfg) {
  JsonDocument doc;

  doc["sensor"]["adcPin"] = cfg.sensor.adcPin;
  doc["sensor"]["sampleCount"] = cfg.sensor.sampleCount;
  doc["sensor"]["updateIntervalMs"] = cfg.sensor.updateIntervalMs;
  doc["sensor"]["disconnectAdc"] = cfg.sensor.disconnectAdc;
  doc["sensor"]["shortGndAdc"] = cfg.sensor.shortGndAdc;
  doc["sensor"]["shortVccAdc"] = cfg.sensor.shortVccAdc;
  doc["sensor"]["maxJumpBar"] = cfg.sensor.maxJumpBar;

  doc["calib"]["adcLow"] = cfg.calib.adcLow;
  doc["calib"]["adcHigh"] = cfg.calib.adcHigh;
  doc["calib"]["barLow"] = cfg.calib.barLow;
  doc["calib"]["barHigh"] = cfg.calib.barHigh;
  doc["calib"]["offsetBar"] = cfg.calib.offsetBar;
  JsonArray points = doc["calib"]["points"].to<JsonArray>();
  for (const auto &p : cfg.calib.points) {
    JsonObject item = points.add<JsonObject>();
    item["bar"] = p.bar;
    item["adc"] = p.adc;
    item["valid"] = p.valid;
  }

  doc["alarm"]["lowBar"] = cfg.alarm.lowBar;
  doc["alarm"]["highBar"] = cfg.alarm.highBar;
  doc["alarm"]["hysteresisBar"] = cfg.alarm.hysteresisBar;
  doc["alarm"]["repeatMinutes"] = cfg.alarm.repeatMinutes;
  doc["alarm"]["telegramBotToken"] = cfg.alarm.telegramBotToken;
  doc["alarm"]["telegramChatId"] = cfg.alarm.telegramChatId;
  doc["alarm"]["emailWebhookUrl"] = cfg.alarm.emailWebhookUrl;

  doc["mqtt"]["enabled"] = cfg.mqtt.enabled;
  doc["mqtt"]["host"] = cfg.mqtt.host;
  doc["mqtt"]["port"] = cfg.mqtt.port;
  doc["mqtt"]["username"] = cfg.mqtt.username;
  doc["mqtt"]["password"] = cfg.mqtt.password;
  doc["mqtt"]["clientId"] = cfg.mqtt.clientId;
  doc["mqtt"]["topicBase"] = cfg.mqtt.topicBase;
  doc["mqtt"]["publishIntervalMs"] = cfg.mqtt.publishIntervalMs;

  doc["network"]["wifiSsid"] = cfg.network.wifiSsid;
  doc["network"]["wifiPassword"] = cfg.network.wifiPassword;
  doc["network"]["apSsid"] = cfg.network.apSsid;
  doc["network"]["apPassword"] = cfg.network.apPassword;
  doc["network"]["hostname"] = cfg.network.hostname;
  doc["network"]["wifiTxPowerDbm"] = cfg.network.wifiTxPowerDbm;
  doc["network"]["wifi11bMode"] = cfg.network.wifi11bMode;

  doc["wireguard"]["enabled"] = cfg.wireguard.enabled;
  doc["wireguard"]["localAddress"] = cfg.wireguard.localAddress;
  doc["wireguard"]["netmask"] = cfg.wireguard.netmask;
  doc["wireguard"]["privateKey"] = cfg.wireguard.privateKey;
  doc["wireguard"]["peerEndpoint"] = cfg.wireguard.peerEndpoint;
  doc["wireguard"]["peerPort"] = cfg.wireguard.peerPort;
  doc["wireguard"]["peerPublicKey"] = cfg.wireguard.peerPublicKey;
  doc["wireguard"]["presharedKey"] = cfg.wireguard.presharedKey;
  doc["wireguard"]["allowedIp1"] = cfg.wireguard.allowedIp1;
  doc["wireguard"]["allowedIp2"] = cfg.wireguard.allowedIp2;
  doc["wireguard"]["keepAliveSeconds"] = cfg.wireguard.keepAliveSeconds;

  std::string out;
  serializeJson(doc, out);
  return out;
}

static void setIfExists(JsonVariantConst v, uint8_t &to) { if (!v.isNull()) to = v.as<uint8_t>(); }
static void setIfExists(JsonVariantConst v, uint16_t &to) { if (!v.isNull()) to = v.as<uint16_t>(); }
static void setIfExists(JsonVariantConst v, uint32_t &to) { if (!v.isNull()) to = v.as<uint32_t>(); }
static void setIfExists(JsonVariantConst v, int &to) { if (!v.isNull()) to = v.as<int>(); }
static void setIfExists(JsonVariantConst v, float &to) { if (!v.isNull()) to = v.as<float>(); }
static void setIfExists(JsonVariantConst v, bool &to) { if (!v.isNull()) to = v.as<bool>(); }
static void setIfExists(JsonVariantConst v, std::string &to) { if (!v.isNull()) to = v.as<const char *>(); }

bool configFromJson(const std::string &json, AppConfig &cfgOut, std::string &error) {
  JsonDocument doc;
  auto err = deserializeJson(doc, json);
  if (err) {
    error = err.c_str();
    return false;
  }

  JsonVariantConst s = doc["sensor"];
  setIfExists(s["adcPin"], cfgOut.sensor.adcPin);
  setIfExists(s["sampleCount"], cfgOut.sensor.sampleCount);
  setIfExists(s["updateIntervalMs"], cfgOut.sensor.updateIntervalMs);
  setIfExists(s["disconnectAdc"], cfgOut.sensor.disconnectAdc);
  setIfExists(s["shortGndAdc"], cfgOut.sensor.shortGndAdc);
  setIfExists(s["shortVccAdc"], cfgOut.sensor.shortVccAdc);
  setIfExists(s["maxJumpBar"], cfgOut.sensor.maxJumpBar);

  JsonVariantConst c = doc["calib"];
  setIfExists(c["adcLow"], cfgOut.calib.adcLow);
  setIfExists(c["adcHigh"], cfgOut.calib.adcHigh);
  setIfExists(c["barLow"], cfgOut.calib.barLow);
  setIfExists(c["barHigh"], cfgOut.calib.barHigh);
  setIfExists(c["offsetBar"], cfgOut.calib.offsetBar);
  JsonArrayConst points = c["points"].as<JsonArrayConst>();
  if (!points.isNull()) {
    size_t idx = 0;
    for (JsonObjectConst point : points) {
      if (idx >= cfgOut.calib.points.size()) break;
      setIfExists(point["bar"], cfgOut.calib.points[idx].bar);
      setIfExists(point["adc"], cfgOut.calib.points[idx].adc);
      setIfExists(point["valid"], cfgOut.calib.points[idx].valid);
      ++idx;
    }
  }

  JsonVariantConst a = doc["alarm"];
  setIfExists(a["lowBar"], cfgOut.alarm.lowBar);
  setIfExists(a["highBar"], cfgOut.alarm.highBar);
  setIfExists(a["hysteresisBar"], cfgOut.alarm.hysteresisBar);
  setIfExists(a["repeatMinutes"], cfgOut.alarm.repeatMinutes);
  setIfExists(a["telegramBotToken"], cfgOut.alarm.telegramBotToken);
  setIfExists(a["telegramChatId"], cfgOut.alarm.telegramChatId);
  setIfExists(a["emailWebhookUrl"], cfgOut.alarm.emailWebhookUrl);

  JsonVariantConst m = doc["mqtt"];
  setIfExists(m["enabled"], cfgOut.mqtt.enabled);
  setIfExists(m["host"], cfgOut.mqtt.host);
  setIfExists(m["port"], cfgOut.mqtt.port);
  setIfExists(m["username"], cfgOut.mqtt.username);
  setIfExists(m["password"], cfgOut.mqtt.password);
  setIfExists(m["clientId"], cfgOut.mqtt.clientId);
  setIfExists(m["topicBase"], cfgOut.mqtt.topicBase);
  setIfExists(m["publishIntervalMs"], cfgOut.mqtt.publishIntervalMs);

  JsonVariantConst n = doc["network"];
  setIfExists(n["wifiSsid"], cfgOut.network.wifiSsid);
  setIfExists(n["wifiPassword"], cfgOut.network.wifiPassword);
  setIfExists(n["apSsid"], cfgOut.network.apSsid);
  setIfExists(n["apPassword"], cfgOut.network.apPassword);
  setIfExists(n["hostname"], cfgOut.network.hostname);
  setIfExists(n["wifiTxPowerDbm"], cfgOut.network.wifiTxPowerDbm);
  setIfExists(n["wifi11bMode"], cfgOut.network.wifi11bMode);

  JsonVariantConst w = doc["wireguard"];
  setIfExists(w["enabled"], cfgOut.wireguard.enabled);
  setIfExists(w["localAddress"], cfgOut.wireguard.localAddress);
  setIfExists(w["netmask"], cfgOut.wireguard.netmask);
  setIfExists(w["privateKey"], cfgOut.wireguard.privateKey);
  setIfExists(w["peerEndpoint"], cfgOut.wireguard.peerEndpoint);
  setIfExists(w["peerPort"], cfgOut.wireguard.peerPort);
  setIfExists(w["peerPublicKey"], cfgOut.wireguard.peerPublicKey);
  setIfExists(w["presharedKey"], cfgOut.wireguard.presharedKey);
  setIfExists(w["allowedIp1"], cfgOut.wireguard.allowedIp1);
  setIfExists(w["allowedIp2"], cfgOut.wireguard.allowedIp2);
  setIfExists(w["keepAliveSeconds"], cfgOut.wireguard.keepAliveSeconds);

  return cfgOut.validate(error);
}

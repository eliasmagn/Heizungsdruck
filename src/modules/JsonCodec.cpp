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

  doc["alarm"]["lowBar"] = cfg.alarm.lowBar;
  doc["alarm"]["highBar"] = cfg.alarm.highBar;
  doc["alarm"]["hysteresisBar"] = cfg.alarm.hysteresisBar;

  doc["mqtt"]["enabled"] = cfg.mqtt.enabled;
  doc["mqtt"]["host"] = cfg.mqtt.host;
  doc["mqtt"]["port"] = cfg.mqtt.port;
  doc["mqtt"]["username"] = cfg.mqtt.username;
  doc["mqtt"]["password"] = cfg.mqtt.password;
  doc["mqtt"]["clientId"] = cfg.mqtt.clientId;
  doc["mqtt"]["topicBase"] = cfg.mqtt.topicBase;
  doc["mqtt"]["publishIntervalMs"] = cfg.mqtt.publishIntervalMs;

  doc["network"]["hostname"] = cfg.network.hostname;

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

  JsonVariantConst a = doc["alarm"];
  setIfExists(a["lowBar"], cfgOut.alarm.lowBar);
  setIfExists(a["highBar"], cfgOut.alarm.highBar);
  setIfExists(a["hysteresisBar"], cfgOut.alarm.hysteresisBar);

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
  setIfExists(n["hostname"], cfgOut.network.hostname);

  return cfgOut.validate(error);
}

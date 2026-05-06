#include "MqttManager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
namespace {
constexpr uint16_t kMqttBufferSize = 2048;
String sanitizeId(const std::string &in) {
  String out;
  for (char c : in) {
    const char lc = static_cast<char>(tolower(c));
    if ((lc >= 'a' && lc <= 'z') || (lc >= '0' && lc <= '9') || lc == '_') out += lc;
    else out += '_';
  }
  if (out.isEmpty()) out = "ch";
  return out;
}
}

MqttManager::MqttManager() : client_(wifiClient_) {}
void MqttManager::setWireGuardStateProvider(bool (*isOnlineFn)()) { wireGuardOnlineFn_ = isOnlineFn; }

void MqttManager::begin(const AppConfig &cfg) {
  cfg_ = cfg;
  discoveryPublished_ = false;
  client_.setServer(cfg_.mqtt.host.c_str(), cfg_.mqtt.port);
  client_.setBufferSize(kMqttBufferSize);
  Serial.printf("[MQTT] begin host=%s port=%u topicBase=%s buffer=%u enabled=%d\n", cfg_.mqtt.host.c_str(), cfg_.mqtt.port,
                cfg_.mqtt.topicBase.c_str(), kMqttBufferSize, cfg_.mqtt.enabled);
  client_.setCallback([](char *topic, uint8_t *payload, unsigned int length) {
    (void)payload;
    (void)length;
    const String t(topic);
    if (t.endsWith("/cmd/restart")) {
      delay(100);
      ESP.restart();
    }
  });
}

void MqttManager::reconnect(uint32_t nowMs) {
  if (!cfg_.mqtt.enabled) return;
  if (cfg_.mqtt.requireWireguard && wireGuardOnlineFn_ != nullptr && !wireGuardOnlineFn_()) {
    if (nowMs - lastReconnectTryMs_ >= 5000) {
      lastReconnectTryMs_ = nowMs;
      lastError_ = "wireguard requested but offline (fallback active)";
      Serial.println("[MQTT] requireWireguard=1 but tunnel offline; fallback via standard routing remains active");
    }
  }
  if (client_.connected()) return;
  if (nowMs - lastReconnectTryMs_ < 5000) return;
  lastReconnectTryMs_ = nowMs;

  const bool ok = client_.connect(cfg_.mqtt.clientId.c_str(), cfg_.mqtt.username.c_str(), cfg_.mqtt.password.c_str(),
                                  topicStatus().c_str(), 1, true, "offline");
  lastClientState_ = client_.state();
  if (!ok) {
    lastError_ = "connect failed";
    Serial.printf("[MQTT] connect failed state=%d host=%s port=%u clientId=%s\n", lastClientState_, cfg_.mqtt.host.c_str(),
                  cfg_.mqtt.port, cfg_.mqtt.clientId.c_str());
    return;
  }
  Serial.printf("[MQTT] connected state=%d\n", lastClientState_);
  const String statusTopic = topicStatus();
  const String restartTopic = String(cfg_.mqtt.topicBase.c_str()) + "/cmd/restart";
  const bool onlineOk = client_.publish(statusTopic.c_str(), "online", true);
  const bool subOk = client_.subscribe(restartTopic.c_str());
  publishHomeAssistantDiscovery();
  Serial.printf("[MQTT] publish online topic=%s ok=%d | subscribe %s ok=%d discovery=%d\n", statusTopic.c_str(), onlineOk,
                restartTopic.c_str(), subOk, discoveryPublished_);
}

String MqttManager::stateToString(PressureState s) const {
  switch (s) {
    case PressureState::SENSOR_FAULT: return "SENSOR_FAULT";
    case PressureState::PRESSURE_LOW: return "PRESSURE_LOW";
    case PressureState::OK: return "PRESSURE_OK";
    case PressureState::PRESSURE_HIGH: return "PRESSURE_HIGH";
    default: return "PRESSURE_UNKNOWN";
  }
}

void MqttManager::loop(uint32_t nowMs) {
  reconnect(nowMs);
  client_.loop();
  const int st = client_.state();
  if (st != lastClientState_) {
    lastClientState_ = st;
    Serial.printf("[MQTT] state changed -> %d connected=%d\n", st, client_.connected());
  }
}

void MqttManager::publishReading(const PressureReading &reading, PressureState state, bool wifiConnected,
                                 uint32_t uptimeSec) {
  if (!cfg_.mqtt.enabled || !client_.connected()) return;
  if (cfg_.mqtt.requireWireguard && wireGuardOnlineFn_ != nullptr && !wireGuardOnlineFn_()) {
    lastError_ = "wireguard requested but offline (publishing via standard routing)";
    Serial.println("[MQTT] requireWireguard=1 but tunnel offline; publish continues via standard routing");
  }

  const uint32_t nowMs = millis();
  if (nowMs - lastPublishMs_ < cfg_.mqtt.publishIntervalMs) return;
  lastPublishMs_ = nowMs;

  JsonDocument doc;
  doc["pressureBar"] = reading.pressureBar;
  doc["rawAdc"] = reading.rawAdc;
  doc["filteredAdc"] = reading.filteredAdc;
  doc["valid"] = reading.valid;
  doc["voltage"] = reading.voltage;
  doc["temperatureC"] = reading.temperatureC;
  doc["temperatureValid"] = reading.temperatureValid;
  JsonObject channels = doc["channels"].to<JsonObject>();
  for (std::map<std::string, int>::const_iterator it = reading.channelRaw.begin(); it != reading.channelRaw.end(); ++it) {
    JsonObject c = channels[it->first.c_str()].to<JsonObject>();
    c["rawAdc"] = it->second;
  }
  for (std::map<std::string, int>::const_iterator it = reading.channelFiltered.begin(); it != reading.channelFiltered.end(); ++it) {
    JsonObject c = channels[it->first.c_str()].to<JsonObject>();
    c["filteredAdc"] = it->second;
  }
  doc["fault"] = static_cast<int>(reading.fault);
  doc["state"] = stateToString(state);
  doc["wifiConnected"] = wifiConnected;
  doc["uptimeSec"] = uptimeSec;

  String payload;
  serializeJson(doc, payload);
  const String telemetryTopic = topicTelemetry();
  const String stateTopic = topicState();
  lastPublishTopic_ = telemetryTopic;
  lastPublishPayloadLen_ = payload.length();
  if (payload.length() >= client_.getBufferSize()) {
    Serial.printf("[MQTT] telemetry payload too large len=%u buffer=%u topic=%s\n", payload.length(), client_.getBufferSize(),
                  telemetryTopic.c_str());
  }
  const bool telOk = client_.publish(telemetryTopic.c_str(), payload.c_str(), false);
  const bool stateOk = client_.publish(stateTopic.c_str(), stateToString(state).c_str(), true);
  if (!telOk || !stateOk) {
    lastError_ = "publish failed";
    lastPublishOk_ = false;
    publishFailCount_++;
    Serial.printf("[MQTT] publish failed telemetryOk=%d stateOk=%d topic=%s payloadLen=%u clientState=%d connected=%d\n",
                  telOk, stateOk, telemetryTopic.c_str(), payload.length(), client_.state(), client_.connected());
    return;
  }
  lastPublishOk_ = true;
  publishOkCount_++;
  Serial.printf("[MQTT] publish ok topic=%s payloadLen=%u stateTopic=%s\n", telemetryTopic.c_str(), payload.length(),
                stateTopic.c_str());
}

bool MqttManager::publishTestMessage(const String &note) {
  if (!cfg_.mqtt.enabled || !client_.connected()) {
    lastError_ = "test publish skipped: mqtt disabled/disconnected";
    return false;
  }
  JsonDocument doc;
  doc["type"] = "manual_test";
  doc["note"] = note;
  doc["millis"] = millis();
  String payload;
  serializeJson(doc, payload);
  const String topic = String(cfg_.mqtt.topicBase.c_str()) + "/telemetry_test";
  lastPublishTopic_ = topic;
  lastPublishPayloadLen_ = payload.length();
  const bool ok = client_.publish(topic.c_str(), payload.c_str(), false);
  lastPublishOk_ = ok;
  if (ok) {
    publishOkCount_++;
    Serial.printf("[MQTT] test publish ok topic=%s payloadLen=%u\n", topic.c_str(), payload.length());
  } else {
    publishFailCount_++;
    lastError_ = "test publish failed";
    Serial.printf("[MQTT] test publish failed topic=%s payloadLen=%u state=%d\n", topic.c_str(), payload.length(),
                  client_.state());
  }
  return ok;
}

String MqttManager::diagnosticsJson() {
  JsonDocument doc;
  doc["connected"] = client_.connected();
  doc["clientState"] = client_.state();
  doc["lastError"] = lastError_;
  doc["lastPublishTopic"] = lastPublishTopic_;
  doc["lastPublishPayloadLen"] = lastPublishPayloadLen_;
  doc["lastPublishOk"] = lastPublishOk_;
  doc["publishOkCount"] = publishOkCount_;
  doc["publishFailCount"] = publishFailCount_;
  doc["bufferSize"] = client_.getBufferSize();
  doc["requireWireguard"] = cfg_.mqtt.requireWireguard;
  doc["wireguardOnline"] = wireGuardOnlineFn_ != nullptr ? wireGuardOnlineFn_() : false;
  String out;
  serializeJson(doc, out);
  return out;
}

String MqttManager::topicTelemetry() const { return String(cfg_.mqtt.topicBase.c_str()) + "/telemetry"; }
String MqttManager::topicState() const { return String(cfg_.mqtt.topicBase.c_str()) + "/state"; }
String MqttManager::topicStatus() const { return String(cfg_.mqtt.topicBase.c_str()) + "/status"; }

bool MqttManager::publishDiscoveryEntity(const String &component, const String &objectId, const String &name,
                                         const JsonDocument &payload) {
  String topic = "homeassistant/" + component + "/" + objectId + "/config";
  JsonDocument doc;
  doc.set(payload);
  doc["name"] = name;
  String out;
  serializeJson(doc, out);
  return client_.publish(topic.c_str(), out.c_str(), true);
}

void MqttManager::publishHomeAssistantDiscovery() {
  const String cleanId = String("heizungsdruck_") + cfg_.deviceId.c_str();
  const String deviceName = String("Heizungsdruck ") + cfg_.deviceId.c_str();

  auto basePayload = [&](JsonDocument &doc, const String &uniq) {
    doc["uniq_id"] = uniq;
    doc["avty_t"] = topicStatus();
    doc["pl_avail"] = "online";
    doc["pl_not_avail"] = "offline";
    JsonObject dev = doc["dev"].to<JsonObject>();
    JsonArray ids = dev["ids"].to<JsonArray>();
    ids.add(cleanId);
    dev["name"] = deviceName;
    dev["mf"] = "EliasMagn";
    dev["mdl"] = "ESP32 Pressure Monitor";
    dev["sw"] = "firmware";
  };

  bool ok = true;
  JsonDocument pressure;
  basePayload(pressure, cleanId + "_pressure");
  pressure["stat_t"] = topicTelemetry(); pressure["val_tpl"] = "{{ value_json.pressureBar }}";
  pressure["unit_of_meas"] = "bar"; pressure["dev_cla"] = "pressure"; pressure["stat_cla"] = "measurement";
  ok &= publishDiscoveryEntity("sensor", cleanId + "_pressure", String(cfg_.deviceId.c_str()) + " Druck", pressure);

  JsonDocument state;
  basePayload(state, cleanId + "_state"); state["stat_t"] = topicState();
  ok &= publishDiscoveryEntity("sensor", cleanId + "_state", String(cfg_.deviceId.c_str()) + " Status", state);

  JsonDocument raw;
  basePayload(raw, cleanId + "_rawadc"); raw["stat_t"] = topicTelemetry(); raw["val_tpl"] = "{{ value_json.rawAdc }}";
  ok &= publishDiscoveryEntity("sensor", cleanId + "_rawadc", String(cfg_.deviceId.c_str()) + " ADC Raw", raw);

  JsonDocument filt;
  basePayload(filt, cleanId + "_filteredadc"); filt["stat_t"] = topicTelemetry(); filt["val_tpl"] = "{{ value_json.filteredAdc }}";
  ok &= publishDiscoveryEntity("sensor", cleanId + "_filteredadc", String(cfg_.deviceId.c_str()) + " ADC Filtered", filt);

  JsonDocument volt;
  basePayload(volt, cleanId + "_voltage");
  volt["stat_t"] = topicTelemetry();
  volt["val_tpl"] = "{{ value_json.voltage }}";
  volt["unit_of_meas"] = "V";
  volt["dev_cla"] = "voltage";
  ok &= publishDiscoveryEntity("sensor", cleanId + "_voltage", String(cfg_.deviceId.c_str()) + " Spannung", volt);

  JsonDocument temp;
  basePayload(temp, cleanId + "_temperature");
  temp["stat_t"] = topicTelemetry();
  temp["val_tpl"] = "{{ value_json.temperatureC }}";
  temp["unit_of_meas"] = "°C";
  temp["dev_cla"] = "temperature";
  temp["stat_cla"] = "measurement";
  ok &= publishDiscoveryEntity("sensor", cleanId + "_temperature", String(cfg_.deviceId.c_str()) + " Temperatur", temp);

  JsonDocument tempValid;
  basePayload(tempValid, cleanId + "_temperature_valid");
  tempValid["stat_t"] = topicTelemetry();
  tempValid["val_tpl"] = "{{ value_json.temperatureValid }}";
  tempValid["pl_on"] = true;
  tempValid["pl_off"] = false;
  ok &= publishDiscoveryEntity("binary_sensor", cleanId + "_temperature_valid",
                               String(cfg_.deviceId.c_str()) + " Temperatur gültig", tempValid);

  for (const auto &ch : cfg_.sensor.analogChannels) {
    const String sid = sanitizeId(ch.id);
    JsonDocument c;
    basePayload(c, cleanId + "_adc_" + sid);
    c["stat_t"] = topicTelemetry();
    c["val_tpl"] = "{{ value_json.channels['" + String(ch.id.c_str()) + "'].filteredAdc }}";
    c["stat_cla"] = "measurement";
    ok &= publishDiscoveryEntity("sensor", cleanId + "_adc_" + sid, String(cfg_.deviceId.c_str()) + " ADC " + String(ch.id.c_str()), c);
  }

  JsonDocument valid;
  basePayload(valid, cleanId + "_valid"); valid["stat_t"] = topicTelemetry(); valid["val_tpl"] = "{{ value_json.valid }}";
  valid["pl_on"] = true; valid["pl_off"] = false;
  ok &= publishDiscoveryEntity("binary_sensor", cleanId + "_valid", String(cfg_.deviceId.c_str()) + " Valid", valid);

  JsonDocument alarm;
  basePayload(alarm, cleanId + "_alarm"); alarm["stat_t"] = topicState();
  alarm["val_tpl"] = "{{ value in ['PRESSURE_LOW','PRESSURE_HIGH','SENSOR_FAULT'] }}";
  alarm["pl_on"] = true; alarm["pl_off"] = false;
  ok &= publishDiscoveryEntity("binary_sensor", cleanId + "_alarm", String(cfg_.deviceId.c_str()) + " Alarm", alarm);

  discoveryPublished_ = ok;
}

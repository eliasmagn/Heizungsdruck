#include "AlarmManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

namespace {
const char *stateName(PressureState s) {
  switch (s) {
    case PressureState::SENSOR_FAULT: return "SENSOR_FAULT";
    case PressureState::PRESSURE_LOW: return "PRESSURE_LOW";
    case PressureState::PRESSURE_HIGH: return "PRESSURE_HIGH";
    case PressureState::OK: return "OK";
    default: return "UNKNOWN";
  }
}
}

void AlarmManager::begin(const AppConfig &cfg) {
  cfg_ = cfg;
  lastState_ = PressureState::UNKNOWN;
  firstAlarmSent_ = false;
  lastAlertMs_ = 0;
  lastTelegramPollMs_ = 0;
  telegramUpdateOffset_ = 0;
}
void AlarmManager::updateConfig(const AppConfig &cfg) { cfg_ = cfg; }
void AlarmManager::attachConfigSaver(bool (*saveFn)(const AppConfig &)) { saveConfig_ = saveFn; }
bool AlarmManager::isAlarmState(PressureState s) const { return s == PressureState::PRESSURE_LOW || s == PressureState::PRESSURE_HIGH || s == PressureState::SENSOR_FAULT; }

AlarmDispatchResult AlarmManager::sendTelegramMessage(const String &text) const {
  if (cfg_.alarm.telegramBotToken.empty() || cfg_.alarm.telegramChatId.empty()) {
    Serial.println("[Alarm][Telegram] missing config: token/chat-id");
    lastTelegramResult_ = AlarmDispatchResult(false, 0, "telegram config missing");
    return lastTelegramResult_;
  }
  const String url = "https://api.telegram.org/bot" + String(cfg_.alarm.telegramBotToken.c_str()) + "/sendMessage";
  JsonDocument payload; payload["chat_id"] = cfg_.alarm.telegramChatId.c_str(); payload["text"] = text;
  String body; serializeJson(payload, body);
  for (int attempt = 1; attempt <= 2; ++attempt) {
    HTTPClient http; http.setTimeout(6000); http.begin(url); http.addHeader("Content-Type", "application/json");
    const int code = http.POST(body); String resp = http.getString(); http.end();
    Serial.printf("[Alarm][Telegram] attempt=%d status=%d response=%s\n", attempt, code, resp.c_str());
    if (code >= 200 && code < 300) {
      lastTelegramResult_ = AlarmDispatchResult(true, code, resp);
      return lastTelegramResult_;
    }
    if (code > 0) {
      lastTelegramResult_ = AlarmDispatchResult(false, code, resp);
      return lastTelegramResult_;
    }
  }
  lastTelegramResult_ = AlarmDispatchResult(false, -1, "network/TLS/API request failed");
  return lastTelegramResult_;
}

AlarmDispatchResult AlarmManager::sendWebhookTest(const PressureReading &reading, PressureState state, const String &event) const {
  if (cfg_.alarm.emailWebhookUrl.empty()) return AlarmDispatchResult(false, 0, "webhook url missing");
  HTTPClient http; http.setTimeout(6000); http.begin(cfg_.alarm.emailWebhookUrl.c_str()); http.addHeader("Content-Type", "application/json");
  JsonDocument payload; payload["event"] = event; payload["state"] = stateName(state); payload["pressureBar"] = reading.pressureBar; payload["rawAdc"] = reading.rawAdc; payload["valid"] = reading.valid;
  String body; serializeJson(payload, body);
  const int code = http.POST(body); String resp = http.getString(); http.end();
  Serial.printf("[Alarm][Webhook] status=%d response=%s\n", code, resp.c_str());
  return AlarmDispatchResult(code >= 200 && code < 300, code, resp);
}

void AlarmManager::handleTelegramCommand(const String &cmd) {
  if (cmd == "/start") {
    sendTelegramMessage("Heizungsdruck Bot aktiv. Befehle: /start /getpres /setoffset <value> /setcalpoint <bar> <adc> /saveconfig");
    return;
  }
  if (cmd == "/getpres") {
    String text = "Druck=" + String(lastReading_.pressureBar, 2) + " bar, ADC=" + String(lastReading_.filteredAdc) + ", State=" + stateName(lastState_);
    sendTelegramMessage(text);
    return;
  }
  if (cmd.startsWith("/setoffset ")) {
    cfg_.calib.offsetBar = cmd.substring(11).toFloat();
    sendTelegramMessage("Offset gesetzt (RAM): " + String(cfg_.calib.offsetBar, 3) + " | mit /saveconfig persistent speichern");
    return;
  }
  if (cmd.startsWith("/setcalpoint ")) {
    float bar = 0.0f;
    int adc = 0;
    if (sscanf(cmd.c_str(), "/setcalpoint %f %d", &bar, &adc) == 2) {
      CalibrationConfig::Point point;
      point.bar = bar;
      point.adc = adc;
      point.valid = true;
      if (cfg_.calib.points.size() < CalibrationConfig::kMaxPointCount) cfg_.calib.points.push_back(point);
      else cfg_.calib.points.back() = point;
      sendTelegramMessage("Kalibrierpunkt gesetzt (RAM): bar=" + String(bar, 2) + " adc=" + String(adc) + " | mit /saveconfig persistent speichern");
      return;
    }
  }
  if (cmd == "/saveconfig") {
    if (saveConfig_ == nullptr) {
      sendTelegramMessage("Speichern nicht verfügbar (save callback fehlt).");
      return;
    }
    AppConfig candidate = cfg_;
    std::string err;
    if (!candidate.validate(err)) {
      sendTelegramMessage("Config ungültig: " + String(err.c_str()));
      return;
    }
    if (!saveConfig_(candidate)) {
      sendTelegramMessage("Config speichern fehlgeschlagen.");
      return;
    }
    sendTelegramMessage("Config gespeichert.");
    return;
  }
  sendTelegramMessage("Unbekannter Befehl. Nutze /start.");
}

void AlarmManager::pollTelegramCommands(uint32_t nowMs) {
  if (cfg_.alarm.telegramBotToken.empty()) return;
  if (nowMs - lastTelegramPollMs_ < 10000U) return;
  lastTelegramPollMs_ = nowMs;

  HTTPClient http;
  http.setTimeout(6000);
  String url = "https://api.telegram.org/bot" + String(cfg_.alarm.telegramBotToken.c_str()) + "/getUpdates?timeout=0";
  if (telegramUpdateOffset_ > 0) url += "&offset=" + String(static_cast<long long>(telegramUpdateOffset_));
  http.begin(url);
  const int code = http.GET();
  const String resp = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    Serial.printf("[Alarm][TelegramCmd] poll failed status=%d response=%s\n", code, resp.c_str());
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, resp)) return;
  JsonArray updates = doc["result"].as<JsonArray>();
  for (JsonObject u : updates) {
    telegramUpdateOffset_ = u["update_id"].as<int64_t>() + 1;
    String text = u["message"]["text"].as<const char *>();
    if (text.length() > 0) handleTelegramCommand(text);
  }
}

void AlarmManager::loop(uint32_t nowMs, const PressureReading &reading, PressureState state) {
  lastReading_ = reading;
  pollTelegramCommands(nowMs);

  if (!isAlarmState(state)) { firstAlarmSent_ = false; lastState_ = state; return; }
  const uint32_t repeatMs = static_cast<uint32_t>(cfg_.alarm.repeatMinutes) * 60U * 1000U;
  const bool stateChanged = state != lastState_;
  const bool dueRepeat = firstAlarmSent_ && (nowMs - lastAlertMs_ >= repeatMs);
  if (!firstAlarmSent_ || stateChanged || dueRepeat) {
    String text = "Heizungsdruck Alarm: "; text += stateName(state); text += " | pressure="; text += String(reading.pressureBar, 2); text += " bar";
    sendTelegramMessage(text);
    sendWebhookTest(reading, state, stateChanged ? "state_change" : "repeat");
    firstAlarmSent_ = true; lastAlertMs_ = nowMs;
  }
  lastState_ = state;
}

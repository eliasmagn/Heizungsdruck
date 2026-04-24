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
}  // namespace

void AlarmManager::begin(const AppConfig &cfg) {
  cfg_ = cfg;
  lastState_ = PressureState::UNKNOWN;
  firstAlarmSent_ = false;
  lastAlertMs_ = 0;
}

void AlarmManager::updateConfig(const AppConfig &cfg) { cfg_ = cfg; }

bool AlarmManager::isAlarmState(PressureState state) const {
  return state == PressureState::PRESSURE_LOW || state == PressureState::PRESSURE_HIGH ||
         state == PressureState::SENSOR_FAULT;
}

void AlarmManager::sendTelegram(const String &text) {
  if (cfg_.alarm.telegramBotToken.empty() || cfg_.alarm.telegramChatId.empty()) return;
  HTTPClient http;
  const String url = "https://api.telegram.org/bot" + String(cfg_.alarm.telegramBotToken.c_str()) + "/sendMessage";
  JsonDocument payload;
  payload["chat_id"] = cfg_.alarm.telegramChatId.c_str();
  payload["text"] = text;
  String body;
  serializeJson(payload, body);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.POST(body);
  http.end();
}

void AlarmManager::sendWebhook(const PressureReading &reading, PressureState state, const String &event) {
  if (cfg_.alarm.emailWebhookUrl.empty()) return;
  HTTPClient http;
  http.begin(cfg_.alarm.emailWebhookUrl.c_str());
  http.addHeader("Content-Type", "application/json");
  JsonDocument payload;
  payload["event"] = event;
  payload["state"] = stateName(state);
  payload["pressureBar"] = reading.pressureBar;
  payload["rawAdc"] = reading.rawAdc;
  payload["valid"] = reading.valid;
  String body;
  serializeJson(payload, body);
  http.POST(body);
  http.end();
}

void AlarmManager::loop(uint32_t nowMs, const PressureReading &reading, PressureState state) {
  if (!isAlarmState(state)) {
    firstAlarmSent_ = false;
    lastState_ = state;
    return;
  }

  const uint32_t repeatMs = static_cast<uint32_t>(cfg_.alarm.repeatMinutes) * 60U * 1000U;
  const bool stateChanged = state != lastState_;
  const bool dueRepeat = firstAlarmSent_ && (nowMs - lastAlertMs_ >= repeatMs);

  if (!firstAlarmSent_ || stateChanged || dueRepeat) {
    String text = "Heizungsdruck Alarm: ";
    text += stateName(state);
    text += " | pressure=";
    text += String(reading.pressureBar, 2);
    text += " bar";
    sendTelegram(text);
    sendWebhook(reading, state, stateChanged ? "state_change" : "repeat");
    firstAlarmSent_ = true;
    lastAlertMs_ = nowMs;
  }

  lastState_ = state;
}

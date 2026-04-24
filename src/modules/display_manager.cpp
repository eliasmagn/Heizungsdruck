#include "display_manager.h"

#include <math.h>

DisplayManager::DisplayManager(TwoWire *wire)
    : wire_(wire), display_(kWidth, kHeight, wire, -1) {}

bool DisplayManager::begin() {
  wire_->begin(kSdaPin, kSclPin);
  initialized_ = display_.begin(SSD1306_SWITCHCAPVCC, kI2cAddress);
  if (!initialized_) return false;

  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextColor(SSD1306_WHITE);
  display_.display();
  return true;
}

void DisplayManager::showBootScreen() {
  if (!initialized_) return;
  display_.clearDisplay();
  display_.setTextSize(2);
  display_.setCursor(8, 12);
  display_.print(F("Heizungsdruck"));
  display_.setTextSize(1);
  display_.setCursor(30, 38);
  display_.print(F("Starte..."));
  display_.display();
}

void DisplayManager::showError(const char *message) {
  if (!initialized_) return;
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setCursor(0, 2);
  display_.print(F("Display Fehler"));
  display_.drawLine(0, 11, kWidth - 1, 11, SSD1306_WHITE);
  display_.setCursor(0, 18);
  display_.print(message != nullptr ? message : "unknown");
  display_.display();
}

void DisplayManager::update(const DisplayState &state) {
  if (!initialized_) return;

  if (!state.alarm_active) {
    showAlarmHeader_ = true;
    lastAlarmHeaderSwitchMs_ = millis();
  } else if (millis() - lastAlarmHeaderSwitchMs_ > 3000) {
    showAlarmHeader_ = !showAlarmHeader_;
    lastAlarmHeaderSwitchMs_ = millis();
  }

  if (!stateChanged(state)) return;

  drawFrame(state);
  lastState_ = state;
  hasLastState_ = true;
}

String DisplayManager::buildTopStatus(const DisplayState &state) const {
  if (state.alarm_active && showAlarmHeader_) return F("ALARM");

  String top;
  if (state.wifi_connected) {
    top = F("WLAN ");
    top += shorten(state.wifi_ssid, 12);
  } else {
    top = F("AP ");
    top += shorten(state.ap_ssid, 14);
  }

  if (state.wireguard_enabled) {
    top += state.wireguard_online ? F(" WG") : F(" wg?");
  }
  return shorten(top, 20);
}

String DisplayManager::buildBottomStatus(const DisplayState &state) const {
  if (state.ap_mode) {
    String pw = shorten(state.ap_password, 10);
    if (pw.isEmpty()) pw = F("-");
    return String(F("PW:")) + pw;
  }

  if (state.alarm_active) {
    String line = F("ALARM < ");
    line += String(state.low_alarm_bar, 2);
    line += F(" bar");
    return shorten(line, 20);
  }

  String line = F("U ");
  if (isnan(state.adc_voltage)) {
    line += F("---");
  } else {
    line += String(state.adc_voltage, 2);
  }
  line += F("V");
  return line;
}

void DisplayManager::drawFrame(const DisplayState &state) {
  display_.clearDisplay();

  display_.setTextSize(1);
  display_.setCursor(0, 2);
  display_.print(buildTopStatus(state));
  display_.drawLine(0, kStatusHeight, kWidth - 1, kStatusHeight, SSD1306_WHITE);

  display_.setTextSize(3);
  String value = state.pressure_valid && !isnan(state.pressure_bar) ? String(state.pressure_bar, 2) : String(F("---"));
  int16_t x1, y1;
  uint16_t w, h;
  display_.getTextBounds(value, 0, 0, &x1, &y1, &w, &h);
  int16_t centerY = kStatusHeight + ((kHeight - kStatusHeight - 10) / 2);
  int16_t valueX = (kWidth - static_cast<int16_t>(w) - 22) / 2;
  if (valueX < 0) valueX = 0;
  int16_t valueY = centerY - (static_cast<int16_t>(h) / 2);
  if (valueY < kStatusHeight + 2) valueY = kStatusHeight + 2;
  display_.setCursor(valueX, valueY);
  display_.print(value);

  display_.setTextSize(1);
  display_.setCursor(valueX + w + 2, valueY + h - 9);
  display_.print(F("bar"));

  display_.setCursor(0, kHeight - 8);
  display_.print(buildBottomStatus(state));

  display_.display();
}

bool DisplayManager::nearlyEqual(float a, float b, float eps) {
  return fabsf(a - b) <= eps;
}

bool DisplayManager::stateChanged(const DisplayState &state) const {
  if (!hasLastState_) return true;

  if (state.wifi_connected != lastState_.wifi_connected) return true;
  if (state.ap_mode != lastState_.ap_mode) return true;
  if (state.wireguard_enabled != lastState_.wireguard_enabled) return true;
  if (state.wireguard_online != lastState_.wireguard_online) return true;
  if (state.alarm_active != lastState_.alarm_active) return true;
  if (state.pressure_valid != lastState_.pressure_valid) return true;
  if (!nearlyEqual(state.pressure_bar, lastState_.pressure_bar, 0.01f)) return true;
  if (!nearlyEqual(state.adc_voltage, lastState_.adc_voltage, 0.01f)) return true;
  if (!nearlyEqual(state.low_alarm_bar, lastState_.low_alarm_bar, 0.01f)) return true;
  if (shorten(state.wifi_ssid, 12) != shorten(lastState_.wifi_ssid, 12)) return true;
  if (shorten(state.ap_ssid, 14) != shorten(lastState_.ap_ssid, 14)) return true;
  if (shorten(state.ap_password, 10) != shorten(lastState_.ap_password, 10)) return true;

  return false;
}

String DisplayManager::shorten(const String &input, size_t maxChars) {
  if (input.length() <= static_cast<unsigned int>(maxChars)) return input;
  if (maxChars <= 1) return String("?");
  return input.substring(0, maxChars - 1) + "~";
}

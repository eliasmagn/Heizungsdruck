#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

struct DisplayState {
  bool wifi_connected{false};
  String wifi_ssid;
  bool ap_mode{false};
  String ap_ssid;
  String ap_password;
  bool wireguard_enabled{false};
  bool wireguard_online{false};
  bool alarm_active{false};
  float pressure_bar{0.0f};
  bool pressure_valid{false};
  float adc_voltage{0.0f};
  float low_alarm_bar{0.0f};
};

class DisplayManager {
 public:
  explicit DisplayManager(TwoWire *wire = &Wire);

  bool begin();
  void update(const DisplayState &state);
  void showBootScreen();
  void showError(const char *message);

 private:
  static constexpr uint8_t kWidth = 128;
  static constexpr uint8_t kHeight = 64;
  static constexpr uint8_t kStatusHeight = 12;
  static constexpr uint8_t kI2cAddress = 0x3C;
  static constexpr uint8_t kSdaPin = 21;
  static constexpr uint8_t kSclPin = 22;

  String buildTopStatus(const DisplayState &state) const;
  String buildBottomStatus(const DisplayState &state) const;
  static String shorten(const String &input, size_t maxChars);
  static bool nearlyEqual(float a, float b, float eps);
  bool stateChanged(const DisplayState &state) const;
  void drawFrame(const DisplayState &state);

  TwoWire *wire_;
  Adafruit_SSD1306 display_;
  DisplayState lastState_{};
  bool initialized_{false};
  bool hasLastState_{false};
  uint32_t lastAlarmHeaderSwitchMs_{0};
  bool showAlarmHeader_{true};
};

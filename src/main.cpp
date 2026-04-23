#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_system.h>

#include "config/ProjectConfig.h"
#include "modules/AppConfig.h"
#include "modules/AlarmManager.h"
#include "modules/ConfigStore.h"
#include "modules/MqttManager.h"
#include "modules/PressureHistory.h"
#include "modules/PressureSensor.h"
#include "modules/PressureStateMachine.h"
#include "modules/WebUI.h"

namespace {
ConfigStore gStore;
AppConfig gConfig;
PressureSensor *gSensor = nullptr;
PressureStateMachine *gStateMachine = nullptr;
PressureHistory gHistory(240);
MqttManager gMqtt;
AlarmManager gAlarm;
WebUI gWeb;

PressureReading gLastReading;
PressureState gLastState = PressureState::UNKNOWN;
uint32_t gLastSampleMs = 0;

String generateApPassword() {
  uint32_t rnd = esp_random();
  char buf[11];
  snprintf(buf, sizeof(buf), "%08lx", static_cast<unsigned long>(rnd));
  return String(buf);
}

bool saveCfg(const AppConfig &cfg) {
  if (!gStore.save(cfg)) return false;
  gConfig = cfg;
  if (gSensor) gSensor->updateConfig(gConfig);
  if (gStateMachine) gStateMachine->updateConfig(gConfig);
  gMqtt.begin(gConfig);
  gAlarm.updateConfig(gConfig);
  return true;
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(gConfig.network.hostname.c_str());
  const char *ssid = gConfig.network.wifiSsid.empty() ? WIFI_SSID : gConfig.network.wifiSsid.c_str();
  const char *pass = gConfig.network.wifiPassword.empty() ? WIFI_PASSWORD : gConfig.network.wifiPassword.c_str();
  WiFi.begin(ssid, pass);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP_STA);
    if (gConfig.network.apPassword.empty()) {
      gConfig.network.apPassword = generateApPassword().c_str();
      gStore.save(gConfig);
    }
    WiFi.softAP(gConfig.network.apSsid.c_str(), gConfig.network.apPassword.c_str());
    Serial.printf("Setup AP started: %s / %s\n", gConfig.network.apSsid.c_str(), gConfig.network.apPassword.c_str());
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  gStore.begin();
  gConfig = gStore.load();
  if (gConfig.network.hostname.empty()) gConfig.network.hostname = DEVICE_HOSTNAME;
  if (gConfig.mqtt.host.empty()) gConfig.mqtt.host = MQTT_HOST;
  if (gConfig.mqtt.port == 0) gConfig.mqtt.port = MQTT_PORT;
  if (gConfig.mqtt.username.empty()) gConfig.mqtt.username = MQTT_USERNAME;
  if (gConfig.mqtt.password.empty()) gConfig.mqtt.password = MQTT_PASSWORD;
  gConfig.mqtt.enabled = !gConfig.mqtt.host.empty();

  connectWifi();
  ArduinoOTA.setHostname(gConfig.network.hostname.c_str());
  ArduinoOTA.begin();

  static PressureSensor sensor(gConfig);
  static PressureStateMachine stateMachine(gConfig);
  gSensor = &sensor;
  gStateMachine = &stateMachine;

  gSensor->begin();

  gMqtt.begin(gConfig);
  gAlarm.begin(gConfig);
  gWeb.attachConfig(&gConfig, saveCfg);
  gWeb.attachHistory(&gHistory);
  gWeb.begin();
}

void loop() {
  const uint32_t now = millis();

  if (now - gLastSampleMs >= gConfig.sensor.updateIntervalMs) {
    gLastSampleMs = now;
    gLastReading = gSensor->sample(now);
    gLastState = gStateMachine->update(gLastReading);
    gHistory.add(gLastReading, gLastState);
  }

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  gMqtt.loop(now);
  gMqtt.publishReading(gLastReading, gLastState, wifiConnected, now / 1000);
  gAlarm.loop(now, gLastReading, gLastState);

  gWeb.updateLiveData(gLastReading, gLastState, wifiConnected, gMqtt.connected(), now / 1000);
  gWeb.loop();
  ArduinoOTA.handle();

  delay(5);
}

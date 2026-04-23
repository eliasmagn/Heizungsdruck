#include <Arduino.h>
#include <WiFi.h>

#include "config/ProjectConfig.h"
#include "modules/AppConfig.h"
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
WebUI gWeb;

PressureReading gLastReading;
PressureState gLastState = PressureState::UNKNOWN;
uint32_t gLastSampleMs = 0;

bool saveCfg(const AppConfig &cfg) { return gStore.save(cfg); }

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  gStore.begin();
  gConfig = gStore.load();
  gConfig.network.hostname = DEVICE_HOSTNAME;
  gConfig.mqtt.host = MQTT_HOST;
  gConfig.mqtt.port = MQTT_PORT;
  gConfig.mqtt.username = MQTT_USERNAME;
  gConfig.mqtt.password = MQTT_PASSWORD;
  gConfig.mqtt.enabled = strlen(MQTT_HOST) > 0;

  connectWifi();

  static PressureSensor sensor(gConfig);
  static PressureStateMachine stateMachine(gConfig);
  gSensor = &sensor;
  gStateMachine = &stateMachine;

  gSensor->begin();

  gMqtt.begin(gConfig);
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

  gWeb.updateLiveData(gLastReading, gLastState, wifiConnected, gMqtt.connected(), now / 1000);
  gWeb.loop();

  delay(5);
}

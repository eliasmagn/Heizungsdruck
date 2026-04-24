#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_system.h>

#include "config/ProjectConfig.h"
#include "modules/AppConfig.h"
#include "modules/AlarmManager.h"
#include "modules/ConfigStore.h"
#include "modules/JsonCodec.h"
#include "modules/MqttManager.h"
#include "modules/display_manager.h"
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
DisplayManager gDisplay;

PressureReading gLastReading;
PressureState gLastState = PressureState::UNKNOWN;
uint32_t gLastSampleMs = 0;
uint32_t gLastMinimalLogMs = 0;
bool gDebugVerbose = false;
constexpr uint32_t kEnforcedPollIntervalMs = 100;

bool bridgeDebugEnabled() {
  pinMode(25, OUTPUT);
  digitalWrite(25, LOW);
  pinMode(26, INPUT_PULLUP);
  delay(2);
  int lowReads = 0;
  for (int i = 0; i < 5; ++i) {
    if (digitalRead(26) == LOW) lowReads++;
    delay(1);
  }
  return lowReads >= 4;
}

void logConfigIfVerbose() {
  if (!gDebugVerbose) return;
  const std::string cfgJson = configToJson(gConfig);
  Serial.println("==== CONFIG DUMP (DEBUG BRIDGE D25<->D26) ====");
  Serial.println(cfgJson.c_str());
  Serial.println("==============================================");
}

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

bool connectWithCreds(const char *ssid, const char *pass, uint32_t timeoutMs) {
  if (ssid == nullptr || strlen(ssid) == 0) return false;
  WiFi.begin(ssid, pass);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(gConfig.network.hostname.c_str());
  const String configuredSsid = gConfig.network.wifiSsid.c_str();
  const String configuredPass = gConfig.network.wifiPassword.c_str();
  const String secretsSsid = WIFI_SSID;
  const String secretsPass = WIFI_PASSWORD;

  bool connected = false;
  String usedSsid;

  if (!configuredSsid.isEmpty()) {
    if (gDebugVerbose) Serial.printf("WiFi: try configured SSID '%s'\n", configuredSsid.c_str());
    connected = connectWithCreds(configuredSsid.c_str(), configuredPass.c_str(), WIFI_CONNECT_TIMEOUT_MS);
    usedSsid = configuredSsid;
  } else if (!secretsSsid.isEmpty()) {
    if (gDebugVerbose) Serial.printf("WiFi: try secrets SSID '%s'\n", secretsSsid.c_str());
    connected = connectWithCreds(secretsSsid.c_str(), secretsPass.c_str(), WIFI_CONNECT_TIMEOUT_MS);
    usedSsid = secretsSsid;
  }

  if (!connected && !secretsSsid.isEmpty() && configuredSsid != secretsSsid) {
    if (gDebugVerbose) Serial.printf("WiFi: fallback to secrets SSID '%s'\n", secretsSsid.c_str());
    connected = connectWithCreds(secretsSsid.c_str(), secretsPass.c_str(), WIFI_CONNECT_TIMEOUT_MS);
    usedSsid = secretsSsid;
    if (connected) {
      gConfig.network.wifiSsid = secretsSsid.c_str();
      gConfig.network.wifiPassword = secretsPass.c_str();
      gStore.save(gConfig);
    }
  }

  if (connected) {
    Serial.printf("WiFi connected: SSID=%s IP=%s\n", usedSsid.c_str(), WiFi.localIP().toString().c_str());
    return;
  }

  if (!connected) {
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
  gDebugVerbose = bridgeDebugEnabled();
  Serial.printf("Debug mode: %s (bridge D25<->D26 %s)\n", gDebugVerbose ? "VERBOSE" : "MINIMAL",
                gDebugVerbose ? "detected" : "not detected");

  gStore.begin();
  gConfig = gStore.load();
  if (gConfig.network.hostname.empty()) gConfig.network.hostname = DEVICE_HOSTNAME;
  if (gConfig.mqtt.host.empty()) gConfig.mqtt.host = MQTT_HOST;
  if (gConfig.mqtt.port == 0) gConfig.mqtt.port = MQTT_PORT;
  if (gConfig.mqtt.username.empty()) gConfig.mqtt.username = MQTT_USERNAME;
  if (gConfig.mqtt.password.empty()) gConfig.mqtt.password = MQTT_PASSWORD;
  gConfig.mqtt.enabled = !gConfig.mqtt.host.empty();
  // Enforce fast constant polling (battery is not a concern for this project).
  if (gConfig.sensor.updateIntervalMs != kEnforcedPollIntervalMs) {
    gConfig.sensor.updateIntervalMs = kEnforcedPollIntervalMs;
    gStore.save(gConfig);
  }
  logConfigIfVerbose();

  connectWifi();
  if (!gDisplay.begin()) {
    Serial.println("Display init failed");
  } else {
    gDisplay.showBootScreen();
  }
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

  DisplayState ds;
  ds.wifi_connected = wifiConnected;
  ds.wifi_ssid = WiFi.SSID();
  ds.ap_mode = !wifiConnected;
  ds.ap_ssid = gConfig.network.apSsid.c_str();
  ds.ap_password = gConfig.network.apPassword.c_str();
  ds.wireguard_enabled = gConfig.wireguard.enabled;
  ds.wireguard_online = false;
  ds.alarm_active = (gLastState == PressureState::PRESSURE_LOW || gLastState == PressureState::PRESSURE_HIGH ||
                     gLastState == PressureState::SENSOR_FAULT);
  ds.pressure_bar = gLastReading.pressureBar;
  ds.pressure_valid = gLastReading.valid;
  ds.adc_voltage = gLastReading.voltage;
  ds.low_alarm_bar = gConfig.alarm.lowBar;
  gDisplay.update(ds);

  if (!gDebugVerbose && now - gLastMinimalLogMs > 10000) {
    gLastMinimalLogMs = now;
    Serial.printf("WiFi:%s IP:%s Pressure:%.2fbar Valid:%d\n", wifiConnected ? "ok" : "no",
                  WiFi.localIP().toString().c_str(), gLastReading.pressureBar, gLastReading.valid ? 1 : 0);
  } else if (gDebugVerbose && now - gLastMinimalLogMs > 5000) {
    gLastMinimalLogMs = now;
    Serial.printf("[DBG] state=%d fault=%d adc=%d filt=%d V=%.3f bar=%.3f wifi=%d mqtt=%d heap=%u\n",
                  static_cast<int>(gLastState), static_cast<int>(gLastReading.fault), gLastReading.rawAdc,
                  gLastReading.filteredAdc, gLastReading.voltage, gLastReading.pressureBar, wifiConnected ? 1 : 0,
                  gMqtt.connected() ? 1 : 0, ESP.getFreeHeap());
  }

  delay(1);
}

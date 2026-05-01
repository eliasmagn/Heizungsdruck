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
#include "modules/WireGuardManager.h"

#ifndef SENSOR_ADC_PIN_DEFAULT
#define SENSOR_ADC_PIN_DEFAULT 34
#endif
#ifndef SENSOR_SAMPLE_COUNT_DEFAULT
#define SENSOR_SAMPLE_COUNT_DEFAULT 9
#endif
#ifndef SENSOR_UPDATE_INTERVAL_MS_DEFAULT
#define SENSOR_UPDATE_INTERVAL_MS_DEFAULT 100
#endif
#ifndef SENSOR_POWER_PIN
#define SENSOR_POWER_PIN -1
#endif
#ifndef DEBUG_BRIDGE_OUT_PIN
#define DEBUG_BRIDGE_OUT_PIN 25
#endif
#ifndef DEBUG_BRIDGE_IN_PIN
#define DEBUG_BRIDGE_IN_PIN 26
#endif
#ifndef WIREGUARD_ENABLED_DEFAULT
#define WIREGUARD_ENABLED_DEFAULT 0
#endif
#ifndef WIREGUARD_LOCAL_ADDRESS
#define WIREGUARD_LOCAL_ADDRESS ""
#endif
#ifndef WIREGUARD_NETMASK
#define WIREGUARD_NETMASK "255.255.255.0"
#endif
#ifndef WIREGUARD_PRIVATE_KEY
#define WIREGUARD_PRIVATE_KEY ""
#endif
#ifndef WIREGUARD_PEER_ENDPOINT
#define WIREGUARD_PEER_ENDPOINT ""
#endif
#ifndef WIREGUARD_PEER_PORT
#define WIREGUARD_PEER_PORT 0
#endif
#ifndef WIREGUARD_PEER_PUBLIC_KEY
#define WIREGUARD_PEER_PUBLIC_KEY ""
#endif
#ifndef WIREGUARD_PRESHARED_KEY
#define WIREGUARD_PRESHARED_KEY ""
#endif
#ifndef WIREGUARD_ALLOWED_IP1
#define WIREGUARD_ALLOWED_IP1 ""
#endif
#ifndef WIREGUARD_ALLOWED_IP2
#define WIREGUARD_ALLOWED_IP2 ""
#endif
#ifndef WIREGUARD_KEEPALIVE_SECONDS
#define WIREGUARD_KEEPALIVE_SECONDS 0
#endif

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
WireGuardManager gWireGuard;

PressureReading gLastReading;
PressureState gLastState = PressureState::UNKNOWN;
uint32_t gLastSampleMs = 0;
uint32_t gLastMinimalLogMs = 0;
bool gDebugVerbose = false;
DisplayState gDisplayState;
portMUX_TYPE gDisplayMux = portMUX_INITIALIZER_UNLOCKED;

void displayTask(void *) {
  DisplayState local;
  for (;;) {
    taskENTER_CRITICAL(&gDisplayMux);
    local = gDisplayState;
    taskEXIT_CRITICAL(&gDisplayMux);
    gDisplay.update(local);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

bool bridgeDebugEnabled() {
  pinMode(DEBUG_BRIDGE_OUT_PIN, OUTPUT);
  digitalWrite(DEBUG_BRIDGE_OUT_PIN, LOW);
  pinMode(DEBUG_BRIDGE_IN_PIN, INPUT_PULLUP);
  delay(2);
  int lowReads = 0;
  for (int i = 0; i < 5; ++i) {
    if (digitalRead(DEBUG_BRIDGE_IN_PIN) == LOW) lowReads++;
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
  if (gConfig.wireguard.enabled) {
    gWireGuard.enable(gConfig.wireguard);
  } else {
    gWireGuard.disable();
  }
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

wifi_power_t txPowerFromDbm(float dbm) {
  if (dbm <= 2.0f) return WIFI_POWER_2dBm;
  if (dbm <= 5.0f) return WIFI_POWER_5dBm;
  if (dbm <= 7.0f) return WIFI_POWER_7dBm;
  if (dbm <= 8.5f) return WIFI_POWER_8_5dBm;
  if (dbm <= 11.0f) return WIFI_POWER_11dBm;
  if (dbm <= 13.0f) return WIFI_POWER_13dBm;
  if (dbm <= 15.0f) return WIFI_POWER_15dBm;
  if (dbm <= 17.0f) return WIFI_POWER_17dBm;
  if (dbm <= 18.5f) return WIFI_POWER_18_5dBm;
  return WIFI_POWER_19_5dBm;
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(gConfig.network.hostname.c_str());
  WiFi.setTxPower(txPowerFromDbm(gConfig.network.wifiTxPowerDbm));
  const wifi_protocol_t protocol = gConfig.network.wifi11bMode ? WIFI_PROTOCOL_11B : WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N;
  WiFi.setProtocol(protocol);
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
  if (gConfig.sensor.adcPin == 34) gConfig.sensor.adcPin = SENSOR_ADC_PIN_DEFAULT;
  if (gConfig.sensor.sampleCount == 9) gConfig.sensor.sampleCount = SENSOR_SAMPLE_COUNT_DEFAULT;
  if (gConfig.mqtt.host.empty()) gConfig.mqtt.host = MQTT_HOST;
  if (gConfig.mqtt.port == 0) gConfig.mqtt.port = MQTT_PORT;
  if (gConfig.mqtt.username.empty()) gConfig.mqtt.username = MQTT_USERNAME;
  if (gConfig.mqtt.password.empty()) gConfig.mqtt.password = MQTT_PASSWORD;
  if (gConfig.wireguard.localAddress.empty()) gConfig.wireguard.localAddress = WIREGUARD_LOCAL_ADDRESS;
  if (gConfig.wireguard.netmask.empty()) gConfig.wireguard.netmask = WIREGUARD_NETMASK;
  if (gConfig.wireguard.privateKey.empty()) gConfig.wireguard.privateKey = WIREGUARD_PRIVATE_KEY;
  if (gConfig.wireguard.peerEndpoint.empty()) gConfig.wireguard.peerEndpoint = WIREGUARD_PEER_ENDPOINT;
  if (gConfig.wireguard.peerPort == 0) gConfig.wireguard.peerPort = WIREGUARD_PEER_PORT;
  if (gConfig.wireguard.peerPublicKey.empty()) gConfig.wireguard.peerPublicKey = WIREGUARD_PEER_PUBLIC_KEY;
  if (gConfig.wireguard.presharedKey.empty()) gConfig.wireguard.presharedKey = WIREGUARD_PRESHARED_KEY;
  if (gConfig.wireguard.allowedIp1.empty()) gConfig.wireguard.allowedIp1 = WIREGUARD_ALLOWED_IP1;
  if (gConfig.wireguard.allowedIp2.empty()) gConfig.wireguard.allowedIp2 = WIREGUARD_ALLOWED_IP2;
  if (gConfig.wireguard.keepAliveSeconds == 0) gConfig.wireguard.keepAliveSeconds = WIREGUARD_KEEPALIVE_SECONDS;
  if (!gConfig.wireguard.enabled && WIREGUARD_ENABLED_DEFAULT) gConfig.wireguard.enabled = true;
  gConfig.mqtt.enabled = !gConfig.mqtt.host.empty();
  // Enforce fast constant polling (battery is not a concern for this project).
  if (gConfig.sensor.updateIntervalMs != SENSOR_UPDATE_INTERVAL_MS_DEFAULT) {
    gConfig.sensor.updateIntervalMs = SENSOR_UPDATE_INTERVAL_MS_DEFAULT;
    gStore.save(gConfig);
  }
  logConfigIfVerbose();

  if (SENSOR_POWER_PIN >= 0) {
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, HIGH);
  }

  connectWifi();
  if (!gDisplay.begin()) {
    Serial.println("Display init failed");
  } else {
    gDisplay.showBootScreen();
    xTaskCreatePinnedToCore(displayTask, "displayTask", 4096, nullptr, 1, nullptr, 0);
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
  gWeb.attachWireGuardManager(&gWireGuard);
  gWireGuard.begin(gConfig.wireguard);
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

  gWireGuard.loop(now / 1000);
  gWeb.updateLiveData(gLastReading, gLastState, wifiConnected, gMqtt.connected(), now / 1000);
  gWeb.loop();
  ArduinoOTA.handle();

  DisplayState ds;
  ds.wifi_connected = wifiConnected;
  ds.wifi_ssid = WiFi.SSID();
  ds.ap_mode = !wifiConnected;
  ds.ap_ssid = gConfig.network.apSsid.c_str();
  ds.ap_password = gConfig.network.apPassword.c_str();
  const WireGuardStatus wgStatus = gWireGuard.status();
  ds.wireguard_enabled = wgStatus.enabled;
  ds.wireguard_online = wgStatus.online;
  ds.alarm_active = (gLastState == PressureState::PRESSURE_LOW || gLastState == PressureState::PRESSURE_HIGH ||
                     gLastState == PressureState::SENSOR_FAULT);
  ds.pressure_bar = gLastReading.pressureBar;
  ds.pressure_valid = gLastReading.valid;
  ds.adc_voltage = gLastReading.voltage;
  ds.low_alarm_bar = gConfig.alarm.lowBar;
  taskENTER_CRITICAL(&gDisplayMux);
  gDisplayState = ds;
  taskEXIT_CRITICAL(&gDisplayMux);

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

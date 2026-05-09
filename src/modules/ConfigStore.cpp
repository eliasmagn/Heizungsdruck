#include "ConfigStore.h"
#include <LittleFS.h>

#if defined(ESP32)
#include <Preferences.h>
#endif

#include "JsonCodec.h"

namespace {
#if defined(ESP32)
Preferences prefs;
constexpr const char *kNs = "heizungsdruck";
constexpr const char *kKey = "appcfg";
#else
constexpr const char *kConfigPath = "/appcfg.json";
#endif
}

bool ConfigStore::begin() {
#if defined(ESP32)
  return prefs.begin(kNs, false);
#else
  return LittleFS.begin();
#endif
}

AppConfig ConfigStore::load() {
  AppConfig cfg = defaultConfig();
  String json;
#if defined(ESP32)
  if (!prefs.isKey(kKey)) return cfg;
  json = prefs.getString(kKey, "");
#else
  if (!LittleFS.exists(kConfigPath)) return cfg;
  File file = LittleFS.open(kConfigPath, "r");
  if (!file) return cfg;
  json = file.readString();
  file.close();
#endif
  if (json.isEmpty()) {
    return cfg;
  }

  std::string err;
  AppConfig loaded = cfg;
  if (configFromJson(std::string(json.c_str()), loaded, err)) {
    return loaded;
  }
  return cfg;
}

bool ConfigStore::save(const AppConfig &cfg) {
  std::string err;
  if (!cfg.validate(err)) return false;
  auto json = configToJson(cfg);
#if defined(ESP32)
  return prefs.putString(kKey, json.c_str()) > 0;
#else
  File file = LittleFS.open(kConfigPath, "w");
  if (!file) return false;
  const size_t written = file.print(json.c_str());
  file.close();
  return written == json.size();
#endif
}

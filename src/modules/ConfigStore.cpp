#include "ConfigStore.h"

#include <Preferences.h>

#include "JsonCodec.h"

namespace {
Preferences prefs;
constexpr const char *kNs = "heizungsdruck";
constexpr const char *kKey = "appcfg";
}

bool ConfigStore::begin() { return prefs.begin(kNs, false); }

AppConfig ConfigStore::load() {
  AppConfig cfg = defaultConfig();
  if (!prefs.isKey(kKey)) {
    return cfg;
  }

  String json = prefs.getString(kKey, "");
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
  return prefs.putString(kKey, json.c_str()) > 0;
}

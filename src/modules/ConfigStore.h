#pragma once

#include "AppConfig.h"

class ConfigStore {
 public:
  bool begin();
  AppConfig load();
  bool save(const AppConfig &cfg);
};

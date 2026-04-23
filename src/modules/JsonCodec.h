#pragma once

#include <string>

#include "AppConfig.h"

std::string configToJson(const AppConfig &cfg);
bool configFromJson(const std::string &json, AppConfig &cfgOut, std::string &error);

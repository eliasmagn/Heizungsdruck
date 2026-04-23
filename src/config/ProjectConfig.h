#pragma once

// Use explicit relative include names to avoid accidentally picking up framework/system config.h
#if __has_include("config/config.h")
#include "config/config.h"
#else
#include "config/config.h.example"
#warning "Using config.h.example defaults. Copy src/config/config.h.example to src/config/config.h"
#endif

#if __has_include("config/secrets.h")
#include "config/secrets.h"
#else
#include "config/secrets.h.example"
#warning "Using secrets.h.example defaults. Copy src/config/secrets.h.example to src/config/secrets.h"
#endif

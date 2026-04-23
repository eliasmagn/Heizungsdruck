#pragma once

#if __has_include("config.h")
#include "config.h"
#else
#include "config.h.example"
#warning "Using config.h.example defaults. Copy src/config/config.h.example to src/config/config.h"
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.h.example"
#warning "Using secrets.h.example defaults. Copy src/config/secrets.h.example to src/config/secrets.h"
#endif

#pragma once

#if defined(ESP8266)
#define TARGET_ESP8266 1
#else
#define TARGET_ESP8266 0
#endif

#if defined(ESP32)
#define TARGET_ESP32 1
#else
#define TARGET_ESP32 0
#endif

#if TARGET_ESP32
#define HAS_FULL_WEBUI 1
#define HAS_OPTIONAL_TEMPERATURE 1
#define HAS_MULTI_ANALOG_CHANNELS 1
#else
#define HAS_FULL_WEBUI 0
#define HAS_OPTIONAL_TEMPERATURE 1
#define HAS_MULTI_ANALOG_CHANNELS 0
#endif

#if TARGET_ESP32
#define HAS_WIREGUARD 1
#else
#define HAS_WIREGUARD 0
#endif

#if TARGET_ESP32 && __has_include("driver/adc.h") && __has_include("esp_adc/adc_continuous.h")
#define HAS_ADC_CONTINUOUS 1
#else
#define HAS_ADC_CONTINUOUS 0
#endif

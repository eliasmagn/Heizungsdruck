# Heizungsdruck Monitor (PlatformIO Firmware)

Lean ESP32 firmware for heating pressure monitoring with local web app, persistent config, and MQTT telemetry.

## Migration note (from ESPHome to custom firmware)
This repository is now a **full PlatformIO project** and no longer ESPHome-centric. The old limitations (restricted UI/control paths) are replaced with modular C++ firmware and an embedded web application.

## Project purpose
- Robustly read an analog pressure transducer (10 bar type, operation focus 0–2 bar).
- Detect meaningful heating pressure states and sensor faults.
- Expose actionable local UI (dashboard/history/settings/diagnostics).
- Publish clean MQTT telemetry with reconnect behavior.
- Keep resource usage low and code maintainable.

## Hardware assumptions
- ESP32 dev board (`esp32dev`)
- Analog pressure transducer (0–10V or conditioned signal within ADC-safe range)
- ADC input on GPIO34 by default
- Proper voltage scaling and common ground

## Wiring overview
```
Pressure Sensor OUT -> Signal conditioning / divider -> ESP32 GPIO34 (ADC)
Pressure Sensor GND ---------------------------------> ESP32 GND
Pressure Sensor VCC ---------------------------------> Sensor supply (per datasheet)
```
> IMPORTANT: ESP32 ADC max input is ~3.3V.

## Architecture (concise)
- `PressureSensor`: sampling + robust filtering + fault classification
- `PressureMath`: ADC→voltage/bar conversion + calibration math
- `PressureStateMachine`: `UNKNOWN`, `SENSOR_FAULT`, `PRESSURE_LOW`, `OK`, `PRESSURE_HIGH` with hysteresis
- `PressureHistory`: bounded in-memory trend buffer
- `ConfigStore`: persistent config storage in NVS/Preferences
- `MqttManager`: telemetry + reconnect + status topics
- `WebUI`: local web app and JSON APIs

## Folder map
- `platformio.ini`
- `src/main.cpp`
- `src/modules/*.h|*.cpp`
- `src/config/config.h.example`
- `src/config/secrets.h.example`
- `src/config/ProjectConfig.h`
- `test/test_logic.cpp`

## Setup and secrets handling
1. Copy examples:
```bash
cp src/config/config.h.example src/config/config.h
cp src/config/secrets.h.example src/config/secrets.h
```
2. Edit `src/config/secrets.h` with real Wi-Fi/MQTT credentials.
3. `src/config/config.h` is for device-level overrides.

Tracked source never needs real credentials. `.gitignore` excludes local config/secrets.

## Build / flash / monitor
```bash
pio run
pio run -t upload
pio device monitor
```

## Web UI overview
- `/` Dashboard: live pressure, alarm state, connectivity, debug payload
- `/history` Trend chart (lightweight canvas)
- `/settings` threshold and offset updates
- `/diag` diagnostics + restart action

## Calibration procedure
1. Measure ADC at known low point (`adcLow`, `barLow`).
2. Measure ADC at known high point (`adcHigh`, `barHigh`).
3. Save values via config defaults or later extension in settings.
4. Optionally tune `offsetBar` for local correction.

Linear 2-point conversion is used:
`bar = barLow + (adc-adcLow)/(adcHigh-adcLow) * (barHigh-barLow) + offset`

## Threshold behavior
- Configurable `lowBar`, `highBar`, `hysteresisBar`
- Hysteresis prevents UI/alarm flicker near limits
- Faults are explicit (`SENSOR_FAULT`) and separate from low pressure

## Sensor fault handling
- short to GND (very low ADC)
- short to VCC/rail (very high ADC)
- disconnected (below disconnect threshold)
- implausible jump vs previous valid reading

On fault, reading is flagged invalid and last valid pressure is retained for context.

## MQTT topic map
Base topic: `heizungsdruck` (configurable)
- `heizungsdruck/telemetry` (JSON, periodic)
- `heizungsdruck/state` (retained state string)
- `heizungsdruck/status` (retained online/offline)
- Optional subscription: `heizungsdruck/cmd/restart`

## Reliability notes
- Main loop remains responsive (short loop delay).
- Wi-Fi/MQTT reconnect attempts are throttled.
- Invalid config falls back to defaults through validation.
- Project config loader uses explicit `config/...` include paths to avoid accidental framework `config.h` collisions.

## Testing & verification
Automated:
```bash
pio test -e native
```
Covers:
- ADC→bar conversion
- robust filter behavior with spike
- state/hysteresis transitions
- config validation
- config JSON roundtrip

Manual checklist:
1. Boot device and verify dashboard loads.
2. Check pressure changes in dashboard and history.
3. Force threshold crossing and verify state transitions.
4. Simulate sensor disconnect and verify `SENSOR_FAULT`.
5. Update settings in UI and verify persistence after reboot.
6. Verify MQTT publishes and offline/online status behavior.

## Troubleshooting
- `ModuleNotFoundError: intelhex`: run `pip install intelhex`.
- Build warnings about example config: copy `.example` files as described.
- No web UI: verify Wi-Fi connection and IP in serial monitor.
- Arduino compile error around `LOW`/`HIGH`: fixed by using non-Arduino-conflicting pressure enum names (`PRESSURE_LOW`/`PRESSURE_HIGH`).
- ArduinoJson v7 `MemberProxy ... is private`: avoid `auto section = doc["..."]` copies; use `JsonVariantConst` for section access.

## Limitations / future improvements
- Settings page currently exposes core threshold/offset values only.
- Optional WireGuard status/toggle can be added if networking stack requires it.
- Add OTA update page and structured config export/import endpoint.

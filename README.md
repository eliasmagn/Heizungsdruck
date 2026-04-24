# Heizungsdruck Monitor (PlatformIO Firmware)

Lean ESP32 firmware for heating pressure monitoring with local web app, persistent config, and MQTT telemetry.

## Migration note (from ESPHome to custom firmware)
This repository is now a **full PlatformIO project** and no longer ESPHome-centric. The old limitations (restricted UI/control paths) are replaced with modular C++ firmware and an embedded web application.

## Architekturentscheidung (ergänzt)
Für das Projektziel ist **eigene Firmware + eigene Webapp in PlatformIO** der saubere Weg. ESPHome bleibt nützlich für einfache "Sensor + MQTT"-Setups, ist für folgende Anforderungen aber zu eng:
- echte Konfigurationsseiten
- geführte Kalibrierung (21 Punkte)
- Passwort- und Netzwerkkonfiguration
- später modulare WireGuard-Steuerung
- saubere, erweiterbare Alarm- und UX-Logik

Deshalb bleibt der technische Schnitt:
- **ESP32/Arduino unter PlatformIO** als Backend
- **LittleFS + statische Web-Dateien** als Frontend-Auslieferung
- **JSON/REST API** als klare Schnittstelle zwischen Firmware und UI

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

## Build / flash / monitor / filesystem upload
```bash
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor
```

## Web UI overview
- LittleFS-hosted SPA (`data/index.html`, `data/app.js`, `data/style.css`)
- Dashboard: live pressure + history canvas
- Settings: network, MQTT, alarm channels
- Optional WireGuard control integration (status/enable/disable via configured URLs)
- Calibration: 21 points (0.0…10.0 bar in 0.5 steps), capture + clear
- Calibration table edit + persist from UI
- Config export/import via JSON textarea
- Diagnostics: telemetry dump, telegram/webhook test, reboot

## OLED Display (SSD1306) – live status
- Display module: `src/modules/display_manager.h` + `src/modules/display_manager.cpp`
- Hardware: 128x64, I2C `0x3C`, SDA `GPIO21`, SCL `GPIO22`
- Top line (yellow area): `WLAN <SSID>` oder `AP <SSID>`, optional `WG`/`wg?`, bei Alarm periodisch `ALARM`
- Main area: großer Druckwert (oder `---` bei invalid)
- Bottom line: `U <wert>V`, bei Alarm `ALARM < <low> bar`, im AP-Setup `PW:<passwort>`
- Render-Strategie: flackerfrei über Full-Frame-Buffer, Update nur bei Zustandsänderung

Beispielverwendung aus `main.cpp`:
```cpp
if (!gDisplay.begin()) {
  Serial.println("Display init failed");
} else {
  gDisplay.showBootScreen();
}
```

## Zielbild für API und Konfiguration
REST/API ist umgesetzt und umfasst:
- `GET /api/status`
- `GET /api/config`
- `POST /api/config/network`
- `POST /api/config/mqtt`
- `POST /api/config/alarm`
- `POST /api/config/calibration`
- `POST /api/config/wireguard`
- `GET /api/config/export`
- `POST /api/config/import`
- `POST /api/calibration/capture`
- `POST /api/calibration/clear`
- `POST /api/test/telegram`
- `POST /api/test/webhook`
- `GET /api/wireguard/status`
- `POST /api/wireguard/enable`
- `POST /api/wireguard/disable`
- `POST /api/reboot`

Konfigurationsdaten werden als JSON in Preferences persistiert (inkl. WLAN/AP, MQTT, Alarm und Kalibrierpunkten).

## Geplante Modul-Feinstruktur
Die aktuelle modulare Struktur bleibt erhalten und wird bei Bedarf in klarere Verantwortlichkeiten weiter getrennt (z. B. `config_manager`, `sensor_manager`, `calibration`, `display_manager`, `web_server`, `alarm_manager`).
Ziel bleibt: kleine, klar abgegrenzte Komponenten ohne schwergewichtige Frontend-Frameworks.

## Calibration procedure
1. Measure ADC at known low point (`adcLow`, `barLow`).
2. Measure ADC at known high point (`adcHigh`, `barHigh`).
3. Save values via config defaults or later extension in settings.
4. Optionally tune `offsetBar` for local correction.

Wenn mindestens zwei gültige Kalibrierpunkte vorliegen, wird stückweise linear zwischen Punkten interpoliert.
Fallback ohne Punkte bleibt die 2-Punkt-Gerade:
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
- AlarmManager sends real Telegram/Webhook notifications on alarm states and repeat interval.
- ArduinoOTA is enabled for in-network firmware updates.

## CI / quality gate
- GitHub Actions workflow runs:
  - `pio test -e native`
  - `pio run -e esp32dev`

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
- OTA-Update-Seite ist weiterhin offen.

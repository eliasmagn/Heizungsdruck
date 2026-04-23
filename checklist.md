# Checklist – Delivery Status

## A. Repository / Migration
- [x] PlatformIO-Projektstruktur erstellt
- [x] ESPHome-zentrierte Architektur durch modulare Firmware ersetzt
- [x] `.gitignore` für Secrets/Build-Artefakte ergänzt
- [x] `AGENTS.md` mit Build/Test/Done-Kriterien erstellt

## B. Sensor acquisition / pressure logic
- [x] Mehrfach-Sampling + robustes Hybrid-Filter implementiert
- [x] 2-Punkt Kalibrierung + linear conversion + Offset
- [x] Konfigurierbares Update-Intervall
- [x] Fault detection (disconnect/short/jump)

## C. State machine
- [x] Zustände `UNKNOWN`, `SENSOR_FAULT`, `PRESSURE_LOW`, `OK`, `PRESSURE_HIGH`
- [x] Threshold + Hysterese umgesetzt
- [x] Letzten gültigen Wert bei Fault beibehalten

## D. Web app
- [x] Dashboard
- [x] History view
- [x] Settings page
- [x] Diagnostics page
- [x] Lightweight HTML/CSS/JS ohne schweres Frontend-Framework

## E. Persistence / config
- [x] Persistenz via Preferences
- [x] Konfig-Validierung vor Save
- [x] `config.h.example` und `secrets.h.example`
- [ ] Vollständiger Export/Import-Endpunkt (optional next step)

## F. MQTT
- [x] Telemetry + state + status topics
- [x] Reconnect-Logik
- [x] Publish interval throttling

## G. Reliability
- [x] Non-blocking main loop pattern
- [x] Netzwerkunterbrechungen blockieren Sensorik nicht dauerhaft
- [x] Recovery auf Defaults bei invalid config

## H/I. Docs & tests
- [x] README vollständig überarbeitet
- [x] Deterministische Tests für Kernlogik hinzugefügt
- [x] Manuelle Verifikations-Checkliste dokumentiert

## J. Build-Fixes (2026-04-23)
- [x] Arduino-Makrokonflikt `LOW`/`HIGH` in `PressureState` behoben
- [x] ArduinoJson-v7-kompatibles Parsing (`JsonVariantConst` statt kopierter Proxies)

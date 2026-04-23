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
- [x] Geführte Kalibrierseite (21 Punkte à 0.5 bar von 0.0–10.0)
- [x] UI-Workflow für Kalibrierpunkte speichern/löschen via API-Capture/Clear
- [x] UI-Workflow für Kalibriertabelle editieren + persistieren
- [x] LittleFS-Webapp-Dateien (`index.html`, `app.js`, `style.css`)

## E. Persistence / config
- [x] Persistenz via Preferences
- [x] Konfig-Validierung vor Save
- [x] `config.h.example` und `secrets.h.example`
- [x] Vollständige JSON-Konfiguration für WLAN/AP/MQTT/Alarm/Sensor/Kalibrierung harmonisiert
- [x] Setup-Flow mit AP-Passwort-Generierung (OLED-Anzeige weiterhin separat)
- [x] Config Export/Import per JSON API (`/api/config/export`, `/api/config/import`)

## F. MQTT
- [x] Telemetry + state + status topics
- [x] Reconnect-Logik
- [x] Publish interval throttling

## G. Reliability
- [x] Non-blocking main loop pattern
- [x] Netzwerkunterbrechungen blockieren Sensorik nicht dauerhaft
- [x] Recovery auf Defaults bei invalid config
- [x] Aktive Alarmbenachrichtigung (Telegram/Webhook, inkl. Wiederholung)

## H/I. Docs & tests
- [x] README vollständig überarbeitet
- [x] Deterministische Tests für Kernlogik hinzugefügt
- [x] Manuelle Verifikations-Checkliste dokumentiert
- [x] CI-Workflow für native Tests + ESP32 Build ergänzt

## J. Build-Fixes (2026-04-23)
- [x] Arduino-Makrokonflikt `LOW`/`HIGH` in `PressureState` behoben
- [x] ArduinoJson-v7-kompatibles Parsing (`JsonVariantConst` statt kopierter Proxies)
- [x] `ProjectConfig.h` auf explizite `config/...` Includes umgestellt (verhindert versehentliches Einbinden eines fremden `config.h`)
- [x] `MqttManager::connected()` nicht-`const` gemacht (PubSubClient API ist nicht `const`-korrekt)

## K. Architekturentscheidung / Ausbaupfad (2026-04-23)
- [x] Entscheidung dokumentiert: PlatformIO + eigene Firmware + eigene Webapp als Zielbild
- [x] REST-Endpunkte für getrennte Config-Domänen (`network`, `mqtt`, `alarm`, `calibration`) vervollständigt
- [x] Endpunkte für Kalibrier-Capture/Clear ergänzt
- [x] WireGuard-Statusintegration (status/enable/disable via API + UI) umgesetzt

# Roadmap – Heizungsdruck Monitor

## Phase 1 (abgeschlossen)
- PlatformIO-Basis und modulare Firmwarestruktur
- Sensorpipeline + Fault-Handling
- Zustandsmaschine mit Hysterese
- Lokale Web-App (Dashboard/History/Settings/Diagnostics)
- MQTT-Telemetrie + reconnect
- Persistente, validierte Konfiguration

## Phase 2 (in Arbeit)
- ✅ Architekturentscheidung festgezogen: PlatformIO + eigene Firmware + eigene Webapp
- ✅ Settings/API auf getrennte Konfig-Domänen ausgebaut (`network`, `mqtt`, `alarm`, `calibration`)
- ✅ Setup-Flow für Erstinbetriebnahme mit AP-Passwort-Generierung (OLED-Ausgabe als separates Display-Thema)
- Tooling-Härtung: Build-Umgebung mit dokumentierter `intelhex`-Abhängigkeit für `esptool` stabilisieren
- ✅ Konfig-Export/Import als JSON Endpoint
- ✅ UI-Verbesserungen für Alarmübergänge im History-Chart
- ✅ CI/Build-Absicherung (native tests + esp32 build via GitHub Actions)

## Phase 3
- ✅ Geführte 21-Punkte-Kalibrierung (Erfassen, Speichern, Löschen)
- ✅ OTA-Update Workflow (ArduinoOTA)
- ✅ Optional WireGuard-Statusintegration (nach stabiler Mess-/UI-/MQTT-Basis)
- Erweiterte Selbstdiagnose und Telemetrie-Ratensteuerung
- ✅ Alarm-Benachrichtigungen via Telegram/Webhook mit Repeat-Intervall

# Roadmap – Heizungsdruck Monitor

## Phase 1 (abgeschlossen)
- PlatformIO-Basis und modulare Firmwarestruktur
- Sensorpipeline + Fault-Handling
- Zustandsmaschine mit Hysterese
- Lokale Web-App (Dashboard/History/Settings/Diagnostics)
- MQTT-Telemetrie + reconnect
- Persistente, validierte Konfiguration

## Phase 2 (nächstes Ziel)
- Tooling-Härtung: Build-Umgebung mit dokumentierter `intelhex`-Abhängigkeit für `esptool` stabilisieren
- Settings-Seite um vollständige Sensor/MQTT-Felder erweitern
- Konfig-Export/Import als JSON Endpoint
- UI-Verbesserungen für Alarmübergänge im History-Chart
- CI/Build-Absicherung gegen Arduino-Makrokollisionen und ArduinoJson-Major-Upgrades

## Phase 3
- OTA-Update Workflow
- Optional WireGuard-Statusintegration (falls im Zielnetz benötigt)
- Erweiterte Selbstdiagnose und Telemetrie-Ratensteuerung

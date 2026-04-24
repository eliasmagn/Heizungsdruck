# Roadmap – Heizungsdruck Monitor

## Phase 1 (abgeschlossen)
- PlatformIO-Basis und modulare Firmwarestruktur
- Sensorpipeline + Fault-Handling
- Zustandsmaschine mit Hysterese
- Persistente, validierte Konfiguration
- MQTT-Telemetrie + reconnect

## Phase 2 (abgeschlossen)
- ✅ Architekturentscheidung festgezogen: PlatformIO + eigene Firmware + eigene Webapp
- ✅ WebUI-Routing bereinigt: nur noch LittleFS-UI als kanonisches Frontend
- ✅ Inline-C++-Seiten entfernt/deaktiviert; Legacy-Routen zeigen auf Root
- ✅ Vollständiger API-Abgleich für Live/Verlauf/Kalibrierung/Settings/Diagnose
- ✅ UI-Design auf Produkt-Branding umgestellt (dark + cyan/amber + technische Kartenstruktur)
- ✅ README um klaren Deployment-Workflow (`upload` vs `uploadfs`) ergänzt

## Phase 3 (in Arbeit)
- Erweiterte Selbstdiagnose und Telemetrie-Ratensteuerung
- OTA-Update-UX in die Webapp integrieren (Backend via ArduinoOTA steht)
- Erweiterte WireGuard-Statusvisualisierung (strukturierte Response-Darstellung)

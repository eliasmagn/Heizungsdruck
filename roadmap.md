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
- ✅ Build-Stabilisierung: WebUI Root-Handler an `WebServer::streamFile(T&)`-Signatur angepasst (kein temporäres File mehr)
- Erweiterte Selbstdiagnose und Telemetrie-Ratensteuerung
- OTA-Update-UX in die Webapp integrieren (Backend via ArduinoOTA steht)
- Erweiterte WireGuard-Statusvisualisierung (strukturierte Response-Darstellung)


## Phase 3 (Update 2026-04-26)
- ✅ WireGuard-Planungsnetz in Config-Modell und SPA aufgenommen; Boot-Defaults aus `config.h`/`secrets.h`
- ✅ Single-UI-Architektur finalisiert (nur LittleFS-SPA, inkl. SPA-Fallback)
- ✅ Settings/Diagnose/Kalibrierung auf konsistente API-Endpunkte abgeglichen
- ✅ Deploy-Hinweise für `upload` vs `uploadfs` vereinheitlicht

- ✅ Diagnose um atomaren Full-Config-Save über `POST /api/config` ergänzt

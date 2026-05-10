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
- Lokale WireGuard-Laufzeitmetriken und bessere Online/Fehler-Visualisierung
- ✅ Netzwerk-UX: WLAN-Scan + SSID-Dropdown integriert
- ✅ Funkprofil: reduzierte TX-Leistung und 802.11b-only als persistente Einstellung integrieren


## Phase 3 (Update 2026-04-26)
- ✅ WireGuard-Planungsnetz in Config-Modell und SPA aufgenommen; Boot-Defaults aus `config.h`/`secrets.h`
- ✅ Single-UI-Architektur finalisiert (nur LittleFS-SPA, inkl. SPA-Fallback)
- ✅ Settings/Diagnose/Kalibrierung auf konsistente API-Endpunkte abgeglichen
- ✅ Deploy-Hinweise für `upload` vs `uploadfs` vereinheitlicht

- ✅ Diagnose um atomaren Full-Config-Save über `POST /api/config` ergänzt

- ✅ 2026-04-26: WireGuard lokal auf ESP32 umgesetzt (Config-Modell, Runtime-Manager, API/UI-Migration)

- ✅ Build-Stabilisierung: WLAN-Protokollumschaltung auf ESP-IDF API (`esp_wifi_set_protocol`) angepasst; behebt ESP32-Arduino 3.x Compile-Fehler um `WiFi.setProtocol`/`wifi_protocol_t`.

- ✅ Codehygiene: IDF-Fallback für WLAN-Protokoll in lokaler Helper-Funktion gekapselt, Arduino-Hauptfluss bleibt lesbar

- 2026-05-01: Stabilisierung abgeschlossen: Alarmdiagnose, freie Kalibrierpunkte, OTA-Doku, SPA-only Betrieb.

- 2026-05-02: Telegram-Command-Polling mit Runtime-Kommandos ergänzt.

- 2026-05-06: Telegram /saveconfig live gesetzt (Runtime->persistent).

- 2026-05-06: MQTT-Diagnosepfad mit Publish-Validierung und Web-Diagnose (`mqttDiag`) ergänzt.
- 2026-05-06: Telegram-Diagnosezustand (`telegramDiag`) in /api/diag ergänzt.

- 2026-05-06: Optionales MQTT-over-WireGuard-Gating (`requireWireguard`) umgesetzt.

- 2026-05-06: MQTT-WireGuard-Betrieb auf Dual-Interface-Usecase angepasst (WG bevorzugt, Fallback aktiv).

- 2026-05-06: WireGuard-Setup robuster: CIDR in localAddress wird toleriert (IP-Extraktion) und diagnostisch markiert.

- 2026-05-06: MQTT-Fehlersuche erweitert: manueller Testpublish und Publish-Erfolgszähler in Diagnose.
- 2026-05-06: Mehrgerätebetrieb (kreis1..kreis4) mit MQTT Discovery abgeschlossen.


## Nächster Meilenstein abgeschlossen
- Multisensor-Basis (analoge Kanal-Liste)
- DS18B20-Basissupport
- MQTT/Discovery-Erweiterung um Temperatur+Spannung

- 2026-05-06: Sensorpfad auf ADC-Continuous (DMA) mit kompatiblem Fallback auf klassisches `analogRead` umgestellt.

- 2026-05-06: End-to-End Sensor-Stack abgeschlossen: neue Kanal-/Temperaturwerte von Erfassung bis MQTT/HA/UI durchgängig.

- 2026-05-06: Build-Fixes für PlatformIO/ESP32-Arduino: C++11-Kompatibilität (kein lambda-auto/std::clamp/structured bindings), MQTT String/JsonDocument-Konsistenz, AlarmDispatchResult- und Kalibrierpunkt-Zuweisungen gehärtet.

- 2026-05-07: Lokale Drift-Basis abgeschlossen (1h/24h für Druck + optionale Temperaturdrift) als bewusst schlanke Vorstufe für externe HA/PC-Analyse.

- 2026-05-07: Plattform-Capabilities zentralisiert, NTC-Standardpfad und globalen noise_ref-Diagnosepfad umgesetzt.

- 2026-05-07: Follow-up abgeschlossen: WebUI-Konfigurationspfad (Temp-Modi/NTC) und noise_ref-Kompensationszuordnung stabilisiert.

- 2026-05-07: Restprobleme abgeschlossen: klare ESP8266-Slim-Abgrenzung (ohne WireGuard), lokale Driftlogik auf Druckdrift reduziert, README konsolidiert.
- 2026-05-08: Slim-Profil finalisiert (Task/critical/WiFi-ESP32 APIs entkoppelt), Druckwert-Linie auf pressureBar konsolidiert.

- 2026-05-08: Plattformkonsistenz final: Slim ohne Full-WebUI/Display, Capability-Gates bis Objektpfad durchgezogen, Discovery-Modell profilabhängig.

## 2026-05-08 Fortschritt
- Async-Webserver als gemeinsame Basis eingeführt.
- ESP32-Profil behält LittleFS-SPA + volle API.
- ESP8266-slim bewusst als reduzierte Async-API-Schicht dokumentiert/umgesetzt.

- 2026-05-08: Async-Restkorrekturen abgeschlossen (korrekte ESP8266 Async-Lib, PressureReading/WebUI konsistent ohne Temperaturdrift, SPA-Fallback für Nicht-API-Routen im Vollprofil).

- 2026-05-09: Profilhärtung abgeschlossen (ESP8266-LittleFS-Persistenz, ehrliche ADC-Defaults, Async-JSON-Body-Handling).

- 2026-05-09: Async-Härtung ergänzt: WLAN-Scan läuft jetzt als asynchrones Polling.
- 2026-05-09: Resthärtung abgeschlossen: eindeutige Druckquelle erzwungen, Sensor-Reinit bei Runtime-Config aktiv, Setup-Overrides für MQTT/Intervall entfernt.

- 2026-05-09: Slim-ADC-Default/Validation, WebUI JSON robustness, FS-mount safety, network-change handling refined.
- 2026-05-09: Shared-ADC/MUX derzeit bewusst zurückgestuft: Konfigurationsfelder bleiben reserviert, bis echte Runtime-Umschaltung implementiert ist.
- 2026-05-09: Konsistenzschritt abgeschlossen: Validierung blockiert Druck+NTC auf gleichem ADC-Pin solange kein echter Shared-ADC-Laufzeitpfad existiert.
- 2026-05-09: WebUI-Konfigurationspfad für Slim-Felder vereinheitlicht: `/api/config` läuft über denselben JSON/Validation-Pfad wie Import/Sensor-POST.


- 2026-05-10: Restprobleme bereinigt: saveUpdatedConfig-RAM/Persistenz-Konsistenz, NTC-Medianfilter, DS18B20-only OneWire-Init, strengere Kanalvalidierung, ConfigStore-Init-Fehlerpfad, klare Shared-ADC/MUX-Ehrlichkeit.
- 2026-05-10 Follow-up: Beim Boot wird nun auch *inhaltlich* invalid geladene Config erkannt; definierter Default-Fallback mit Reparatur-Speicherung (wenn Store verfügbar).
- 2026-05-10 Follow-up 2: `/saveconfig` übernimmt jetzt den zentral normalisierten Persistenzzustand ohne nachträgliches Überschreiben im AlarmManager; `useGlobalNoiseRef` wird auf den Druckkanal eingeschränkt.


- 2026-05-10 Follow-up 3: Dokumentation präzisiert (zentraler Save-Pfad, Shared-ADC/MUX-Ehrlichkeit, useGlobalNoiseRef-Wirkbereich) und Buildversuche mit lokal installiertem PlatformIO erneut ausgeführt.

- 2026-05-10 Follow-up 4: Shared-ADC/MUX-Lücke geschlossen: `PressureSensor` nutzt zentralen Shared-Sampling-Pfad, `sharedAdcFrontend` aktiviert gleiche ADC-Pins (inkl. Druck+NTC), und Validierung blockiert doppelte Pins nur noch ohne Shared-Frontend.

- 2026-05-10 Follow-up 5: WebUI-Konfig-Save-Konsistenz gehärtet: nach zentralem saveCfg()-Pfad wird kein Roh-Kandidat zurückgeschrieben; WireGuard-Post-Save nutzt den normalisierten Runtime-Stand (`cfg_`).

- 2026-05-10 Follow-up 6: Telegram-`/saveconfig` Konsistenz fixiert: AlarmManager synchronisiert nach erfolgreichem zentralem Save explizit aus der gemeinsamen Runtime-Config (`gConfig`) statt im potenziell rohen Kandidatstand zu bleiben.

- 2026-05-10 Follow-up 7: WireGuard/MQTT-Semantik geschärft: `mqtt.requireWireguard=true` blockiert jetzt Reconnect + Publish bei offlineem Tunnel; WireGuard-Status meldet explizit den derzeitigen Heuristik-Charakter (WiFi-Link statt echter Handshake-Telemetrie).

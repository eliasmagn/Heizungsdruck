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
- [x] Eine kanonische LittleFS-Weboberfläche (Root `/` -> `data/index.html`)
- [x] Root-Route-Streaming korrigiert (`fs::File`-Lvalue statt temporäres `LittleFS.open(...)`) für ESP32-Arduino Build-Kompatibilität
- [x] Alte C++-Inline-Seiten für `/`, `/history`, `/settings`, `/calibration`, `/diag` entfernt/deaktiviert
- [x] Dashboard/Live-Bereich vollständig an `/api/status` angebunden
- [x] Verlauf an `/api/history` angebunden + Canvas-Chart + JSON/CSV-Export
- [x] Kalibrier-Workflow über `/api/calibration/capture`, `/api/calibration/clear`, `/api/config/calibration`
- [x] Settings-Workflow über `/api/config/sensor|network|mqtt|alarm|wireguard`
- [x] WLAN-Scan-API (`/api/wifi/scan`) + SSID-Auswahl im Netzwerk-Formular
- [x] Netzwerk-Settings um WLAN-Sendeleistung (dBm) und 802.11b-only Modus erweitert
- [x] Diagnose-Workflow über `/api/diag`, `/api/test/telegram`, `/api/test/webhook`, `/api/reboot`
- [x] Branding-Design (dunkel + cyan/weiß/amber) mit Assets unter `data/assets/`

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
- [x] README überarbeitet inkl. klarer `uploadfs`-Deployment-Hinweise für Webapp-Änderungen
- [x] Deterministische Tests für Kernlogik hinzugefügt
- [x] Manuelle Verifikations-Checkliste dokumentiert
- [x] CI-Workflow für native Tests + ESP32 Build ergänzt

- [x] 2026-04-26: SPA-Fallback für Nicht-API-Routen aktiviert; API-Routen bleiben 404
- [x] 2026-04-26: Kalibrier-Tabelle auf fixe 21 Punkte normalisiert, inklusive Punkt-Löschen/Reload/Save-Flow

- [x] 2026-04-26: Pseudo-WireGuard-Proxy entfernt und echte lokale WireGuard-Konfiguration inkl. Runtime-Manager ergänzt

- [x] 2026-04-26: Diagnose-Tab kann gesamte Konfiguration atomar über `POST /api/config` speichern

- [x] 2026-04-26: Echter lokaler WireGuard-Manager (`WireGuardManager`) integriert; alte URL-Proxy-Steuerung entfernt
- [x] 2026-05-01: WLAN-Scan im GUI + konfigurierbare WLAN-Sendeleistung/11b-Modus umgesetzt

- [x] WLAN-Protokollumschaltung build-kompatibel für ESP32-Arduino 3.x umgesetzt (`esp_wifi_set_protocol` statt nicht verfügbarer `WiFi.setProtocol` API)

- [x] IDF-Fallback für WLAN-Protokoll in separaten Helper gekapselt (Arduino-`connectWifi()` sauber gehalten)

- [x] Telegram-Versand zentralisiert (AlarmManager + Diagnose-Tests)
- [x] Kalibrierung auf freie max.20 Punkte umgestellt
- [x] README auf reale Bedienung gekürzt/vereinheitlicht
- [x] OTA-Nutzung dokumentiert

- [x] Telegram-Kommandos via getUpdates implementiert und dokumentiert

- [x] Telegram-Kommando /saveconfig ergänzt und an Config-Speicher gekoppelt

- [x] 2026-05-06: MQTT-Diagnostik erweitert (Publish-Rückgaben, Buffergröße, Fehler-/State-Logging, /api/diag mqttDiag)
- [x] 2026-05-06: Kalibrierpunkte im UI jetzt echt löschbar (remove statt valid=false)

- [x] 2026-05-06: MQTT optional auf WireGuard erzwungen (`requireWireguard`) inkl. Block-/Fehlerdiagnose.

- [x] 2026-05-06: MQTT-WireGuard-Modus präzisiert: bevorzugen + Fallback statt hartes Blocking.

- [x] 2026-05-06: WireGuard localAddress akzeptiert jetzt IP oder CIDR (z. B. /24), verhindert Startfehler bei CIDR-Eingabe.

- [x] 2026-05-06: MQTT-Testpublish-Endpoint (`/api/test/mqtt`) + UI-Button ergänzt; MQTT-Diagnosezähler erweitert.
- [x] 2026-05-06: Mehrgeräte-MQTT-Identität mit deviceId + abgeleiteten Defaults (hostname/clientId/topicBase) ergänzt.
- [x] 2026-05-06: Home-Assistant MQTT Discovery (retained, eindeutige unique_id pro Gerät/Entity) ergänzt.

- [x] Sensorarchitektur auf mehrere analoge Kanäle vorbereitet
- [x] Temperaturmessung (DS18B20) in Backend/MQTT/UI integriert
- [x] HA Discovery um Temperatur/Spannung erweitert

- [x] 2026-05-06: ADC-Sampling auf bevorzugten ESP-IDF-Continuous-DMA-Pfad umgestellt (Fallback auf analogRead bleibt aktiv).

- [x] 2026-05-06: Sensor-Stack end-to-end vervollständigt (REST/UI + MQTT + HA Discovery inkl. channels/temperature/voltage).

- 2026-05-06: Build-Fixes für PlatformIO/ESP32-Arduino: C++11-Kompatibilität (kein lambda-auto/std::clamp/structured bindings), MQTT String/JsonDocument-Konsistenz, AlarmDispatchResult- und Kalibrierpunkt-Zuweisungen gehärtet.

- [x] 2026-05-07: Schlanke Driftwerte ergänzt (`pressureDrift1h`, `pressureDrift24h`, optional Temperaturdrift) inkl. MQTT/Discovery/WebUI/README ohne Leak-Score-Logik.

- [x] Plattformprofile um `esp8266_slim` und `esp32_standard` geschärft.
- [x] NTC als Standardmodus ergänzt, DS18B20 optional belassen.
- [x] Globaler `noise_ref`-Baselinepfad + Roh/kompensierte MQTT-Werte ergänzt.
- [x] Einfache Driftwerte 1h/24h im lokalen Pfad bestätigt.

- [x] 2026-05-07 Follow-up: WebUI Sensor-Config für `temperature.mode`/NTC vollständig ergänzt.
- [x] 2026-05-07 Follow-up: Kompensationspfad an expliziten Druckkanal gebunden (kein impliziter ADC-Gleichheitsmatch).

- [x] 2026-05-07 Restprobleme: WireGuard sauber ESP32-only gekapselt; esp8266_slim zieht keine ESP32-WG-Includes mehr.
- [x] 2026-05-07 Restprobleme: Temperaturdrift aus lokalem Datenmodell/MQTT/Discovery entfernt (Druckdrift 1h/24h bleibt).
- [x] 2026-05-07 Restprobleme: README gestrafft, Profile/Grenzen klar dokumentiert.
- [x] ESP8266-Slim Pfad: ESP32-only APIs in main.cpp sauber gegated/ersetzt
- [x] pressureBarCompensated entfernt, compensatedAdc als Diagnose beibehalten

- [x] 2026-05-08: Restprobleme finalisiert: WebUI/Display capability-basiert für esp8266_slim deaktiviert, Discovery-Modell profilabhängig benannt.

- [x] Async-Webserver-Umstellung abgeschlossen (ESP32 + ESP8266 slim)
- [x] Blockierende HTTP-Callbacks entfernt (Reboot/Test-Aktionen deferred)
- [x] PlatformIO Async-Abhängigkeiten je Profil gepinnt
- [x] README auf realen Web-Profilstand aktualisiert

- [x] 2026-05-08: Async-Restfehler bereinigt (ESP8266 AsyncTCP korrekt gepinnt, WebUI ohne Temperaturdrift-Altfelder, SPA-Fallback /api-vs-index repariert).

- [x] 2026-05-09: ESP8266-Slim Persistenzpfad auf LittleFS-Datei umgestellt; ESP32 bleibt auf Preferences/NVS.

- [x] 2026-05-09: `/api/wifi/scan` entblockt (async Start/Poll statt synchronem Scan im Callback).
- [x] 2026-05-09: Konfig-Validierung auf exakt eine Druckquelle geschärft (kein mehrdeutiges pressureSource-Verhalten).
- [x] 2026-05-09: Sensor-Rekonfiguration wirksam gemacht (Re-Init in `PressureSensor::updateConfig()` statt stiller Teilübernahme).
- [x] 2026-05-09: Setup-Logik korrigiert: MQTT enabled wird nicht mehr blind aus Host abgeleitet; `updateIntervalMs` wird nicht mehr zwangsweise überschrieben.

- 2026-05-09: Slim-ADC-Default/Validation, WebUI JSON robustness, FS-mount safety, network-change handling refined.
- [x] 2026-05-09: Shared-ADC/MUX-Modellfelder auf ehrlichen Reserve-Status zurückgestuft (keine Scheinunterstützung ohne Runtime-MUX).
- [x] 2026-05-09: Validierung und Laufzeit konsistent gemacht: gleicher ADC-Pin für Druck+NTC ist bis echter Shared-ADC-Implementierung gesperrt.
- [x] 2026-05-09: WebUI-Config-Roundtrip für Slim-Felder gehärtet (`/api/config` nutzt konsistent `configFromJson`; kein fehlerhafter Sonderpfad mehr).


- 2026-05-10: Restprobleme bereinigt: saveUpdatedConfig-RAM/Persistenz-Konsistenz, NTC-Medianfilter, DS18B20-only OneWire-Init, strengere Kanalvalidierung, ConfigStore-Init-Fehlerpfad, klare Shared-ADC/MUX-Ehrlichkeit.
- 2026-05-10 Follow-up: Beim Boot wird nun auch *inhaltlich* invalid geladene Config erkannt; definierter Default-Fallback mit Reparatur-Speicherung (wenn Store verfügbar).
- 2026-05-10 Follow-up 2: `/saveconfig` übernimmt jetzt den zentral normalisierten Persistenzzustand ohne nachträgliches Überschreiben im AlarmManager; `useGlobalNoiseRef` wird auf den Druckkanal eingeschränkt.


- 2026-05-10 Follow-up 3: Dokumentation präzisiert (zentraler Save-Pfad, Shared-ADC/MUX-Ehrlichkeit, useGlobalNoiseRef-Wirkbereich) und Buildversuche mit lokal installiertem PlatformIO erneut ausgeführt.

- 2026-05-10 Follow-up 4: Shared-ADC/MUX-Lücke geschlossen: `PressureSensor` nutzt zentralen Shared-Sampling-Pfad, `sharedAdcFrontend` aktiviert gleiche ADC-Pins (inkl. Druck+NTC), und Validierung blockiert doppelte Pins nur noch ohne Shared-Frontend.

- 2026-05-10 Follow-up 5: WebUI-Konfig-Save-Konsistenz gehärtet: nach zentralem saveCfg()-Pfad wird kein Roh-Kandidat zurückgeschrieben; WireGuard-Post-Save nutzt den normalisierten Runtime-Stand (`cfg_`).

- 2026-05-10 Follow-up 6: Telegram-`/saveconfig` Konsistenz fixiert: AlarmManager synchronisiert nach erfolgreichem zentralem Save explizit aus der gemeinsamen Runtime-Config (`gConfig`) statt im potenziell rohen Kandidatstand zu bleiben.

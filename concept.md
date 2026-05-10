# Konzept – Heizungsdruck Monitor

## Produktziel
Ein stabiler, ressourcenschonender ESP32-Heizungsdruckmonitor mit lokal bedienbarer Web-App, sauberer MQTT-Telemetrie und robuster Sensorfehlererkennung.

## Architekturentscheidung
Für die Zielsetzung gilt explizit: **eigene Firmware + eigene Webapp mit PlatformIO** statt ESPHome-Workarounds.

## Kernprinzipien
- Fokus auf praktischen Nutzwert im Bereich 0–2 bar.
- Klare Trennung von Sensorik, Zustandslogik, Persistenz, Web und MQTT.
- Keine Secrets im Git-Repo.
- Lean implementation statt Framework-Bloat.

## Umgesetzte Architektur
- `PressureSensor` + `PressureMath`: ADC Pipeline, robustes Filter, Kalibrierung.
- `PressureStateMachine`: `UNKNOWN`, `SENSOR_FAULT`, `PRESSURE_LOW`, `OK`, `PRESSURE_HIGH` mit Hysterese.
- `PressureHistory`: Trenddaten für UI/API.
- `ConfigStore` + `JsonCodec`: persistente und validierte Konfiguration.
- `WebUI`: reine API-Schicht + LittleFS-Auslieferung.
- `MqttManager`: reconnect-fähige Telemetrie.

## Weboberfläche (kanonisch)
- Es existiert genau **eine** UI: die LittleFS-Webapp unter `data/`.
- Root `/` liefert ausschließlich `data/index.html`.
- Statische Assets (`/app.js`, `/style.css`, `/assets/*`) kommen ausschließlich aus LittleFS.
- Alte inline C++-HTML-Seiten sind nicht mehr Teil der Produktoberfläche.

## API-first Frontend-Anbindung
- Live: `/api/status`
- Verlauf: `/api/history`
- Diagnose: `/api/diag`
- Konfiguration: `/api/config`, `/api/config/export`, `/api/config/import`
- Domain-Settings: `/api/config/sensor|network|mqtt|alarm|wireguard`
- WLAN-Discovery: `/api/wifi/scan` für Netzwerkauswahl in der UI
- Kalibrierung: `/api/config/calibration`, `/api/calibration/capture`, `/api/calibration/clear`
- Aktionen: `/api/test/telegram`, `/api/test/webhook`, `/api/wireguard/status|enable|disable`, `/api/reboot`

## Deployment-Prinzip
- Firmwareänderungen: `pio run -t upload`
- Webapp-/Asset-Änderungen: zusätzlich `pio run -t uploadfs`
- Ohne `uploadfs` bleiben Änderungen in `data/` auf dem Gerät unsichtbar.

## Qualitätsziele
- Erfolgreicher Build auf `esp32dev`.
- Deterministische Host-Tests für Kernlogik.
- Verständliche Dokumentation für Betrieb und Wartung.

## Technische Notiz (Build-Kompatibilität)
- Die WebUI-Implementierung berücksichtigt die `WebServer::streamFile(T&)`-Signatur explizit und übergibt `fs::File` als benannte Lvalue-Variable (nicht als temporäres `LittleFS.open(...)`).


## Fortschritt 2026-04-26
- WebUI bedient ausschließlich LittleFS-SPA + REST/JSON-API.
- Frontend-Routen werden via SPA-Fallback auf `index.html` aufgelöst; `/api/*` bleibt strikt Backend-Schnittstelle.
- Kalibrierlogik in der SPA ist backend-dominiert (21 feste Punkte, laden/speichern/capture/clear gegen API).

- WireGuard wird lokal auf dem ESP32 betrieben; Tunnelparameter werden via Build-Defaults vorbefüllt und persistent gespeichert.

- Diagnose-Workflow unterstützt jetzt neben Teilupdates auch atomare Gesamtkonfigurations-Saves via `POST /api/config`.
- Netzwerk-Workflow bietet WLAN-Scan sowie ein einstellbares Low-Power-WLAN-Profil (TX-Leistung + 802.11b-only).

- WLAN-Protokollumschaltung nutzt direkt `esp_wifi_set_protocol(WIFI_IF_STA, ...)`, damit der Build mit `framework-arduinoespressif32 @ 3.20017.x` kompatibel bleibt.

- IDF-spezifischer WLAN-Protokollaufruf ist bewusst in einer lokalen Helper-Funktion gekapselt, um den Arduino-Hauptpfad sauber zu halten.

- 2026-05-01: Kalibrierung auf freie max.20 Punkte umgestellt, Telegram-Pfad zentralisiert, SPA-only bestätigt.

- 2026-05-02: Telegram-Bot-Kommandos (/start, /getpres, /setoffset, /setcalpoint) ergänzt.

- 2026-05-06: Telegram-Kommando /saveconfig ergänzt (persistiert Runtime-Änderungen).

- 2026-05-06: MQTT-Publish-Diagnostik (inkl. Buffer-/Payloadsichtbarkeit) und Telegram-Diagnoseobjekt in `/api/diag` ergänzt.

- 2026-05-06: Betriebsmodus ergänzt: MQTT kann optional strikt an WireGuard-Onlinezustand gekoppelt werden.

- 2026-05-06: Netzwerkprinzip präzisiert: MQTT bleibt dual-stack-fähig; WireGuard-Pfad wird bevorzugt/diagnostiziert, Fallback bleibt verfügbar.

- 2026-05-06: WireGuard-Konfigurationsrobustheit erhöht: lokale Tunneladresse akzeptiert nun IP/CIDR-Eingaben.

- 2026-05-06: Betriebsdiagnose erweitert um expliziten MQTT-Testpublishpfad zur Broker-Validierung.
- 2026-05-06: MQTT-Architektur für parallele Geräte um deviceId/Discovery erweitert.


### Sensorik 2026-05
Das System entwickelt sich von Einzel-ADC auf modulare Mehrkanal-Erfassung (ein Kanal als Druckquelle, weitere optional). Temperatur wird als eigener Sensortyp geführt (DS18B20 auf OneWire-Bus).

- 2026-05-06: Sensorpipeline nutzt nun primär ESP-IDF ADC-Continuous (DMA), um CPU-Last bei Mehrkanalabtastung zu senken; nicht unterstützte Targets bleiben über den bestehenden `analogRead`-Fallback funktionsfähig.

- 2026-05-06: API- und Telemetrieschicht folgen nun der neuen Sensorstruktur vollständig (`channels`, Temperaturparameter, Discovery pro Kanal).

- 2026-05-06: Build-Fixes für PlatformIO/ESP32-Arduino: C++11-Kompatibilität (kein lambda-auto/std::clamp/structured bindings), MQTT String/JsonDocument-Konsistenz, AlarmDispatchResult- und Kalibrierpunkt-Zuweisungen gehärtet.

- 2026-05-07: Lokale Minimal-Erweiterung um Driftwerte (1h/24h) für Druck, optional Temperatur. Architektur bleibt lean: 5-Minuten-Snapshots im RAM, keine komplexe Leak-Klassifikation auf dem ESP.

- 2026-05-07 Ergänzung: ESP-seitig nur schlanke lokale Signalaufbereitung (Messung, Filter, Kalibrierung, einfache Drift, kompakte Telemetrie); komplexe Langzeitanalyse bleibt extern.

- 2026-05-07 Follow-up: Konfigurationskonsistenz zwischen JSON-Codec, WebUI-Sensor-Endpoint und Laufzeit-Sensorpfad hergestellt (Temp-Modi + NTC + noise_ref Zuordnung).

- 2026-05-07 Restfokus umgesetzt: Capability-Gates für WireGuard konsistent gemacht; lokale Komplexität reduziert (keine Temperaturdrift im Embedded-Pfad), README auf kompakte Profil-/Grenzdoku gestrafft.

- 2026-05-08: Finaler Plattformzuschnitt: `esp8266_slim` bleibt bewusst minimal (kein Full-WebUI-/Display-Pfad); Capabilities steuern Objektwahl und Discovery-Modell konsistent.

- 2026-05-08: Web-Schicht auf Async-Architektur konsolidiert (ESP32 voll mit LittleFS-SPA, ESP8266 slim API-fokussiert), inklusive non-blocking Request-Callbacks.

- 2026-05-09: Async-POST-Parsing wurde auf echten Body-Handler umgestellt; ESP8266-Slim nutzt eigenständigen LittleFS-Konfigspeicher.

- 2026-05-09: WLAN-Scan-Endpoint auf asynchrones Start/Polling umgestellt, damit Async-Handler kurz bleiben.
- 2026-05-09: Konfiglogik geschärft: exakt eine Druckquelle ist Pflicht; Sensoränderungen greifen zur Laufzeit per Re-Init statt nur nach Neustart.

- 2026-05-09: Slim-ADC-Default/Validation, WebUI JSON robustness, FS-mount safety, network-change handling refined.
- 2026-05-09: Shared-ADC/MUX-Felder im Modell als reserviert markiert, bis eine echte Laufzeit-Umschaltlogik vorhanden ist.
- 2026-05-09: Validierung bewusst auf „ehrlich lauffähig“ geschärft: Druck+NTC auf identischem ADC-Pin bleibt bis zur Runtime-Implementierung gesperrt.
- 2026-05-09: WebUI-Konfigurationsroundtrip nutzt nun einen konsistenten Pfad ohne separates Slim-Sonderparsing; Fehler werden über zentrale Validation gemeldet.


- 2026-05-10: Restprobleme bereinigt: saveUpdatedConfig-RAM/Persistenz-Konsistenz, NTC-Medianfilter, DS18B20-only OneWire-Init, strengere Kanalvalidierung, ConfigStore-Init-Fehlerpfad, klare Shared-ADC/MUX-Ehrlichkeit.
- 2026-05-10 Follow-up: Beim Boot wird nun auch *inhaltlich* invalid geladene Config erkannt; definierter Default-Fallback mit Reparatur-Speicherung (wenn Store verfügbar).
- 2026-05-10 Follow-up 2: `/saveconfig` übernimmt jetzt den zentral normalisierten Persistenzzustand ohne nachträgliches Überschreiben im AlarmManager; `useGlobalNoiseRef` wird auf den Druckkanal eingeschränkt.


- 2026-05-10 Follow-up 3: Dokumentation präzisiert (zentraler Save-Pfad, Shared-ADC/MUX-Ehrlichkeit, useGlobalNoiseRef-Wirkbereich) und Buildversuche mit lokal installiertem PlatformIO erneut ausgeführt.

- 2026-05-10 Follow-up 4: Shared-ADC/MUX-Lücke geschlossen: `PressureSensor` nutzt zentralen Shared-Sampling-Pfad, `sharedAdcFrontend` aktiviert gleiche ADC-Pins (inkl. Druck+NTC), und Validierung blockiert doppelte Pins nur noch ohne Shared-Frontend.

- 2026-05-10 Follow-up 5: WebUI-Konfig-Save-Konsistenz gehärtet: nach zentralem saveCfg()-Pfad wird kein Roh-Kandidat zurückgeschrieben; WireGuard-Post-Save nutzt den normalisierten Runtime-Stand (`cfg_`).

- 2026-05-10 Follow-up 6: Telegram-`/saveconfig` Konsistenz fixiert: AlarmManager synchronisiert nach erfolgreichem zentralem Save explizit aus der gemeinsamen Runtime-Config (`gConfig`) statt im potenziell rohen Kandidatstand zu bleiben.

- 2026-05-10 Follow-up 7: WireGuard/MQTT-Semantik geschärft: `mqtt.requireWireguard=true` blockiert jetzt Reconnect + Publish bei offlineem Tunnel; WireGuard-Status meldet explizit den derzeitigen Heuristik-Charakter (WiFi-Link statt echter Handshake-Telemetrie).

- 2026-05-10 Follow-up 8: Persistenz-/Security-Härtung: Main speichert nur noch bei erfolgreich initialisiertem ConfigStore, und Telegram-Befehle werden strikt auf die konfigurierte `telegramChatId` gefiltert.

- 2026-05-10 Follow-up 9: README/Feature-Semantik geschärft (Display-Hardwarebindung, strict `requireWireguard`, WireGuard-Online-Heuristik, Shared-ADC-Grenzen) für ehrliche Laufzeitdokumentation.

- 2026-05-10 Follow-up 10: Display-Portabilität verbessert (I2C-Pins/Adresse per Build-Defines konfigurierbar) und MQTT-Telemetrie um Shared-ADC-Semantikfelder ergänzt.

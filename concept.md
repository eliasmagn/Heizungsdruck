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

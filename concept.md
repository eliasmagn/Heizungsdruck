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

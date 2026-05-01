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

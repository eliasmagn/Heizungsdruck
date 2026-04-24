# Konzept – Heizungsdruck Monitor

## Produktziel
Ein stabiler, ressourcenschonender ESP32-Heizungsdruckmonitor mit lokal bedienbarer Web-App, sauberer MQTT-Telemetrie und robuster Sensorfehlererkennung.

## Architekturentscheidung (2026-04-23)
Für die Zielsetzung gilt explizit: **eigene Firmware + eigene Webapp mit PlatformIO** statt ESPHome-Workarounds.
Begründung:
- vollständige Konfigurations- und Setup-Flows sind in eigener Firmware sauber abbildbar
- geführte Mehrpunkt-Kalibrierung (21 Punkte) benötigt echte UI- und API-Kontrolle
- Alarmierung, Sicherheit und spätere Erweiterungen (z. B. WireGuard) bleiben modular planbar

## Kernprinzipien
- Fokus auf praktischen Nutzwert im Bereich 0–2 bar.
- Klare Trennung von Sensorik, Zustandslogik, Persistenz, Web und MQTT.
- Keine Secrets im Git-Repo.
- Lean implementation statt Framework-Bloat.

## Umgesetzte Architektur
- `PressureSensor` + `PressureMath`: ADC Pipeline, robustes Filter, 2-Punkt Kalibrierung, Offset.
- `PressureStateMachine`: `UNKNOWN`, `SENSOR_FAULT`, `PRESSURE_LOW`, `OK`, `PRESSURE_HIGH` mit Hysterese.
- `PressureHistory`: bounded trend buffer für UI.
- `ConfigStore` + `JsonCodec`: persistente und validierte Konfiguration.
- `WebUI`: Dashboard/History/Settings/Diagnostics + JSON APIs.
- `MqttManager`: reconnect-fähige Telemetrie.

## Zielarchitektur (nächster Ausbauschritt)
- Web-Assets aus LittleFS (`index.html`, `app.js`, `style.css`)
- REST-orientierte API für Status, Config-Bereiche, Kalibrierungsaktionen und Reboot
- Geführter Setup-Modus (AP mit generiertem Passwort + OLED-Ausgabe)
- Schrittweise, entkoppelte WireGuard-Integration erst nach stabiler Mess-/UI-/MQTT-Basis

## Aktueller Stand (Implementierung)
- LittleFS-Hosting für Web-App-Dateien aktiviert.
- REST-Endpunkte für `network`, `mqtt`, `alarm`, `calibration`, `capture`, `clear` implementiert.
- 21-Punkte-Kalibrierdaten sind persistent und werden in der Bar-zu-ADC-Tabelle verwendet.
- Fallback-Setup-Flow: bei WLAN-Fehler startet ein AP mit generiertem Passwort.
- Alarm-Events werden aktiv per Telegram/Webhook versendet (State Change + Wiederholung).
- JSON-Export/Import der Gesamtkonfiguration ist per API integriert.
- OTA-Updatepfad über ArduinoOTA ist aktiv.
- CI-Absicherung (native tests + esp32 build) liegt als GitHub Workflow vor.
- Optionale WireGuard-Status/Enable/Disable-Integration über externe Control-URLs ist integriert.
- Eigenständiges Display-Modul für SSD1306 (statusorientierte, ruhige 128x64-Anzeige) ist integriert.
- Hardware-basierter Debug-Modus via Brücke D25<->D26 ist integriert (Verbose vs. Minimal Logs).
- WLAN-Verbindungslogik nutzt konfiguriertes WLAN mit Secrets-Fallback und persistiert erfolgreiche Fallback-Credentials.

## Qualitätsziele
- Erfolgreicher Build auf `esp32dev`.
- Deterministische Host-Tests für Kernlogik.
- Verständliche Dokumentation für Betrieb und Wartung.

- Build-Stabilität: Konfigurations-Header dürfen keine fremden Framework-Header maskieren.

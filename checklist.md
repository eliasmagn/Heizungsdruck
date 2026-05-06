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

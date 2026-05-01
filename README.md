# Heizungsdruck Monitor (PlatformIO Firmware)

Lean ESP32 firmware for heating pressure monitoring with local web app, persistent config, and MQTT telemetry.

## Migration note (from ESPHome to custom firmware)
This repository is now a **full PlatformIO project** and no longer ESPHome-centric. The old limitations (restricted UI/control paths) are replaced with modular C++ firmware and an embedded web application.

## Architekturentscheidung (ergänzt)
Für das Projektziel ist **eigene Firmware + eigene Webapp in PlatformIO** der saubere Weg. ESPHome bleibt nützlich für einfache "Sensor + MQTT"-Setups, ist für folgende Anforderungen aber zu eng:
- echte Konfigurationsseiten
- geführte Kalibrierung (21 Punkte)
- Passwort- und Netzwerkkonfiguration
- später modulare WireGuard-Steuerung
- saubere, erweiterbare Alarm- und UX-Logik

Deshalb bleibt der technische Schnitt:
- **ESP32/Arduino unter PlatformIO** als Backend
- **LittleFS + statische Web-Dateien** als Frontend-Auslieferung
- **JSON/REST API** als klare Schnittstelle zwischen Firmware und UI

## Project purpose
- Robustly read an analog pressure transducer (10 bar type, operation focus 0–2 bar).
- Detect meaningful heating pressure states and sensor faults.
- Expose actionable local UI (dashboard/history/settings/diagnostics).
- Publish clean MQTT telemetry with reconnect behavior.
- Keep resource usage low and code maintainable.

## Hardware assumptions
- ESP32 dev board (`esp32dev`)
- Analog pressure transducer (0–10V or conditioned signal within ADC-safe range)
- ADC input on GPIO34 by default
- Proper voltage scaling and common ground

## Wiring overview
```
Pressure Sensor OUT -> Signal conditioning / divider -> ESP32 GPIO34 (ADC)
Pressure Sensor GND ---------------------------------> ESP32 GND
Pressure Sensor VCC ---------------------------------> Sensor supply (per datasheet)
```
> IMPORTANT: ESP32 ADC max input is ~3.3V.

## Architecture (concise)
- `PressureSensor`: sampling + robust filtering + fault classification
- `PressureMath`: ADC→voltage/bar conversion + calibration math
- `PressureStateMachine`: `UNKNOWN`, `SENSOR_FAULT`, `PRESSURE_LOW`, `OK`, `PRESSURE_HIGH` with hysteresis
- `PressureHistory`: bounded in-memory trend buffer
- `ConfigStore`: persistent config storage in NVS/Preferences
- `MqttManager`: telemetry + reconnect + status topics
- `WebUI`: local web app and JSON APIs

## Folder map
- `platformio.ini`
- `src/main.cpp`
- `src/modules/*.h|*.cpp`
- `src/config/config.h.example`
- `src/config/secrets.h.example`
- `src/config/ProjectConfig.h`
- `test/test_logic.cpp`

## Setup and secrets handling
1. Copy examples:
```bash
cp src/config/config.h.example src/config/config.h
cp src/config/secrets.h.example src/config/secrets.h
```
2. Edit `src/config/secrets.h` with real Wi-Fi/MQTT credentials.
3. `src/config/config.h` is for device-level overrides.

Tracked source never needs real credentials. `.gitignore` excludes local config/secrets.

## Build / flash / monitor / filesystem upload
```bash
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor
```

> **Wichtig für Deployment:** Änderungen an `data/index.html`, `data/app.js`, `data/style.css` oder `data/assets/*` werden **erst nach `pio run -t uploadfs`** auf dem Gerät sichtbar. `pio run -t upload` alleine aktualisiert nur die Firmware, nicht das LittleFS-Dateisystem.

## WireGuard defaults from config/secrets
WireGuard läuft lokal auf dem ESP32 (kein HTTP-Proxy auf externe Control-URLs).
Compile-time Defaults können weiterhin über Config/Secrets vorbefüllt werden:
- `src/config/config.h`: `WIREGUARD_ENABLED_DEFAULT`, `WIREGUARD_LOCAL_ADDRESS`, `WIREGUARD_NETMASK`, `WIREGUARD_PEER_ENDPOINT`, `WIREGUARD_PEER_PORT`, `WIREGUARD_ALLOWED_IP1`, `WIREGUARD_ALLOWED_IP2`, `WIREGUARD_KEEPALIVE_SECONDS`
- `src/config/secrets.h`: `WIREGUARD_PRIVATE_KEY`, `WIREGUARD_PEER_PUBLIC_KEY`, `WIREGUARD_PRESHARED_KEY`

Zur Laufzeit wird die Tunnelkonfiguration persistent über `wireguard.*` im AppConfig gespeichert und lokal durch `WireGuardManager` aktiviert/deaktiviert.

## Web UI overview
- LittleFS-hosted SPA (`data/index.html`, `data/app.js`, `data/style.css`)
- Eine kanonische LittleFS-Webapp (`/` lädt ausschließlich `data/index.html`)
- Bereiche: Live, Verlauf, Kalibrierung, Einstellungen, Diagnose
- Einstellungen mit API-gebundener Bearbeitung für Sensor, Netzwerk, MQTT, Alarm und lokalen WireGuard-Tunnel
- Kalibrierung: 21 Punkte (0.0…10.0 bar), Capture/Clear und persistentes Speichern
- Verlauf aus `/api/history` mit Canvas-Chart + JSON/CSV-Export
- Diagnose: Statusdump, Telegram-/Webhook-Test, Neustart, Gesamt-Config-Save (`POST /api/config`) und Config-Export/Import
- Netzwerk: WLAN-Scan (`GET /api/wifi/scan`) für SSID-Auswahl im Dropdown
- Netzwerk: einstellbare WLAN-Sendeleistung (`network.wifiTxPowerDbm`) und optionaler 802.11b-only Modus (`network.wifi11bMode`, max. ~11 Mbit/s)


## WireGuard (lokale Implementierung)
- Runtime-Modul: `src/modules/WireGuardManager.*`
- Konfigurierbare Felder: `enabled`, `localAddress`, `netmask`, `privateKey`, `peerEndpoint`, `peerPort`, `peerPublicKey`, `presharedKey`, `allowedIp1`, `allowedIp2`, `keepAliveSeconds`
- Status-Endpunkte liefern strukturierte Local-State-Infos (`enabled`, `configured`, `online`, `lastHandshake`, `lastError`)

**Limitierungen auf ESP32:**
- Die verwendete Arduino-WireGuard-Bibliothek bietet nur begrenzte Laufzeitmetriken; `lastHandshake` ist daher ein best-effort Online-Zeitstempel.
- Erweiterte Features wie detaillierte Peer-Statistiken/Mehrpeer-Management sind nicht vollständig verfügbar.

## OLED Display (SSD1306) – live status
- Display module: `src/modules/display_manager.h` + `src/modules/display_manager.cpp`
- Hardware: 128x64, I2C `0x3C`, SDA `GPIO21`, SCL `GPIO22`
- Top line (yellow area): `WLAN <SSID>` oder `AP <SSID>`, optional `WG`/`wg?`, bei Alarm periodisch `ALARM`
- Main area: großer Druckwert (oder `---` bei invalid)
- Bottom line: `U <wert>V`, bei Alarm `ALARM < <low> bar`, im AP-Setup `PW:<passwort>`
- Render-Strategie: flackerfrei über Full-Frame-Buffer, Update nur bei Zustandsänderung

Beispielverwendung aus `main.cpp`:
```cpp
if (!gDisplay.begin()) {
  Serial.println("Display init failed");
} else {
  gDisplay.showBootScreen();
}
```

## Debug-Bridge (D25 <-> D26)
- Wenn **GPIO25 mit GPIO26 gebrückt** ist, startet die Firmware im **Verbose-Debug-Modus**:
  - Konfigurationsdump nach Boot
  - dichtere Detail-Logs (ADC, Filter, Fault, State, Heap, MQTT/WiFi)
- Ohne Brücke läuft **Minimal-Debug**:
  - nur periodische Kerninfos (`WiFi`, `IP`, `Pressure`, `Valid`)

## WLAN-Verbindungslogik
- Reihenfolge beim Verbinden:
  1. konfigurierte SSID aus persistenter Config (`network.wifiSsid`), falls gesetzt
  2. sonst `WIFI_SSID` aus `secrets.h`
  3. wenn (1) fehlschlägt und `secrets.h` abweicht: Fallback auf `secrets.h`
- Wenn der Secrets-Fallback erfolgreich ist, werden diese Credentials in die persistente Config übernommen.

## Polling / Update-Rate (Druck)
- Das Projekt erzwingt jetzt **konstantes Fast-Polling mit 100 ms** Zykluszeit für die Druckmessung.
- Hintergrund: Netzbetrieb, **keine Batterie-Optimierung notwendig**.
- Die Firmware überschreibt beim Booten `sensor.updateIntervalMs` auf `100`.

## Sensor-Pins / Versorgung
- Default ADC: **GPIO34** (`sensor.adcPin`)
- Optional schaltbare Sensorversorgung: `SENSOR_POWER_PIN` (Default: GPIO32 -> HIGH bei Boot)
- **Wichtig:** GPIO35 ist input-only und kann **nicht** als softwareseitiges GND genutzt werden.
- Sensor-GND muss auf einen echten GND-Pin des ESP32 gelegt werden.

## Website-UX / Polling
- Dashboard-Status pollt alle **1s**
- History-Ansicht pollt alle **5s**
- Settings enthält zusätzliche Sensor-Parameter (Pin, SampleCount, Interval, Fault-Schwellen)

## Webapp-Architektur (aktuell)
- ESP32 liefert Livewerte über `/api/status` und Verlauf über `/api/history`.
- Die SPA in `data/` ist die **einzige** Weboberfläche; es gibt keine konkurrierenden C++-HTML-Seiten mehr.
- `/` liefert ausschließlich `index.html` aus LittleFS, Assets kommen über `/app.js`, `/style.css`, `/assets/*`.
- Unbekannte Nicht-API-Routen fallen auf die SPA (`index.html`) zurück, API-Routen bleiben strikt 404.
- Historie wird direkt aus den API-Daten gezeichnet und als JSON/CSV exportiert.
- Kalibrierung, Settings und Diagnose sind vollständig API-gebunden umgesetzt.

## UI-Designrichtung
- Web-UI wurde auf die gleiche visuelle Linie wie das Display-Theme gebracht:
  - gelbe Statusleiste oben
  - dunkler Hintergrund
  - cyan/blue Akzente für Mess-/Markenbereiche
  - klare Kartenstruktur für Live/History/Kalibrierung

## Zielbild für API und Konfiguration
REST/API ist umgesetzt und umfasst:
- `GET /api/status`
- `GET /api/config`
- `POST /api/config`
- `POST /api/config/network`
- `GET /api/wifi/scan`
- `POST /api/config/mqtt`
- `POST /api/config/alarm`
- `POST /api/config/calibration`
- `POST /api/config/wireguard`
- `GET /api/config/export`
- `POST /api/config/import`
- `POST /api/calibration/capture`
- `POST /api/calibration/clear`
- `POST /api/test/telegram`
- `POST /api/test/webhook`
- `GET /api/wireguard/status` (lokaler Tunnelstatus)
- `POST /api/wireguard/enable` (lokalen Tunnel aktivieren)
- `POST /api/wireguard/disable` (lokalen Tunnel deaktivieren)
- `POST /api/reboot`

Konfigurationsdaten werden als JSON in Preferences persistiert (inkl. WLAN/AP, MQTT, Alarm und Kalibrierpunkten).

## Geplante Modul-Feinstruktur
Die aktuelle modulare Struktur bleibt erhalten und wird bei Bedarf in klarere Verantwortlichkeiten weiter getrennt (z. B. `config_manager`, `sensor_manager`, `calibration`, `display_manager`, `web_server`, `alarm_manager`).
Ziel bleibt: kleine, klar abgegrenzte Komponenten ohne schwergewichtige Frontend-Frameworks.

## Calibration procedure
1. Measure ADC at known low point (`adcLow`, `barLow`).
2. Measure ADC at known high point (`adcHigh`, `barHigh`).
3. Save values via config defaults or later extension in settings.
4. Optionally tune `offsetBar` for local correction.

Wenn mindestens zwei gültige Kalibrierpunkte vorliegen, wird stückweise linear zwischen Punkten interpoliert.
Fallback ohne Punkte bleibt die 2-Punkt-Gerade:
`bar = barLow + (adc-adcLow)/(adcHigh-adcLow) * (barHigh-barLow) + offset`

## Threshold behavior
- Configurable `lowBar`, `highBar`, `hysteresisBar`
- Hysteresis prevents UI/alarm flicker near limits
- Faults are explicit (`SENSOR_FAULT`) and separate from low pressure

## Sensor fault handling
- short to GND (very low ADC)
- short to VCC/rail (very high ADC)
- disconnected (below disconnect threshold)
- implausible jump vs previous valid reading

On fault, reading is flagged invalid and last valid pressure is retained for context.

## MQTT topic map
Base topic: `heizungsdruck` (configurable)
- `heizungsdruck/telemetry` (JSON, periodic)
- `heizungsdruck/state` (retained state string)
- `heizungsdruck/status` (retained online/offline)
- Optional subscription: `heizungsdruck/cmd/restart`

## Reliability notes
- Main loop remains responsive (short loop delay).
- Wi-Fi/MQTT reconnect attempts are throttled.
- Invalid config falls back to defaults through validation.
- Project config loader uses explicit `config/...` include paths to avoid accidental framework `config.h` collisions.
- AlarmManager sends real Telegram/Webhook notifications on alarm states and repeat interval.
- ArduinoOTA is enabled for in-network firmware updates.

## CI / quality gate
- GitHub Actions workflow runs:
  - `pio test -e native`
  - `pio run -e esp32dev`

## Build-Fix Hinweis (WebUI)
- Für den Root-Handler `/` wird `index.html` jetzt zuerst als `fs::File`-Variable geöffnet und dann an `server_.streamFile(...)` übergeben.
- Hintergrund: `WebServer::streamFile` erwartet eine **nicht-konstante Referenz** auf ein File-Objekt; ein direktes temporäres `LittleFS.open(...)` führt auf ESP32-Arduino zu einem Compile-Fehler.

## Testing & verification
Automated:
```bash
pio test -e native
```
Covers:
- ADC→bar conversion
- robust filter behavior with spike
- state/hysteresis transitions
- config validation
- config JSON roundtrip

Manual checklist:
1. Boot device and verify dashboard loads.
2. Check pressure changes in dashboard and history.
3. Force threshold crossing and verify state transitions.
4. Simulate sensor disconnect and verify `SENSOR_FAULT`.
5. Update settings in UI and verify persistence after reboot.
6. Verify MQTT publishes and offline/online status behavior.

## Troubleshooting
- `ModuleNotFoundError: intelhex`: run `pip install intelhex`.
- Build warnings about example config: copy `.example` files as described.
- No web UI: verify Wi-Fi connection and IP in serial monitor.
- Arduino compile error around `LOW`/`HIGH`: fixed by using non-Arduino-conflicting pressure enum names (`PRESSURE_LOW`/`PRESSURE_HIGH`).
- ArduinoJson v7 `MemberProxy ... is private`: avoid `auto section = doc["..."]` copies; use `JsonVariantConst` for section access.

## Limitations / future improvements
- OTA-Update-Seite ist weiterhin offen.


## Stand 2026-04-26
- Single-UI-Architektur ist konsolidiert: nur LittleFS-SPA als Frontend.
- Kalibrierung zeigt 21 Punkte (0.0..10.0 in 0.5 Schritten) und synchronisiert aktiv mit dem ESP (`laden`, `senden`, `capture`, `clear`).
- Deployment bleibt zweistufig: Firmware mit `pio run -t upload`, Webapp-Dateien zusätzlich mit `pio run -t uploadfs`.

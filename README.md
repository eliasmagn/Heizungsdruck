# Heizungsdruck (ESP32 + PlatformIO)

## Kurzbeschreibung
Heizungsdruck ist eine ESP32-Firmware mit **LittleFS-Weboberfläche**, **MQTT-Telemetrie**, **Telegram/Webhook-Alarmen**, **OTA via ArduinoOTA** und **freier Kalibrierung mit max. 20 Punkten**.

## Hardware / Verdrahtung
- ESP32 DevKit (`esp32dev`)
- Drucksensor-Ausgang auf ADC-taugliche 0–3.3V skaliert
- Standard ADC-Pin: `GPIO34`
- Optional Display (Projekt nutzt separaten Display-Manager)

## Build / Flash
```bash
pio run
pio run -t upload
pio device monitor
```

## LittleFS / Web-Assets (`uploadfs`)
Die Weboberfläche liegt in `data/` und wird nach LittleFS geflasht.

```bash
pio run -t uploadfs
```

**Wichtig:**
- `upload` = nur Firmware (`src/*`)
- `uploadfs` = nur Web-Dateien (`data/*`)
- Bei Änderungen in `data/index.html`, `data/app.js`, `data/style.css` ist `uploadfs` zwingend.

## Ersteinrichtung / AP-Modus
Wenn STA-WLAN nicht verbindet, startet das Gerät einen Setup-AP (`network.apSsid`).
AP-Passwort wird bei Bedarf automatisch erzeugt und gespeichert.

## Weboberfläche
- Root `/` liefert die SPA aus LittleFS (`data/index.html`)
- Tabs: **Live**, **Verlauf**, **Kalibrierung**, **Einstellungen**, **Diagnose**
- Konfiguration läuft über REST-API-Endpunkte (`/api/config/*`)

## MQTT konfigurieren und testen
1. In **Einstellungen → MQTT** aktivieren.
2. Host/Port/User/Pass/Topic-Base setzen und speichern.
3. Diagnose prüfen:
   - Serial-Log zeigt Connect/Reconnect/Publish inkl. Topic, Payload-Länge und Fehlerzustand.
   - `/api/diag` enthält `mqttDiag` (connected/state/last error/buffer info).
4. Broker-Topics prüfen:
   - `<topicBase>/status`
   - `<topicBase>/telemetry`
   - `<topicBase>/state`

## Telegram konfigurieren und testen
1. In **Einstellungen → Alarm** `telegramBotToken` und `telegramChatId` setzen.
2. In **Diagnose** „Telegram Test“ auslösen.
3. Ergebnis wird mit HTTP-Status + API-Antwort angezeigt; Fehler sind nicht mehr still.

Bot-Kommandos:
- `/start`
- `/getpres`
- `/setoffset <wert>`
- `/setcalpoint <bar> <adc>`
- `/saveconfig`

## OTA nutzen
ArduinoOTA ist aktiv (`ArduinoOTA.begin()` in `setup()`).

- OTA-Zielname entspricht `network.hostname`.
- Upload aus gleichem Netz mit OTA-fähigem Tool (z. B. Arduino IDE oder PlatformIO Remote/OTA-Workflow).
- Nach OTA-Flash wird nur Firmware ersetzt; bei UI-Änderungen weiterhin `uploadfs` nötig.

## Kalibrierung
- Maximal **20 frei verwaltete Punkte** (`bar`, `adc`, `valid`).
- Punkte können im UI hinzugefügt und gelöscht werden.
- Interpolation erfolgt stückweise linear über **gültige, nach ADC sortierte** Punkte.
- Bei weniger als 2 gültigen Punkten greift der 2-Punkt-Fallback:
  - `adcLow/adcHigh`
  - `barLow/barHigh`
  - plus `offsetBar`

## Troubleshooting
- Weboberfläche unverändert trotz Update → `pio run -t uploadfs`
- MQTT connected, aber keine Daten am Broker → Serial-Logs auf Publish-Fehler/Buffer prüfen, `/api/diag` → `mqttDiag`
- Telegram fehlgeschlagen → Token/Chat-ID prüfen, HTTP-Status + Detailtext in UI/Serial auswerten
- WLAN-Probleme → AP-Modus nutzen und Netzwerkkonfig im UI korrigieren

## Entwicklungshinweise
- Build: `pio run`
- Host-Tests: `pio test -e native`
- Keine Secrets im Repo
- Nicht-blockierende Loops bevorzugen

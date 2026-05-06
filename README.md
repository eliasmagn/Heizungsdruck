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
3. Optional: **„WireGuard für MQTT bevorzugen (Fallback aktiv)“** aktivieren.
4. Diagnose prüfen:
   - Serial-Log zeigt Connect/Reconnect/Publish inkl. Topic, Payload-Länge und Fehlerzustand.
   - `/api/diag` enthält `mqttDiag` (connected/state/last error/buffer info).
5. Broker-Topics prüfen:
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
- WireGuard kommt nicht hoch bei `/24`-Eingabe: `localAddress` darf jetzt auch als CIDR (`10.66.0.2/24`) eingegeben werden; intern wird die IP extrahiert und der Hinweis im Diagnosefeld gesetzt.

## Entwicklungshinweise
- Build: `pio run`
- Host-Tests: `pio test -e native`
- Keine Secrets im Repo
- Nicht-blockierende Loops bevorzugen
Hinweis WireGuard:
- Standard ist **beide Wege möglich** (normales Routing + WireGuard, falls Tunnel aktiv).
- Mit `requireWireguard=true` wird WireGuard für MQTT bevorzugt und der Tunnelstatus diagnostisch geloggt; wenn der Tunnel offline ist, bleibt Fallback-Routing aktiv.
- Für echte Tunnel-Nutzung als MQTT-Host eine Adresse aus dem Tunnelnetz verwenden und `Allowed IPs` passend setzen.
- Zusätzlicher Funktionstest: Im Diagnose-Tab **MQTT Test Publish** ausführen (publisht auf `<topicBase>/telemetry_test`).

## Mehrgerätebetrieb (4 Heizkreise)
Empfohlen ist **ein Gerät je Heizkreis** mit eigener Identität:

| Heizkreis | deviceId | hostname | topicBase |
|----------|----------|----------|-----------|
| HK1 | kreis1 | heizungsdruck-kreis1 | heizungsdruck/kreis1 |
| HK2 | kreis2 | heizungsdruck-kreis2 | heizungsdruck/kreis2 |
| HK3 | kreis3 | heizungsdruck-kreis3 | heizungsdruck/kreis3 |
| HK4 | kreis4 | heizungsdruck-kreis4 | heizungsdruck/kreis4 |

`deviceId` ist jetzt zentral. Wenn `hostname`, `mqtt.clientId` oder `mqtt.topicBase` leer bzw. auf Legacy-Default stehen, werden sie automatisch aus `deviceId` abgeleitet.

Standard-Topics pro Gerät:
- `<topicBase>/telemetry`
- `<topicBase>/state`
- `<topicBase>/status`
- `<topicBase>/telemetry_test`
- `<topicBase>/cmd/restart`

### Home Assistant MQTT Discovery
Beim erfolgreichen MQTT-Connect veröffentlicht das Gerät retained Discovery-Configs unter:
- `homeassistant/sensor/heizungsdruck_<deviceId>_pressure/config`
- `homeassistant/sensor/heizungsdruck_<deviceId>_state/config`
- `homeassistant/sensor/heizungsdruck_<deviceId>_rawadc/config`
- `homeassistant/sensor/heizungsdruck_<deviceId>_filteredadc/config`
- `homeassistant/binary_sensor/heizungsdruck_<deviceId>_valid/config`
- `homeassistant/binary_sensor/heizungsdruck_<deviceId>_alarm/config`

Verfügbarkeit läuft über `<topicBase>/status` (`online`/`offline`).

### Discovery testen
1. Auf jedem ESP32 eigene `deviceId` setzen (`kreis1`…`kreis4`) und speichern.
2. MQTT neu verbinden (Neustart oder Broker reconnect).
3. Im Broker prüfen, ob Discovery-Topics retained vorhanden sind.
4. In Home Assistant unter MQTT-Integration neue Geräte prüfen.
5. Testweise `/api/test/mqtt` auslösen und `telemetry_test` je Gerät prüfen.

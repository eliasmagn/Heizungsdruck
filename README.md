# Heizungsdruck (ESP32 + PlatformIO)

ESP32-Firmware für Heizungsdrucküberwachung mit:
- LittleFS Single-Page-Webapp
- MQTT Telemetrie
- Alarmen (Telegram + Webhook)
- OTA (ArduinoOTA)
- frei definierbarer Kalibrierung (max. 20 Punkte)

## Hardware / Verdrahtung
- ESP32 Dev Board (`esp32dev`)
- Drucksensor (Signal auf ADC-sichere 0..3.3V skalieren)
- Default ADC-Pin: GPIO34

## Build, Flash, Dateisystem
```bash
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor
```
Wichtig: Webapp-Änderungen in `data/*` werden erst nach `uploadfs` wirksam.

## Ersteinrichtung / AP-Modus
Wenn WLAN nicht verbunden werden kann, startet ein Setup-AP (`network.apSsid`). AP-Passwort wird bei Bedarf automatisch erzeugt.

## Weboberfläche
`/` liefert ausschließlich die LittleFS-SPA (`data/index.html`).
Tabs: Live, Verlauf, Kalibrierung, Einstellungen, Diagnose.

## MQTT
In **Einstellungen → MQTT** aktivieren und Host/Port/Topic setzen.

## Kalibrierung (neu)
- Bis zu **20 frei gesetzte Punkte** (`bar`, `adc`, `valid`)
- Punkte im UI hinzufügen/löschen und speichern
- Interpolation: stückweise linear über sortierte gültige Punkte (nach ADC)
- Fallback: 2-Punkt-Kalibrierung (`adcLow/adcHigh`, `barLow/barHigh`) bei <2 gültigen Punkten

## Telegram / Webhook testen
Unter **Diagnose**:
- Telegram-Test
- Webhook-Test
Antworten zeigen HTTP-Status + Antworttext (kein stilles Scheitern).


## Telegram-Kommandos
Der Bot verarbeitet periodisch `getUpdates` und unterstützt:
- `/start`
- `/getpres`
- `/setoffset <wert>`
- `/setcalpoint <bar> <adc>`
- `/saveconfig`

Hinweis: `setoffset`/`setcalpoint` ändern zuerst RAM. Mit `/saveconfig` werden Änderungen persistent gespeichert.

## OTA
ArduinoOTA ist aktiv.
- Hostname: `network.hostname`
- Upload z.B. per Arduino IDE / OTA-fähigem Tool im gleichen Netz
- Standardmäßig ohne OTA-Passwort (bewusst dokumentiert)

## Troubleshooting
- UI veraltet: `pio run -t uploadfs`
- Telegram fehlschlägt: Token/Chat-ID prüfen, HTTP-Status/Antwort in Diagnose + Serial ansehen
- Kein MQTT: Broker/Port/Netz prüfen

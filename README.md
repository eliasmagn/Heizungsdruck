# Heizungsdruck (ESP32 Standard + ESP8266 Slim)

Firmware zur Heizungsdruck-Messung mit schlanker, profilgetrennter Web-Schicht, MQTT und lokaler Driftberechnung.

## Plattformprofile
- `esp32_standard` (Default): Async-Webserver + LittleFS-SPA + volle REST-/Diagnose-API, Persistenz über ESP32 `Preferences` (NVS).
- `esp8266_slim`: Async-Webserver (API-fokussiert, ohne SPA-Fallback), Persistenz über LittleFS-Datei (`/appcfg.json`) statt `Preferences`.
- ADC-Realität:
  - ESP32: 12-bit Pfad (`adcMax=4095`, Default-Pins Druck `34`, NTC `35`, `analogReadResolution(12)`).
  - ESP8266-slim: Single-ADC A0 (`adcMax=1023`, Druck+NTC Default auf `A0`), kein ESP32-Mehrkanalversprechen.

## Async-Web-Architektur
- Web-Schicht nutzt `ESPAsyncWebServer` profilübergreifend.
- HTTP-Callbacks bleiben kurz: kein `delay()`/`yield()` in Request-Handlern.
- Langsamere Aktionen (`reboot`, Telegram/Webhook/MQTT-Test) werden nur eingeplant und in `loop()` abgearbeitet.
- Struktur bleibt offen für spätere SSE/WebSocket-Erweiterung, ohne jetzt zusätzlichen Feature-Umbau.

## Libraries pro Profil (gepinnt)
- `esp32_standard`:
  - `ESP32Async/ESPAsyncWebServer@3.6.0`
  - `ESP32Async/AsyncTCP@3.4.5`
- `esp8266_slim`:
  - `ESP32Async/ESPAsyncWebServer@3.6.0`
  - `ESP32Async/ESPAsyncTCP@2.0.0`

## Build/Flash/Test
```bash
pio run -e esp32_standard
pio run -e esp8266_slim
pio run -t upload
pio run -t uploadfs
pio device monitor
pio test -e native
```

## Routing-Verhalten
- `esp32_standard`: `/api/*` liefert API-Antworten bzw. echte 404, alle anderen unbekannten Pfade fallen auf `index.html` zurück (SPA-Fallback).
- `esp8266_slim`: unbekannte Pfade liefern 404; kein SPA-Fallback.

## JSON-POST im Async-Modell
- Relevante POST-Endpunkte lesen den Request-Body jetzt über Async-Body-Handler (chunk-sicher), nicht mehr über `request->arg("plain")`.
- Deferred-Actions (`/api/reboot`, Telegram/Webhook/MQTT-Test) bleiben unverändert non-blocking.
- `/api/wifi/scan` läuft als async Start/Polling und blockiert den Request-Callback nicht mehr.

# Heizungsdruck (ESP32 Standard + ESP8266 Slim)

Firmware zur Heizungsdruck-Messung mit schlanker, profilgetrennter Web-Schicht, MQTT und lokaler Driftberechnung.

## Plattformprofile
- `esp32_standard` (Default): **Async-Webserver** + LittleFS-SPA (`/`, `app.js`, `style.css`, `assets`) und volle REST-/Diagnose-API.
- `esp8266_slim`: **Async-Webserver (reduziert, ehrlich)** mit API-Fokus (Status/History/Config/Diag/Test-Endpunkte), ohne volle SPA-Auslieferung.

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

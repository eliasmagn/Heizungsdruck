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
- JSON-POST-Endpunkte akzeptieren nur `application/json` und lesen den Body chunk-sicher über den Async-Body-Handler.
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
- `/api/wifi/scan` läuft als deferred async Start/Polling und blockiert den Request-Callback nicht mehr.

## Laufzeitwirksamkeit von Konfigänderungen
- Änderungen an Sensor-/Temperaturkanälen werden in `PressureSensor::updateConfig()` sofort wirksam (Re-Init der betroffenen Pins/OneWire-Instanz).
- Ein Neustart ist dafür nicht erforderlich; bestehende Deferred-Actions bleiben unverändert.


## Konfigurationsgrenzen & Laufzeitwirkung
- ESP8266 Slim startet standardmäßig mit **Druckmessung aktiv** und **Temperatur standardmäßig deaktiviert** (`mode=NONE`), um A0-ADC-Kollisionen zu vermeiden.
- ESP8266 hat nur einen ADC (A0): parallele Druck+NTC-Nutzung auf A0 ist in der Validierung gesperrt; mehrere Analogkanäle/`noise_ref` sind im Slim-Profil standardmäßig nicht zulässig.
- LittleFS wird für Konfiguration zentral über `ConfigStore` verwendet; WebUI mountet/formatieret die FS nicht mehr eigenständig.
- JSON-POST akzeptiert jetzt praxisübliche Header wie `application/json; charset=utf-8`.
- Netzwerkänderungen werden gespeichert, aber nicht aggressiv live erzwungen; für Hostname/PHY/SSID-Änderungen wird ein Reboot bzw. Reconnect-Zyklus empfohlen.

- Shared-ADC/MUX-Felder (`sensor.slim.sharedAdcFrontend`, `sensor.slim.bootSensorSelection`) sind nun in der Laufzeit angebunden: gleiche ADC-Pins sind mit aktivem Shared-Frontend validierungsseitig erlaubt und werden seriell gesampelt.
- Die konkrete externe Frontend-Ansteuerung (z. B. ADS1115/ADS1015/TLA2024/CD4051/TCA9548A) ist weiterhin nicht hardwarespezifisch ausprogrammiert; das Modell steuert aktuell den Shared-Sampling-Pfad (inkl. kurzer Settling-Zeit vor NTC-Messung).
- `bootSensorSelection` wird als Shared-Profil-Intent akzeptiert; die Messlogik bleibt nicht-blockierend und erfasst weiterhin Druck + Temperatur in den jeweiligen Updatezyklen.
- Druck+NTC auf demselben ADC-Pin ist zulässig, **wenn** `sharedAdcFrontend != NONE` gesetzt ist.


## Shared-ADC/MUX Status (ehrlich)
- `sensor.slim.sharedAdcFrontend` und `bootSensorSelection` werden serialisiert **und** für Shared-Sampling/Validierung genutzt.
- `useGlobalNoiseRef` ist runtime-wirksam: Kompensation wird nur angewandt, wenn der Druckkanal `useGlobalNoiseRef=true` hat und genau ein `NOISE_REF` existiert.
- Kanalregeln: eindeutige `id`, max. ein `NOISE_REF`, doppelte `adcPin` nur mit aktivem Shared-Frontend.
- NTC nutzt jetzt echten Median der Samples; DS18B20-Initialisierung erfolgt nur im DS18B20-Modus.
- Config-Save-Pfad: Normalisierung passiert zentral in `saveCfg()`; WebUI überschreibt den RAM-Stand danach nicht mehr mit rohen Kandidatdaten.

- Boot-Resilienz: Ist eine geladene Konfiguration ungültig, wird auf Defaults zurückgesetzt und (falls Store verfügbar) repariert persistiert.


## Konfig-Save-Konsistenz (Stand 2026-05-10)
- Alle API-/Telegram-Saves laufen über den zentralen `saveCfg()`-Pfad in `main.cpp`.
- Dort passieren Persistenz + Normalisierung (z. B. Device-Identity-Abgleich) + Runtime-Reinit.
- Weder WebUI noch Telegram-`/saveconfig` schreiben danach den rohen Kandidaten zurück in ihren internen Zustand.

## Shared-ADC/MUX Ehrlichkeit
- `sensor.slim.sharedAdcFrontend` und `sensor.slim.bootSensorSelection` sind im Runtime-Pfad aktiv (Shared-ADC-Validierung + sequentielles Sampling).
- `useGlobalNoiseRef` ist nur dann wirksam, wenn der Druckkanal selbst dieses Flag setzt **und** genau ein `NOISE_REF`-Kanal vorhanden ist.
- Dadurch ist sichergestellt: keine akzeptierte Konfiguration ohne reale Laufzeitwirkung.

- 2026-05-10 Follow-up 5: WebUI-Konfig-Save-Konsistenz gehärtet: nach zentralem saveCfg()-Pfad wird kein Roh-Kandidat zurückgeschrieben; WireGuard-Post-Save nutzt den normalisierten Runtime-Stand (`cfg_`).

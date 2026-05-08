# Heizungsdruck (ESP32 Standard + ESP8266 Slim)

Firmware zur Heizungsdruck-Messung mit Web-UI, MQTT und einfacher lokaler Driftberechnung.

## Plattformprofile
- `esp32_standard` (Default): Vollprofil mit Web-UI, Multi-ADC, optional WireGuard, OTA, MQTT Discovery.
- `esp8266_slim`: ehrlich reduziertes Profil ohne Full-WebUI, ohne OLED-Displaypfad, ohne WireGuard und ohne ESP32-spezifische WLAN/ADC-APIs.

Build/Flash:
```bash
pio run -e esp32_standard
pio run -e esp8266_slim
pio run -t upload
pio run -t uploadfs
pio device monitor
pio test -e native
```

## Messlogik (bewusst schlank)
- Primärwert: `pressureBar` aus gefiltertem ADC.
- Optionaler Temperaturpfad, Standard: `temperature.mode = ntc`.
- Lokale Drift nur für Druck:
  - `pressureDrift1h`
  - `pressureDrift24h`
- Komplexe Leckanalyse gehört bewusst nachgelagert in Home Assistant / PC / DB.

## `noise_ref` (transparent, nicht „magisch“)
- Optionaler Referenzkanal `noise_ref` für Baseline-Korrektur von ADC-Werten.
- Originalwerte bleiben immer erhalten (`rawAdc`, `filteredAdc`).
- Kompensierter Zusatzwert (`compensatedAdc`) ist Diagnosehilfe, kein Ersatz für den Primärdruckwert.

## ESP8266-Slim Grenzen (ehrlich)
- Intern praktisch nur **ein** ADC-Eingang (A0).
- Druck + NTC + `noise_ref` gleichzeitig intern sind nicht gleichwertig zum ESP32.
- Mehrere Analogsensoren am ESP8266 nur mit externer Hardware (Mux/externes ADC-Frontend).
- WireGuard ist **nicht** Teil des Slim-Profils.

## MQTT / Discovery
Wichtige Topics:
- `<topicBase>/telemetry`
- `<topicBase>/state`
- `<topicBase>/status`

Discovery wird beim MQTT-Connect publiziert.

## Repo-Überblick
- `platformio.ini` – Environments (`esp32_standard`, `esp8266_slim`, `native`)
- `src/main.cpp` – Orchestrierung
- `src/platform_caps.h` – zentrale Feature-Capabilities
- `src/modules/*` – Sensorik, MQTT, Web, State, Config
- `data/*` – Web-Assets (LittleFS)
- `test/*` – Host-Tests

## Entwicklungsprinzipien
- Keine Secrets im Repo
- Keine unnötigen schweren Abhängigkeiten
- Nicht-blockierende Loops bevorzugen


## Plattformhinweis (final)
- ESP32 nutzt Display-Background-Task, WiFi-TX-Power-Mapping und 11b-Profilumschaltung.
- ESP8266 nutzt einen bewusst schlanken Pfad: WebUI und OLED-Display sind deaktiviert, keine ESP32-spezifischen WiFi-API-Aufrufe.
- `pressureBar` bleibt der einzige Primärdruckwert; `compensatedAdc` bleibt reine Diagnose.

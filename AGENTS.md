# AGENTS.md – Heizungsdruck

## Scope
Diese Anleitung gilt für das gesamte Repository.

## Repo layout
- `platformio.ini` – Build/Test Environments (`esp32dev`, `native`)
- `src/main.cpp` – Hauptloop, Orchestrierung
- `src/modules/*` – modulare Firmwarelogik
- `src/config/*.example` – Beispielkonfigurationen
- `test/*` – deterministische Host-Tests
- `README.md` – Betriebs- und Wartungsdokumentation
- `concept.md`, `checklist.md`, `roadmap.md` – laufendes Projekttracking

## Build/Flash/Test
- Build: `pio run`
- Flash: `pio run -t upload`
- Monitor: `pio device monitor`
- Host-Tests: `pio test -e native`

## Coding conventions
- Lean C++17, keine unnötigen Abstraktionen.
- Nicht-blockierende Schleifen bevorzugen; keine langen Delays.
- Keine Secrets im Repository.
- Kleine, klar getrennte Module (Sensor, State, Web, MQTT, Config).

## Done means
1. `pio run` ist erfolgreich.
2. `pio test -e native` ist erfolgreich.
3. Web-UI liefert Dashboard/History/Settings/Diagnostics.
4. MQTT-Telemetrie und Reconnect funktionieren.
5. Doku (`README.md`, `concept.md`, `checklist.md`, `roadmap.md`) ist aktuell.

## Regression constraints
- ESPHome-Altpfade nicht wieder einführen.
- Keine hardcodierten Zugangsdaten.
- Keine schwergewichtigen Frontend-Frameworks.
- Sensorfaults dürfen nicht als "LOW" maskiert werden.

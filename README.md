# Heizungsdruck Monitor

Ein ESP32-basierter Monitor für Heizungsdruck mit Web-UI und MQTT-Integration.

## Projektzweck

Dieses Projekt überwacht den Druck in einer Heizungsanlage kontinuierlich und:
- zeigt den Druck über eine Web-UI an
- sendet Alarme bei zu niedrigem oder zu hohem Druck
- veröffentlicht Daten über MQTT
- speichert Konfiguration dauerhaft
- bietet Trend-Analyse über Zeitverläufe

Das Projekt ist komplett in C++ für ESP32 geschrieben und verwendet PlatformIO als Build-System.

## Hardware-Annahmen

| Komponente | Spezifikation |
|------------|---------------|
| Mikrocontroller | ESP32 (beliebiges Dev-Board) |
| Drucksensor | Analoger 4-20mA oder 0-10V Drucktransducer, 10 bar Range |
| ADC-Pin | GPIO 34 (ADC1_CH6) - konfigurierbar |
| Stromversorgung | 5V USB oder externe Versorgung |

### Empfohlene Drucksensor-Wiring

```
Sensor --------> Spannungsteiler --------> ESP32 ADC Pin 34
     |                                            |
   GND ---------------------------------------- GND
     |
   VCC ---------------------------------------- 5V/3.3V
```

**Wichtig**: Der ADC des ESP32 kann maximal 3.3V messen. Bei 0-10V Sensoren muss ein Spannungsteiler (z.B. 3:1) verwendet werden.

## Projekt-Struktur

```
Heizungsdruck/
├── .gitignore                   # Git-Ignore für Secrets
├── platformio.ini              # PlatformIO Konfiguration
├── README.md                   # Diese Datei
├── AGENTS.md                   # Detailliertes Handbuch
├── src/
│   ├── main.cpp               # Hauptanwendung
│   └── modules/
│       ├── types.h            # Typdefinitionen
│       ├── PressureSensor.h/.cpp   # Sensor-Lesung & Konvertierung
│       ├── PressureStateMachine.h/.cpp  # Alarm-Zustandsmaschine
│       ├── PressureHistory.h/.cpp     # Verlaufsdaten
│       ├── ConfigManager.h/.cpp       # Konfigurations-Persistenz
│       ├── MqttManager.h/.cpp        # MQTT-Integration
│       └── WebUI.h/.cpp             # Web-Server & UI
├── src/config/
│   ├── config.h.example      # Beispiel-Konfiguration
│   ├── config.h             # Deine Konfiguration (nicht in git)
│   ├── secrets.h.example    # Beispiel-Secrets
│   └── secrets.h           # Deine Secrets (nicht in git)
└── test/
    ├── test_pressure_conversion.cpp  # Tests für Konvertierung
    └── test_state_machine.cpp        # Tests für Zustandsmaschine
```

## Schnellstart

### 1. Voraussetzungen

- PlatformIO CLI oder VS Code mit PlatformIO Extension
- ESP32 mit USB-Kabel
- Drucksensor angeschlossen

### 2. Konfiguration

Kopiere die Beispiel-Konfigurationsdateien und passe sie an:

```bash
cp src/config/config.h.example src/config/config.h
cp src/config/secrets.h.example src/config/secrets.h
```

Bearbeite `src/config/config.h`:
- Sensor-Pin (Standard: 34)
- Kalibrierungswerte (siehe unten)
- Schwellenwerte für Alarme

Bearbeite `src/config/secrets.h`:
- WiFi SSID und Passwort
- MQTT-Broker-Daten (falls verwendet)
- Optional: Web-UI Passwort

### 3. Build und Flash

```bash
# Umgebung auflisten
pio device list

# Build
pio run

# Flashen
pio run --target upload

# Serial Monitor
pio device monitor
```

### 4. Web-UI aufrufen

Nach dem ersten Boot zeige die Serial-Monitor-Ausgabe an. Die IP-Adresse wird angezeigt.

```
WiFi verbunden. IP: 192.168.1.100
Öffne im Browser: http://192.168.1.100
```

Die Web-UI ist verfügbar unter:
- **Dashboard**: `http://<IP>/dashboard` - Live-Anzeige und Charts
- **Settings**: `http://<IP>/settings` - Konfiguration ändern
- **Diagnostics**: `http://<IP>/diagnostics` - System-Status

## Kalibrierung

Der Sensor muss kalibriert werden, um ADC-Werte in Druck in bar umzuwandeln.

### Kalibrierungs-Methode: 2-Punkt-Kalibrierung

1. Sensor bei bekanntem Druck (z.B. 0 bar beim Entleeren) messen
2. ADC-Wert notieren (z.B. 400)
3. Sensor bei anderem bekanntem Druck (z.B. über Manometer) messen
4. ADC-Wert notieren (z.B. 3800 bei 10 bar)
5. Werte in `config.h` eintragen:

```c
#define CALIBRATION_ADC_MIN      400   // ADC bei 0 bar
#define CALIBRATION_ADC_MAX      3800  // ADC bei 10 bar
#define CALIBRATION_BAR_MIN      0.0   // Druck bei ADC_MIN
#define CALIBRATION_BAR_MAX      10.0  // Druck bei ADC_MAX
#define PRESSURE_OFFSET          0.0   // Optionaler Offset
```

### Online-Kalibrierung

Alternativ kann die Kalibrierung über die Web-UI geändert werden:

1. Öffne `http://<IP>/settings`
2. Gehe zum Bereich "Kalibrierung"
3. Trage die ADC-Werte ein
4. Speichern

### Optimierung für 0-2 bar Bereich

Da der echte Betriebsbereich 0-2 bar ist, kann die Kalibrierung für diesen Bereich optimiert werden:

```c
// Beispiel: ADC 400 bei 0 bar, ADC 1000 bei 2 bar
#define CALIBRATION_ADC_MIN      400
#define CALIBRATION_ADC_MAX      1000
#define CALIBRATION_BAR_MIN      0.0
#define CALIBRATION_BAR_MAX      2.0
```

Das liefert genauere Messwerte im relevanten Bereich.

## Alarm-Konfiguration

Die Schwellenwerte können in `config.h` oder über die Web-UI geändert werden:

```c
#define PRESSURE_THRESHOLD_LOW   1.0   // Untere Warnschwelle
#define PRESSURE_THRESHOLD_HIGH  2.0   // Obere Warnschwelle
#define HYSTERESIS_BAR           0.1   // Verhindert Flackern
#define ALARM_DEBOUNCE_TIME      5000  // ms bis Alarm aktiv
```

## MQTT-Integration

### Konfiguration

Aktiviere MQTT in `config.h`:

```c
#define MQTT_ENABLED             true
#define MQTT_BROKER             "192.168.1.50"
#define MQTT_PORT               1883
#define MQTT_USERNAME           ""
#define MQTT_PASSWORD           ""
```

### Topic-Struktur

| Topic | Inhalt | Retain |
|-------|---------|--------|
| `heizungsdruck/pressure` | Aktueller Druck (JSON) | nein |
| `heizungsdruck/state` | Druck-Zustand (JSON) | nein |
| `heizungsdruck/fault` | Sensor-Fehler (JSON) | nein |
| `heizungsdruck/uptime` | Betriebszeit (JSON) | nein |
| `heizungsdruck/commands` | Empfangene Kommandos | nein |
| `heizungsdruck/status` | Online/Offline (LWT) | ja |

### Payload-Beispiel

**Pressure-Topic**:
```json
{
  "pressure": 1.45,
  "adc_raw": 1234,
  "adc_filtered": 1225,
  "voltage": 0.99,
  "valid": true,
  "timestamp": 12345678
}
```

**State-Topic**:
```json
{
  "state": 3,
  "state_str": "OK",
  "alarm_active": false,
  "timestamp": 12345678
}
```

### Home Assistant Integration

Aktiviere HA Discovery in `config.h`:

```c
#define HA_DISCOVERY_ENABLED     true
#define HA_DISCOVERY_PREFIX      "homeassistant"
```

Das Gerät taucht automatisch in Home Assistant auf.

### MQTT-Kommandos

Sende Kommandos an `heizungsdruck/commands`:

```json
{"command": "restart"}
{"command": "get_status"}
{"command": "calibrate", "calibration": {"adc_min": 400, "adc_max": 3800, ...}}
{"command": "set_thresholds", "thresholds": {"low_threshold": 1.0, "high_threshold": 2.0, ...}}
```

## API-Endpunkte

### Status-Informationen

```http
GET /api/status
```

Antwort:
```json
{
  "pressure": 1.45,
  "adc_raw": 1234,
  "adc_filtered": 1225,
  "voltage": 0.99,
  "valid": true,
  "state": 3,
  "state_str": "OK",
  "alarm_active": false,
  "fault": 0,
  "wifi_connected": true,
  "wifi_rssi": -45,
  "ip_address": "192.168.1.100",
  "free_heap": 245760
}
```

### History-Daten

```http
GET /api/history
```

Antwort (komprimiert):
```json
{
  "p": [1.2, 1.3, 1.4, ...],
  "t": [12345, 12355, 12365, ...],
  "s": [3, 3, 3, ...],
  "c": 360,
  "min": 1.1,
  "max": 1.6,
  "avg": 1.35
}
```

### Konfiguration lesen

```http
GET /api/config
```

### Konfiguration ändern

```http
POST /api/config
Content-Type: application/json

{
  "thresholds": {
    "low_threshold": 1.0,
    "high_threshold": 2.0,
    "hysteresis": 0.1,
    "debounce_ms": 5000
  },
  "calibration": {
    "adc_min": 400,
    "adc_max": 3800,
    "bar_min": 0.0,
    "bar_max": 10.0,
    "offset": 0.0
  }
}
```

### Gerät neu starten

```http
POST /api/action/restart
```

## Troubleshooting

### Sensor zeigt immer 0 oder falsche Werte

1. Prüfe das Wiring (Pin muss GPIO 34 sein)
2. Spannungsteiler korrekt?
3. Kalibrierungswerte korrekt?
4. ADC-Werte im Web-UI unter "Aktuelle ADC-Werte" prüfen

### WiFi verbindet nicht

1. SSID und Passwort in `secrets.h` korrekt?
2. Router auf 2.4 GHz?
3. IP-Adresse korrekt?

### MQTT-Verbindung fehlschlägt

1. Broker-IP erreichbar?
2. Port korrekt (standard 1883)?
3. Authentifizierung korrekt?

### Web-UI nicht erreichbar

1. IP-Adresse im Serial Monitor prüfen
2. Gerät im selben Netzwerk?
3. Firewall blockiert Port 80?

### Konfiguration wird nicht gespeichert

1. NVS-Persistenz beim ersten Boot initialisiert
2. Reset der Konfiguration: Taster am BOOT-Pin (GPIO 0) während des Starts halten

### Hohem Speicherverbrauch

- History-Größe in `config.h` reduzieren
- Moving Average Buffer Größe verringern
- Unnötige Libraries entfernen

## Limitationen

- Kein Web-Login aktiviert (Standard) - sollte für sichere Netzwerke erweitert werden
- Keine WiFi-Verwaltung (WiFiManager) - muss über `secrets.h` konfiguriert werden
- Keine OTA-Updates implementiert
- Maximale History-Dauer: ~1 Stunde bei 10s Intervall und 360 Einträgen

## Zukünftige Verbesserungen

- [ ] WiFiManager für einfaches Einrichten
- [ ] OTA-Updates
- [ ] Web-UI Authentifizierung
- [ ] IFTTT/Webhook-Integration
- [ ] Mehrere Sensoren unterstützen
- [ ] Lokal gespeicherte Logs auf SD-Karte
- [ ] ESP32-S3 und ESP32-C3 Support

## Migration von ESPHome

Falls du von einer ESPHome-Lösung migrierst:

| ESPHome | Heizungsdruck Monitor |
|---------|---------------------|
| `sensor` | PressureSensor |
| `binary_sensor` | Alarm-Zustand |
| `api` | Web-UI API |
| `mqtt` | MqttManager |
| `text_sensor` | Web-UI Display |
| `switch.restart` | Web-API Restart |

Die Kalibrierung muss neu eingerichtet werden.

## Lizenz

Dieses Projekt ist frei verfügbar für private und kommerzielle Nutzung.

## Support

Für Fragen und Probleme:
1. Siehe AGENTS.md für detaillierte Dokumentation
2. Prüfe den Serial Monitor für Debug-Ausgaben
3. Siehe Troubleshooting-Abschnitt

---

**Version**: 1.0.0  
**Letztes Update**: 2025

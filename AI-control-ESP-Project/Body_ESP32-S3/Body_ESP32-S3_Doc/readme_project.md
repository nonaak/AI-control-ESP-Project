
# 🤖 Body ESP - AI Biofeedback System V3.0

**Intelligent biometrische monitoring met real-time AI overrule functionaliteit**

---

## 🎯 Projectoverzicht

Dit is een geavanceerd **AI-gestuurd biofeedback systeem** dat draait op een ESP32-2432S028R (CYD board). Het systeem monitort real-time biometrische data (hartslag, temperatuur, GSR stress) en gebruikt **machine learning algoritmes** om automatisch machine parameters aan te passen voor optimale veiligheid en comfort.

### ⚡ Kernfunctionaliteiten
- **🧠 AI Overrule Systeem**: Automatische machine speed aanpassing gebaseerd op sensor data
- **📡 ESP-NOW Mesh Netwerk**: Draadloze communicatie tussen 4 ESP modules
- **📊 5-Kanaal Real-time Grafieken**: Hart, temperatuur, GSR, oxygen, machine data
- **💾 Uitgebreide Data Logging**: CSV bestanden met alle sensor + machine data
- **🎛️ Touchscreen Interface**: Intuïtieve bediening met 4-knops layout
- **🔒 Multi-layer Safety**: Emergency stops, timeout protection, sensor validation
- **📈 AI Data Analyse**: Intelligente analyse van opgeslagen sessie data *(NIEUW V3.1!)*
- **🎨 Visual Timeline**: Scrollbare gebeurtenissen balk met kleurcodering *(NIEUW V3.1!)*
- **⚙️ Event Configuratie**: Aanpasbare AI gebeurtenis namen via touchscreen *(NIEUW V3.1!)*

---

## 🌐 ESP-NOW Mesh Network Architectuur

```
┌─────────────────┐    AI Override    ┌─────────────────┐    Motor Control   ┌─────────────────┐
│    Body ESP     │ ─────────────────► │   HoofdESP      │ ──────────────────► │   Pomp Unit     │
│   (Kanaal 1)    │                   │  (Kanaal 4)     │                    │   (Kanaal 3)    │
│ 🧠 AI + Sensors │ ◄───────────────── │ 🎮 Coordinator  │ ◄────────────────── │ ⚙️  Hardware    │
│ 08:D1:F9:DC:C3:A4│   Machine Status  │ E4:65:B8:7A:85:E4│      Status        │ 60:01:94:59:18:86│
└─────────────────┘                   └─────────────────┘                    └─────────────────┘
         ▲                                       ▲
         │ Sensor Data                           │ Remote Control
         │ (Optioneel)                           │ + Monitoring
         ▼                                       ▼
┌─────────────────┐                     ┌─────────────────┐
│     M5Atom      │ ◄─────────────────► │                 │
│   (Kanaal 2)    │    Emergency        │                 │
│ 📱 Remote UI    │                     │                 │
│ 50:02:91:87:23:F8│                     └─────────────────┘
└─────────────────┘
```

### 📋 Module Specificaties

| Module | MAC Adres | Kanaal | Functie | Status |
|--------|-----------|--------|---------|--------|
| **Body ESP** | `08:D1:F9:DC:C3:A4` | 1 | AI + Biometrische sensoren | ✅ **Volledig geïmplementeerd** |
| **M5Atom** | `50:02:91:87:23:F8` | 2 | Remote monitoring/control | 🔶 TODO |
| **Pomp Unit** | `60:01:94:59:18:86` | 3 | Hardware motor control | 🔶 TODO |
| **HoofdESP** | `E4:65:B8:7A:85:E4` | 4 | Centrale coordinatie | 🔶 TODO |

---

## 🧠 AI Overrule Systeem

### Intelligente Risico Analyse
Het AI systeem analyseert continu meerdere biometrische parameters:

```cpp
// Multi-factor Risk Assessment
if (heartRate < 60 || heartRate > 140) riskLevel += 0.4f;  // Hartslag risico
if (temperature > 37.5°C) riskLevel += 0.3f;              // Temperatuur risico  
if (gsrLevel > threshold) riskLevel += 0.3f;               // Stress risico

// Graduated AI Response
trusOverride = 1.0f - (riskLevel * reductionFactor);
sleeveOverride = 1.0f - (riskLevel * reductionFactor);
```

### 🎛️ Configureerbare Parameters
- **Hartslag Thresholds**: Lage (60) en hoge (140) BPM grenzen
- **Temperatuur Maximum**: 37.5°C standaard  
- **GSR Stress Level**: Instelbare baseline en gevoeligheid
- **Reductie Factoren**: Trust (0.8) en sleeve (0.7) aanpassing
- **Recovery Rate**: 0.02/sec geleidelijk herstel

### 🚨 Emergency Conditions
- **Extreme hartslag**: < 40 of > 180 BPM → Onmiddellijke stop
- **Hoge temperatuur**: > 39°C → Emergency protocol  
- **Sensor disconnectie**: → Safety shutdown
- **Manual override**: Altijd mogelijk via UI

---

## 📊 Real-time Data Visualization

### 5-Kanaal Grafiek Systeem
1. **🔴 Hart**: Hartslag signaal met beat detectie markers
2. **🟡 Temp**: Temperatuur trend (MCP9808 sensor)
3. **🔵 Huid**: GSR/stress level monitoring  
4. **🟣 Oxy**: Zuurstof saturatie (dummy/toekomstig)
5. **🟢 Machine**: Trust/sleeve speed visualisatie *(NIEUW!)*

### 🎮 4-Button Interface
```
[🎯 REC] [▶️ PLAY] [📋 MENU] [🤖 AI OFF] 
  Groen     Paars      Blauw    Groen/Rood
```

- **REC**: Start/stop data opname naar SD kaart
- **PLAY**: Afspelen van opgeslagen sessies  
- **MENU**: Toegang tot alle instellingen
- **AI**: Toggle AI overrule systeem (🟢=uit, 🔴=aan)

---

## 🗂️ Project Structuur

### 📁 Core Bestanden
- **Body_ESP.ino**: Hoofdbestand met setup() en loop()
- **gfx4.cpp/h**: 5-kanaal grafiek rendering engine
- **input_touch.cpp/h**: 4-button touchscreen interface
- **sensor_settings.cpp/h**: Multi-pagina sensor kalibratie UI

### 🤖 AI & Communicatie
- **overrule_view.cpp/h**: AI configuratie interface *(NIEUW!)*
- **ESP-NOW callbacks**: Bidirectionele communicatie setup
- **AI algorithms**: Risk assessment en override logica

### 📋 Specificatie Bestanden
- **ESP_NOW_Body_ESP_TODO.txt**: Volledige implementatie spec *(NIEUW!)*
- **ESP_NOW_HoofdESP_TODO.txt**: HoofdESP implementatie guide *(NIEUW!)*
- **ESP_NOW_PompUnit_TODO.txt**: ESP8266 hardware control spec *(NIEUW!)*
- **ESP_NOW_M5Atom_TODO.txt**: Remote UI implementatie spec *(NIEUW!)*

### 🔧 Support Modules  
- **menu_view.cpp/h**: Uitgebreid menu systeem (6 opties)
- **playlist_view.cpp/h**: SD kaart file management
- **confirm_view.cpp/h**: Veiligheids bevestigings dialogs
- **cal_view.cpp/h**: Touch kalibratie interface

---

## 🔌 Hardware Configuratie

### 📡 Sensoren (I2C Bus: SDA=21, SCL=22)
| Sensor | Adres | Functie | Status |
|--------|-------|---------|--------|
| **MAX30102** | `0x57` | Hartslag + SpO2 | ✅ Geoptimaliseerd |
| **MCP9808** | `0x1F` | Precisie temperatuur | ✅ Gekalibreerd |
| **GSR** | `GPIO34` | Huid geleiding/stress | ✅ Smoothing toegevoegd |

### 🖥️ Display System (SPI)
- **ILI9341 TFT**: 320x240 pixels, 16-bit color
  - MISO: GPIO 12, MOSI: GPIO 13, SCK: GPIO 14
  - CS: GPIO 15, DC: GPIO 2, Backlight: GPIO 21
- **XPT2046 Touch**: Multi-touch support
  - IRQ: GPIO 36, MOSI: GPIO 32, MISO: GPIO 39
  - CLK: GPIO 25, CS: GPIO 33

### 💾 Data Storage (SD Card SPI)
- **MicroSD**: High-speed logging
  - MISO: GPIO 19, MOSI: GPIO 23, SCK: GPIO 18, CS: GPIO 5
  - Format: CSV met timestamp, alle sensoren + machine data

### 🌈 Status LEDs (Optioneel)
- Red: GPIO 4, Green: GPIO 16, Blue: GPIO 17

---

## 💾 Data Logging Systeem

### 📈 CSV Format (Uitgebreid)
```
Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause
1234,72.5,36.8,145.2,98.1,1,1.2,0.8,50.0,2.5
```

### 🔄 Real-time Features
- **Automatic file numbering**: `data1.csv`, `data2.csv`, etc.
- **Flush elke 100 samples**: Geen data verlies bij power loss
- **Playback functionaliteit**: Replay opgeslagen sessies
- **File management**: Delete/format via veilige confirmatie dialogs

---

## 🎛️ Menu Systeem (6 Opties)

```
📋 HOOFDMENU (Nieuwe volgorde):
├── 🤖 AI Overrule          [Magenta] ← Prioriteit!
├── ⚙️  Sensor afstelling    [Groen]
├── 🔄 Scherm 180° draaien  [Geel]
├── 🎯 Touch kalibratie     [Oranje] 
├── 💾 Format SD card       [Rood]
└── ⬅️  Terug               [Blauw]
```

### 🤖 AI Overrule Menu (6 Knoppen)
**Uitgebreide AI management interface:**
- **AI Status**: Real-time override status weergave
- **Hartslag**: Lage/hoge BPM thresholds
- **Temperatuur**: Maximum temperatuur grens
- **GSR Parameters**: Baseline, sensitivity, smoothing
- **Reductie Factors**: Trust/sleeve aanpassing sterkte
- **Controls**: 
  - 🎯 **AI AAN/UIT**: Toggle AI overrule systeem
  - 📈 **ANALYSE**: Data analyse van opgeslagen sessies *(NIEUW!)*
  - 🎨 **CONFIG**: Gebeurtenis namen configuratie *(NIEUW!)*
  - 💾 **OPSLAAN**: Instellingen naar EEPROM
  - 🔄 **RESET**: Terug naar standaard waarden
  - ⬅️ **TERUG**: Naar hoofdmenu

### ⚙️ Sensor Instellingen (3 Pagina's)
**Pagina 1 - Hartslag (1/3):**
- Beat Detection Threshold: 10K-100K range
- LED Power: 0-255 (geoptimaliseerd voor detectie)

**Pagina 2 - Temp & GSR (2/3):**
- Temperatuur Offset: ±10°C kalibratie
- Temperatuur Smoothing: 0.0-1.0 filter
- GSR Baseline: 0-4095 referentie punt
- GSR Sensitivity: 0.1-5.0 multiplier
- GSR Smoothing: 0.0-1.0 stabilisatie *(NIEUW!)*

**Pagina 3 - Communicatie (3/3):**
- Baud Rate: 9600-921600 (ESP-NOW backup)
- Timeout: 10-1000ms communicatie timeout

---

## 📈 AI Data Analyse Systeem *(NIEUW V3.1)*

### 📊 Intelligente Sessie Analyse
Het AI systeem kan opgeslagen CSV bestanden analyseren om patronen te ontdekken en instellingen te optimaliseren.

**🔍 Toegang via:** `AI Overrule Menu → ANALYSE`

### 📊 Data Analyse Flow
```
┌───── Bestand Selectie ─────┐
│ • Toont beschikbare CSV's     │
│ • [SELECTEER] kiest bestand   │ → [ANALYSE] → ┌──── Resultaten ────┐
│ • [TERUG] naar AI menu        │              │ • Tekstuele analyse  │
└───────────────────────┘              │ • AI aanbevelingen   │
                                            │ • [TIMELINE] knop    │
                                            └───────────────────┘
                                                       ↓
                                            ┌───── Timeline ─────┐
                                            │ • Visual gebeurtenissen │
                                            │ • Scrollbare balk      │
                                            │ • Kleurcodering        │
                                            └───────────────────┘
```

### 📊 Multi-Factor Analyse Algorithm
```cpp
// Intelligente risico berekening
float eventRatio = (highHR + highTemp + highGSR) / totalSamples;

if (eventRatio > 0.1f) {        // >10% problematisch
  trustReduction = 0.6f;         // Sterke reductie (40%)
  sleeveReduction = 0.7f;        // Sterke reductie (30%)
} else if (eventRatio > 0.05f) { // 5-10% problematisch  
  trustReduction = 0.75f;        // Gemiddelde reductie (25%)
  sleeveReduction = 0.8f;        // Gemiddelde reductie (20%)
} else {                         // <5% problematisch
  trustReduction = 0.9f;         // Milde reductie (10%)
  sleeveReduction = 0.9f;        // Milde reductie (10%)
}
```

### 📈 Analyse Rapport Format
```
Analyse van 1245 samples:
- Gem. hartslag: 78.5 BPM
- Risico events: 23 (1.8%)
- Max snelheden: T=4.2 S=3.8
- Timeline events: 47

ADVIES:
Trust reductie: 10%
Sleeve reductie: 10%
```

---

## 🎨 Visual Timeline Systeem *(NIEUW V3.1)*

### 🗓️ Scrollbare Gebeurtenissen Balk
Intelligente visualisatie van stress events tijdens sessies met focus op **laatste 33%** periode.

### 🎨 Kleurgecodeerde Gebeurtenissen
| Kleur | Event Type | Beschrijving |
|-------|------------|-------------|
| 🔴 **Rood** | Type 0 | Hoge hartslag gedetecteerd |
| 🟠 **Oranje** | Type 1 | Temperatuur boven threshold |
| 🟡 **Geel** | Type 2 | GSR stress indicator |
| 🔵 **Blauw** | Type 3 | Lage hartslag waarschuwing |
| 🟣 **Magenta** | Type 4 | Machine snelheidspieken |
| 🟢 **Paars** | Type 5 | Combinatie stress signalen |
| 🟦 **Cyaan** | Type 6 | Onregelmatige hartslag |
| 🔴 **Dkrood** | Type 7 | Langdurige stress periode |

### ⏱️ Timeline Navigatie
```
┌────────── AI Timeline: data1.csv ──────────┐
│ Duur: 15.3 min | Events: 23 | Focus: laatste 33%  │
├────────────────────────────────────────────────┤
│ ████░░░░░░█░░░██░░░░░░░░██│Kritiek░░░████ │ ← Timeline
│  ▲    ▲      ▲▲          ▲▲           ▲▲▲▲  │
│  │    │      ││          ││           ││││  │
│ 2.1  5.3   8.7█        11.2█        14.6█ │ ← Tijd (min)
├────────────────────────────────────────────────┤
│ [<< VORIGE] [VOLGENDE >>] [TERUG]              │
└────────────────────────────────────────────────┘
```

### 📊 Smart Timeline Features
- **🟡 Gele Lijn**: Markeert laatste 33% (meest kritieke periode)
- **⚪ Witte Randen**: Events in kritieke zone extra gemarkeerd  
- **📉 Event Hoogte**: Toont ernst van gebeurtenis (0-100%)
- **🌀 Auto-scroll**: Start op 70% positie (focus op einde)
- **🔍 Zoom View**: Toont 1/3 van timeline tegelijk
- **🎨 Kleur Legenda**: Interactieve uitleg per event type

---

## ⚙️ AI Event Configuratie *(NIEUW V3.1)*

### 🎨 Aanpasbare Gebeurtenis Namen
Personaliseer de AI gebeurtenis beschrijvingen via een gebruiksvriendelijke interface.

**🔍 Toegang via:** `AI Overrule Menu → CONFIG`

### 📄 Event Namen Editor Interface
```
┌─────── AI Gebeurtenis Config ───────┐
│ Pagina 1/2 - Events 1-4                │
├─────────────────────────────────────────┤
│ 1: [Hoge hartslag gedetecteerd    ] [EDIT] │
│ 2: [Temperatuur boven drempel     ] [EDIT] │  
│ 3: [GSR stress indicator          ] [EDIT] │
│ 4: [Lage hartslag waarschuwing    ] [EDIT] │
├─────────────────────────────────────────┤
│ Druk EDIT om gebeurtenis naam te wijzigen  │
│ [VORIGE] [VOLGENDE] [OPSLAAN] [RESET] [TERUG] │
└─────────────────────────────────────────┘
```

### ✏️ Smart Edit Functionaliteit
**Voor demonstratie doeleinden cycleerd elke EDIT knop door 3 voorbeeldteksten:**

**Event 1 (Hoge Hartslag) voorbeelden:**
1. `"Hartslag te hoog - interventie nodig"`
2. `"Hoge hartslag gedetecteerd"` 
3. `"Hartfrequentie boven drempel"`

**Event 2 (Temperatuur) voorbeelden:**
1. `"Temperatuurstijging gedetecteerd"`
2. `"Lichaamstemperatuur te hoog"`
3. `"Temperatuur boven veilige grens"`

*(Verdere events hebben vergelijkbare cyclische voorbeelden...)*

### 💾 EEPROM Persistent Storage
- **💾 Opslag Locatie**: EEPROM adres 600 (na sensor config)
- **🔒 Validatie**: Magic number 0xABCD1234
- **📏 Capaciteit**: 64 karakters per event naam (8 events)
- **🔄 Auto-restore**: Laadt vorige instellingen bij opstarten
- **⚙️ Default Fallback**: Automatische terugval naar standaard namen

### 🔗 Integratie met Timeline
- **📈 Live Updates**: Gewijzigde namen direct zichtbaar in analyse
- **🎨 Kleur Legenda**: Timeline gebruikt configureerbare namen
- **🔄 Consistentie**: Één enkele bron voor alle event beschrijvingen

### 🎮 Bediening
1. **🔍 Pagina Navigatie**: VORIGE/VOLGENDE voor events 1-4 en 5-8
2. **✏️ Edit Mode**: Groen gekleurde achtergrond tijdens bewerken
3. **💾 Opslaan**: OPSLAAN knop voor permanent opslag
4. **🔄 Reset**: RESET voor terugkeer naar standaard namen
5. **⬅️ Navigatie**: TERUG naar AI Overrule hoofdmenu

---

## 🔍 Debug & Monitoring

### 📊 Serial Output (115200 baud)

**Startup Configuration:**
```
=== SENSOR CONFIGURATIE GELADEN ===
HR Threshold: 50000, LED Power: 47
Temp Offset: 0.00, Temp Smoothing: 0.20
GSR Baseline: 512, Sensitivity: 1.00, Smoothing: 0.10
Comm Baud: 115200, Timeout: 100ms
=====================================
```

**Real-time Debug (elke 2 seconden):**
```
=== SENSOR DEBUG ===
Hartslag - IR: 85432, Filtered: 12.34, Beat: JA, BPM: 72
Temperatuur - MCP9808: OK, Waarde: 36.8°C
GSR - Raw: 1523, Baseline: 512, Smooth: 145.67, Smoothing: 0.10
ESP-NOW - Trust: 1.2, Sleeve: 0.8, Suction: 50.0, Pause: 2.5
Status - Recording: NEE, Playing: NEE, SD: OK
=====================
```

**ESP-NOW Communicatie:**
```
ESP-NOW RX: T:1.2 S:0.8 Su:50.0 P:2.5
ESP-NOW TX: T:0.9 S:0.7 Overrule:ON Cmd:AI_OVERRIDE
```

---

## 🚀 Implementatie Roadmap

### ✅ **FASE 1: Body ESP (VOLTOOID + V3.1 UITBREIDINGEN)**
- [x] ESP-NOW bidirectionele communicatie
- [x] AI overrule algoritme met risico assessment  
- [x] 5-kanaal real-time grafiek systeem
- [x] Uitgebreide sensor configuratie (3 pagina's)
- [x] AI overrule menu met live status
- [x] CSV data logging met machine parameters
- [x] 4-button touchscreen interface
- [x] Debug output en monitoring
- [x] **AI Data Analyse Systeem** *(NIEUW V3.1)*
- [x] **Visual Timeline met Gebeurtenissen** *(NIEUW V3.1)*
- [x] **Event Configuratie Interface** *(NIEUW V3.1)*
- [x] **EEPROM Persistent Storage** *(NIEUW V3.1)*
- [x] **Smart Menu Reorganization** *(NIEUW V3.1)*

### 🔶 **FASE 2: Critical Path (TODO)**
- [ ] **HoofdESP**: Central coordinator implementation
  - [ ] ESP-NOW mesh setup (3 peers)
  - [ ] AI override command processing
  - [ ] Machine parameter calculation
  - [ ] Safety timeout management
- [ ] **Pomp Unit**: Hardware control (ESP8266)
  - [ ] ESP-NOW receiver (ESP8266 API)
  - [ ] Motor PWM control (trust/sleeve)
  - [ ] Safety monitoring (temp, limits, emergency)
  - [ ] Status feedback naar HoofdESP

### 🔶 **FASE 3: Advanced Features (OPTIONEEL)**
- [ ] **M5Atom**: Remote monitoring interface
  - [ ] LED matrix status visualization
  - [ ] Button-based remote control
  - [ ] Session data logging
  - [ ] Emergency override capabilities
- [ ] **System Integration**: End-to-end testing
- [ ] **Optimization**: Latency, reliability, power

---

## 📚 Required Libraries

```cpp
// Core ESP32 libraries
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>

// ESP-NOW Communication
#include <esp_now.h>
#include <WiFi.h>

// Display & Graphics
#include <Arduino_GFX_Library.h>

// Sensors
#include "MAX30105.h"  // Hartslag sensor
#include "heartRate.h" // Beat detection algorithms

// Custom Modules
#include "gfx4.h"                 // 5-channel graphics engine
#include "input_touch.h"          // 4-button interface
#include "sensor_settings.h"       // Multi-page configuration
#include "overrule_view.h"        // AI settings interface
#include "ai_analyze_view.h"      // AI data analysis *(NIEUW V3.1)*
#include "ai_event_config_view.h" // Event configuration *(NIEUW V3.1)*
#include "menu_view.h"            // Main menu system
```

---

## ⚠️ Safety & Compliance

### 🔒 **Multi-Layer Safety System**
1. **Hardware Emergency Stops**: Independent physical circuits
2. **AI Safety Bounds**: Validated parameter ranges (0.0-1.0)
3. **Communication Timeouts**: 10s Body ESP, 5s Pomp Unit
4. **Sensor Validation**: Range checking, disconnect detection
5. **Graceful Degradation**: Gradual speed reduction, not sudden stops
6. **Manual Override**: User always has final control

### 📊 **Performance Targets**
- **AI Response Time**: < 200ms (sensor → action)
- **Communication Latency**: < 100ms ESP-NOW
- **Emergency Stop**: < 500ms total system response
- **Data Logging**: 1Hz continuous, no missed samples
- **Touch Response**: < 100ms UI feedback
- **Reliability**: >99% uptime, <1% packet loss

### 🧪 **Testing Requirements**
- [ ] Normal operation under all sensor conditions
- [ ] Emergency stop scenarios (hardware + software)
- [ ] Communication timeout recovery
- [ ] AI threshold breach responses  
- [ ] Long-term stability (8+ hour sessions)
- [ ] Data integrity verification

---

## 🔧 Troubleshooting Guide

### 🚨 **Common Issues**

**ESP-NOW Communication Failures:**
```
1. Check MAC addresses in all modules
2. Verify WiFi channels (Body=1, M5=2, Pomp=3, Hoofd=4)
3. Ensure peer registration is successful
4. Monitor Serial debug output for TX/RX errors
```

**Sensor Reading Problems:**
```
1. I2C device scan: Wire.beginTransmission() tests
2. Check power supply stability (sensors sensitive to voltage)
3. Verify sensor configuration in Settings menu
4. Use Serial debug output for real-time values
```

**AI System Issues:**
```
1. Check AI enable status (green=off, red=on)
2. Verify sensor thresholds in Overrule menu
3. Monitor risk level calculations in debug output
4. Test manual emergency override
```

**Touch Interface Problems:**
```
1. Run touch calibration via menu
2. Check touch rotation settings (if display rotated)
3. Verify XPT2046 SPI connections
4. Test with Serial debug touch coordinates
```

---

## 📝 Version History

### 🎆 **V3.1 (Current) - "AI Analytics & Configuration"**
- **📈 AI Data Analyse**: Complete CSV file analysis with intelligent recommendations
- **🎨 Visual Timeline**: Scrollable event visualization with color coding
- **⚙️ Event Configuration**: Customizable AI event names via touchscreen
- **💾 EEPROM Storage**: Persistent event name configuration
- **🔄 Menu Reorganization**: AI-first menu priority structure
- **🎨 UI Improvements**: Compact buttons, better color contrast
- **🔍 Smart Analysis**: Risk-based parameter recommendations

### 🚀 **V3.0 - "AI Revolution"**
- **🧠 AI Overrule System**: Complete machine learning integration
- **📡 ESP-NOW Mesh**: 4-module wireless communication
- **📊 5th Graph Channel**: Machine data visualization
- **🎛️ 4th Button**: AI toggle in main interface
- **🤖 AI Settings Menu**: Complete configuration interface
- **💾 Extended CSV**: Machine + sensor data logging
- **🔧 Enhanced Debugging**: Real-time system monitoring

### 📈 **V2.0 - "Sensor Excellence"**
- Multi-page sensor settings (3 pages)
- Improved MAX30102 configuration
- GSR sensor with smoothing
- MCP9808 temperature with offset
- EEPROM settings persistence

### 🌱 **V1.0 - "Foundation"**
- Basic 4-channel graphing
- Touch interface
- SD card logging
- Menu system
- Sensor integration

---

## 🤝 Contributing & Development

### 📋 **TODO Specifications**
Elke ESP module heeft een complete implementatie specificatie:
- **Message structures** met exacte data types
- **Code examples** voor ESP-NOW setup
- **Safety requirements** en error handling
- **Testing scenarios** en acceptance criteria
- **Hardware integration** details

### 🔗 **Getting Started**
1. Begin met **HoofdESP** implementatie (critical path)
2. Gebruik de TODO files als complete blauwdruk
3. Test elke module afzonderlijk voor integration
4. Volg de debug output formats voor troubleshooting

### 🎯 **Next Priority**
**HoofdESP Implementation** - De centrale coordinator die:
- AI overrides van Body ESP ontvangt
- Machine commands naar Pomp Unit stuurt
- Safety management en timeout handling
- Status feedback naar alle modules

---

## 🌟 Hoe AI Event Namen Te Wijzigen

### 📝 Stap-voor-stap Instructies:
1. **🔍 Navigate**: Hoofdmenu → 🤖 AI Overrule → 🎨 CONFIG
2. **📄 Selecteer**: Events 1-4 (pagina 1) of 5-8 (pagina 2) 
3. **✏️ Bewerk**: Druk [EDIT] naast gewenste gebeurtenis
4. **🔄 Cycle**: Knop cycleerd door voorbeeldteksten (demo mode)
5. **💾 Opslaan**: Druk [OPSLAAN] voor permanente opslag
6. **✅ Verificatie**: Check timeline legenda voor nieuwe namen

### 📊 Waar Event Namen Verschijnen:
- **📈 AI Analyse Timeline**: Kleur legenda gebruikt configureerbare namen
- **📋 Debug Output**: Serial monitor toont event beschrijvingen  
- **🔄 Consistentie**: Alle modules gebruiken één enkele bron

**🏆 EEPROM locatie**: Adres 600 | **🔒 Magic**: 0xABCD1234 | **📏 Capaciteit**: 64 chars/event

---

**🚀 Ready to revolutionize biofeedback with AI-powered analytics & customization!**


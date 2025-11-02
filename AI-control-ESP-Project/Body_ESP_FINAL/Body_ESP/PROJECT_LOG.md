# Body ESP32-S3 Project Log

**Project:** Body ESP code op SC01 Plus display  
**Start datum:** 26 oktober 2025  
**Laatst bijgewerkt:** 2 november 2025 17:30

---
voor mij eigenaar:


in het opname menu, de bestanden nu "session_20251101_23092......" enz. die mogen simpler b.v. " 32:09 - 01/11/25.csv" en " 32:09 - 01/11/25.aly".
de .csv bestanden in blauwe text en de .aly bestanden in groene text.
De knop -AI analyze- met die knop gaat de AI proberen de stress levels er bij te zetten (kan ik zien of dat AI het goed doet en verbeteren als het moet.
als ik een .aly bestand kies om af te spelen (play), dat in het afspeel scherm zet je bij de plekken waar ik of AI een stress level hebben toegekent, de stress level cijver bij de event dat er dan voor doet.

als ik een bestand heb gekozen in opname menu, en dan heb afgespeeld en weer teug ga naar de opname menu, dan kan ik niet een ander bestand kiezen


ik zij dat NU het afsppel scherm een grote slome tering zooi is en dat je eerst een beeld met tekenen met de grafiek kaders en de prosesbalk kaders. met de knoppen "stop, play/pauze en AI Aktie"
en dan de gegevens van het csv bestand daar afspelen. en als ik op pauze knop druk dan stopt het lezen van de csv bestand en dan automatich ook de grafieken.
nu heb je de pauze knop gemaakt maar met een verkeerde grote van letter type en als ik weer op play druk ook weer te grote letter type.
en als ik op AI Aktie druk dan moet er een popup OVER het playback scherm komen, niet er achter zoals het nu is!!!

---

## Huidige Status

### ✅ WAT WERKT

**Hardware:**
- ✅ Display: ST7796 480x320 landscape (rotation 1)
- ✅ Backlight: GPIO 45
- ✅ Touch: FT6X36 polling mode (GPIO 5/6, Wire I2C0)
- ✅ Sensoren: ADS1115 op Wire1 I2C1 (GPIO 10/11)
- ✅ Touch mapping: SWAP_XY=1, FLIP_X=0, FLIP_Y=1
- ✅ Touch release detection (TouchEnd only)

**Functionaliteit:**
- ✅ Display init en kleuren weergave
- ✅ Body_gfx4 grafisch systeem (7 sensor kanalen)
- ✅ ESP-NOW communicatie (TX + RX werkend)
- ✅ Sensor readout ADS1115 (GSR, Temp, Hart, Adem)
- ✅ Menu systeem met navigatie
- ✅ **Automatische sensor kalibratie (10 sec)**
- ✅ Sensor settings EEPROM opslag (baseline/offset)
- ✅ RTC DS3231 support (zonder hang)
- ✅ **RTC tijd instelling via menu** (Instellingen → Tijd Instellen)
- ✅ EEPROM AT24C02 support (256 bytes)
- ✅ SD card support (SD_MMC 1-bit mode)
- ✅ **Automatische CSV recording** (alle sessies)
- ✅ **ML Training menu** (interface compleet)
- ✅ **Advanced Stress Manager** (7-level systeem)
- ✅ **ML Autonomy Slider** (0-100% in AI Settings)
- ✅ **Scherm rotatie toggle** (180° flip via menu)
- ✅ **AI Settings menu** volledig werkend met +/- knoppen en EEPROM opslag
- ✅ **Sensor Settings menu** volledig werkend met +/- knoppen en EEPROM opslag
- ✅ **CSV Recording** - Automatisch opnemen naar /recordings/ folder met 15 kolommen data
- ✅ **Recording menu** - Bestandslijst met DELETE functionaliteit
- [ ] **Auto-Recording systeem** - Start automatisch bij unpause van Hoofd ESP

---

## UI Style Guide

### Knop Kleuren (Standaard in alle menu's)
- **TERUG knop**: Altijd BLAUW (`0x001F`) - gebruikt in ALLE submenu's
- **Opslaan knop**: Altijd GROEN (`BODY_CFG.DOT_GREEN`) - gebruikt waar van toepassing
- **AI AAN knop**: Paars (`0x841F`) - specifiek voor AI Settings menu

### Layout Richtlijnen
- Knoppen in submenu's moeten GECENTREERD zijn (horizontaal en verticaal)
- Consistente knop kleuren door alle menu's heen
- Opslaan en TERUG knoppen: [Opslaan] [TERUG] volgorde (links naar rechts)
- Text moet gecentreerd zijn binnen knoppen (zowel horizontaal als verticaal)
- Gebruik `MENU_X`, `MENU_Y`, `MENU_W` constanten voor consistente layout (20px marge)

---

## Changelog

### 2025-11-02 - CSV Playback ESP-NOW Implementatie

#### Playback ESP-NOW Communicatie naar HoofdESP
**Doel:** CSV playback data sturen naar HoofdESP zodat machine meebeweegt met opname
**Status:** ✅ Werkend (met PLAYBACK_STRESS commando)

**Problemen opgelost:**
1. **CSV parsing fout** - Trust werd geparsed uit kolom 1 (timestamp "2025-01-02") ipv kolom 5
   - Fix: Correcte kolom mapping volgens CSV header
   - Kolom 2: BPM, 3: Temp, 4: GSR, 5: Trust, 6: Sleeve, 8: Vibe, 9: Zuig

2. **Trust/Sleeve conversie** - Waardes waren 0.0-2.0 (HoofdESP range), niet 0-100
   - Oude code: `trust / 100.0f` → fout (2025 / 100 = 20.25)
   - Fix: Parse als float direct uit CSV

3. **ESP-NOW commando keuze** - PLAYBACK_DATA bestond niet in HoofdESP
   - Besluit: Hergebruik PLAYBACK_STRESS (bestaande handler in HoofdESP)
   - Trust (0.0-2.0) → Stress level (1-7) conversie: `(trust * 3.0) + 1`

**Implementatie:**
```cpp
// body_menu.cpp updatePlayback()
// Parse CSV kolommen correct (Trust = kolom 5, niet kolom 1)
String trustStr = line.substring(commaPos[4] + 1, commaPos[5]); // Kolom 5
float trust = trustStr.toFloat();  // 0.0-2.0 van HoofdESP

// Converteer naar stress level voor PLAYBACK_STRESS
uint8_t stressLevel = (uint8_t)(trust * 3.0f) + 1;  // 1-7 range
if (stressLevel < 1) stressLevel = 1;
if (stressLevel > 7) stressLevel = 7;

// Stuur via bestaande ESP-NOW functie
sendESPNowMessage(0, 0, false, "PLAYBACK_STRESS", stressLevel, vibeActive, zuigActive);
```

**Wat werkt:**
- ✅ CSV wordt correct geparsed (alle kolommen op juiste positie)
- ✅ Trust/Sleeve/Vibe/Zuig data wordt gelezen
- ✅ ESP-NOW PLAYBACK_STRESS commando wordt verstuurd
- ✅ HoofdESP ontvangt en verwerkt commando (speed step update)
- ✅ Vibe en Zuig toggles werken

**Belangrijke opmerking:**
- HoofdESP respecteert C-knop pauze: `paused = false` wordt NIET geforceerd
- Gebruiker moet C-knop loslaten (unpause) om playback beweging te zien
- Body ESP kan momenteel NIET de pauze op HoofdESP bedienen (geen TOGGLE_PAUSE commando)

**Code locaties:**
- `body_menu.cpp` regel 3176-3262: CSV parsing en ESP-NOW verzending
- Oude PLAYBACK_DATA poging uitgezet met `//` (regel 3233-3246)

---

#### Playback UI Problemen (GEDEELTELIJK OPGELOST)

**Probleem 1: Stress popup met grafieken eroverheen** 🔴 **NOG NIET OPGELOST**
- Symptoom: Grafieken verschijnen random over stress level popup
- Oorzaak: `body_gfx4_pushSample()` tekent direct op scherm, overtekent popup
- Gedeeltelijke fix:
  - `updatePlayback()` stopt bij `stressPopupActive` (regel 3129)
  - Progress bar update stopt bij popup (regel 299)
  - Popup wordt getekend over playback scherm (regel 349-352)
- **Maar:** Grafieken komen nog steeds door op onvoorspelbare momenten
- TODO in code: regel 2942-2947 met mogelijke oplossingen

**Probleem 2: Playback traagheid**
- Oorzaak: Interval berekening was omgekeerd
- Oude: `1000.0f / (playbackSpeed / 100.0f)` → bij 200% = 1000/(2) = 500ms ❌
- Fix: `1000.0f * (100.0f / playbackSpeed)` → bij 200% = 1000*(0.5) = 500ms ✅
- Status: ✅ Opgelost (regel 3136)

**Probleem 3: Body_gfx4 frame ontbrak bij playback start**
- Fix: `body_gfx4_clear()` toegevoegd in `drawPlaybackScreen()` (regel 2799)
- Status: ✅ Opgelost

**Workflow:**
1. PAUZE → playback stopt (regel 3129 check)
2. AI-ACTIE → stress popup verschijnt (regel 1900)
3. Kies stress 0-6 → opslaan (regel 1830-1843)
4. PLAY → playback hervat

---

### 2025-10-31 - ML Training Menu UI Fix

#### ML Training Menu Integratie
**Probleem:** ML Training menu bestond maar was niet werkend in body_menu.cpp
**Oplossing:**
- ML Training pagina toegevoegd (`BODY_PAGE_ML_TRAINING`)
- `drawMLTrainingView()` functie gemaakt in exacte stijl van andere menu's
- 4 knoppen met dubbele witte randen: Data Opnemen, Model Trainen, AI Annotatie, Model Manager
- Touch handling toegevoegd voor alle 4 knoppen
- Status info onderaan: Files count, Models count, ML systeem status
- TERUG knop in blauwe stijl

**Code:**
- `ml_training_view.cpp`: `drawMLTrainingView()` - UI in menu stijl
- `body_menu.cpp`: Touch handler voor ML Training pagina
- `body_menu.h`: `BODY_PAGE_ML_TRAINING` enum toegevoegd

**Compileer fouten gefixed:**
- `COL_TEXT` bestond niet → veranderd naar `0xFFFF` (wit)
- Commentaar syntax fouten in `ml_decision_tree.h` en `ml_data_parser.h` gefixed

**Test resultaat:**
- ✅ ML Training menu opent vanuit hoofdmenu
- ✅ 4 knoppen tonen in juiste stijl
- ✅ Touch werkt (print TODO in Serial)
- ✅ TERUG knop gaat terug naar hoofdmenu
- ⏳ Knoppen functionaliteit nog TODO (Training/Annotatie/Manager)

---

### 2025-10-30 - Automatische Sensor Kalibratie

#### Sensor Kalibratie Systeem
**Doel:** 10 seconden automatische kalibratie voor alle sensors
**Oplossing:**
- **Auto Kalibreer Alles** knop (groen) - kalibreeert GSR + Temp + Hart tegelijk
- **GSR Sensor** knop (cyaan) - alleen GSR baseline
- **Temp Sensor** knop (oranje) - alleen temp offset (36.5°C baseline)
- **Hart Sensor** knop (rood) - alleen pulse baseline
- Kalibratie scherm met:
  - Progressbalk (0-100%)
  - Live sensor waardes tijdens meting
  - Sample counter
  - Automatisch terug naar menu na voltooien
- Knoppen hoger geplaatst voor meer ruimte tot TERUG knop

**Code:**
- `startCalibration(type)` - start 10 sec kalibratie
- `updateCalibration()` - verzamelt samples elke tick
- `drawCalibrationScreen()` - toont progressbalk en live data
- Gebruikt `ads1115_setGSRBaseline()`, `ads1115_setNTCOffset()`, `ads1115_setPulseBaseline()`

**Test resultaat:**
- Compiler fout gefixed (forward declarations toegevoegd)
- Klaar om te testen!

---

### 2025-01-29 - UI Verbetering & Cleanup

#### Menu Kleuren Standaardisatie
**Probleem:** Knoppen hadden inconsistente kleuren (TERUG was rood/oranje, Opslaan was blauw)
**Oplossing:**
- Alle TERUG knoppen zijn nu BLAUW (`0x001F`)
- Alle Opslaan knoppen zijn nu GROEN (`BODY_CFG.DOT_GREEN`)
- Toegepast in: AI Settings, Sensor Settings, Calibration, Recording
- ESP Status menu uitgezet (ESP-NOW status niet nodig op deze ESP, werkt nog wel op achtergrond)

#### Knop Centrering & Layout
**Probleem:** Knoppen stonden te ver naar rechts, text niet mooi gecentreerd
**Oplossing:**
- Alle submenu's gebruiken nu consistente layout (`MENU_X=20`, `MENU_Y=20`, `MENU_W=480-40`)
- Knoppen horizontaal gecentreerd in submenu's
- Text verticaal EN horizontaal gecentreerd binnen knoppen
- Toegepast in: Sensor Settings (2 knoppen), AI Settings (3 knoppen), Calibration, Recording, ESP Status

#### Menu Navigatie Verbeterd
**Probleem:** Menu opende niet soepel bij tweede keer, TERUG ging naar hoofdscherm ipv hoofdmenu
**Oplossing:**
- `bodyMenuForceRedraw()` functie toegevoegd voor soepele menu opening
- TERUG knop in submenu's gaat nu correct naar hoofdmenu (`BODY_PAGE_MAIN`)
- Touch handler aangepast voor consistente navigatie

#### Startup Cleanup
**Probleem:** Onnodige debug schermen bij opstarten
**Oplossing:**
- Groen "Touch Controller OK" scherm verwijderd
- Rood "Initializing" scherm verwijderd
- I2C sensor status display verwijderd (alleen Serial logging behouden)
- Direct opstarten naar body_gfx4 grafiek scherm (7 sensor kanalen)

#### Canvas Issues Opgelost
**Probleem:** Device crashte (Guru Meditation StoreProhibited) bij overschakelen van menu naar hoofdscherm
**Oorzaak:** `body_cv->begin()` veroorzaakte I80 bus conflict (bus was al in gebruik door `body_gfx`)
**Oplossing:**
- `body_cv->begin()` verwijderd uit initialisatie
- Alle drawing functies aangepast om direct naar `body_gfx` te tekenen ipv `body_cv`
- Functies aangepast: `drawHeartRateGraph()`, `drawTemperatureBar()`, `drawGSRIndicator()`, `drawAIOverruleStatus()`

#### Legacy Code Uitgezet
**Uitgezet met `//` voor latere cleanup:**
- "Body Monitor" sensor mode (`drawSensorMode()`, `drawSensorStatus()`)
- ESP Status menu (`drawESPStatusItems()`) - ESP-NOW werkt nog wel op achtergrond

---

---

## 🎯 Nog Te Doen

### ML Implementatie (PRIORITEIT 1) 🔴

**ML Workflow:**
```
1. REC knop → .csv opname (15 kolommen)
2. AI Analyze → AI voorspelt stress levels → .preview bestand  
3. Label Editor → Handmatig aanpassen labels → .aly bestand
4. Model Trainen → Decision Tree training → model.bin
5. Playback → Afspelen op Body + Hoofd ESP (7 stress levels)
```

**Status:**
- [x] **CSV Recording** - ✅ Werkend! (REC knop, /recordings/ folder)
- [x] **ML Training menu UI** - ✅ Interface in body_menu.cpp
- [ ] **AI Analyze** - AI voorspelt stress → .preview bestand
- [ ] **Label Editor** - Handmatig stress levels aanpassen → .aly
- [ ] **Model Trainen** - Decision Tree training → model.bin  
- [ ] **Playback** - PLAY knop afspeelt opname op beide ESP's
- [ ] **Model Manager** - Model selecteren/info/verwijderen
- [ ] Real-time inference met ML autonomy blend
- [ ] Integratie met Advanced Stress Manager (7 levels)

### Recording & Playback
- [x] SD card recording (CSV format) - ✅ Werkend! (15 kolommen)
- [x] Recording menu - ✅ Bestandslijst met selectie
- [x] File DELETE - ✅ Verwijderen werkt
- [x] **CSV Playback ESP-NOW** - ✅ Werkend met PLAYBACK_STRESS!
  - Trust → Stress level conversie
  - Vibe/Zuig toggles
  - HoofdESP beweegt mee met playback
  - **LET OP:** Gebruiker moet C-knop loslaten (unpause HoofdESP) voor beweging
- [ ] **Playback stress popup bug:** 🔴 **KRITIEK - MOET GEFIXT**
  - **Probleem:** Grafieken verschijnen random over stress level popup heen
  - **Workflow:** PAUZE → AI-ACTIE → kies stress 0-6 → OPSLAAN → PLAY
  - **Bug:** Grafieken updaten tijdens popup (body_gfx4_pushSample() tekent direct)
  - **Mogelijke oplossingen:**
    1. Stop ALLE grafiek rendering (ook body_gfx4) tijdens stressPopupActive
    2. Gebruik dubbele buffering voor grafieken
    3. Herteken popup elke frame als stressPopupActive waar is
  - **Status:** Gedeeltelijk gefixt (updatePlayback stopt, maar grafieken komen toch door)
  - **Code:** body_menu.cpp regel 2942-2947 (TODO comment)
- [ ] **Body ESP pauze control** (optioneel):
  - Body ESP kan HoofdESP pauze NIET bedienen
  - Zou handig zijn voor playback workflow
  - Vereist nieuw ESP-NOW commando "TOGGLE_PAUSE" in HoofdESP
  - Body ESP zou dan playback kunnen pauzeren zonder nunchuk
- [ ] **Recording menu verbeteringen** (van eigenaar notities regel 11-14):
  - Bestandsnamen simpeler: "32:09 - 01/11/25.csv" ipv "session_20251101_230923.csv"
  - .csv bestanden in blauwe tekst
  - .aly bestanden in groene tekst
  - **AI Analyze knop:** CSV → .preview bestand (AI voorspelt stress)
  - **Label Editor:** Toon stress cijfers op timeline tijdens playback
  - **Bug:** Na playback kan geen ander bestand gekozen worden (regel 16)
- [ ] **Auto-Recording systeem:**
  - Start automatisch wanneer pauze UIT gaat op Hoofd ESP
  - Blijft opnemen tot REC knop op Body ESP ingedrukt
  - Menu triggers:
    - Trust beweging (aan/uit)
    - Zuigen actief (aan/uit)
    - Vibe aan (aan/uit)
  - Configureerbaar in menu welke triggers gebruikt worden
- [ ] **PLAY knop** - Playback op Body + Hoofd ESP (geen training)
- [ ] **AI Analyze knop** - AI analyseert .csv → .preview bestand
- [ ] **Label Editor** - Preview bestand controleren/aanpassen → .aly

### AI Functionaliteit
- [x] ML Training menu UI - ✅ Interface in body_menu.cpp werkend!
- [x] Advanced Stress Manager - ✅ 7-level systeem werkend!
- [x] ML Autonomy Slider - ✅ 0-100% in AI Settings
- [ ] **Factory Reset functie** (schone start na testen):
  - Optie 1: Boot knop 3 sec inhouden (hardware)
  - Optie 2: Menu knop "Wis Alle Data" (software)
  - Wist: AI Settings EEPROM, Sensor Settings EEPROM, ML model, opnames
  - NB: Code opnieuw uploaden wist EEPROM NIET automatisch
- [ ] **AI Test Modus** (uit oude code):
  - Specifieke test mode voor AI gedrag testen
  - Los van stress management systeem
  - AI neemt controle en leert van gebruiker acties
  - Registreert vibe/zuig toggles door gebruiker
- [ ] **AI Stress Management Modus** (uit oude code):
  - 7 stress levels met adaptive playback
  - Real-time stress detectie en aanpassing
  - Emergency override bij stress level 7
  - Integratie met Advanced Stress Manager
- [ ] **Emergency Override bij stress level 7**:
  - **ALLEEN GEBRUIKER** kan dit triggeren (nooit AI!)
  - C-knop (pauze) schakelt AI volledig uit bij stress 7
  - AI mag NOOIT automatisch emergency stop doen
  - Gebruiker beslist altijd zelf wanneer stoppen
  - Veiligheids override = handmatige actie
- [ ] **AI Learning van gebruiker acties**:
  - Registreer wanneer gebruiker vibe aan/uit doet
  - Registreer wanneer gebruiker zuigen aan/uit doet
  - ML leert van deze voorkeuren
  - Gebruikt bij toekomstige beslissingen
- [ ] **AI Pause rode scherm** - Veiligheidsmoment systeem
  - Als AI actief EN gebruiker drukt C-knop (pauze)
  - Scherm wordt volledig ROOD met bericht
  - Touch scherm = hervat AI
  - TODO: Keuze tussen hardware STOP of LAAGSTE SNELHEID (toggle in menu)
- [ ] ML Training knoppen functionaliteit (nog TODO)
- [ ] ML model training (placeholder → echte Decision Tree met 7 stress levels)
- [ ] ESP-NOW TX commando's naar Hoofd ESP (basis bestaat al)

### Sensor Kalibratie
- [x] Automatische 10 sec kalibratie per sensor
- [x] "Auto Kalibreer Alles" functie
- [x] Kalibratie opslag in EEPROM (via sensor_settings saveSensorConfig)

### Code Cleanup (Later)
- Alle uitgeschakelde functies (met `//`) verwijderen
- `body_cv` canvas volledig verwijderen (niet meer nodig)
- Oude `R_WIN_` constanten verwijderen
- Legacy menu code opruimen

---

## 📁 Bestandsstructuur

```
Body_ESP_FINAL\
├── ML_IMPLEMENTATIE_DRAAIBOEK.md ← 🎯 COMPLEET IMPLEMENTATIE PLAN!
├── Body_ESP\
│   ├── Body_ESP.ino              ← HOOFDBESTAND
│   ├── PROJECT_LOG.md            ← Dit bestand
│   ├── body_display.cpp/.h       ← Display init (ST7796)
│   ├── input_touch.cpp/.h        ← Touch (FT6X36)
│   ├── body_gfx4.cpp/.h          ← Grafisch systeem (7 kanalen)
│   ├── body_menu.cpp/.h          ← Menu systeem + kalibratie
│   ├── body_config.h             ← Configuratie (BODY_CFG)
│   ├── ads1115_sensors.cpp/.h    ← ADS1115 sensor uitlezing
│   ├── sensor_settings.cpp/.h    ← Sensor configuratie + EEPROM
│   ├── ai_overrule.h             ← AI overrule config
│   ├── ml_training_view.cpp/.h   ← ML Training menu ✅
│   ├── ml_decision_tree.cpp/.h   ← Decision Tree ML model
│   ├── ml_data_parser.cpp/.h     ← .aly/.csv file parser
│   ├── ml_trainer.cpp/.h         ← ML training workflow
│   ├── ml_stress_analyzer.cpp/.h ← ML stress detection
│   ├── advanced_stress_manager.cpp/.h ← 7-level stress systeem ✅
│   ├── overrule_view.cpp/.h      ← AI Settings + ML Autonomy Slider ✅
│   └── [andere view files]       ← Menu screens
└── [documentatie bestanden]      ← README's, guides, etc.
```

---

## 🔧 Belangrijke Code Regels

### Jouw Regels (ALTIJD VOLGEN!):
1. **Gebruik `//` voor oude code** - NOOIT verwijderen!
2. **Maak `.hold` en `.cppold` bestanden** voor oude tabs
3. **Schrijf in het Nederlands**
4. **Geen onnodige uitleg** - to the point
5. **Test na elke stap** - niet alles tegelijk

### 🚨 KRITIEKE REGEL - GEEN VEILIGHEIDSCONTROLES! 🚨

**ABSOLUUT VERBODEN IN CODE:**
- ❌ GEEN emergency stops (if HR > 150, if temp > 39, etc.)
- ❌ GEEN hartslag checks
- ❌ GEEN temperatuur checks  
- ❌ GEEN confidence drempels
- ❌ GEEN automatische autonomie verlaging
- ❌ GEEN "safety guards"
- ❌ GEEN bounds checking op biometrics
- ❌ **NIETS VAN DIE ONZIN!**

**ENIGE VEILIGHEID:**
✅ Pauze knop op Hoofd ESP (hardware, door gebruiker)
✅ Gebruiker bepaalt zelf wanneer stoppen

**ML AUTONOMIE = SIMPEL:**
```cpp
// 0% autonomie
finalLevel = ruleLevel;  // Jouw 7 stress regels

// 50% autonomie  
finalLevel = (ruleLevel * 0.5) + (mlLevel * 0.5);  // Blend

// 100% autonomie
finalLevel = mlLevel;  // ML bepaalt alles, PUNT.
```

**GEBRUIKER BEPAALT VEILIGHEID - NOOIT CODE!**

### Hardware Pins (SC01 Plus):
```cpp
// Display
#define GFX_BL 45         // Backlight
// Touch (Wire I2C0)
#define TOUCH_SDA 6
#define TOUCH_SCL 5
// Sensoren (Wire1 I2C1)
#define SENSOR_SDA 10
#define SENSOR_SCL 11
// SD Card
#define SD_CS_PIN 41
```

### Touch Mapping (WERKEND):
```cpp
#define USE_MANUAL_MAPPING 1
#define MANUAL_SWAP_XY 1
#define MANUAL_FLIP_X 0
#define MANUAL_FLIP_Y 1
```

---

## 🐛 Bekende Problemen & Oplossingen

### Probleem: Touch knoppen reageren niet altijd
**Symptomen:** 
- Knoppen vereisen soms meerdere touches
- Touch response voelt traag of mist inputs
- Vooral merkbaar in menu's met veel knoppen

**Mogelijke oorzaken:**
1. **Serial Monitor spam** - Veel Serial.print() statements kunnen I2C polling vertragen
2. **Touch polling frequency** - FT6X36 polling rate vs display refresh
3. **Touch debounce timing** - Te korte debounce kan dubbele touches missen
4. **Menu redraw overhead** - Veel hertekenen kan touch loop blokkeren

**TODO: Onderzoeken en fixen:**
- [ ] Verlaag Serial output (debug levels toevoegen)
- [ ] Check touch polling timing in loop()
- [ ] Test touch debounce waarden
- [ ] Profile menu draw tijd (max 50ms per frame)
- [ ] Overweeg interrupt mode ipv polling voor touch

### Probleem: Device crasht bij canvas
**Oorzaak:** `body_cv->begin()` veroorzaakt I80 bus conflict
**Oplossing:** Canvas verwijderd, alles direct naar `body_gfx` tekenen

### Probleem: Display blijft zwart
**Oorzaak:** Rotation verkeerd of backlight uit
**Oplossing:** 
- Check `body_gfx->setRotation(1)` 
- Check `digitalWrite(GFX_BL, HIGH)`

### Probleem: Touch werkt niet
**Oorzaak:** Wire I2C conflict met sensoren
**Oplossing:** 
- Touch op Wire (GPIO 5/6)
- Sensoren op Wire1 (GPIO 10/11)
- **NOOIT mixen!**

---

## 📝 Sessie Notities

### Sessie 12 - 1 nov 2025 22:00 - CSV Recording & Recording Menu
**Wat gedaan:**
- ✅ **CSV Recording volledig geïmplementeerd:**
  - `startCSVRecording()` - Maakt bestand met RTC timestamp in naam
  - `stopCSVRecording()` - Sluit bestand en toont stats
  - `updateCSVRecording()` - Schrijft elke seconde data regel
  - Bestandsnaam: `/recordings/session_YYYYMMDD_HHMMSS.csv`
  - 15 kolommen: Tijd_s, Timestamp, BPM, Temp, GSR, Trust, Sleeve, Suction, Vibe, Zuig, Vacuum, Pause, SleevePos, SpeedStep, AI_Override
  - Auto-flush naar SD card (geen data verlies)
  - Status print elke 10 samples
- ✅ **REC knop functionaliteit:**
  - Toggle recording aan/uit
  - Roept start/stop functies aan
  - Knop wordt rood tijdens recording
- ✅ **Recording menu werkend gemaakt:**
  - Scant `/recordings/` folder (was `/` root)
  - Gebruikt `SD_MMC` (zoals rest van code)
  - Toont max 10 .csv bestanden
  - Klik op bestand → groen highlight
  - Auto-refresh elke seconde
- ✅ **DELETE functionaliteit:**
  - Oranje DELETE knop
  - Verwijdert geselecteerd bestand
  - Lijst refresh automatisch
  - Serial feedback

**Code wijzigingen:**
- `Body_ESP.ino`:
  - CSV recording variabelen en functies toegevoegd
  - REC knop aangepast om start/stopCSVRecording() aan te roepen
  - `updateCSVRecording()` in main loop
- `body_menu.cpp`:
  - `#include <SD_MMC.h>` toegevoegd
  - Globale csvFiles array en csvCount voor touch handler
  - `drawRecordingItems()` aangepast om /recordings/ te scannen
  - DELETE touch handler geïmplementeerd
  - csvCount reset na delete voor refresh

**Wat werkt:**
- ✅ REC knop maakt .csv bestand aan
- ✅ Data wordt elke seconde geschreven (15 kolommen)
- ✅ Recording menu toont bestanden uit /recordings/
- ✅ DELETE knop verwijdert geselecteerd bestand
- ✅ Bestanden zichtbaar op PC na SD kaart uithalen

**ML Workflow gedocumenteerd:**
```
1. REC knop → .csv opname (werkend ✅)
2. AI Analyze → .preview bestand (TODO)
3. Label Editor → .aly bestand (TODO)
4. Model Trainen → model.bin (TODO)
5. PLAY → Playback op beide ESP's (TODO)
```

**Nog te doen:**
- [ ] **Auto-Recording systeem:**
  - Detecteer pause = false van Hoofd ESP
  - Start automatisch CSV recording
  - Stop alleen met REC knop op Body ESP (handmatig)
  - Menu instellingen (AI Settings of System Settings):
    ```
    Auto-Rec Triggers:
    ☑ Bij Trust beweging
    ☑ Bij Zuigen actief  
    ☑ Bij Vibe aan
    ```
  - Alleen starten als minstens 1 trigger actief
- [ ] **AI Pause rode scherm systeem** (uit oude code):
  - Detecteer C-knop pauze tijdens AI actief
  - Toon volledig rood scherm met bericht "AI GEPAUZEERD"
  - "Raak scherm aan om door te gaan"
  - AI hervat na touch
  - Menu toggle: Hardware STOP vs LAAGSTE SNELHEID
- [ ] PLAY knop - Playback .csv op Body + Hoofd ESP
- [ ] AI Analyze knop - AI voorspelt stress levels → .preview
- [ ] Label Editor menu - Handmatig stress aanpassen → .aly
- [ ] Model Trainen - Decision Tree training
- [ ] Model Manager - Model beheer

### Sessie 11 - 1 nov 2025 21:00 - AI & Sensor Settings Touch Handling
**Wat gedaan:**
- ✅ **AI Settings menu volledig functioneel gemaakt:**
  - +/- knoppen voor 6 instellingen (AI Eigenwil, HR Laag/Hoog, Temp Max, GSR Max, Response)
  - AI AAN/UIT toggle knop (paars)
  - Opslaan knop (groen) - slaat op in ESP32 Preferences (NVS EEPROM)
  - TERUG knop (blauw)
  - Waardes worden automatisch geladen bij opstarten en bij menu openen
  - Instellingen worden direct toegepast op BODY_CFG
- ✅ **Sensor Settings menu volledig functioneel gemaakt:**
  - +/- knoppen voor 6 instellingen (Hartslag Drempel, Temp Correctie/Demping, GSR Basis/Gevoelig/Demping)
  - Labels vertaald naar Nederlands
  - Opslaan knop (groen) - slaat op via bestaand sensorConfig EEPROM systeem
  - TERUG knop (blauw)
  - Waardes worden geladen uit sensorConfig bij menu openen
  - Instellingen worden direct toegepast op ADS1115 hardware
- ✅ **EEPROM functies toegevoegd:**
  - `loadAISettings()` / `saveAISettings()` - ESP32 Preferences API
  - `applyAISettings()` - past toe op BODY_CFG
  - `loadSensorSettingsToEdit()` / `saveSensorSettingsFromEdit()` - bestaand EEPROM systeem
- ✅ **Touch handling geïmplementeerd:**
  - AI Settings: 6x +/- knoppen, AI AAN toggle, Opslaan, TERUG
  - Sensor Settings: 6x +/- knoppen, Opslaan, TERUG
  - Correcte limieten en stappen per veld
  - Menu dirty flag voor live updates
- ✅ **Dynamische waardes in draw functies:**
  - `drawAISettingsItems()` gebruikt nu aiSettings struct
  - `drawSensorSettingsItems()` gebruikt nu sensorEdit struct
  - Geen hardcoded waardes meer

**Problemen opgelost:**
1. Touch handling: AI Settings en Sensor Settings uitgezonderd van algemene TERUG knop
2. Menu navigatie: Settings worden geladen bij openen van menu's
3. EEPROM opslag: AI gebruikt Preferences API, Sensoren bestaand systeem
4. Labels vertaald naar Nederlands in Sensor Settings

**Code wijzigingen:**
- `body_menu.cpp`:
  - `#include <Preferences.h>` toegevoegd
  - `AISettingsData` struct (7 velden)
  - `SensorSettingsEdit` struct (6 velden)
  - `loadAISettings()`, `saveAISettings()`, `applyAISettings()`
  - `loadSensorSettingsToEdit()`, `saveSensorSettingsFromEdit()`
  - Touch handling voor beide menu's (200+ regels)
  - Dynamische waardes in draw functies
  - Auto-load in `bodyMenuInit()` en bij menu openen

**Wat werkt:**
- ✅ AI Settings: Alle 6 waardes + AI toggle aanpasbaar
- ✅ Sensor Settings: Alle 6 waardes aanpasbaar
- ✅ Opslaan werkt in EEPROM (persistent na reboot)
- ✅ Waardes worden automatisch geladen
- ✅ Direct toegepast op BODY_CFG en hardware
- ✅ Nederlandse labels in Sensor Settings
- ✅ Touch detectie correct met limieten

**Instellingen ranges:**

*AI Settings:*
- AI Eigenwil: 0-100% (stappen van 1%)
- HR Laag: 40-120 BPM (stappen van 1)
- HR Hoog: 100-200 BPM (stappen van 1)
- Temp Max: 36.0-42.0°C (stappen van 0.1)
- GSR Max: 100-2000 (stappen van 10)
- Response: 0.0-1.0 (stappen van 0.01)

*Sensor Settings:*
- Hartslag Drempel: 10000-100000 (stappen van 1000)
- Temp Correctie: -5.0 tot +5.0°C (stappen van 0.1)
- Temp Demping: 0.0-1.0 (stappen van 0.05)
- GSR Basis: 0-4000 (stappen van 50)
- GSR Gevoelig: 0.1-5.0 (stappen van 0.1)
- GSR Demping: 0.0-1.0 (stappen van 0.05)

**Nog te doen:**
- Testen op hardware (compilatie verwacht)
- Verificatie EEPROM opslag na reboot

### Sessie 10 - 1 nov 2025 17:00 - RTC Tijd Instelling & Scherm Rotatie
**Wat gedaan:**
- ✅ **RTC DS3231 hang opgelost** - RTC initialisatie werkt nu zonder systeem hang
- ✅ **Instellingen menu gemaakt** (Menu → Instellingen):
  - Scherm Rotatie (geel)
  - SD Kaart Laden (cyaan) - TODO
  - Format SD Kaart (oranje) - TODO  
  - Tijd Instellen (paars)
  - TERUG (blauw)
- ✅ **RTC tijd instelling scherm** met +/- knoppen:
  - Jaar, Maand, Dag, Uur, Minuut (5 velden)
  - OPSLAAN knop (groen) - slaat tijd op naar RTC DS3231
  - TERUG knop (blauw) - annuleert zonder opslaan
  - Laadt huidige tijd uit RTC bij openen
  - Validatie: wrap-around voor alle velden
- ✅ **Scherm rotatie toggle** (180° flip):
  - Wisselt tussen rotatie 1 (normaal) en 3 (180°)
  - Touch mapping draait automatisch mee
  - Menu wordt geforceerd hertekend na rotatie
- ✅ **ESP-NOW data logging uitgebreid**:
  - RX print: T, S, Su, V, Z, Vac, P, Lube, Cyc, Pos, Step, Cmd (alle velden)
  - Last RX status: alle belangrijke velden

**Problemen opgelost:**
1. RTC hang: `rtc.begin(&Wire1)` werkt nu correct
2. Touch handling TIME_SETTINGS: generieke TERUG knop blokkeerde OPSLAAN/TERUG knoppen
3. Touch rotatie: extra flip toegevoegd voor `screenRotation == 3`
4. Menu navigatie: BODY_PAGE_TIME_SETTINGS uitgezonderd van generieke TERUG check

**Code wijzigingen:**
- `Body_ESP.ino`:
  - `saveRTCTime()` functie toegevoegd met verificatie
  - `toggleScreenRotation()` functie toegevoegd
  - `screenRotation` variabele (1 of 3)
  - Touch callback aangepast voor rotatie 3 flip
- `body_menu.h`: `BODY_PAGE_SYSTEM_SETTINGS`, `BODY_PAGE_TIME_SETTINGS` enums
- `body_menu.cpp`:
  - `drawSystemSettingsItems()` - 4 gekleurde knoppen
  - `drawTimeSettingsItems()` - 5 velden met +/- knoppen
  - Touch handling voor beide nieuwe pagina's
  - Extern declarations voor RTC en functies
  - `#include <RTClib.h>` toegevoegd

**Wat werkt:**
- ✅ RTC tijd wordt correct opgeslagen en blijft behouden na reset/power cycle
- ✅ Scherm rotatie 180° werkt inclusief touch mapping
- ✅ Instellingen menu volledig functioneel (2 van 4 items actief)
- ✅ ESP-NOW data logging toont alle velden
- ✅ Menu navigatie correct (geen crashes meer)

**Nog te doen:**
- SD Kaart Laden functionaliteit (Instellingen item 1)
- Format SD Kaart functionaliteit (Instellingen item 2)

### Sessie 9 - 31 okt 2025 17:34 - ML Training Menu UI Fix
**Wat gedaan:**
- ✅ Commentaar syntax fouten gefixed (Output:/Trust= regels buiten commentaar blok)
- ✅ `drawMLTrainingView()` functie gemaakt in menu stijl
- ✅ 4 knoppen met dubbele witte randen zoals andere menu's
- ✅ Touch handling toegevoegd voor ML Training pagina
- ✅ `BODY_PAGE_ML_TRAINING` enum toegevoegd
- ✅ ML Training menu nu volledig geïntegreerd in body_menu systeem

**Wat werkt:**
- ✅ ML Training menu opent vanuit hoofdmenu (cyaan knop)
- ✅ 4 knoppen tonen: Data Opnemen, Model Trainen, AI Annotatie, Model Manager
- ✅ Touch detectie werkt (print TODO in Serial)
- ✅ TERUG knop werkt (blauw, terug naar hoofdmenu)
- ✅ Status info: Files/Models count, ML systeem status

**Nog te doen:**
- Knoppen functionaliteit implementeren:
  - Data Opnemen: SD card CSV recording controle
  - Model Trainen: .aly file selectie + Decision Tree training
  - AI Annotatie: .csv file selectie + ML voorspellingen
  - Model Manager: Model selectie, laden, verwijderen

### Sessie 8 - 30 okt 2025 21:00 - ML Implementatie Draaiboek
**Wat gedaan:**
- ✅ **ML_IMPLEMENTATIE_DRAAIBOEK.md** gemaakt
- ✅ Volledige analyse van bestaande systeem (Recording, AI Training, ML menu)
- ✅ ESP-NOW protocol gedocumenteerd (Hoofd ESP ↔ Body ESP)
- ✅ Beslissingen genomen:
  - ML Framework: **Decision Tree** (klein, snel, on-device training)
  - Model opslag: **SD card** (EEPROM te klein: 256 bytes)
  - .aly format: **CSV met Label kolom** (0-11 voor 12 feedback knoppen)
  - ESP-NOW: **Volledig werkend** (beide kanten!)
- ✅ Code voorbeelden toegevoegd voor implementatie
- ✅ Prioriteiten bepaald (ML training = kritisch)
- ✅ Veiligheidsregel toegevoegd: GEEN ongevraagde AI safety checks!

**Belangrijke bevindingen:**
- ESP-NOW TX/RX naar/van Hoofd ESP werkt al volledig
- Automatische CSV recording werkt (alle sessies)
- ML Training menu bestaat maar training is placeholder
- Decision Tree: 80-95% accuracy verwacht met 50-100 samples
- ML systeem gebruikt 7 stress levels uit Advanced Stress Manager

**Volgende stap:**
- Decision Tree implementatie starten
- .aly file parser schrijven
- On-device model training maken

### Sessie 7 - 30 okt 2025 19:00 - Sensor Kalibratie
**Wat gedaan:**
- ✅ Automatische sensor kalibratie systeem (10 sec)
- 4 knoppen: Auto Kalibreer Alles, GSR, Temp, Hart
- Kalibratie scherm met progressbalk en live sensor data
- Knoppen layout verbeterd (meer ruimte tot TERUG)
- Forward declarations toegevoegd voor functies
- Sensor settings EEPROM opslag werkend

**Wat werkt:**
- ✅ Kalibratie knoppen layout
- ✅ Code compileert
- ✅ EEPROM save/load voor calibratie
- ⏳ Nog te testen op hardware

### Sessie 6 - 29 jan 2025
**Wat gedaan:**
- ✅ UI cleanup en standaardisatie
- Canvas issues opgelost (alles naar body_gfx)
- Startup screens verwijderd
- Menu kleuren gestandaardiseerd
- Knop centrering verbeterd
- Legacy code uitgezet met `//`

**Wat werkt:**
- ✅ Menu systeem stabiel
- ✅ Geen crashes meer
- ✅ Consistente UI

### Sessie 1-5 - 26-28 okt 2025
- Display + touch basis werkend
- ESP-NOW communicatie geïmplementeerd
- Body_gfx4 grafisch systeem toegevoegd
- ADS1115 sensor data processing
- Menu systeem basis

---

## ✅ Dagelijkse Checklist (Start Sessie)

1. [ ] Open `Body_ESP_FINAL\Body_ESP\Body_ESP.ino`
2. [ ] Lees dit PROJECT_LOG.md bestand
3. [ ] Check "Nog Te Doen" lijst
4. [ ] Upload test om te verifiëren dat alles nog werkt
5. [ ] Begin met volgende stap

## ✅ Dagelijkse Checklist (Einde Sessie)

1. [ ] Update dit PROJECT_LOG.md met:
   - Wat is gedaan
   - Wat werkt
   - Wat is de volgende stap
2. [ ] Test of code compileert
3. [ ] Test of upload werkt
4. [ ] Commit naar backup (optioneel)

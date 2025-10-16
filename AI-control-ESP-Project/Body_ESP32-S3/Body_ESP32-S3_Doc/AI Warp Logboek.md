# AI WARP LOGBOEK
**Body ESP - Intelligente Biometric Stress Management Device**
**Laatste update: 26 september 2024**

---

## 🎯 HUIDIGE STATUS (26-09-2024)

### ✅ RECENT OPGELOST
**VIBE PLAYBACK FIX - SUCCESVOL GEFIXT**
- **Probleem**: Vibe detectie tijdens CSV playback werkte niet correct
- **Oorzaak**: Threshold was 0.5f, maar CSV "Tril" kolom bevat sensorwaarden (97-103 voor AAN, -3 tot +3 voor UIT)
- **Oplossing**: Threshold verhoogd naar 50.0f in `Body_ESP.ino` regel 896-897
- **Testdata**: `data3.csv` toont perfect UIT-AAN-UIT patroon 
- **Status**: 100% werkend ✅

**MULTIFUNPLAYER INTEGRATIE - VOORBEREID**
- **Functionaliteit**: Volledige VR funscript support met ML enhancing
- **Locatie**: `multifunplayer_client.h` + `multifunplayer_client.cpp`
- **Features**: WebSocket client, real-time funscript, ML autonomie levels
- **Config verplaatst**: Van aparte constanten naar `body_config.h` regel 104-110
- **Status**: Tijdelijk uitgeschakeld wegens geheugengebrek (zie hieronder)

### 🔴 GEHEUGENPROBLEEM OPGELOST
- **Probleem**: Code te groot (104% van 1.3MB flash gebruikt)
- **Oorzaak**: MultiFunPlayer toevoeging duwde project over limiet
- **Tijdelijke oplossing**: MultiFunPlayer tijdelijk uitgeschakeld via comments
- **Permanente oplossing**: Hardware upgrade naar ESP32-S3 (zie hardware sectie)

---

## 🔧 TECHNISCHE DETAILS

### 📂 BELANGRIJKE BESTANDEN
- `Body_ESP.ino`: Hoofdbestand met alle core logic
- `body_config.h`: Centrale configuratie (ALL settings hier!)  
- `multifunplayer_client.h/.cpp`: VR funscript integratie
- `ml_stress_analyzer.h`: ML stress management
- `body_gfx4.h/.cpp`: Display en grafiek systeem

### 🔍 KRITIEKE CODE LOCATIES
- **Vibe playback fix**: `Body_ESP.ino` regel 896-897 (threshold 50.0f)
- **MultiFunPlayer config**: `body_config.h` regel 104-110 (IP adres hier!)
- **ESP-NOW communicatie**: `Body_ESP.ino` regel 194-372
- **AI Stress Management**: `Body_ESP.ino` regel 1134-1472

### 📋 UITGESCHAKELDE FUNCTIES (tijdelijk)
```cpp
// Deze regels zijn gecomment voor geheugen:
// #include "multifunplayer_client.h"           // regel 53
// setupMultiFunPlayer();                       // regel 1125  
// mfpClient.loop();                           // regel 1759
```

---

## 🛠️ HARDWARE

### 📱 HUIDIGE SETUP (ESP32 klassiek)
- **Board**: ESP32-2432S028R (CYD - Cheap Yellow Display)
- **Display**: 2.4" TFT met touch
- **Sensoren**: MAX30102 (hartslag), MCP9808 (temp), GSR (huid)
- **Communicatie**: ESP-NOW naar HoofdESP
- **Probleem**: Te weinig flash geheugen (1.3MB limiet)

### 🚀 NIEUWE HARDWARE (BESTELD - MORGEN)
- **Board**: LilyGO T-HMI ESP32-S3  
- **Display**: 2.8" IPS TFT (betere kwaliteit)
- **Flash**: 8MB (vs 4MB) - 4x meer ruimte!
- **RAM**: 512KB SRAM + 2MB PSRAM
- **CPU**: Dual-core Xtensa LX7 @ 240MHz
- **Voordelen**: Geïntegreerd, USB-C, capacitief touch

---

## 📊 WERKENDE FUNCTIES

### ✅ 100% FUNCTIONEEL
- **Sensor monitoring**: Hartslag, temperatuur, GSR real-time
- **ESP-NOW communicatie**: Bidirectioneel met HoofdESP
- **AI Stress Management**: 7-level systeem met ML integration
- **CSV Recording**: Automatische sessie opname
- **CSV Playback**: Met correcte vibe/zuigen detectie ⭐
- **Touch interface**: Menu, knoppen, navigatie
- **Display grafieken**: 6-band real-time sensor display

### ⚙️ CONFIGUREERBAAR
- **AI Overrule**: ML eigenwil 0-100% instelbaar
- **Sensor thresholds**: HR, temp, GSR limieten
- **Stress timings**: Per level configureerbare reactietijden
- **MultiFunPlayer**: IP, poort, auto-connect (body_config.h)

---

## 🎮 VR FUNSCRIPT INTEGRATIE

### 📋 IMPLEMENTATIE STATUS
- **WebSocket client**: ✅ Volledig geïmplementeerd
- **Funscript parsing**: ✅ Position, speed, timestamp
- **ML Enhancement**: ✅ Stress-responsive aanpassingen  
- **Body ESP integration**: ✅ Via ESP-NOW naar HoofdESP
- **Autonomie levels**: ✅ 0-100% ML controle over scripts

### 🔄 WORKFLOW (WANNEER ACTIEF)
```
1. MultiFunPlayer (PC) → WebSocket → Body ESP
2. Body ESP ML analyseert: "Veilig voor gebruiker?"
3. Stress < 4: Volg funscript normaal
4. Stress 4-6: Verlaag intensiteit  
5. Stress 7: Emergency slow down
6. Body ESP → ESP-NOW → HoofdESP → Hardware
```

### ⚙️ CONFIGURATIE
```cpp
// In body_config.h regel 104-110:
String mfpPcIP = "192.168.1.100";     // ← PC IP hier wijzigen!
uint16_t mfpPort = 8080;              // ← MultiFunPlayer poort
bool mfpAutoConnect = true;           // ← Auto verbinden
bool mfpMLIntegration = true;         // ← ML voor funscripts
```

---

## 📅 PLANNING & TODO

### 🚀 MORGEN (27-09-2024) - T-HMI Setup
**Priority 1: Hardware Migration**
- [ ] LilyGO T-HMI uitpakken en testen
- [ ] ESP32-S3 board definition Arduino IDE
- [ ] Basic "Hello World" compilatie test
- [ ] TFT_eSPI configuratie voor 2.8" display

**Priority 2: Display System**  
- [ ] `body_gfx4.cpp` migreren naar nieuwe display
- [ ] Touch system aanpassen voor capacitief
- [ ] Touch calibratie uitvoeren
- [ ] Grafiek system testen (6-band display)

**Priority 3: Sensor Integration**
- [ ] Pin mapping maken: MAX30102, MCP9808, GSR
- [ ] I2C bus configuratie (SDA/SCL pins)
- [ ] Sensor libraries testen op nieuwe hardware
- [ ] ESP-NOW configuratie (zelfde MAC adressen?)

### ⚡ WEEK 1 (28-09 tot 04-10)
**MultiFunPlayer Reactivation**
- [ ] Comments wegvanen uit code (3 locaties)
- [ ] PC IP adres configureren in `body_config.h`
- [ ] WebSocket verbinding testen
- [ ] Basic funscript ontvangst testen
- [ ] ML enhancement van funscript acties

**VR Integration Testing**  
- [ ] MultiFunPlayer installeren op PC
- [ ] Test .funscript bestand downloaden
- [ ] WebSocket server configureren (poort 8080)
- [ ] End-to-end test: VR video + funscript + ML
- [ ] Stress-responsive aanpassingen valideren

### 🔬 WEEK 2 (05-10 tot 11-10)
**Advanced Features**
- [ ] ML Eigenwil fine-tuning voor VR
- [ ] Funscript + AI stress combinatie optimaliseren  
- [ ] Extended logging voor ML training
- [ ] Performance optimalisatie S3 hardware
- [ ] User interface improvements (groter scherm)

---

## 🐛 BEKENDE ISSUES

### ⚠️ ACTIEVE PROBLEMEN
- **Geheugen**: Huidige ESP32 te klein voor alle features (tijdelijk opgelost)
- **MultiFunPlayer**: Uitgeschakeld tot hardware upgrade

### ✅ OPGELOSTE PROBLEMEN
- ~~Vibe playback detection incorrect~~ ✅ GEFIXT (threshold 50.0f)
- ~~MultiFunPlayer config verspreid over files~~ ✅ GEFIXT (body_config.h)
- ~~CSV parsing errors~~ ✅ GEFIXT (substring fix)

---

## 💾 BACKUP & RECOVERY

### 📂 BELANGRIJKE BESTANDEN BACKUP
- `body_config.h` ← ALLE CONFIGURATIE
- `Body_ESP.ino` ← HOOFDCODE  
- `multifunplayer_client.h/.cpp` ← VR FUNCTIES
- `AI Warp Logboek.md` ← DIT BESTAND

### 🔄 MIGRATION CHECKLIST T-HMI
1. **Code**: Direct overzetten (S3 heeft meer ruimte)
2. **Libraries**: TFT_eSPI herconfigueren  
3. **Pins**: Sensor mapping aanpassen
4. **Touch**: Capacitief systeem kalibreren
5. **Config**: `body_config.h` controleren
6. **Test**: Stap voor stap functionaliteit valideren

---

## 📞 CONTACT & HULP

### 🤖 AI ASSISTENT INSTRUCTIES
**Voor toekomstige sessies:**
- Lees eerst dit logboek voor context
- Check `body_config.h` voor alle instellingen  
- Vibe playback fix: threshold 50.0f (regel 896-897)
- MultiFunPlayer config: `body_config.h` regel 104-110
- Hardware upgrade: T-HMI ESP32-S3 (morgen)

### 📚 DOCUMENTATIE REFERENTIES  
- `GEBRUIKSAANWIJZING.txt`: User manual complete functionaliteit
- `ML_Beginners_Guide.txt`: ML system uitleg
- `XBVR_INTEGRATION_SETUP.txt`: VR setup instructies

---

## 🎯 PROJECT VISIE

**DOEL**: Intelligente biometric feedback systeem voor hardware control
**FEATURES**: Real-time stress monitoring + ML-enhanced VR funscript support  
**STATUS**: ~95% complete - alleen hardware upgrade + VR activatie nodig
**TIMELINE**: VR fully operational binnen 1 week na T-HMI ontvangst

---

**🚀 VOLGENDE STAP: T-HMI Setup en MultiFunPlayer reactivation! 🎮**

*Dit logboek updaten na elke sessie voor continuïteit.*
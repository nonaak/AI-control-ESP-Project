# 🧠 AI EDGING SYSTEEM - Wat Claude Begrijpt & Vragen

**Datum:** 11 november 2025  
**Doel:** Compleet begrip voor implementatie AI-gestuurde edging sessies

---

## ✅ WAT IK WEL BEGRIJP

### 🏗️ Systeem Architectuur

**Hooft_ESP (Master Controller):**
- [✓] Animatie rendering (sleeve visual)
- [✓] Nunchuk input voor manuele controle
- [✓] Berekent sleevePercentage (0-100%)
- [✓] **KEON BLE controle zit HIER** (niet in Body_ESP!)
- [✓] ESP-NOW: Stuurt sleeve% naar Body_ESP
- [✓] ESP-NOW: Ontvangt AI overrides van Body_ESP
- [✓] Heeft pause knop (C button)

**Body_ESP (AI Brain):**
- [✓] SC01 Plus touchscreen
- [✓] Biometrische sensoren: BPM, GSR, Temp, Ademhaling
- [✓] ESP-NOW: Ontvangt sleeve% van Hooft_ESP
- [✓] CSV recording voor training data
- [✓] AI model voor stress level voorspelling (0-7)
- [✓] Stuurt overrides naar Hooft_ESP (binnen vrijheid%)

**KEON Device:**
- [✓] BLE verbinding met Hooft_ESP
- [✓] Ontvangt positie (0-99) + speed commands

---

### 🎯 AI Stress Levels (0-7)

**Levels 0-6: AI EDGING MODE**
```
Level 0: Rustig, opbouwen
Level 1: Warming up  
Level 2: Moderate stimulatie
Level 3: Building intensity
Level 4: Pre-edge zone
Level 5: Edge territory (spannend!)
Level 6: Maximum edge (net niet over grens!)
```
- [✓] AI heeft controle (binnen vrijheid%)
- [✓] AI past speed/vibe/zuig aan
- [✓] Doel: Edge zo lang mogelijk
- [✓] User kan altijd override via Nunchuk

**Level 7: ORGASME RELEASE MODE** 🚀
```
- Speed: MAX (7)
- Vibe: AAN
- Zuig: AAN
- AI: LOCKED OUT (geen overrides meer!)
- User: VOLLEDIGE controle via Nunchuk
- Doel: Naar orgasme gaan
```
- [✓] AI detecteert wanneer tijd voor release
- [✓] AI stuurt 1x "ORGASME_RELEASE" command
- [✓] Hooft_ESP zet alles op MAX
- [✓] AI stopt met overrides
- [✓] User gaat voor orgasme

**Pause bij Level 7 = Session Complete:**
- [✓] User drukt C (pause) = "Klaar!"
- [✓] Body_ESP detecteert: stressLevel7 + pause
- [✓] Stuurt "SESSION_COMPLETE" naar Hooft_ESP
- [✓] Alles reset naar cooldown (speed 0, vibe uit)

---

### 📊 Training & AI Werking

**Training Mode (Manueel):**
- [✓] User bestuurt via Nunchuk (Hooft_ESP)
- [✓] Body_ESP observeert + record naar CSV
- [✓] CSV bevat: BPM, GSR, Temp, Ademhaling, Speed, Vibe, Zuig, SleevePos%
- [✓] AI leert patronen: "Bij BPM=140, GSR=80 → speed=6 werkt goed"

**AI Mode (met vrijheid%):**
- [✓] AI analyseert sensor data
- [✓] AI voorspelt optimal stress level (0-6)
- [✓] AI stuurt overrides binnen vrijheid% (bijv 30%)
- [✓] Example: User op speed 5, AI mag 3.5-6.5 kiezen
- [✓] Hooft_ESP accepteert alleen binnen grenzen

---

## ❓ WAT IK NIET BEGRIJP / VRAGEN

### 🎮 Keon Control Details

**1. Hoe werkt Keon sync precies?**
- [ ] Gebruikt Hooft_ESP sleevePercentage direct voor Keon positie?
  - Formule: `keonPos = sleevePercentage * 0.99` (0-100% → 0-99)?
- [ ] Of blijft het de oude methode (richting gebaseerd)?
  - VelEMA < 0 → pos 99, velEMA > 0 → pos 0
- [ ] Speed parameter voor Keon:
  - Altijd 99 (instant)? Of lineair op basis van speedStep?
- [ ] Sync rate: 10Hz (elke 100ms) correct?

**2. Keon tijdens AI override:**
- [ ] Als AI speed aanpast (bijv 5→6), sync dan Keon direct?
- [ ] Of wacht tot volgende 100ms interval?
- [ ] Moet Keon "smooth transition" doen tussen speeds?

**3. Keon bij Level 7 release:**
- [ ] Gewoon max speed (7) → Keon sync normaal?
- [ ] Of speciale "orgasme mode" voor Keon? (harder/faster?)

---

### 🧠 AI Decision Logic

**4. Stress Level Detection:**
Hoe bepaalt AI welke level? Heb je thresholds?

**Voorbeeld thresholds (vul in of corrigeer):**
```
Level 0: BPM < 80,  GSR < 30
Level 1: BPM < 90,  GSR < 40  
Level 2: BPM < 100, GSR < 50
Level 3: BPM < 110, GSR < 60
Level 4: BPM < 120, GSR < 70  (pre-edge)
Level 5: BPM < 135, GSR < 80  (edge!)
Level 6: BPM < 150, GSR < 90  (max edge!)
Level 7: TRIGGERED BY AI DECISION (niet sensor threshold)
```

- [ ] Zijn dit realistische waardes voor jou?
- [ ] Worden alle 4 sensors gebruikt (BPM, GSR, Temp, Ademhaling)?
- [ ] Of vooral BPM + GSR?
- [ ] Hoe wordt Temperatuur gebruikt? (hogere temp = hogere arousal?)
- [ ] Hoe wordt Ademhaling gebruikt? (sneller = hoger level?)

**5. Release Trigger (naar Level 7):**
Wanneer beslist AI om naar Level 7 te gaan?

**Mogelijke logica (vul in wat klopt):**
- [ ] Na X aantal edges (bijv 3 edges)?
- [ ] Na X minuten in Level 6 (bijv 2 minuten)?
- [ ] Op basis van sensor pattern? (BPM piek + dip?)
- [ ] Tijd van sessie? (na 20 min altijd release?)
- [ ] Combinatie van bovenstaande?

**Jouw gewenste logica:**
```
(vul hier in hoe AI moet beslissen wanneer Level 7)




```

**6. Edge Detection:**
Wat is een "edge" precies in de code?

- [ ] Wanneer Level 5 of 6 bereikt wordt?
- [ ] Minimale tijd in Level 5/6 om als "edge" te tellen? (bijv 20 sec?)
- [ ] Reset edge counter als terug naar Level 0-3?

---

### 🎛️ AI Vrijheid & Overrides

**7. Vrijheid Percentage:**
- [ ] Is 30% vrijheid een goed default?
- [ ] Moet dit instelbaar zijn via menu?
  - Bijv: 10% (weinig AI), 50% (veel AI), 100% (volledige AI)?
- [ ] Is vrijheid hetzelfde voor alle parameters?
  - Speed vrijheid: ___%
  - Vibe vrijheid: ___% (of alleen aan/uit?)
  - Zuig vrijheid: ___% (of alleen aan/uit?)

**8. Welke parameters kan AI aanpassen?**

Vink aan wat AI MAG overriden:
- [ ] Speed (0-6, NIET 7)
- [ ] Vibe (aan/uit timing)
- [ ] Zuig (aan/uit timing)
- [ ] SleevePercentage? (of blijft dit Hooft_ESP?)
- [ ] Andere?

**9. AI Override Timing:**
- [ ] Hoe vaak stuurt AI overrides? Elke 1 sec? 5 sec?
- [ ] Alleen bij level change? Of continu updates?
- [ ] Mag AI mid-stroke speed veranderen? Of wachten op cycle?

---

### 🛑 Pause & Safety

**10. Pause Functionaliteit:**

**In normale mode (Level 0-6):**
- [ ] Pause = tijdelijk stop (zoals nu)?
- [ ] Keon naar bottom?
- [ ] AI blijft actief (observeren)?
- [ ] Of AI uit tijdens pause?

**In Level 7 mode:**
- [✓] Pause = "Klaar, orgasme gehad"
- [✓] Triggert SESSION_COMPLETE
- [✓] Reset naar cooldown

**11. Emergency Override:**
- [ ] Kan user ALTIJD AI uitschakelen? (bijv lange C druk?)
- [ ] Of alleen via Body_ESP menu?
- [ ] Wat gebeurt er bij emergency stop?
  - Stop alles direct?
  - Smooth slowdown?

**12. Safety Limits:**
Zijn er harde limieten waar AI NOOIT overheen mag?

- [ ] Max BPM: ___ (bijv 160?)
- [ ] Max time in Level 6: ___ minuten?
- [ ] Max session duration: ___ minuten?
- [ ] Bij overschrijding: Auto pause? Auto cooldown?

---

### 📡 ESP-NOW Communication

**13. Message Structuur:**

**Van Body_ESP naar Hooft_ESP (AI overrides):**
```cpp
typedef struct {
  float newSpeed;         // AI suggested speed (0-6, float voor smooth)
  bool vibeOn;            // AI vibe on/off
  bool zuigOn;            // AI zuig on/off  
  bool overruleActive;    // AI override actief?
  uint8_t stressLevel;    // Current level (0-7)
  char command[32];       // "AI_OVERRIDE", "ORGASME_RELEASE", "SESSION_COMPLETE"
} esp_now_ai_message_t;
```

- [ ] Is deze struct compleet?
- [ ] Missen er parameters?
- [ ] Te veel overhead? (keep it minimal?)

**14. ESP-NOW Frequency:**
- [ ] Hoe vaak stuurt Body_ESP updates?
- [ ] Alleen bij verandering? (event-driven)
- [ ] Of vaste interval? (bijv 1 Hz)
- [ ] Heartbeat nog nodig als AI actief is?

---

### 📊 Training Data & ML Model

**15. CSV Recording:**

**Huidige CSV format:**
```
Tijd_s, Timestamp, BPM, Temp_C, GSR, Trust, Sleeve, Suction, 
Vibe, Zuig, Vacuum_mbar, Pause, SleevePos_%, SpeedStep, AI_Override
```

- [ ] Is deze format goed voor training?
- [ ] Missen er kolommen?
  - Bijv: Current_StressLevel, Edge_Count, Session_Time?
- [ ] Sample rate: 1 Hz (elke seconde) OK?
- [ ] Of hogere rate nodig voor ML? (5 Hz?)

**16. ML Model Type:**
Welk type model wil je gebruiken?

- [ ] Decision Tree (snel, interpretable)
- [ ] Random Forest (robuuster)
- [ ] Neural Network (meest flexibel)
- [ ] Rule-based system (handmatige thresholds)
- [ ] Anders: _______________

**17. Training Approach:**
- [ ] Hoeveel training sessies nodig? (10? 50? 100?)
- [ ] Train on-device (ESP32)? Of offline (PC)?
- [ ] Model update frequency? (na elke sessie? weekly?)
- [ ] Personalized model (alleen jouw data)? Of general?

---

### 🖥️ User Interface

**18. Body_ESP Display:**

**Tijdens AI Mode, wat moet zichtbaar zijn?**
- [ ] Current stress level (0-7) - groot nummer?
- [ ] Sensor grafieken (BPM, GSR, etc)?
- [ ] AI freedom %?
- [ ] Edge count (hoeveel edges tot nu toe)?
- [ ] Time in current level?
- [ ] Anders: _______________

**19. Level 7 Visuele Feedback:**
Wat moet scherm tonen bij ORGASME RELEASE?

- [ ] Groot "LEVEL 7" tekst?
- [ ] Flashing indicator?
- [ ] "AI LOCKED OUT" melding?
- [ ] Countdown? Timer?
- [ ] Minimaal (niet afleiden)?
- [ ] Anders: _______________

**20. Hooft_ESP Display:**
- [ ] Moet Hooft_ESP ook stress level tonen?
- [ ] Of alleen Body_ESP?
- [ ] Indicator dat AI actief is? (LED? Icon?)

---

### 🎚️ Menu Settings

**21. Instelbare Parameters:**

Welke settings moet gebruiker kunnen aanpassen?

**Body_ESP Menu:**
- [ ] AI Mode: Aan/Uit
- [ ] AI Vrijheid %: 0-100%
- [ ] Stress thresholds (advanced)?
- [ ] Edge count target (bijv 3 edges)?
- [ ] Session duration?
- [ ] ML Model selectie?
- [ ] Sensor calibratie?
- [ ] Anders: _______________

**Hooft_ESP Menu:**
- [ ] Accept AI overrides: Ja/Nee
- [ ] AI override limits?
- [ ] Keon sync mode (nieuwe methode/oude)?
- [ ] Anders: _______________

---

### ⚙️ Technical Implementation

**22. Code Structuur:**

**Body_ESP nieuwe files nodig:**
- [ ] `ai_edging.cpp/h` - Main AI logic
- [ ] `stress_detector.cpp/h` - Sensor → Level mapping
- [ ] `ml_edging_model.cpp/h` - ML model implementation
- [ ] Anders: _______________

**Hooft_ESP nieuwe files nodig:**
- [ ] `ai_override.cpp/h` - AI message handling
- [ ] Aanpassingen in `ui.cpp` - AI mode integration
- [ ] Anders: _______________

**23. Testing Approach:**
Hoe wil je testen tijdens development?

- [ ] Dummy sensor data (gecodeerde waardes)?
- [ ] Live testing (met echte sensoren)?
- [ ] Simulator mode (fake BPM/GSR)?
- [ ] Safe mode (lagere limits eerst)?

---

### 🔄 Edge Cases & Error Handling

**24. Wat als...**

**Scenario 1: ESP-NOW disconnects tijdens Level 7**
- [ ] Hooft_ESP blijft in Level 7? (user heeft controle)
- [ ] Auto fallback naar manual mode?
- [ ] Visual warning?
- [ ] Actie: _______________

**Scenario 2: Sensor fail (BPM = 0) tijdens AI mode**
- [ ] Auto pause?
- [ ] Fallback naar manual?
- [ ] Blijf doorgan met laatste known level?
- [ ] Actie: _______________

**Scenario 3: User override via Nunchuk tijdens AI**
- [ ] AI accepteert nieuwe user input als baseline?
- [ ] AI blijft suggereren binnen nieuwe baseline?
- [ ] Actie: _______________

**Scenario 4: Level 7 triggered maar user niet klaar**
- [ ] Kan user terug naar Level 6? (hoe?)
- [ ] Of altijd doorgaan tot orgasme?
- [ ] Actie: _______________

**Scenario 5: Accidental pause tijdens Level 0-6**
- [ ] Resume naar zelfde level?
- [ ] Reset naar Level 0?
- [ ] Actie: _______________

---

## 🚀 IMPLEMENTATION PRIORITEIT

**Vul in welke volgorde je wilt implementeren:**

**Phase 1 (Basis):**
1. _______________
2. _______________
3. _______________

**Phase 2 (AI Core):**
1. _______________
2. _______________
3. _______________

**Phase 3 (Advanced):**
1. _______________
2. _______________
3. _______________

---

## 📝 OPMERKINGEN & EXTRA VRAGEN

**Vul hier alles in wat ik nog NIET gevraagd heb maar WEL belangrijk is:**

```
(vrije ruimte voor jouw input)












```

---

## ✅ CHECKLIST VOOR START IMPLEMENTATIE

Voordat we beginnen coderen, deze dingen moeten duidelijk zijn:

- [ ] Keon sync methode gekozen
- [ ] Stress level thresholds gedefinieerd
- [ ] Release trigger logica bepaald
- [ ] AI vrijheid % gekozen
- [ ] ESP-NOW message struct compleet
- [ ] CSV format definitief
- [ ] ML model type gekozen
- [ ] UI design duidelijk
- [ ] Safety limits bepaald
- [ ] Error handling scenarios covered

**Als alles ✅ is → START IMPLEMENTATION! 🚀**

---

**Document versie:** 1.0  
**Laatst bijgewerkt:** 2025-11-11  
**Claude Context:** Dit document gebruiken bij nieuwe gesprekken om direct verder te kunnen!

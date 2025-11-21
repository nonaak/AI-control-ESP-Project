# 🎯 KEON LEVELS - QUICK REFERENCE

## 📊 SNELHEID TABEL:

```
╔═══════╦══════════╦═══════════╦══════════════════╗
║ LEVEL ║ INTERVAL ║  STROKES  ║   BESCHRIJVING   ║
║       ║   (ms)   ║   /min    ║                  ║
╠═══════╬══════════╬═══════════╬══════════════════╣
║   0   ║  1000ms  ║    60     ║ ZEER LANGZAAM    ║
║   1   ║   887ms  ║    67     ║ Langzaam         ║
║   2   ║   775ms  ║    77     ║ Relaxed          ║
║   3   ║   662ms  ║    90     ║ Medium           ║
║   4   ║   550ms  ║   109     ║ Medium-Fast      ║
║   5   ║   437ms  ║   137     ║ Fast             ║
║   6   ║   325ms  ║   184     ║ Very Fast        ║
║   7   ║   200ms  ║   300     ║ MAXIMUM SPEED    ║
╚═══════╩══════════╩═══════════╩══════════════════╝
```

---

## 🎮 NUNCHUK CONTROL:

```
Joystick Y-as:
  ▲ OMHOOG  → Sneller (level++)
  │
  │ MIDDEN  → Behoud level
  │
  ▼ OMLAAG → Langzamer (level--)

C Knop:
  - Kort = Pause/Unpause toggle
  
Z Knop:
  - 1x = Zuigen aan/uit
  - 2x = Vibe aan/uit
```

---

## 🤖 BODY ESP AI CONTROL:

```
Stress Level → Speed Level:
  Stress 1 → Level 0 (langzaam)
  Stress 2 → Level 1
  Stress 3 → Level 2
  Stress 4 → Level 3
  Stress 5 → Level 4
  Stress 6 → Level 5
  Stress 7 → Level 6 (snel)
```

**NOTE:** AI respecteert altijd Nunchuk input!

---

## 🔧 DEBUG OUTPUT:

```
[KEON INDEPENDENT] Level:3 Pos:99->0 Interval:662ms (90 strokes/min)
                    ▲      ▲       ▲          ▲
                    │      │       │          │
              speedStep  direction timing  strokes/min
```

---

## ✅ VERWACHT GEDRAG:

### **Level 0 (Langzaam):**
- Interval: 1000ms
- Stroke: 1 seconde van 0→99, 1 seconde van 99→0
- Totaal: 2 seconden per cyclus = 30 cycli/min = 60 strokes/min

### **Level 7 (Snel):**
- Interval: 200ms
- Stroke: 0.2 seconde van 0→99, 0.2 seconde van 99→0
- Totaal: 0.4 seconden per cyclus = 150 cycli/min = 300 strokes/min

---

## 🚨 TROUBLESHOOTING QUICK CHECK:

```
Geen beweging?
  → Check: keonConnected = true?
  → Check: paused = false?
  → Check: g_speedStep binnen 0-7?

Te langzaam/snel?
  → Check: Serial output interval
  → Check: g_speedStep waarde
  → Formule: 1000 - ((speedStep * 800) / 7)

Ruis/sprongen?
  → Check: Geen oude keonSyncToAnimation()?
  → Check: Alleen keonIndependentTick()?

Body ESP werkt niet?
  → Check: syncKeonToStressLevel() bestaat?
  → Check: ESP-NOW connected?
  → Check: Stress level 1-7?
```

---

## 📏 FORMULE REFERENCE:

```cpp
// Timing berekening:
uint32_t interval = 1000 - ((speedStep * 800) / 7);

// Voorbeelden:
speedStep = 0 → 1000 - ((0 * 800) / 7) = 1000ms
speedStep = 3 → 1000 - ((3 * 800) / 7) = 662ms
speedStep = 7 → 1000 - ((7 * 800) / 7) = 200ms

// Strokes per minuut:
strokes/min = 60000 / (interval * 2)
```

---

## 🎯 TEST SCENARIO'S:

### **Scenario 1: Manual Control**
```
1. Start op Level 0
2. Elke 5 seconden +1 level
3. Observeer snelheid toename
4. Bij Level 7: Controleer max speed
5. Terug naar Level 0
```

### **Scenario 2: AI Override**
```
1. Body ESP stuurt Level 5
2. Observeer Keon op Level 5
3. Jij zet Nunchuk op Level 2
4. Observeer Keon gaat naar Level 2
   → Nunchuk wint!
```

### **Scenario 3: Pause Test**
```
1. Keon op Level 4
2. Druk C (pause)
   → Keon stopt
3. Wacht 10 seconden
4. Druk C (unpause)
   → Keon hervat op Level 4
```

---

**PRINT DIT EN HOUD BIJ DE HAND TIJDENS TESTEN!** 📋

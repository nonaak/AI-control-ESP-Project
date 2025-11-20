# KEON OSCILLATIE - TEST BEVINDINGEN & 8 LEVELS

## 🔍 WAT WE ONTDEKT HEBBEN

### **De Breakthrough: Positie = Oscillatie Frequentie**

**Niet wat we dachten:**
- ❌ `move(position, speed)` → "Ga naar positie X met snelheid Y"
- ❌ Wisselen tussen 0 en 99 voor volle slagen
- ❌ Speed parameter bepaalt snelheid

**Wat het ECHT doet:**
- ✅ `move(position, speed)` → "Oscilleer rond positie X"
- ✅ Keon maakt ZELF automatisch slagen
- ✅ **Positie bepaalt oscillatie frequentie!**

---

## 📊 TEST RESULTATEN (30 seconden per positie)

### **Test Setup:**
```cpp
move(0, 99);    // 30 seconden vasthouden
→ 16 strokes geteld

move(99, 99);   // 30 seconden vasthouden  
→ 60 strokes geteld

move(50, 99);   // 30 seconden vasthouden
→ 40 strokes geteld
```

### **Berekening naar strokes/minuut:**

```
Pos 0:  16 strokes/30sec × 2 = 32 strokes/min  (LANGZAAM)
Pos 99: 60 strokes/30sec × 2 = 120 strokes/min (SNEL)
Pos 50: 40 strokes/30sec × 2 = 80 strokes/min  (MIDDEL)
```

**CONCLUSIE:**
- **Lagere positie = Langzamere oscillatie**
- **Hogere positie = Snellere oscillatie**
- **Speed parameter (99) = Maximale kracht/agressiviteit**

---

## 🎯 8 LEVELS SYSTEEM

Op basis van deze bevindingen hebben we 8 levels gemaakt:

```
Level  Positie  Frequentie (geschat)     Gebruik
═══════════════════════════════════════════════════════════
L0     0        ~32 strokes/min          Zeer relaxed
L1     15       ~45 strokes/min          Relaxed
L2     30       ~60 strokes/min          Light activity
L3     45       ~75 strokes/min          Normale rust
L4     60       ~85 strokes/min          Lichte stress
L5     75       ~95 strokes/min          Middelmatige stress
L6     85       ~110 strokes/min         Hoge stress
L7     99       ~120 strokes/min         Zeer hoge stress
```

**Mapping:**
- Body ESP meet stress (0-7)
- Hoofd ESP stuurt `L0` t/m `L7` via UART
- ESP32-C3 vertaalt naar positie 0-99
- Keon oscilleert op die frequentie

---

## 🧪 HOE HET WERKT

### **Oud concept (FOUT):**
```cpp
// Dit dachten we:
move(0, 99);     // Ga naar beneden
delay(2000);     // Wacht
move(99, 99);    // Ga naar boven
delay(2000);     // Wacht
// Herhaal voor stroke pattern
```

**Probleem:** Complex timing, delays nodig, onbetrouwbaar

### **Nieuw concept (CORRECT):**
```cpp
// Wat we nu doen:
move(45, 99);    // Set positie 45 (level 3)
// KLAAR! Keon doet de rest automatisch!
```

**Voordelen:**
- ✅ Simpele code
- ✅ Geen timing issues
- ✅ Keon regelt zelf het ritme
- ✅ Direct level veranderen zonder stoppen

---

## 🔧 IMPLEMENTATIE IN UART BRIDGE

### **Level Array:**
```cpp
const uint8_t levelPositions[8] = {
  0,   // L0: ~32 strokes/min
  15,  // L1
  30,  // L2
  45,  // L3
  60,  // L4
  75,  // L5
  85,  // L6
  99   // L7: ~120 strokes/min
};
```

### **Set Level:**
```cpp
void setLevel(uint8_t level) {
  if (level > 7) level = 7;
  currentLevel = level;
  
  // Als actief, update direct
  if (keonActive && keonConnected) {
    keonMove(levelPositions[currentLevel], 99);
  }
}
```

### **Start:**
```cpp
void startKeon() {
  keonActive = true;
  keonMove(levelPositions[currentLevel], 99);
  // Keon oscilleert nu automatisch!
}
```

### **Stop:**
```cpp
void stopKeon() {
  keonActive = false;
  
  // Meerdere stop commando's voor zekerheid
  keonMove(50, 0);   // Midden, speed 0
  delay(300);
  keonMove(50, 0);   // Herhaal
  delay(300);
  keonMove(0, 0);    // Naar 0, speed 0
}
```

---

## 📈 WAAROM DIT WERKT

### **Probleem met oude aanpak:**
```
Hoofd-ESP → Timing code voor slagen
          → Delays tussen up/down
          → Complex pauseren/hervatten
          → Animatie sync problemen
```

### **Oplossing met oscillatie:**
```
Hoofd-ESP → Simpel: "L3" via UART
ESP32-C3  → Set positie 45
Keon      → Oscilleert automatisch
          → Geen timing code nodig!
```

---

## 🎮 GEBRUIK IN PRAKTIJK

### **Voorbeeld 1: Stress mapping**
```cpp
// Body ESP meet stress
int stress = calculateStress();  // 0-7

// Hoofd ESP stuurt level
Serial2.print("L");
Serial2.println(stress);
Serial2.println("START");

// Keon oscilleert nu op juiste frequentie!
```

### **Voorbeeld 2: Level veranderen tijdens gebruik**
```cpp
// Als stress verandert, gewoon nieuw level sturen:
Serial2.println("L5");  // Keon past direct aan!

// Geen stop/start nodig, vloeiende overgang!
```

### **Voorbeeld 3: Geleidelijk opbouwen**
```cpp
// Start langzaam
Serial2.println("L0");
Serial2.println("START");
delay(30000);  // 30 sec

// Verhoog geleidelijk
for (int i = 1; i <= 7; i++) {
  Serial2.print("L");
  Serial2.println(i);
  delay(60000);  // 1 minuut per level
}

// Stop
Serial2.println("STOP");
```

---

## 🔬 TECHNISCHE DETAILS

### **BLE Protocol:**
```
Commando: [0x04][0x00][POSITION][0x00][SPEED]

Voorbeelden:
[0x04][0x00][0x00][0x00][0x63]  → Pos 0, speed 99
[0x04][0x00][0x63][0x00][0x63]  → Pos 99, speed 99
[0x04][0x00][0x2D][0x00][0x63]  → Pos 45, speed 99
[0x04][0x00][0x00][0x00][0x00]  → Stop
```

### **Speed Parameter:**
- Waarde: 0-99 (0x00-0x63)
- **NIET snelheid, maar kracht/intensiteit!**
- 99 = Maximale kracht → Beste oscillatie
- 0 = Stop

### **Position Parameter:**
- Waarde: 0-99 (0x00-0x63)
- **Oscillatie frequentie!**
- Lagere waarde = Langzamer
- Hogere waarde = Sneller

### **Delay tussen commando's:**
```cpp
keonTxChar->writeValue(cmd, 5, true);
delay(200);  // BELANGRIJK! Anders missed commando's
```

---

## ⚠️ BELANGRIJKE ONTDEKKINGEN

### **1. Stop werkt niet direct:**
```cpp
// FOUT:
keonMove(0, 0);  // Keon blijft nog 2-3 sec bewegen

// GOED:
keonMove(50, 0);
delay(300);
keonMove(50, 0);  // Herhaal
delay(300);
keonMove(0, 0);   // Nu stopt hij echt
```

### **2. Geen wisselen nodig:**
```cpp
// FOUT (oud):
while (active) {
  move(0, 99);
  delay(interval);
  move(99, 99);
  delay(interval);
}

// GOED (nieuw):
move(45, 99);  // Set en vergeet!
// Keon oscilleert automatisch
```

### **3. Level changes zijn vloeiend:**
```cpp
// Geen stop nodig tussen levels:
move(30, 99);  // L2
delay(5000);
move(60, 99);  // L4 - Keon past direct aan!
```

---

## 📊 TEST VERIFICATIE

Om zelf te testen:

```cpp
// Test code om frequenties te meten:
move(0, 99);
delay(30000);   // Tel hoeveel strokes in 30 sec
// × 2 voor strokes/min

move(99, 99);
delay(30000);   // Tel hoeveel strokes in 30 sec
// × 2 voor strokes/min
```

**Verwachte resultaten:**
- Pos 0: ~32 strokes/min
- Pos 25: ~55 strokes/min
- Pos 50: ~80 strokes/min
- Pos 75: ~105 strokes/min
- Pos 99: ~120 strokes/min

---

## 🎯 CONCLUSIE

**OUDE MANIER (Complex):**
- Code moet slagen maken
- Timing met delays
- Start/stop tussen elke slag
- Moeilijk te pauzeren
- Sync problemen met animaties

**NIEUWE MANIER (Simpel):**
- Keon maakt zelf slagen
- Geen timing code nodig
- Set level en klaar
- Direct aanpasbaar
- Perfect sync mogelijk

**RESULTAAT:**
- 8 levels van langzaam naar snel
- Eenvoudige UART interface
- Betrouwbaar en voorspelbaar
- Ideaal voor stress-based control

---

## 🚀 VOLGENDE STAPPEN

1. ✅ ESP32-C3 UART bridge getest
2. ✅ 8 levels werkend
3. ✅ Oscillatie begrepen
4. 🔄 Integreren met hoofd-ESP
5. 🔄 Body ESP stress → Keon level mapping
6. 🔄 Live testen met biometrics

**Het systeem is klaar voor integratie!** 💪

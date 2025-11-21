# 🔧 UI.CPP WIJZIGING - KEON ONAFHANKELIJK MAKEN

## ✅ WAT TE DOEN:

In `ui.cpp`, zoek naar deze sectie (rond regel 1100-1200):

```cpp
void uiTick() {
  // ... veel code ...
  
  // ================= ANIMATIE EERST =================
  // We need to calculate animation first to get updated phase for auto vacuum
  updateSpeedStepWithJoystick(jy);
  drawSpeedBarTop(g_speedStep, CFG.SPEED_STEPS);
  
  // ... animatie berekeningen ...
  
  // Ergens hierna staat waarschijnlijk:
  // keonSyncToAnimation() OF syncKeonToAnimation()
}
```

---

## 🔍 ZOEK DEZE REGEL(S):

Zoek in `uiTick()` functie naar één van deze aanroepen:

```cpp
keonSyncToAnimation(g_speedStep, CFG.SPEED_STEPS, isMovingUp);
```

OF:

```cpp
syncKeonToAnimation();  // (in espnow_comm.cpp)
```

---

## ✏️ VERVANG MET:

```cpp
  // ===============================================================================
  // 🚀 KEON ONAFHANKELIJKE CONTROL - 100% LOS VAN ANIMATIE!
  // ===============================================================================
  
  // Roep Keon's EIGEN tick functie aan (NIET meer gekoppeld aan animatie!)
  extern void keonIndependentTick();  // Declared in keon.h
  keonIndependentTick();  // Keon draait nu op eigen tempo!
  
  // NOTE: Body ESP kan nog steeds Keon sturen via syncKeonToStressLevel()
  // in espnow_comm.cpp - die blijft werken voor AI control!
  
  // ===============================================================================
```

---

## ⚠️ BELANGRIJK:

**VERWIJDER NIET:**
- De `extern void keonIndependentTick();` declaratie is nodig!
- Dit vertelt de compiler dat de functie in keon.cpp zit

**BEHOUD:**
- Alle andere code in `uiTick()`
- De animatie berekeningen
- De auto vacuum logic
- Alles blijft zoals het was!

**ENIGE WIJZIGING:**
- De manier waarop Keon wordt aangeroepen
- Van: gekoppeld aan animatie direction
- Naar: eigen onafhankelijke timing

---

## 📋 VOLLEDIGE CONTEXT:

De nieuwe aanroep moet ergens BINNEN de `uiTick()` functie, 
bij voorkeur NA de animatie berekeningen maar dat is niet kritiek.

Zolang `keonIndependentTick()` maar één keer per frame wordt aangeroepen!

---

## ✅ VERIFICATIE:

Na de wijziging zou je moeten zien:

```cpp
void uiTick() {
  // ... animatie code ...
  
  // Keon control (NIEUW!)
  extern void keonIndependentTick();
  keonIndependentTick();
  
  // ... rest van de code ...
}
```

---

## 🔧 ALS JE MEERDERE AANROEPEN VINDT:

Als je MEERDERE plekken vindt waar Keon wordt aangeroepen:
1. Verwijder ALLE oude `keonSyncToAnimation()` aanroepen
2. Voeg ÉÉN keer `keonIndependentTick()` toe
3. Bij voorkeur NA de speedStep update maar VOOR het einde van uiTick()

---

## 🚀 RESULTAAT:

Na deze wijziging:
- ✅ Keon draait op eigen tempo (0-7 levels)
- ✅ Geen ruis meer van animatie!
- ✅ Level 0 = langzaam, Level 7 = snel
- ✅ Body ESP AI control blijft werken
- ✅ Nunchuk heeft nog steeds prioriteit

---

**KLAAR!** Upload de aangepaste ui.cpp en test! 🎯

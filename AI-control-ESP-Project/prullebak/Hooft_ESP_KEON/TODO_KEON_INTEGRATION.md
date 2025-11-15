# 📋 TODO - KEON INTEGRATIE PROJECT

**Laatst bijgewerkt:** 15 November 2025  
**Status:** In progress - Sync jitter probleem

---

## ✅ VOLTOOID (DONE)

### **Phase 1: Basic Setup**
- [x] Keon BLE protocol reverse-engineered (0x04 move command)
- [x] Werkende keon_ble.cpp van GitHub geïntegreerd
- [x] Blocking connect geïmplementeerd (acceptabel met ESP-NOW)
- [x] Connection menu item toegevoegd (Menu item 0)
- [x] Connection indicator (groene dot) toegevoegd

### **Phase 2: Sync Foundation**
- [x] Simple 1:1 position mapping (0-99 full range)
- [x] Speed parameter = 99 (instant response)
- [x] Tempo control via animatie frequency
- [x] Rate limiting (10Hz updates)

### **Phase 3: Compatibility**
- [x] ESP-NOW blijft werken (M5Atom TX succesvol)
- [x] Animatie blijft smooth (geen freeze)
- [x] Auto-reconnect disabled (prevent ESP freeze)
- [x] Compiler errors gefixed (missing exports, multiple definitions)

### **Phase 4: Visual Indicators**
- [x] Vibe lightning symbols terug toegevoegd (⚡ onderaan)
- [x] Suction symbols terug toegevoegd ()( bovenaan)
- [x] Vacuum arrow header werkt (pijl gevuld/outline)

---

## 🔴 ACTIEF (IN PROGRESS)

### **Phase 5: Sync Quality**
**Probleem:** Keon schokt tijdens sync met animatie

**Status:** Versie 2 (simple sync) was BESTE, maar schokte  
**Status:** Versie 3 (debug) was SLECHTER

**Diagnose:**
- [ ] Update rate te hoog? (10Hz = 100ms)
- [ ] Threshold te laag? (±2 position)
- [ ] Geen smoothing filter?
- [ ] BLE command queue overflow?

**Volgende stap:**
- [ ] User test huidige versie en geeft details over schokken
- [ ] Implementeer Plan A: Anti-jitter versie
  - [ ] Verlaag update rate naar 5Hz (200ms)
  - [ ] Verhoog threshold naar ±5
  - [ ] Voeg exponential smoothing toe
  - [ ] Test en evalueer

---

## 📝 TODO (PLANNED)

### **Phase 6: Sync Refinement**
**Als Plan A niet genoeg helpt:**

#### **Plan B: Predictive Sync**
- [ ] Bereken sleeve velocity
- [ ] Voorspel toekomstige positie
- [ ] Stuur alleen correctie commands
- [ ] Test en evalueer

#### **Plan C: Velocity Matching**
- [ ] Direct velocity control ipv position updates
- [ ] Match Keon velocity met sleeve velocity
- [ ] Test en evalueer

### **Phase 7: Edge Cases**
- [ ] Test lange sessies (30+ minuten)
- [ ] Test snelle speed changes (0→7→0)
- [ ] Test pause/unpause cycling
- [ ] Test disconnect/reconnect scenario
- [ ] Test met ESP-NOW heavy load

### **Phase 8: Polish**
- [ ] Verwijder debug output (of maak toggle-able)
- [ ] Optimize BLE command efficiency
- [ ] Memory usage check (Huge APP partition)
- [ ] Code cleanup en comments
- [ ] Final testing alle features samen

### **Phase 9: Documentation**
- [ ] User manual voor Keon setup
- [ ] Troubleshooting guide
- [ ] MAC address change guide
- [ ] Video demo maken?

---

## 🐛 KNOWN ISSUES

### **Critical**
1. **Keon schokt tijdens sync** (ACTIEF)
   - Severity: High
   - Impact: User experience
   - Status: Investigating

### **Minor**
1. **Auto-reconnect disabled**
   - Severity: Low
   - Impact: User moet handmatig reconnecten
   - Status: By design (voorkomt ESP freeze)

---

## 💡 IDEAS / FUTURE ENHANCEMENTS

### **Nice to Have**
- [ ] Keon stroke range configureerbaar maken via menu
- [ ] Keon speed multiplier (0.5x, 1x, 2x)
- [ ] Keon offset adjustment (±10 position shift)
- [ ] Multiple device support (Keon + Solace samen?)
- [ ] Patterns library (custom stroke patterns)
- [ ] OTA update support
- [ ] Web interface voor configuratie

### **Advanced**
- [ ] AI/ML based sync optimization
- [ ] Haptic feedback patterns
- [ ] Sync with biometric data (Body_ESP)
- [ ] VR headset integration
- [ ] Cloud sync voor patterns

---

## 🔧 TESTING CHECKLIST

### **Basic Functionality**
- [ ] Keon connects via menu
- [ ] Connection indicator shows status
- [ ] Disconnect works cleanly
- [ ] Reconnect works after disconnect

### **Sync Quality**
- [ ] Paused: Keon stil at bottom
- [ ] Running slow: Long strokes, slow tempo
- [ ] Running fast: Long strokes, fast tempo
- [ ] Speed change: Smooth transition
- [ ] Pause/unpause: Clean stop/start

### **Stability**
- [ ] No ESP crashes
- [ ] No ESP-NOW interference
- [ ] No animation lag
- [ ] Memory stable (no leaks)
- [ ] 30+ minute run test

### **Integration**
- [ ] Vibe works with Keon active
- [ ] Suction works with Keon active
- [ ] Vacuum works with Keon active
- [ ] ESP-NOW works with Keon active
- [ ] All menu items accessible

---

## 📊 VERSION HISTORY

### **v0.1 - Complex State Machine** ❌
- Datum: ~Nov 10
- Status: Failed
- Issue: Non-blocking connect didn't work with ESP-NOW

### **v0.2 - Simple Blocking** ✅
- Datum: ~Nov 11
- Status: Works
- Improvement: Blocking connect acceptable

### **v0.3 - Stroke Range Mapping** ❌
- Datum: Nov 14
- Status: Wrong approach
- Issue: Varied stroke length instead of tempo

### **v0.4 - Simple 1:1 Sync** ⚠️
- Datum: Nov 15 (morning)
- Status: Best so far, but jitters
- Issue: Keon schokt
- Config: 10Hz, threshold ±2

### **v0.5 - Debug + Paused Check** ❌
- Datum: Nov 15 (afternoon)
- Status: Worse than v0.4
- Issue: Made jitter worse

### **v0.6 - Anti-Jitter (PLANNED)** 🔄
- Datum: Nov 15 (evening)
- Status: To be implemented
- Plan: 5Hz, threshold ±5, smoothing

---

## 🎯 CURRENT PRIORITY

**#1 FIX JITTER**
- Status: BLOCKING
- Impact: High (user experience)
- Next step: Get detailed jitter description from user
- Then: Implement Plan A (anti-jitter)

**#2 TEST STABILITY**
- Status: Waiting for jitter fix
- Impact: Medium
- Next step: Long session tests

**#3 POLISH & DOCUMENT**
- Status: Waiting for stability
- Impact: Low
- Next step: Code cleanup

---

## 📞 WAITING FOR USER INPUT

**Questions for Aak:**
1. Welke versie draait nu? (v0.4 of v0.5?)
2. Hoe schokt Keon precies?
   - Kleine trillingen constant?
   - Grote sprongen af en toe?
   - Heen-en-weer wiebelen?
   - Bij welke speeds erger/beter?
3. Serial Monitor output beschikbaar?
4. Zal ik Plan A maken (anti-jitter)?

---

## 🚀 NEXT ACTIONS

### **Immediate (Today)**
1. Wacht op user feedback over jitter
2. Analyseer jitter pattern
3. Implementeer Plan A (anti-jitter)
4. User test nieuwe versie

### **Short Term (This Week)**
1. Finalize sync quality
2. Extended stability testing
3. Edge case testing
4. Code cleanup

### **Long Term (Next Week)**
1. Documentation
2. Video demo
3. Consider future enhancements
4. Release v1.0

---

**END TODO**

# 🎯 QUICK CONTEXT: Lovense Solace Pro 2 Reverse Engineering

## GOAL
Reverse engineer Lovense Solace Pro 2 BLE protocol and create ESP32 controller (same as I did for Kiiroo Keon).

## WHAT I DID WITH KEON (Success!)
1. ✅ Captured BLE packets with nRF Sniffer + Wireshark
2. ✅ Found MAC: AC:67:B2:25:42:5A
3. ✅ Found Service: 00001900-0000-1000-8000-00805f9b34fb
4. ✅ Tested characteristics: 0x1902 works (not 0x1901!)
5. ✅ Decoded protocol: `[0x04][0x00][POS 0-99][0x00][SPEED 0-99]`
6. ✅ Created working ESP32 code

## PROCESS TO REPEAT FOR SOLACE

### 1. CAPTURE
```
- Device OFF → Start Wireshark (COM7) → Device ON → Note MAC
- Connect in Lovense Remote app
- Do movements: stop/slow/medium/fast/positions (10 sec each)
- Save as lovense_solace.pcapng
```

### 2. ANALYZE
```
Find: MAC address, Service UUID, Characteristic UUIDs
Extract: All write commands (btatt.opcode == 0x52)
Decode: Command byte patterns
```

### 3. TEST CHARACTERISTICS
```cpp
// Try EACH writable characteristic with test command
// Watch Solace - which one makes it move?
// That's the correct TX characteristic!
```

### 4. BUILD CONTROLLER
```cpp
#define SOLACE_MAC "XX:XX:XX:XX:XX:XX"      // From capture
#define SOLACE_SERVICE_UUID "xxx"            // From capture  
#define SOLACE_TX_CHAR_UUID "xxx"            // From testing

void move(pos, speed) {
  uint8_t cmd[] = {/* format from analysis */};
  txChar->writeValue(cmd, len, false);
  delay(200);
}
```

## KEY LESSONS FROM KEON
⚠️ **Don't assume UUIDs** - test all writable characteristics  
⚠️ **Stop ≠ disconnect** - Keon uses speed=0 to stop, 0x00 disconnects  
⚠️ **Need delays** - 200ms between commands prevents issues  
⚠️ **Test thoroughly** - characteristic tester is essential  

## TOOLS READY
✅ nRF Sniffer configured (COM7)  
✅ Wireshark working  
✅ ESP32 + Arduino IDE  
✅ Lovense Remote app  

## NEED HELP WITH
1. Capture Solace packets
2. Analyze service/characteristics
3. Decode command protocol
4. Create characteristic tester
5. Build final ESP32 code

## EXPECTED RESULTS
```
Service UUID:      xxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
TX Characteristic: xxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
Command Format:    [byte structure]
Timing:           Xms between commands
```

**Let's do it!** 🚀

---

*See full context document for detailed methodology, code templates, and analysis scripts.*

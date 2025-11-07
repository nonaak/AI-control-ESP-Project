# 🎯 PROJECT CONTEXT: BLE Device Reverse Engineering & ESP32 Control

## 📋 OVERVIEW

I successfully reverse-engineered the **Kiiroo Keon** BLE protocol and created a working ESP32 controller. I want to do the **EXACT SAME PROCESS** for the **Lovense Solace Pro 2**.

---

## ✅ WHAT WE ACCOMPLISHED WITH KEON

### Complete Success:
1. ✅ Captured BLE packets using nRF Sniffer + Wireshark
2. ✅ Analyzed GATT services and characteristics
3. ✅ Decoded command protocol
4. ✅ Found correct characteristic (0x1902, not 0x1901!)
5. ✅ Created working ESP32 Arduino code
6. ✅ Tested and verified everything works

---

## 🔬 METHODOLOGY USED (Apply to Lovense Solace Pro 2)

### Phase 1: BLE Packet Capture
**Tools:**
- nRF Sniffer for Bluetooth LE (COM7 in Wireshark)
- Wireshark 4.2.2
- Official app (FeelConnect for Keon, Lovense Remote for Solace)
- Device under test

**Process:**
1. Install nRF Sniffer
2. Configure Wireshark to see nRF Sniffer (COM7)
3. Start capture BEFORE connecting app
4. Connect device in official app
5. Perform test movements (slow/stop/fast/positions)
6. Capture all packets
7. Save as .pcapng file

**Key Discovery Method:**
- Device identification: Turn device OFF, start capture, turn device ON → new MAC appears
- Connection following: Use nRF Sniffer toolbar "Follow" feature on device MAC
- Command isolation: Filter on `btatt.opcode == 0x52` for write commands

### Phase 2: Protocol Analysis
**Analysis Steps:**
1. Identify device MAC address
2. Find service UUID(s)
3. Find all characteristics in service
4. Identify writable characteristics
5. Capture command bytes during movements
6. Decode command format by comparing patterns

**Tools Used:**
```bash
tshark -r capture.pcapng -Y "btatt" -T fields -e btatt.value
```

### Phase 3: Characteristic Testing
**Critical Discovery:** 
The UUID we thought was correct (0x1901) DIDN'T WORK. We created a test program to try ALL writable characteristics and found 0x1902 was correct.

**Test Program Structure:**
```cpp
// Try characteristic 1
write(testCommand);
delay(5000);  // WATCH DEVICE - does it move?

// Try characteristic 2  
write(testCommand);
delay(5000);  // WATCH DEVICE - does it move?

// Report which one worked
```

### Phase 4: Final Implementation
- Create complete ESP32 controller code
- Implement all discovered commands
- Add demo patterns
- Test stability

---

## 📊 KEON RESULTS (Reference for Solace)

### BLE Details:
```
Device Name:     Keon
MAC Address:     AC:67:B2:25:42:5A
Manufacturer:    Espressif (ESP32-based)

Service UUID:    00001900-0000-1000-8000-00805f9b34fb

Characteristics:
  0x1901:        Read/Write - Does NOT work for commands
  0x1902:        Read/Write/Notify - WORKS! (correct one)
  0x1903:        Read/Notify only
```

### Protocol Discovered:
```
Command Format:  [0x04][0x00][POSITION 0-99][0x00][SPEED 0-99]
Stop Command:    [0x04][0x00][POSITION][0x00][0x00] (speed = 0)
Disconnect:      [0x00] (NEVER use - disconnects device!)

Examples:
  04 00 63 00 63  - Move to position 99, speed 99
  04 00 32 00 42  - Move to position 50, speed 66
  04 00 32 00 00  - Stop at position 50
```

### Timing Requirements:
```
Minimum interval:     30ms
Recommended:          100-200ms between commands
Write method:         WRITE_NO_RESPONSE (faster)
```

### Key Lessons Learned:
1. ⚠️ **Can't assume characteristic from UUID pattern** - must test all
2. ⚠️ **Stop is NOT a separate command** - it's speed = 0
3. ⚠️ **Standalone 0x00 byte disconnects** - don't use
4. ✅ **Need delays between commands** - 200ms works well
5. ✅ **Write without response is faster** - use it

---

## 🎯 GOAL: Lovense Solace Pro 2

### What I Need:
1. **Capture BLE packets** from Lovense Solace Pro 2
2. **Find service and characteristic UUIDs**
3. **Decode command protocol**
4. **Create characteristic tester** (like we did for Keon)
5. **Build working ESP32 code**

### Known Information About Lovense Solace:
- Manufacturer: Lovense (likely uses different chip than Keon)
- Official app: Lovense Remote
- Connection: Bluetooth LE
- Features: Position control, speed control, patterns

### Expected Differences from Keon:
- Different service UUID
- Different characteristic UUIDs
- Possibly different command format
- May use different protocol structure
- Might have more features (vibration, rotation, etc.)

---

## 🛠️ TOOLS & SETUP NEEDED

### Hardware:
- Lovense Solace Pro 2 (charged and ready)
- ESP32 development board
- Computer with Bluetooth
- USB cable for ESP32

### Software:
- nRF Sniffer for Bluetooth LE (already installed)
- Wireshark (already configured)
- Arduino IDE with ESP32 support
- Lovense Remote app (on phone)
- Python 3 (for analysis scripts)

---

## 📝 STEP-BY-STEP PROCESS TO FOLLOW

### Step 1: Initial Capture
```
1. Ensure Solace is OFF
2. Open Wireshark
3. Start capture on COM7 (nRF Sniffer)
4. Turn Solace ON
5. Note new MAC address that appears
6. Open Lovense Remote app
7. Connect to Solace
8. Wait for connection to stabilize
```

### Step 2: Command Capture
```
In Lovense Remote app, do these actions (SLOWLY):
- Stop (wait 10 sec)
- Speed 20% (wait 10 sec)
- Speed 50% (wait 10 sec)
- Speed 100% (wait 10 sec)
- Stop (wait 10 sec)
- Position 0% (wait 10 sec)
- Position 50% (wait 10 sec)
- Position 100% (wait 10 sec)
- Stop (wait 10 sec)

This gives clear command patterns to analyze.
```

### Step 3: Save and Analyze
```
1. Stop Wireshark capture
2. Save as lovense_solace_commands.pcapng
3. Analyze with tshark/Python scripts
4. Identify service UUID
5. Identify characteristic UUIDs
6. Extract command bytes
7. Find patterns in commands
```

### Step 4: Decode Protocol
```
Compare command bytes:
- What changes between speeds?
- What changes between positions?
- Is there a command ID byte?
- How many bytes per command?
- What's the structure?
```

### Step 5: Create Characteristic Tester
```cpp
// Test EVERY writable characteristic
// Structure:
void setup() {
  // Connect to Solace
  // Get all characteristics
  // For each writable characteristic:
  //   - Send test command
  //   - Wait 5 seconds
  //   - Watch if Solace moves
  //   - Report which one worked
}
```

### Step 6: Build Final Controller
```cpp
// Based on discovered protocol
#define SOLACE_SERVICE_UUID "xxx"
#define SOLACE_TX_CHAR_UUID "xxx"  // From testing

void move(position, speed) {
  // Use discovered protocol
  uint8_t cmd[] = {/* format from analysis */};
  writeValue(cmd);
}
```

---

## 📦 CODE TEMPLATES TO USE

### 1. Characteristic Tester Template
```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

#define SOLACE_MAC "XX:XX:XX:XX:XX:XX"  // From capture
#define SOLACE_SERVICE_UUID "xxxxx"     // From capture

BLEClient* pClient = nullptr;

void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32_Solace_Tester");
  
  // Connect
  BLEAddress addr(SOLACE_MAC);
  pClient = BLEDevice::createClient();
  pClient->connect(addr);
  
  // Get service
  BLERemoteService* svc = pClient->getService(SOLACE_SERVICE_UUID);
  
  // Get all characteristics
  auto* chars = svc->getCharacteristics();
  
  // Test each writable characteristic
  for (auto &entry : *chars) {
    BLERemoteCharacteristic* pChar = entry.second;
    
    if (pChar->canWrite() || pChar->canWriteNoResponse()) {
      Serial.printf("\n=== Testing: %s ===\n", 
                    pChar->getUUID().toString().c_str());
      
      // Send test command (adjust based on capture analysis)
      uint8_t testCmd[] = {/* test bytes */};
      pChar->writeValue(testCmd, sizeof(testCmd), false);
      
      Serial.println("WATCH SOLACE - Does it move?");
      delay(5000);
      
      // Send stop (if format known)
      uint8_t stopCmd[] = {/* stop bytes */};
      pChar->writeValue(stopCmd, sizeof(stopCmd), false);
      delay(2000);
    }
  }
  
  Serial.println("\n=== Which characteristic made Solace move? ===");
  while(1) delay(1000);
}

void loop() {}
```

### 2. Final Controller Template
```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

#define SOLACE_MAC "XX:XX:XX:XX:XX:XX"
#define SOLACE_SERVICE_UUID "xxxxx"
#define SOLACE_TX_CHAR_UUID "xxxxx"  // Verified working one

BLEClient* pClient;
BLERemoteCharacteristic* pTxChar;
bool connected = false;

bool connectToSolace() {
  BLEDevice::init("ESP32_Solace");
  BLEAddress addr(SOLACE_MAC);
  
  pClient = BLEDevice::createClient();
  if (!pClient->connect(addr)) return false;
  
  BLERemoteService* svc = pClient->getService(SOLACE_SERVICE_UUID);
  if (!svc) return false;
  
  pTxChar = svc->getCharacteristic(SOLACE_TX_CHAR_UUID);
  if (!pTxChar) return false;
  
  connected = true;
  return true;
}

void move(uint8_t position, uint8_t speed) {
  // Use discovered protocol format
  uint8_t cmd[] = {/* format from analysis */};
  pTxChar->writeValue(cmd, sizeof(cmd), false);
  delay(200);  // Adjust based on testing
}

void stop() {
  // Use discovered stop format
  uint8_t cmd[] = {/* stop format */};
  pTxChar->writeValue(cmd, sizeof(cmd), false);
}

void setup() {
  Serial.begin(115200);
  
  if (!connectToSolace()) {
    Serial.println("Connection failed!");
    ESP.restart();
  }
  
  Serial.println("Connected! Running demo...");
  
  // Demo
  move(50, 99);
  delay(2000);
  stop();
}

void loop() {
  // Custom patterns here
}
```

---

## 🔍 ANALYSIS HELPERS

### Python Script to Parse Packets
```python
#!/usr/bin/env python3
import subprocess

pcap = 'lovense_solace_commands.pcapng'

# Extract write commands
cmd = ['tshark', '-r', pcap, '-Y', 'btatt.opcode == 0x52',
       '-T', 'fields', '-e', 'frame.time_relative', 
       '-e', 'btatt.value']

result = subprocess.run(cmd, capture_output=True, text=True)

print("=== WRITE COMMANDS FOUND ===\n")
for line in result.stdout.strip().split('\n'):
    if line:
        parts = line.split('\t')
        if len(parts) >= 2:
            timestamp = parts[0]
            value = parts[1]
            print(f"Time: {timestamp}s")
            print(f"Bytes: {value}")
            print()
```

### Find Device MAC
```python
#!/usr/bin/env python3
import subprocess

pcap = 'lovense_capture.pcapng'

# Find devices that appeared after capture started
cmd = ['tshark', '-r', pcap, '-Y', 
       'frame.time_relative > 5 && btle.advertising_address',
       '-T', 'fields', '-e', 'btle.advertising_address',
       '-e', 'frame.time_relative']

result = subprocess.run(cmd, capture_output=True, text=True)

devices = {}
for line in result.stdout.strip().split('\n'):
    parts = line.split('\t')
    if len(parts) >= 2:
        mac = parts[0]
        time = float(parts[1])
        if mac not in devices:
            devices[mac] = time

print("=== DEVICES APPEARED LATE (likely Solace) ===\n")
for mac, time in sorted(devices.items(), key=lambda x: x[1]):
    if time > 5.0:
        print(f"MAC: {mac} @ {time:.2f}s")
```

---

## ⚠️ CRITICAL THINGS TO REMEMBER

### From Keon Experience:

1. **Don't assume UUIDs work without testing**
   - We thought 0x1901 was correct
   - Actually 0x1902 was the right one
   - ALWAYS test with tester program

2. **Watch for disconnect commands**
   - Keon: standalone 0x00 disconnects
   - Find what Solace uses for stop vs disconnect

3. **Timing is crucial**
   - Too fast = buffer overflow or disconnect
   - 100-200ms between commands is safe
   - Adjust based on testing

4. **Write method matters**
   - Try both writeValue(data, len, true) and false
   - false = no response = faster
   - true = with response = more reliable

5. **Stop might not be a separate command**
   - Keon: stop = speed 0, not a different command
   - Check if Solace is similar

---

## 📋 DELIVERABLES NEEDED

### 1. Packet Capture File
- lovense_solace_commands.pcapng
- With clear test movements

### 2. Analysis Results
```
Device MAC:      XX:XX:XX:XX:XX:XX
Service UUID:    xxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
Characteristics:
  UUID 1:        Properties, Purpose
  UUID 2:        Properties, Purpose
  (etc)

Command Format:  [byte structure discovered]
Examples:        Actual hex values from capture
```

### 3. Characteristic Tester Code
- Complete .ino file
- Tests all writable characteristics
- Reports which one works

### 4. Final Controller Code
- Complete ESP32 Arduino code
- Uses correct characteristic
- Implements all commands
- Has demo patterns
- Well documented

### 5. Documentation
- Complete protocol documentation
- All UUIDs and formats
- Timing requirements
- Example commands
- Troubleshooting guide

---

## 🎯 EXPECTED TIMELINE

Based on Keon experience:

1. **Packet Capture:** 30-60 minutes
2. **Analysis:** 1-2 hours
3. **Characteristic Testing:** 30 minutes
4. **Final Code:** 1-2 hours
5. **Testing & Debugging:** 1-2 hours

**Total:** 4-7 hours (if everything goes smoothly)

---

## 💡 TIPS FOR SUCCESS

### Do:
✅ Start capture BEFORE turning device on
✅ Use nRF Sniffer "Follow" feature
✅ Do movements SLOWLY with long delays
✅ Test ALL writable characteristics
✅ Add delays between commands (200ms)
✅ Document everything as you go
✅ Save all capture files

### Don't:
❌ Assume UUID patterns without testing
❌ Send commands too fast
❌ Skip the characteristic tester step
❌ Forget to disconnect old app before ESP32
❌ Rush the analysis phase

---

## 🚀 READY TO START

### I have:
- ✅ Lovense Solace Pro 2 (charged)
- ✅ ESP32 board
- ✅ nRF Sniffer configured
- ✅ Wireshark working
- ✅ Lovense Remote app installed
- ✅ Understanding of process from Keon
- ✅ Code templates ready

### I need help with:
1. Capturing Solace BLE packets
2. Analyzing service/characteristic UUIDs
3. Decoding command protocol
4. Creating characteristic tester
5. Building final ESP32 controller

---

## 📞 REQUEST TO NEW AI

Please help me:

1. **Guide through packet capture** process for Lovense Solace Pro 2
2. **Analyze captured packets** to find service and characteristics
3. **Decode the command protocol** from packet data
4. **Create characteristic tester** to find working UUID
5. **Build complete ESP32 controller code** that works

Use the **exact same methodology** we used for Keon (documented above).

---

## 📎 ATTACHMENTS

I will provide:
- ✅ Packet capture file (.pcapng)
- ✅ Initial analysis results
- ✅ Any error messages encountered
- ✅ Test results from characteristic tester

---

## ✨ SUCCESS CRITERIA

Project is complete when:
- ✅ Solace BLE protocol fully documented
- ✅ Correct characteristic UUID identified
- ✅ Command format decoded
- ✅ ESP32 code compiles
- ✅ ESP32 successfully controls Solace
- ✅ All movements work (position, speed, stop)
- ✅ Connection is stable
- ✅ Demo patterns work

---

**Let's reverse engineer the Lovense Solace Pro 2 protocol!** 🚀

---

*Context document version: 1.0*  
*Based on: Successful Keon reverse engineering (Nov 2025)*  
*Ready for: Lovense Solace Pro 2 analysis*

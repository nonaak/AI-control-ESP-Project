/*
  ESP32-C3 KEON - 8 LEVELS (op basis van posities)
  
  Levels:
  L0 = pos 0   (langzaam ~32/min)
  L1 = pos 15
  L2 = pos 30
  L3 = pos 45
  L4 = pos 60
  L5 = pos 75
  L6 = pos 85
  L7 = pos 99  (snel ~120/min)
  
  Commands:
  0-7 = Set level
  s   = Start/Stop
  h   = Help
*/

#include <U8g2lib.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// OLED
#define X_OFFSET 28
#define Y_OFFSET 24
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// KEON
#define KEON_MAC "ac:67:b2:25:42:5a"
#define KEON_SERVICE_UUID "00001900-0000-1000-8000-00805f9b34fb"
#define KEON_TX_CHAR_UUID "00001902-0000-1000-8000-00805f9b34fb"

BLEClient* keonClient = nullptr;
BLERemoteCharacteristic* keonTxChar = nullptr;
bool keonConnected = false;

// Control
uint8_t currentLevel = 0;
bool keonActive = false;

// 8 Levels op basis van positie (oscillatie frequentie)
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

// ════════════════════════════════════════════════════════════
// BLE
// ════════════════════════════════════════════════════════════

class KeonCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* p) { 
    keonConnected = true;
    Serial.println("[BLE] ✅ Connected!");
  }
  void onDisconnect(BLEClient* p) { 
    keonConnected = false;
    keonActive = false;
    Serial.println("[BLE] ❌ Disconnected!");
  }
};

bool keonConnect() {
  Serial.println("[BLE] Connecting...");
  
  keonClient = BLEDevice::createClient();
  keonClient->setClientCallbacks(new KeonCallback());
  
  if (!keonClient->connect(BLEAddress(KEON_MAC))) {
    Serial.println("[BLE] ❌ Failed!");
    return false;
  }
  delay(500);
  
  BLERemoteService* s = keonClient->getService(BLEUUID(KEON_SERVICE_UUID));
  if (!s) { 
    Serial.println("[BLE] ❌ Service not found!");
    keonClient->disconnect(); 
    return false; 
  }
  
  keonTxChar = s->getCharacteristic(BLEUUID(KEON_TX_CHAR_UUID));
  if (!keonTxChar) { 
    Serial.println("[BLE] ❌ TX char not found!");
    keonClient->disconnect(); 
    return false; 
  }
  
  keonConnected = true;
  Serial.println("[BLE] ✅ Ready!");
  return true;
}

void keonMove(uint8_t pos, uint8_t spd) {
  if (!keonConnected || !keonTxChar) return;
  
  uint8_t cmd[5] = {0x04, 0x00, pos, 0x00, spd};
  
  try {
    keonTxChar->writeValue(cmd, 5, true);
    delay(200);
  } catch(...) {}
}

void keonStop() {
  if (!keonConnected) return;
  
  Serial.println("[KEON] Stopping...");
  
  // Stop commando meerdere keren voor zekerheid
  keonMove(50, 0);   // Midden positie, speed 0
  delay(300);
  keonMove(50, 0);   // Herhaal
  delay(300);
  keonMove(0, 0);    // Naar 0, speed 0
  delay(300);
  
  Serial.println("[KEON] ✅ Stopped");
}

// ════════════════════════════════════════════════════════════
// DISPLAY
// ════════════════════════════════════════════════════════════

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.drawFrame(X_OFFSET, Y_OFFSET, 72, 40);
  
  // BLE status
  if (keonConnected) {
    u8g2.drawDisc(X_OFFSET + 64, Y_OFFSET + 8, 3);
  }
  
  // Level groot in midden
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.setCursor(X_OFFSET + 8, Y_OFFSET + 28);
  
  if (!keonConnected) {
    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.setCursor(X_OFFSET + 4, Y_OFFSET + 25);
    u8g2.print("NO BLE");
  } else {
    u8g2.print("L");
    u8g2.print(currentLevel);
    
    // Active indicator
    if (keonActive) {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.setCursor(X_OFFSET + 50, Y_OFFSET + 28);
      u8g2.print("ON");
    }
  }
  
  u8g2.sendBuffer();
}

// ════════════════════════════════════════════════════════════
// CONTROL
// ════════════════════════════════════════════════════════════

void setLevel(uint8_t level) {
  if (level > 7) level = 7;
  currentLevel = level;
  
  Serial.print("[LEVEL] ");
  Serial.print(level);
  Serial.print(" (pos ");
  Serial.print(levelPositions[level]);
  Serial.println(")");
  
  // Als actief, update direct
  if (keonActive && keonConnected) {
    keonMove(levelPositions[currentLevel], 99);
  }
  
  updateDisplay();
}

void toggleActive() {
  keonActive = !keonActive;
  
  if (keonActive) {
    Serial.println("[CMD] ▶️ START");
    if (keonConnected) {
      keonMove(levelPositions[currentLevel], 99);
    }
  } else {
    Serial.println("[CMD] ⏸️ STOP");
    keonStop();
  }
  
  updateDisplay();
}

void showHelp() {
  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.println("║  ESP32-C3 KEON - 8 LEVELS        ║");
  Serial.println("╚═══════════════════════════════════╝");
  Serial.println();
  Serial.println("COMMANDS:");
  Serial.println("  0-7  → Set level");
  Serial.println("  s    → Start/Stop");
  Serial.println("  r    → Reconnect");
  Serial.println("  h    → Help");
  Serial.println();
  Serial.println("LEVELS (oscillatie frequentie):");
  Serial.println("  L0 = pos 0   (~32 strokes/min)");
  Serial.println("  L1 = pos 15");
  Serial.println("  L2 = pos 30");
  Serial.println("  L3 = pos 45");
  Serial.println("  L4 = pos 60");
  Serial.println("  L5 = pos 75");
  Serial.println("  L6 = pos 85");
  Serial.println("  L7 = pos 99  (~120 strokes/min)");
  Serial.println();
  Serial.print("CURRENT: Level ");
  Serial.print(currentLevel);
  Serial.print(", ");
  Serial.println(keonActive ? "ACTIVE" : "STOPPED");
  Serial.println("═══════════════════════════════════\n");
}

// ════════════════════════════════════════════════════════════
// SERIAL COMMANDS
// ════════════════════════════════════════════════════════════

void processCommand(char c) {
  if (c >= '0' && c <= '7') {
    setLevel(c - '0');
  }
  else if (c == 's' || c == 'S') {
    toggleActive();
  }
  else if (c == 'r' || c == 'R') {
    if (keonConnected) {
      keonClient->disconnect();
      delay(500);
    }
    keonConnect();
    updateDisplay();
  }
  else if (c == 'h' || c == 'H') {
    showHelp();
  }
}

// ════════════════════════════════════════════════════════════
// SETUP
// ════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔═══════════════════════════════════╗");
  Serial.println("║  ESP32-C3 KEON - 8 LEVELS        ║");
  Serial.println("╚═══════════════════════════════════╝\n");
  
  // OLED
  Serial.println("[OLED] Init...");
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setBusClock(400000);
  Serial.println("[OLED] ✅ Ready!");
  
  // BLE
  Serial.println("[BLE] Init...");
  BLEDevice::init("ESP32-C3-Keon");
  keonConnect();
  
  updateDisplay();
  
  Serial.println("\nTyp 'h' voor help\n");
}

// ════════════════════════════════════════════════════════════
// LOOP
// ════════════════════════════════════════════════════════════

void loop() {
  if (Serial.available()) {
    processCommand(Serial.read());
  }
  
  // Display update
  static unsigned long lastDisp = 0;
  if (millis() - lastDisp > 300) {
    updateDisplay();
    lastDisp = millis();
  }
  
  delay(10);
}

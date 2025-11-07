/*
 * KIIROO KEON BLE CONTROLLER
 * ESP32 Arduino Code
 * 
 * Controls Kiiroo Keon via Bluetooth Low Energy
 * Protocol reverse-engineered from Wireshark captures
 * 
 * Hardware: ESP32 (any board)
 * 
 * PROTOCOL DISCOVERED:
 * - Service UUID: 00001900-0000-1000-8000-00805f9b34fb
 * - TX Characteristic UUID: 00001901-0000-1000-8000-00805f9b34fb (estimated)
 * - Command Format:
 *   MOVE:  [0x04][0x00][POSITION 0-99][0x00][SPEED 0-99]
 *   STOP:  [0x00]
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// ========== CONFIGURATION ==========
#define KEON_MAC_ADDRESS "ac:67:b2:25:42:5a"  // Jouw Keon MAC adres
#define KEON_SERVICE_UUID "00001900-0000-1000-8000-00805f9b34fb"
#define KEON_TX_CHAR_UUID "00001902-0000-1000-8000-00805f9b34fb"  // CORRECT! (was 1901)

// Fallback: probeer alle characteristics in de service
#define SCAN_ALL_CHARACTERISTICS true

// ========== GLOBAL VARIABLES ==========
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pTxCharacteristic = nullptr;
BLEAddress keonAddress(KEON_MAC_ADDRESS);
bool connected = false;

// Track current position
uint8_t currentPosition = 50;

// ========== KEON COMMAND FUNCTIONS ==========

/**
 * Send raw bytes to Keon
 */
bool sendCommand(uint8_t* data, size_t length) {
  if (!connected || pTxCharacteristic == nullptr) {
    Serial.println("❌ Not connected!");
    return false;
  }
  
  // Debug print BEFORE sending
  Serial.print("📤 Sending: ");
  for (size_t i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  
  // Send command - try WITH response first (more reliable)
  try {
    pTxCharacteristic->writeValue(data, length, true);  // WITH response
    Serial.println("✅ Sent successfully (with response)");
  } catch (...) {
    // If that fails, try without response
    Serial.println("⚠️  Trying without response...");
    pTxCharacteristic->writeValue(data, length, false);
    Serial.println("✅ Sent (no response)");
  }
  
  // CRITICAL: Add delay between commands to prevent disconnect!
  delay(200);  // 200ms between commands
  
  return true;
}

/**
 * Move Keon to position with speed
 * @param position 0-99 (0=bottom, 99=top)
 * @param speed 0-99 (0=stopped, 99=fastest)
 */
bool move(uint8_t position, uint8_t speed) {
  if (position > 99) position = 99;
  if (speed > 99) speed = 99;
  
  currentPosition = position;  // Track position
  
  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, speed};
  
  Serial.printf("🎯 Moving to position %d with speed %d\n", position, speed);
  return sendCommand(cmd, 5);
}

/**
 * Stop at specific position
 */
bool stopAtPosition(uint8_t position) {
  if (position > 99) position = 99;
  
  Serial.printf("⏹️  Stopping at position %d\n", position);
  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, 0x00};  // speed 0
  
  return sendCommand(cmd, 5);
}

/**
 * Stop Keon (hold current position)
 * NOTE: 0x00 disconnects the device!
 * Instead, we send current position with speed 0
 */
bool stop() {
  return stopAtPosition(currentPosition);
}

/**
 * Convenience functions
 */
bool moveSlow(uint8_t position) {
  return move(position, 33);  // 33% speed
}

bool moveMedium(uint8_t position) {
  return move(position, 66);  // 66% speed
}

bool moveFast(uint8_t position) {
  return move(position, 99);  // 99% speed = maximum
}

// ========== BLE CONNECTION ==========

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("✅ Connected to Keon!");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("❌ Disconnected from Keon!");
  }
};

/**
 * Connect to Keon
 */
bool connectToKeon() {
  Serial.println("\n🔍 Connecting to Keon...");
  Serial.printf("   MAC: %s\n", KEON_MAC_ADDRESS);
  
  // Create BLE Client
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  // Connect to Keon
  Serial.println("   Connecting...");
  if (!pClient->connect(keonAddress)) {
    Serial.println("❌ Failed to connect!");
    return false;
  }
  
  Serial.println("✅ Connected!");
  delay(500);  // Let connection stabilize
  
  // Get service
  Serial.printf("   Finding service %s...\n", KEON_SERVICE_UUID);
  BLERemoteService* pRemoteService = pClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("❌ Service not found!");
    pClient->disconnect();
    return false;
  }
  Serial.println("✅ Service found!");
  
  // Get TX characteristic
  Serial.printf("   Finding TX characteristic...\n");
  
  // First, list ALL characteristics for debugging
  Serial.println("   📋 Available characteristics:");
  std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
  
  for (auto &entry : *characteristics) {
    BLERemoteCharacteristic* pChar = entry.second;
    String uuid = pChar->getUUID().toString().c_str();  // Fix: use String and .c_str()
    Serial.printf("      UUID: %s\n", uuid.c_str());
    Serial.printf("         Can Read: %s\n", pChar->canRead() ? "YES" : "no");
    Serial.printf("         Can Write: %s\n", pChar->canWrite() ? "YES" : "no");
    Serial.printf("         Can Write No Response: %s\n", pChar->canWriteNoResponse() ? "YES" : "no");
    Serial.printf("         Can Notify: %s\n", pChar->canNotify() ? "YES" : "no");
  }
  
  // Try the known UUID first
  pTxCharacteristic = pRemoteService->getCharacteristic(KEON_TX_CHAR_UUID);
  
  // If UUID doesn't work, find ANY writable characteristic
  if (pTxCharacteristic == nullptr) {
    Serial.println("   ⚠️  Known UUID not found, trying writable characteristics...");
    
    for (auto &entry : *characteristics) {
      BLERemoteCharacteristic* pChar = entry.second;
      
      if (pChar->canWrite() || pChar->canWriteNoResponse()) {
        pTxCharacteristic = pChar;
        Serial.printf("   ✅ Using writable characteristic: %s\n", pChar->getUUID().toString().c_str());
        break;
      }
    }
  } else {
    Serial.printf("   ✅ Found characteristic by UUID: %s\n", KEON_TX_CHAR_UUID);
  }
  
  if (pTxCharacteristic == nullptr) {
    Serial.println("❌ TX Characteristic not found!");
    pClient->disconnect();
    return false;
  }
  
  Serial.println("✅ TX Characteristic found!");
  connected = true;
  
  return true;
}

// ========== DEMO PATTERNS ==========

/**
 * Demo: Simple up-down movement
 */
void demoUpDown() {
  Serial.println("\n🎬 DEMO: Up-Down Movement");
  
  moveFast(0);    // Move to bottom, fast
  delay(2000);
  
  moveFast(99);   // Move to top, fast
  delay(2000);
  
  moveFast(50);   // Move to middle, fast
  delay(2000);
  
  stop();
}

/**
 * Demo: Speed variations
 */
void demoSpeeds() {
  Serial.println("\n🎬 DEMO: Speed Variations");
  
  moveSlow(99);   // Slow to top
  delay(3000);
  
  moveMedium(0);  // Medium to bottom
  delay(2000);
  
  moveFast(99);   // Fast to top
  delay(1500);
  
  stop();
}

/**
 * Demo: Stroking pattern
 */
void demoStroke() {
  Serial.println("\n🎬 DEMO: Stroking Pattern");
  
  for (int i = 0; i < 10; i++) {
    move(10, 99);   // Fast down
    delay(400);
    
    move(90, 99);   // Fast up
    delay(400);
  }
  
  stop();
}

// ========== SETUP & LOOP ==========

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  KIIROO KEON BLE CONTROLLER       ║");
  Serial.println("║  ESP32 Arduino Code                ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  // Initialize BLE
  Serial.println("🚀 Initializing BLE...");
  BLEDevice::init("ESP32_Keon_Controller");
  
  // Connect to Keon
  if (!connectToKeon()) {
    Serial.println("\n❌ Failed to connect. Check:");
    Serial.println("   - Keon is ON and nearby");
    Serial.println("   - MAC address is correct");
    Serial.println("   - Keon is not connected to another device");
    Serial.println("\nRestarting in 10 seconds...");
    delay(10000);
    ESP.restart();
  }
  
  Serial.println("\n✅ Setup complete!");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("📝 COMMANDS AVAILABLE:");
  Serial.println("   - move(position, speed)");
  Serial.println("   - stop()");
  Serial.println("   - moveSlow/Medium/Fast(position)");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
  
  // Wait a bit before starting demo
  delay(2000);
  
  // Run demo
  Serial.println("🎬 Starting demo in 3 seconds...");
  delay(3000);
  
  demoUpDown();
  delay(3000);
  
  demoSpeeds();
  delay(3000);
  
  demoStroke();
  
  Serial.println("\n✅ Demo complete!");
}

void loop() {
  // Keep connection alive
  if (!connected) {
    Serial.println("❌ Connection lost, attempting reconnect...");
    delay(5000);
    
    if (connectToKeon()) {
      Serial.println("✅ Reconnected!");
    }
  }
  
  // You can add your own control code here
  // Examples:
  
  // Simple continuous stroking:
  // move(10, 99);
  // delay(500);
  // move(90, 99);
  // delay(500);
  
  // Or add button inputs, serial commands, etc.
  
  delay(100);
}

// ========== SERIAL COMMAND INTERFACE (OPTIONAL) ==========

/*
 * Uncomment this section to control via Serial Monitor
 * 
 * Commands:
 *   m <pos> <speed>  - Move to position with speed (e.g., "m 50 99")
 *   s                - Stop
 *   u                - Demo up-down
 *   v                - Demo speeds
 *   t                - Demo stroke
 */

/*
void serialEvent() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd.startsWith("m ")) {
      int pos = cmd.substring(2, cmd.indexOf(' ', 2)).toInt();
      int spd = cmd.substring(cmd.lastIndexOf(' ') + 1).toInt();
      move(pos, spd);
    }
    else if (cmd == "s") {
      stop();
    }
    else if (cmd == "u") {
      demoUpDown();
    }
    else if (cmd == "v") {
      demoSpeeds();
    }
    else if (cmd == "t") {
      demoStroke();
    }
    else {
      Serial.println("Unknown command. Use: m <pos> <speed>, s, u, v, t");
    }
  }
}
*/

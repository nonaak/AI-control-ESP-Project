/*
 * KIIROO KEON - CHARACTERISTIC TESTER
 * Test which characteristic controls the Keon
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>

// ========== CONFIGURATION ==========
#define KEON_MAC_ADDRESS "ac:67:b2:25:42:5a"
#define KEON_SERVICE_UUID "00001900-0000-1000-8000-00805f9b34fb"

// ========== GLOBAL VARIABLES ==========
BLEClient* pClient = nullptr;
BLEAddress keonAddress(KEON_MAC_ADDRESS);
bool connected = false;

// ========== CONNECTION ==========
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("✅ Connected to Keon!");
    connected = true;
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("❌ Disconnected from Keon!");
  }
};

bool connectToKeon() {
  Serial.println("\n🔍 Connecting to Keon...");
  Serial.printf("   MAC: %s\n", KEON_MAC_ADDRESS);
  
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  Serial.println("   Connecting...");
  if (!pClient->connect(keonAddress)) {
    Serial.println("❌ Failed to connect!");
    return false;
  }
  
  Serial.println("✅ Connected!");
  delay(500);
  
  Serial.printf("   Finding service %s...\n", KEON_SERVICE_UUID);
  BLERemoteService* pRemoteService = pClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("❌ Service not found!");
    pClient->disconnect();
    return false;
  }
  Serial.println("✅ Service found!");
  
  return true;
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════╗");
  Serial.println("║  KIIROO KEON - CHAR TESTER        ║");
  Serial.println("╚════════════════════════════════════╝\n");
  
  BLEDevice::init("ESP32_Keon_Tester");
  
  if (!connectToKeon()) {
    Serial.println("\n❌ Failed to connect!");
    Serial.println("Restarting in 10 seconds...");
    delay(10000);
    ESP.restart();
  }
  
  // Get service
  BLERemoteService* svc = pClient->getService(KEON_SERVICE_UUID);
  
  // Test commands
  uint8_t testCmd[5] = {0x04, 0x00, 0x32, 0x00, 0x63};  // pos 50, speed 99
  uint8_t stopCmd[5] = {0x04, 0x00, 0x32, 0x00, 0x00};  // pos 50, speed 0
  
  Serial.println("\n🧪 TESTING ALL CHARACTERISTICS:\n");
  
  // ==================== TEST 1: 0x1901 ====================
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("📝 TEST 1: Characteristic 0x1901");
  Serial.println("   UUID: 00001901-0000-1000-8000-00805f9b34fb");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  
  BLERemoteCharacteristic* char1901 = svc->getCharacteristic("00001901-0000-1000-8000-00805f9b34fb");
  if (char1901) {
    Serial.println("✅ Characteristic found!");
    Serial.println("\n📤 Sending MOVE command: 04 00 32 00 63");
    Serial.println("   (Position 50, Speed 99)");
    
    try {
      char1901->writeValue(testCmd, 5, false);
      Serial.println("✅ Command sent!");
      Serial.println("\n⏰ WATCHING KEON FOR 5 SECONDS...");
      Serial.println("   👀 DOES IT MOVE? YES or NO?");
      delay(5000);
      
      Serial.println("\n📤 Sending STOP command: 04 00 32 00 00");
      char1901->writeValue(stopCmd, 5, false);
      Serial.println("✅ Stop sent!");
      delay(2000);
      
    } catch (...) {
      Serial.println("❌ Write failed!");
    }
  } else {
    Serial.println("❌ Characteristic NOT found!");
  }
  
  delay(2000);
  
  // ==================== TEST 2: 0x1902 ====================
  Serial.println("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("📝 TEST 2: Characteristic 0x1902");
  Serial.println("   UUID: 00001902-0000-1000-8000-00805f9b34fb");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  
  BLERemoteCharacteristic* char1902 = svc->getCharacteristic("00001902-0000-1000-8000-00805f9b34fb");
  if (char1902) {
    Serial.println("✅ Characteristic found!");
    Serial.println("\n📤 Sending MOVE command: 04 00 32 00 63");
    Serial.println("   (Position 50, Speed 99)");
    
    try {
      char1902->writeValue(testCmd, 5, false);
      Serial.println("✅ Command sent!");
      Serial.println("\n⏰ WATCHING KEON FOR 5 SECONDS...");
      Serial.println("   👀 DOES IT MOVE? YES or NO?");
      delay(5000);
      
      Serial.println("\n📤 Sending STOP command: 04 00 32 00 00");
      char1902->writeValue(stopCmd, 5, false);
      Serial.println("✅ Stop sent!");
      delay(2000);
      
    } catch (...) {
      Serial.println("❌ Write failed!");
    }
  } else {
    Serial.println("❌ Characteristic NOT found!");
  }
  
  // ==================== RESULTS ====================
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════╗");
  Serial.println("║          TEST COMPLETE!            ║");
  Serial.println("╚════════════════════════════════════╝");
  Serial.println("\n🎯 RESULTS:");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println("Did Keon move during:");
  Serial.println("  TEST 1 (0x1901)? → [YES/NO]");
  Serial.println("  TEST 2 (0x1902)? → [YES/NO]");
  Serial.println("\n💡 NEXT STEPS:");
  Serial.println("  1. Note which test made Keon move");
  Serial.println("  2. Report back the results!");
  Serial.println("  3. We'll update the main code with correct UUID");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  
  // Done - stop here
  while(1) { 
    delay(1000); 
  }
}

void loop() {
  // Not used
}

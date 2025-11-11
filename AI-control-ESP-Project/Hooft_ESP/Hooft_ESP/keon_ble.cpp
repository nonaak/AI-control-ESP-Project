#include "keon_ble.h"
#include <BLEUtils.h>

// ===============================================================================
// GLOBAL STATE VARIABLES
// ===============================================================================

BLEClient* keonClient = nullptr;
BLERemoteCharacteristic* keonTxCharacteristic = nullptr;
BLEAddress keonAddress(KEON_MAC_ADDRESS);

bool keonConnected = false;
uint8_t keonCurrentPosition = 50;
uint8_t keonCurrentSpeed = 0;

// Connection state tracking
static uint32_t lastKeonCommand = 0;
static uint32_t lastKeonReconnectAttempt = 0;
static bool keonInitialized = false;
static String keonMacAddress = KEON_MAC_ADDRESS;

// ===============================================================================
// BLE CALLBACK HANDLER
// ===============================================================================

class KeonClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    keonConnected = true;
    Serial.println("[KEON] ✅ Connected!");
  }
  
  void onDisconnect(BLEClient* pclient) {
    keonConnected = false;
    Serial.println("[KEON] ❌ Disconnected!");
  }
};

// ===============================================================================
// COMMAND SENDING
// ===============================================================================

/**
 * Send raw command to Keon (internal helper)
 */
static bool keonSendCommand(uint8_t* data, size_t length) {
  if (!keonConnected || keonTxCharacteristic == nullptr) {
    return false;
  }

  // Rate limiting
  uint32_t now = millis();
  if ((now - lastKeonCommand) < KEON_COMMAND_DELAY_MS) {
    return false;
  }

  // Send with response (more reliable)
  try {
    keonTxCharacteristic->writeValue(data, length, true);
    lastKeonCommand = now;
    return true;
  } catch (...) {
    // Try without response as fallback
    try {
      keonTxCharacteristic->writeValue(data, length, false);
      lastKeonCommand = now;
      return true;
    } catch (...) {
      Serial.println("[KEON] ❌ Write failed");
      return false;
    }
  }
}

// ===============================================================================
// MOVEMENT CONTROL
// ===============================================================================

/**
 * Move Keon to position with speed
 */
bool keonMove(uint8_t position, uint8_t speed) {
  if (position > 99) position = 99;
  if (speed > 99) speed = 99;

  keonCurrentPosition = position;
  keonCurrentSpeed = speed;

  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, speed};
  return keonSendCommand(cmd, 5);
}

/**
 * Stop at specific position
 */
static bool keonStopAtPosition(uint8_t position) {
  if (position > 99) position = 99;
  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, 0x00};
  return keonSendCommand(cmd, 5);
}

/**
 * Stop at current position
 */
bool keonStop() {
  return keonStopAtPosition(keonCurrentPosition);
}

/**
 * Convenience functions
 */
bool keonMoveSlow(uint8_t position) {
  return keonMove(position, 33);  // 33% speed
}

bool keonMoveMedium(uint8_t position) {
  return keonMove(position, 66);  // 66% speed
}

bool keonMoveFast(uint8_t position) {
  return keonMove(position, 99);  // 99% speed
}

/**
 * Park Keon at bottom when paused
 */
void keonParkToBottom() {
  if (!keonConnected) return;
  keonMove(0, 0);  // Bottom position, speed 0
}

// ===============================================================================
// CONNECTION MANAGEMENT
// ===============================================================================

/**
 * Initialize Keon BLE system
 */
void keonInit() {
  if (keonInitialized) {
    Serial.println("[KEON] Already initialized");
    return;
  }
  
  Serial.println("[KEON] Initializing BLE...");
  BLEDevice::init("HoofdESP_KeonController");
  keonInitialized = true;
  Serial.println("[KEON] BLE initialized");
}

/**
 * Connect to Keon via BLE
 * WARNING: This function BLOCKS for ~500ms-1s during connection!
 * This is INTENTIONAL and works fine with ESP-NOW.
 */
bool keonConnect() {
  if (!keonInitialized) {
    keonInit();
  }

  Serial.printf("[KEON] Connecting to %s...\n", keonMacAddress.c_str());

  // Create client
  keonClient = BLEDevice::createClient();
  keonClient->setClientCallbacks(new KeonClientCallback());

  // Connect (BLOCKING ~500ms-1s)
  if (!keonClient->connect(keonAddress)) {
    Serial.println("[KEON] ❌ Connect failed");
    return false;
  }

  // Stabilization delay (BLOCKING)
  delay(500);

  // Get service
  BLERemoteService* pRemoteService = keonClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("[KEON] ❌ Service not found");
    keonClient->disconnect();
    return false;
  }

  // Get characteristic
  keonTxCharacteristic = pRemoteService->getCharacteristic(KEON_TX_CHAR_UUID);

  // Fallback: search for writable characteristic
  if (keonTxCharacteristic == nullptr) {
    Serial.println("[KEON] Searching for writable characteristic...");
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
    
    if (characteristics != nullptr) {
      for (auto &entry : *characteristics) {
        BLERemoteCharacteristic* pChar = entry.second;
        if (pChar->canWrite() || pChar->canWriteNoResponse()) {
          keonTxCharacteristic = pChar;
          Serial.printf("[KEON] Found: %s\n", pChar->getUUID().toString().c_str());
          break;
        }
      }
    }
  }

  if (keonTxCharacteristic == nullptr) {
    Serial.println("[KEON] ❌ No writable characteristic found");
    keonClient->disconnect();
    return false;
  }

  keonConnected = true;
  lastKeonReconnectAttempt = millis();
  Serial.println("[KEON] ✅ Connected and ready!");
  return true;
}

/**
 * Disconnect from Keon
 */
void keonDisconnect() {
  if (keonConnected && keonClient != nullptr) {
    Serial.println("[KEON] Disconnecting...");
    keonStop();
    delay(200);
    keonClient->disconnect();
    keonConnected = false;
    keonTxCharacteristic = nullptr;
  }
}

/**
 * Check if Keon is connected
 */
bool keonIsConnected() {
  return keonConnected && keonClient != nullptr && keonTxCharacteristic != nullptr;
}

/**
 * Check connection state and attempt reconnect if needed
 * CALL THIS IN YOUR MAIN LOOP!
 * 
 * AUTO-RECONNECT IS DISABLED to prevent ESP freezing!
 * User must manually connect via menu.
 */
void keonCheckConnection() {
  // Check if still connected
  if (keonConnected && keonClient != nullptr && !keonClient->isConnected()) {
    Serial.println("[KEON] Connection lost!");
    keonConnected = false;
    keonTxCharacteristic = nullptr;
  }
  
  // AUTO-RECONNECT DISABLED - prevents ESP freezing with ESP-NOW
  // User must manually reconnect via menu
}

/**
 * Manual reconnect attempt
 */
void keonReconnect() {
  Serial.println("[KEON] Manual reconnect requested");
  if (keonConnected) {
    keonDisconnect();
  }
  keonConnect();
}

// ===============================================================================
// ANIMATION SYNCHRONIZATION
// ===============================================================================

/**
 * Sync Keon movement with HoofdESP animation
 * 
 * Velocity/Direction Mapping:
 * - isMovingUp=true  → Keon naar BOVEN (99)
 * - isMovingUp=false → Keon naar BENEDEN (0)
 * - Speed follows animation speed step (0-7 → 0-99)
 */
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp) {
  if (!keonConnected) return;

  static uint8_t lastPosition = 50;
  static uint8_t lastSpeed = 0;
  static bool lastDirection = false;
  static uint32_t lastSyncTime = 0;
  const uint32_t SYNC_INTERVAL_MS = 100;  // 10Hz updates

  uint32_t now = millis();
  
  // Rate limit
  if ((now - lastSyncTime) < SYNC_INTERVAL_MS) {
    return;
  }
  
  // Calculate speed (0-99)
  uint8_t speed = (speedSteps <= 1) ? 0 : (speedStep * 99) / (speedSteps - 1);
  if (speed > 99) speed = 99;

  // Map direction to position
  uint8_t position = isMovingUp ? 99 : 0;
  
  // Only send if state changed
  if (position != lastPosition || speed != lastSpeed || isMovingUp != lastDirection) {
    keonMove(position, speed);
    lastPosition = position;
    lastSpeed = speed;
    lastDirection = isMovingUp;
    lastSyncTime = now;
  }
}

// ===============================================================================
// MAC ADDRESS MANAGEMENT
// ===============================================================================

String keonGetLastMAC() {
  return keonMacAddress;
}

void keonSetMAC(const char* mac) {
  keonMacAddress = String(mac);
  keonAddress = BLEAddress(mac);
  Serial.printf("[KEON] MAC updated to: %s\n", mac);
}

// ===============================================================================
// DEBUG & UTILITY
// ===============================================================================

void keonPrintStatus() {
  Serial.println("[KEON] === STATUS ===");
  Serial.printf("  Connected: %s\n", keonConnected ? "YES" : "NO");
  Serial.printf("  Position: %u\n", keonCurrentPosition);
  Serial.printf("  Speed: %u\n", keonCurrentSpeed);
  Serial.printf("  MAC: %s\n", keonMacAddress.c_str());
}

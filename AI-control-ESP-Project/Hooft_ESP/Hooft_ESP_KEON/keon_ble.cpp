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
  Serial.println("[KEON] Parking to bottom (paused)");
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
 * Check connection state - ULTRA LIGHTWEIGHT!
 * CALL THIS IN YOUR MAIN LOOP!
 * 
 * Only checks connection status, no reconnect logic.
 * This ensures animation keeps running smoothly.
 */
void keonCheckConnection() {
  // Ultra-fast connection check (no blocking!)
  if (keonConnected && keonClient != nullptr) {
    if (!keonClient->isConnected()) {
      keonConnected = false;
      keonTxCharacteristic = nullptr;
      Serial.println("[KEON] Connection lost - reconnect via menu");
    }
  }
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
 * Simple 1:1 mapping:
 * - Keon follows sleeve position exactly (0-99 full range)
 * - Tempo is determined by animation speed (slow → fast)
 * - Just like the on-screen animation!
 * 
 * IMPORTANT: This should ONLY be called when animation is running!
 */
 void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp) {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) {
    Serial.println("[KEON SYNC] ERROR: Called while paused!");
    return;
  }

  static bool lastDirection = false;
  static uint32_t lastSyncTime = 0;
  static uint32_t lastDebugTime = 0;

  uint32_t now = millis();
  
  // Alleen updaten bij direction change
  if (isMovingUp != lastDirection) {
    
    // Dynamische debounce op basis van animatie snelheid
    // Bij hogere snelheid = kortere debounce
    // MIN_SPEED = 0.22 Hz → ~2270ms per stroke → debounce 150ms
    // MAX_SPEED = 1.17 Hz → ~427ms per stroke → debounce 50ms
    float step01 = (speedSteps <= 1) ? 0.0f : (float)speedStep / (float)(speedSteps - 1);
    uint32_t debounce = 150 - (uint32_t)(step01 * 100.0f);  // 150ms @ level 0, 50ms @ level 6
    
    if ((now - lastSyncTime) < debounce) {
      return;
    }
    
    uint8_t targetPos = isMovingUp ? 99 : 0;
    uint8_t keonSpeed = 99;
    
    keonMove(targetPos, keonSpeed);
    
    lastDirection = isMovingUp;
    lastSyncTime = now;
    
    // Debug output
    if (now - lastDebugTime > 2000) {
      Serial.printf("[KEON STROKE] %s → Pos:%u Speed:99 (level %u, debounce:%ums)\n", 
                    isMovingUp ? "UP  " : "DOWN", targetPos, speedStep, debounce);
      lastDebugTime = now;
    }
  }
}
 //void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp) {
  //if (!keonConnected) return;
  
  // Extra safety: Don't sync if paused
  //extern bool paused;
  //if (paused) {
    //Serial.println("[KEON SYNC] ERROR: Called while paused!");
    //return;
  //}

  //static bool lastDirection = false;
  //static uint32_t lastSyncTime = 0;
  //static uint32_t lastDebugTime = 0;

  //uint32_t now = millis();
  
  // Alleen updaten bij direction change (volle stroke)
  //if (isMovingUp != lastDirection) {
    
    // Debounce: min 100ms tussen strokes (voorkomt jitter)
    //if ((now - lastSyncTime) < 100) {
      //return;
    //}
    
    // Full stroke positie
    //uint8_t targetPos = isMovingUp ? 99 : 0;
    
    // Altijd max speed voor snelste response
    //uint8_t keonSpeed = 99;
    
    //keonMove(targetPos, keonSpeed);
    
    //lastDirection = isMovingUp;
    //lastSyncTime = now;
    
    // Debug output (elke 2 seconden)
    //if (now - lastDebugTime > 2000) {
      //Serial.printf("[KEON STROKE] %s → Pos:%u Speed:99 (level %u)\n", 
                    //isMovingUp ? "UP  " : "DOWN", targetPos, speedStep);
      //lastDebugTime = now;
    //}
  //}
//}
//void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, float sleevePercentage) {
  //if (!keonConnected) return;
  
  // Extra safety: Don't sync if paused (this should never happen, but just in case)
  //extern bool paused;
  //if (paused) {
    //Serial.println("[KEON SYNC] ERROR: Called while paused!");
    //return;
  //}

  //static uint8_t lastPosition = 50;
  //static uint32_t lastSyncTime = 0;
  //static uint32_t lastDebugTime = 0;
  //const uint32_t SYNC_INTERVAL_MS = 100;  // 10Hz to avoid blocking animation

  // Variable sync interval based on speedStep
  // speedStep 0 = slow updates, speedStep 7 = fast updates
  //uint32_t syncInterval = 300 - (speedStep * 35);  // 300ms @ speed 0, 55ms @ speed 7
  //if (syncInterval < 50) syncInterval = 50;  // Minimum 50ms

  //uint32_t now = millis();
  
  // Rate limit to prevent blocking main loop
  //if ((now - lastSyncTime) < syncInterval) {
    //return;
  //}
  //if ((now - lastSyncTime) < SYNC_INTERVAL_MS) {
    //return;
  //}
  
  // Map sleeve percentage (0-100) directly to Keon position (0-99)
  // FULL RANGE at all speeds, just like the animation!
  //uint8_t position = (uint8_t)(sleevePercentage * 0.99f);
  //if (position > 99) position = 99;
  
  // Keon speed: always 99 for instant response
  // The TEMPO is controlled by animation frequency (speedStep affects animation Hz)
  //uint8_t keonSpeed = 99;

  
  // Only send if position changed (reduce BLE traffic)
  //int posDiff = abs((int)position - (int)lastPosition);
  
  //if (posDiff >= 2) {  // Small threshold to reduce jitter
    //keonMove(position, keonSpeed);
    //lastPosition = position;
    //lastSyncTime = now;
    
    // Debug output every 2 seconds
    //if (now - lastDebugTime > 2000) {
      //Serial.printf("[KEON SYNC] Pos:%u Speed:%u SleevePercent:%.1f speedStep:%u\n", 
                   // position, keonSpeed, sleevePercentage, speedStep);
      //lastDebugTime = now;
    //}
 // }
//}

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

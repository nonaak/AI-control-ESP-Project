#include "keon.h"
#include "config.h"
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

// ===============================================================================
// BLE CALLBACK HANDLER
// ===============================================================================

class KeonClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    keonConnected = true;
  }
  void onDisconnect(BLEClient* pclient) {
    keonConnected = false;
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
    Serial.println("[KEON] ❌ Not connected - cannot send command");
    return false;
  }

  // Send with response (more reliable)
  try {
    keonTxCharacteristic->writeValue(data, length, true);
    lastKeonCommand = millis();
    return true;
  } catch (...) {
    // Try without response
    try {
      keonTxCharacteristic->writeValue(data, length, false);
      lastKeonCommand = millis();
      return true;
    } catch (...) {
      return false;
    }
  }

  // Delay to prevent disconnect
  delay(KEON_COMMAND_DELAY_MS);
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
bool keonStopAtPosition(uint8_t position) {
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

// ===============================================================================
// CONNECTION MANAGEMENT
// ===============================================================================

/**
 * Initialize Keon BLE system
 */
void keonInit() {
  if (keonInitialized) return;
  BLEDevice::init("HoofdESP_KeonController");
  keonInitialized = true;
}

/**
 * Connect to Keon via BLE
 */
bool keonConnect() {
  if (!keonInitialized) {
    keonInit();
  }

  keonClient = BLEDevice::createClient();
  keonClient->setClientCallbacks(new KeonClientCallback());

  if (!keonClient->connect(keonAddress)) {
    return false;
  }

  delay(500);

  BLERemoteService* pRemoteService = keonClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    keonClient->disconnect();
    return false;
  }

  keonTxCharacteristic = pRemoteService->getCharacteristic(KEON_TX_CHAR_UUID);

  if (keonTxCharacteristic == nullptr) {
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
    if (characteristics != nullptr) {  // NULL check toegevoegd voor crash fix
      for (auto &entry : *characteristics) {
        BLERemoteCharacteristic* pChar = entry.second;
        if (pChar->canWrite() || pChar->canWriteNoResponse()) {
          keonTxCharacteristic = pChar;
          break;
        }
      }
    }
  }

  if (keonTxCharacteristic == nullptr) {
    keonClient->disconnect();
    return false;
  }

  keonConnected = true;
  lastKeonReconnectAttempt = millis();
  return true;
}

/**
 * Disconnect from Keon
 */
void keonDisconnect() {
  if (keonConnected && keonClient != nullptr) {
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
 */
void keonCheckConnection() {
  if (!keonConnected) {
    uint32_t now = millis();
    if ((now - lastKeonReconnectAttempt) > KEON_RECONNECT_INTERVAL_MS) {
      keonConnect();
    }
  }
}

/**
 * Manual reconnect attempt
 */
void keonReconnect() {
  if (keonConnected) {
    keonDisconnect();
  }
  keonConnect();
}

// ===============================================================================
// ANIMATION SYNCHRONIZATION
// ===============================================================================

/**
 * Park Keon at bottom when paused
 * Called when C button triggers pause
 */
void keonParkToBottom() {
  if (!keonConnected) return;
  keonMove(0, 0);  // Bottom position, speed 0 (stopped)
}

/**
 * Sync Keon movement with HoofdESP animation
 * Maps animation speed to Keon position and speed
 * 
 * Velocity/Direction Mapping (from velEMA):
 * - velEMA NEGATIVE (omhoog) → isMovingUp=true  → Keon naar BOVEN (99)
 * - velEMA POSITIVE (omlaag)  → isMovingUp=false → Keon naar BENEDEN (0)
 * - Speed follows HoofdESP animation speed step (0-7 → 0-99)
 */
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp) {
  if (!keonConnected) return;

  // Static variables to track previous state
  static uint8_t lastPosition = 50;
  static uint8_t lastSpeed = 0;
  static bool lastDirection = false;
  static uint32_t lastSyncTime = 0;
  const uint32_t SYNC_INTERVAL_MS = 100;  // Send sync every 100ms minimum

  uint32_t now = millis();
  
  // Rate limit to prevent command flooding
  if ((now - lastSyncTime) < SYNC_INTERVAL_MS) {
    return;
  }
  
  // Calculate speed as percentage (0-99)
  // speedStep 0-7 → speed 0-99 lineair
  uint8_t speed = (speedSteps <= 1) ? 0 : (speedStep * 99) / (speedSteps - 1);
  if (speed > 99) speed = 99;

  // Map animation direction (velEMA) to Keon position
  // isMovingUp = true  (velEMA < 0, moving UP)    → position 99 (BOVEN)
  // isMovingUp = false (velEMA >= 0, moving DOWN) → position 0 (BENEDEN)
  uint8_t position = isMovingUp ? 99 : 0;
  
  // Only send if state changed (position, speed, or direction)
  if (position != lastPosition || speed != lastSpeed || isMovingUp != lastDirection) {
    keonMove(position, speed);
    lastPosition = position;
    lastSpeed = speed;
    lastDirection = isMovingUp;
    lastSyncTime = now;
  }
}

// ===============================================================================
// DEBUG & UTILITY
// ===============================================================================

/**
 * Print Keon status information
 */
void keonPrintStatus() {
  // Debug status (minimal output)
}

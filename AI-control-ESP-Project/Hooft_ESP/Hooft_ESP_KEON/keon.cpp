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
// COMMAND SENDING (NO DELAYS!)
// ===============================================================================

/**
 * Send raw command to Keon (internal helper)
 * NO DELAYS to prevent ESP-NOW crashes!
 */
static bool keonSendCommand(uint8_t* data, size_t length) {
  if (!keonConnected || keonTxCharacteristic == nullptr) {
    return false;
  }

  // Rate limiting via timestamp check ONLY
  uint32_t now = millis();
  if ((now - lastKeonCommand) < KEON_COMMAND_DELAY_MS) {
    return false;  // Skip this command, too soon
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
      return false;
    }
  }
  
  // NO delay() here! Would crash ESP-NOW!
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
  return keonMove(position, 33);
}

bool keonMoveMedium(uint8_t position) {
  return keonMove(position, 66);
}

bool keonMoveFast(uint8_t position) {
  return keonMove(position, 99);
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
 * WARNING: This function BLOCKS for ~500ms-1s!
 * Should ONLY be called from user menu action, NOT from loop!
 */
bool keonConnect() {
  if (!keonInitialized) {
    keonInit();
  }

  keonClient = BLEDevice::createClient();
  keonClient->setClientCallbacks(new KeonClientCallback());

  // BLOCKING connect (500ms-1s)
  if (!keonClient->connect(keonAddress)) {
    return false;
  }

  // BLOCKING stabilization delay
  delay(500);

  BLERemoteService* pRemoteService = keonClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    keonClient->disconnect();
    return false;
  }

  keonTxCharacteristic = pRemoteService->getCharacteristic(KEON_TX_CHAR_UUID);

  if (keonTxCharacteristic == nullptr) {
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
    if (characteristics != nullptr) {
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
  return true;
}

/**
 * Disconnect from Keon
 */
void keonDisconnect() {
  if (keonConnected && keonClient != nullptr) {
    keonStop();
    delay(200);  // Allow stop command to be sent
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
 * NO AUTO-RECONNECT to prevent ESP crashes!
 * User must manually reconnect via menu.
 */
void keonCheckConnection() {
  // Ultra-fast connection check (no blocking!)
  if (keonConnected && keonClient != nullptr) {
    if (!keonClient->isConnected()) {
      keonConnected = false;
      keonTxCharacteristic = nullptr;
    }
  }
  
  // NO auto-reconnect! Would crash ESP-NOW!
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
 */
void keonParkToBottom() {
  if (!keonConnected) return;
  keonMove(0, 0);  // Bottom position, speed 0
}

/**
 * Sync Keon movement with HoofdESP animation
 * 
 * Uses velocity-based direction mapping:
 * - isMovingUp = true  (velEMA < 0) → Keon naar BOVEN (position 99)
 * - isMovingUp = false (velEMA >= 0) → Keon naar BENEDEN (position 0)
 * - Speed mapped from speedStep (0-7 → 0-99)
 */
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp) {
  if (!keonConnected) return;

  static uint8_t lastPosition = 50;
  static uint8_t lastSpeed = 0;
  static bool lastDirection = false;
  static uint32_t lastSyncTime = 0;
  const uint32_t SYNC_INTERVAL_MS = 100;  // 10Hz to avoid blocking animation

  uint32_t now = millis();
  
  // Rate limit to prevent blocking main loop
  if ((now - lastSyncTime) < SYNC_INTERVAL_MS) {
    return;
  }
  
  // Map speedStep (0-7) to Keon speed (0-99)
  uint8_t keonSpeed = (speedSteps <= 1) ? 0 : (speedStep * 99) / (speedSteps - 1);
  if (keonSpeed > 99) keonSpeed = 99;
  
  // Map animation direction to Keon position
  // isMovingUp = true  → position 99 (TOP)
  // isMovingUp = false → position 0 (BOTTOM)
  uint8_t position = isMovingUp ? 99 : 0;
  
  // Only send if state changed (position, speed, or direction)
  if (position != lastPosition || keonSpeed != lastSpeed || isMovingUp != lastDirection) {
    keonMove(position, keonSpeed);
    lastPosition = position;
    lastSpeed = keonSpeed;
    lastDirection = isMovingUp;
    lastSyncTime = now;
  }
}

// ===============================================================================
// DEBUG & UTILITY
// ===============================================================================

void keonPrintStatus() {
  // Minimal output
}

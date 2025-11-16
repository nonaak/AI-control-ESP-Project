#include "keon_ble.h"
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

// MAC address storage
static String lastConnectedMAC = "";

// ===============================================================================
// BLE CALLBACK HANDLER
// ===============================================================================

class KeonClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    keonConnected = true;
    Serial.println("[KEON] BLE Connected");
  }
  void onDisconnect(BLEClient* pclient) {
    keonConnected = false;
    Serial.println("[KEON] BLE Disconnected");
  }
};

// ===============================================================================
// COMMAND SENDING (NO DELAYS!)
// ===============================================================================

static bool keonSendCommand(uint8_t* data, size_t length) {
  if (!keonConnected || keonTxCharacteristic == nullptr) {
    return false;
  }

  uint32_t now = millis();
  if ((now - lastKeonCommand) < KEON_COMMAND_DELAY_MS) {
    return false;
  }

  try {
    keonTxCharacteristic->writeValue(data, length, true);
    lastKeonCommand = now;
    return true;
  } catch (...) {
    try {
      keonTxCharacteristic->writeValue(data, length, false);
      lastKeonCommand = now;
      return true;
    } catch (...) {
      return false;
    }
  }
}

// ===============================================================================
// MOVEMENT CONTROL
// ===============================================================================

bool keonMove(uint8_t position, uint8_t speed) {
  if (position > 99) position = 99;
  if (speed > 99) speed = 99;

  keonCurrentPosition = position;
  keonCurrentSpeed = speed;

  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, speed};
  return keonSendCommand(cmd, 5);
}

bool keonStopAtPosition(uint8_t position) {
  if (position > 99) position = 99;
  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, 0x00};
  return keonSendCommand(cmd, 5);
}

bool keonStop() {
  return keonStopAtPosition(keonCurrentPosition);
}

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

void keonInit() {
  if (keonInitialized) return;
  BLEDevice::init("HoofdESP_KeonController");
  keonInitialized = true;
  Serial.println("[KEON] BLE initialized");
}

bool keonConnect() {
  if (!keonInitialized) {
    keonInit();
  }

  Serial.println("[KEON] Attempting connection...");
  
  keonClient = BLEDevice::createClient();
  keonClient->setClientCallbacks(new KeonClientCallback());

  if (!keonClient->connect(keonAddress)) {
    Serial.println("[KEON] Connection failed!");
    return false;
  }

  delay(500);

  BLERemoteService* pRemoteService = keonClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("[KEON] Service not found!");
    keonClient->disconnect();
    return false;
  }

  keonTxCharacteristic = pRemoteService->getCharacteristic(KEON_TX_CHAR_UUID);

  if (keonTxCharacteristic == nullptr) {
    Serial.println("[KEON] TX Characteristic not found, searching...");
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
    if (characteristics != nullptr) {
      for (auto &entry : *characteristics) {
        BLERemoteCharacteristic* pChar = entry.second;
        if (pChar->canWrite() || pChar->canWriteNoResponse()) {
          keonTxCharacteristic = pChar;
          Serial.println("[KEON] Found writable characteristic!");
          break;
        }
      }
    }
  }

  if (keonTxCharacteristic == nullptr) {
    Serial.println("[KEON] No writable characteristic found!");
    keonClient->disconnect();
    return false;
  }

  keonConnected = true;
  lastConnectedMAC = String(KEON_MAC_ADDRESS);
  Serial.printf("[KEON] Connected successfully to %s\n", KEON_MAC_ADDRESS);
  return true;
}

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

bool keonIsConnected() {
  return keonConnected && keonClient != nullptr && keonTxCharacteristic != nullptr;
}

void keonCheckConnection() {
  if (keonConnected && keonClient != nullptr) {
    if (!keonClient->isConnected()) {
      Serial.println("[KEON] Connection lost!");
      keonConnected = false;
      keonTxCharacteristic = nullptr;
    }
  }
}

void keonReconnect() {
  Serial.println("[KEON] Manual reconnect...");
  if (keonConnected) {
    keonDisconnect();
  }
  keonConnect();
}

// ===============================================================================
// MAC ADDRESS MANAGEMENT
// ===============================================================================

String keonGetLastMAC() {
  return lastConnectedMAC;
}

void keonSetMAC(const char* mac) {
  lastConnectedMAC = String(mac);
  Serial.printf("[KEON] MAC set to: %s (requires recompile)\n", mac);
}

// ===============================================================================
// PARK TO BOTTOM
// ===============================================================================

void keonParkToBottom() {
  if (!keonConnected) return;
  Serial.println("[KEON] Parking to bottom (pause)");
  keonMove(0, 50);
}

// ===============================================================================
// 🚀 NIEUWE ONAFHANKELIJKE KEON CONTROL - GEFIXTE VERSIE!
// ===============================================================================

/**
 * ✅ KEON INDEPENDENT TICK - 100% ONAFHANKELIJK MET SPEED MAPPING!
 * 
 * FIXED:
 * - Speed niet meer altijd 99, maar gemapped naar speedStep (20-99)
 * - Bij lage levels = langzame Keon beweging
 * - Bij hoge levels = snelle Keon beweging
 * - Volle strokes (0↔99) blijven behouden
 * 
 * TIMING:
 * - Level 0: 1200ms interval, speed 20 = 25 strokes/min (LANGZAAM)
 * - Level 7: 400ms interval, speed 99 = 75 strokes/min (SNEL)
 */
//void keonIndependentTick() {
  //if (!keonConnected) return;
  
  //extern bool paused;
  //if (paused) return;
  
  //static bool keonDirection = false;
  //static uint32_t lastStroke = 0;
  
  //uint32_t now = millis();
  
  //extern uint8_t g_speedStep;
  
  // ✅ MAP speedStep (0-7) naar Keon speed (20-99)
  // Level 0 = langzaam (speed 20)
  // Level 7 = snel (speed 99)
  //uint8_t keonSpeed = 20 + ((g_speedStep * 79) / 7);
  //if (keonSpeed < 20) keonSpeed = 20;  // Minimum voor soepele beweging
  //if (keonSpeed > 99) keonSpeed = 99;  // Maximum
  
  // ✅ Sync interval: hoe vaak we direction togglen
  // Level 0: 1200ms (traag)
  // Level 7: 400ms (snel)
  //uint32_t syncInterval = 1200 - ((g_speedStep * 800) / 7);
  
  //if ((now - lastStroke) < syncInterval) {
    //return;  // Nog niet tijd voor volgende toggle
  //}
  
  // Toggle direction
  //keonDirection = !keonDirection;
  //uint8_t targetPos = keonDirection ? 99 : 0;
  
  // Stuur naar Keon met variabele speed
  //bool success = keonMove(targetPos, keonSpeed);
  
  //if (success) {
    //lastStroke = now;
    
    // Debug output (elke 2 seconden)
    //static uint32_t lastDebug = 0;
    //if (now - lastDebug > 2000) {
      //Serial.printf("[KEON INDEPENDENT] Level:%u Speed:%u Pos:%u Interval:%ums (%.0f strokes/min)\n", 
                    //g_speedStep,
                    //keonSpeed,
                    //targetPos,
                    //syncInterval,
                    //60000.0f / (syncInterval * 2));
      //lastDebug = now;
    //}
  //}
//}
void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool keonDirection = false;
  static uint32_t lastStroke = 0;
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // ✅ Variabele STROKE RANGE
  // Level 0: Korte strokes (40-60) = range 20
  // Level 7: Lange strokes (0-99) = range 99
  uint8_t strokeRange = 20 + ((g_speedStep * 79) / 7);  // 20 bij level 0, 99 bij level 7
  
  // ✅ Interval: Hoe vaak we togglen
  // Level 0: 1200ms (langzaam)
  // Level 7: 400ms (snel)
  uint32_t syncInterval = 1200 - ((g_speedStep * 800) / 7);
  
  if ((now - lastStroke) < syncInterval) {
    return;
  }
  
  // Toggle direction
  keonDirection = !keonDirection;
  
  // Bereken positie gebaseerd op range
  uint8_t targetPos;
  if (keonDirection) {
    targetPos = 50 + (strokeRange / 2);  // Omhoog
  } else {
    targetPos = 50 - (strokeRange / 2);  // Omlaag
  }
  
  // ✅ Speed ALTIJD 99 (volle kracht!)
  uint8_t keonSpeed = 99;
  
  bool success = keonMove(targetPos, keonSpeed);
  
  if (success) {
    lastStroke = now;
    
    static uint32_t lastDebug = 0;
    if (now - lastDebug > 2000) {
      Serial.printf("[KEON INDEPENDENT] Level:%u Range:%u Pos:%u Speed:99 Interval:%ums\n", 
                    g_speedStep, strokeRange, targetPos, syncInterval);
      lastDebug = now;
    }
  }
}

// ===============================================================================
// OUDE FUNCTIE - Voor Body ESP AI (indien gebruikt)
// ===============================================================================

void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp) {
  if (!keonConnected) return;

  static uint8_t lastPosition = 50;
  static uint8_t lastSpeed = 0;
  static bool lastDirection = false;
  static uint32_t lastSyncTime = 0;
  const uint32_t SYNC_INTERVAL_MS = 100;

  uint32_t now = millis();
  
  if ((now - lastSyncTime) < SYNC_INTERVAL_MS) {
    return;
  }
  
  uint8_t keonSpeed = (speedSteps <= 1) ? 0 : (speedStep * 99) / (speedSteps - 1);
  if (keonSpeed > 99) keonSpeed = 99;
  
  uint8_t position = isMovingUp ? 99 : 0;
  
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
  Serial.printf("[KEON STATUS] Connected:%d Pos:%u Speed:%u MAC:%s\n", 
                keonConnected, keonCurrentPosition, keonCurrentSpeed, lastConnectedMAC.c_str());
}

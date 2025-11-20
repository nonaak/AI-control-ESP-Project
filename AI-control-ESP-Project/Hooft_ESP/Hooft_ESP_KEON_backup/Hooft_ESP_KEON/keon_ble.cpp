#include "keon_ble.h"
#include "config.h"
#include <BLEUtils.h>

// ═══════════════════════════════════════════════════════════════════════════
// 8 LEVELS
// ═══════════════════════════════════════════════════════════════════════════


const uint8_t KEON_LEVEL_POSITIONS[8] = {
  0,   // L0: ~32 strokes/min (LANGZAAMST)
  30,  // L1: ~60 strokes/min (+28/min verschil!)
  45,  // L2: ~75 strokes/min
  55,  // L3: ~85 strokes/min  
  65,  // L4: ~95 strokes/min
  75,  // L5: ~103 strokes/min
  85,  // L6: ~110 strokes/min
  99   // L7: ~120 strokes/min (SNELST)
};
/*
const uint8_t KEON_LEVEL_POSITIONS[8] = {
  0,   // L0: ~32 strokes/min
  15,  // L1: ~45 strokes/min
  30,  // L2: ~60 strokes/min
  45,  // L3: ~75 strokes/min
  60,  // L4: ~85 strokes/min
  75,  // L5: ~95 strokes/min
  85,  // L6: ~110 strokes/min
  99   // L7: ~120 strokes/min
};*/

// ═══════════════════════════════════════════════════════════════════════════
// GLOBAL STATE
// ═══════════════════════════════════════════════════════════════════════════

BLEClient* keonClient = nullptr;
BLERemoteCharacteristic* keonTxCharacteristic = nullptr;
BLEAddress keonAddress(KEON_MAC_ADDRESS);

bool keonConnected = false;
uint8_t keonCurrentLevel = 0;

static bool keonInitialized = false;
static String lastConnectedMAC = "";
static bool keonActive = false;

// ═══════════════════════════════════════════════════════════════════════════
// BLE CALLBACK
// ═══════════════════════════════════════════════════════════════════════════

class KeonClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    keonConnected = true;
    Serial.println("[KEON] ✅ BLE Connected");
  }
  void onDisconnect(BLEClient* pclient) {
    keonConnected = false;
    keonActive = false;
    Serial.println("[KEON] ❌ BLE Disconnected");
  }
};

// ═══════════════════════════════════════════════════════════════════════════
// MOVEMENT - ZOALS ESP32-C3 (direct send, delay erna!)
// ═══════════════════════════════════════════════════════════════════════════

bool keonMove(uint8_t position, uint8_t speed) {
  if (!keonConnected || keonTxCharacteristic == nullptr) {
    return false;
  }
  
  if (position > 99) position = 99;
  if (speed > 99) speed = 99;

  uint8_t cmd[5] = {0x04, 0x00, position, 0x00, speed};
  
  Serial.printf("[KEON] Move: pos=%u, speed=%u\n", position, speed);
  
  try {
    keonTxCharacteristic->writeValue(cmd, 5, true);
    delay(200);  // Delay ERNA, zoals ESP32-C3!
    return true;
  } catch (...) {
    return false;
  }
}

bool keonStop() {
  if (!keonConnected) return false;
  
  Serial.println("[KEON] Stopping...");
  keonActive = false;
  
  // Meerdere stop commando's
  keonMove(50, 0);
  delay(100);
  keonMove(50, 0);
  delay(100);
  keonMove(0, 0);
  
  Serial.println("[KEON] ✅ Stopped");
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// LEVEL CONTROL
// ═══════════════════════════════════════════════════════════════════════════

void keonSetLevel(uint8_t level) {
  if (level > 7) level = 7;
  keonCurrentLevel = level;
  
  Serial.printf("[KEON] Level set to %u (pos %u)\n", 
                level, KEON_LEVEL_POSITIONS[level]);
  
  // Als actief, direct update!
  if (keonActive && keonConnected) {
    keonMove(KEON_LEVEL_POSITIONS[level], 99);
  }
}

uint8_t keonGetLevel() {
  return keonCurrentLevel;
}

// ═══════════════════════════════════════════════════════════════════════════
// ANIMATION SYNC
// ═══════════════════════════════════════════════════════════════════════════


float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  // AANGEPAST voor nieuwe positions (0,30,45,55,65,75,85,99)
  // GEEN halvering meer - 1:1 met Keon strokes!
  const float frequencies[8] = {
    0.533f,  // L0: pos 0   = 32/min  = 0.533 Hz
    1.000f,  // L1: pos 30  = 60/min  = 1.0 Hz  
    1.250f,  // L2: pos 45  = 75/min  = 1.25 Hz
    1.450f,  // L3: pos 55  = 87/min  = 1.45 Hz
    1.580f,  // L4: pos 65  = 95/min  = 1.58 Hz
    1.720f,  // L5: pos 75  = 103/min = 1.72 Hz
    1.830f,  // L6: pos 85  = 110/min = 1.83 Hz
    2.000f   // L7: pos 99  = 120/min = 2.0 Hz
  };
  
  return frequencies[level];
}
/*
float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  const float frequencies[8] = {
    0.133f, 0.188f, 0.250f, 0.313f,
    0.354f, 0.396f, 0.458f, 0.500f
  };
  
  return frequencies[level];
}*/

// ═══════════════════════════════════════════════════════════════════════════
// CONNECTION
// ═══════════════════════════════════════════════════════════════════════════

void keonInit() {
  if (keonInitialized) return;
  BLEDevice::init("HoofdESP_KeonController");
  keonInitialized = true;
  Serial.println("[KEON] BLE initialized");
}

bool keonConnect() {
  if (!keonInitialized) keonInit();

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
  Serial.printf("[KEON] ✅ Connected to %s\n", KEON_MAC_ADDRESS);
  return true;
}

void keonDisconnect() {
  if (keonConnected && keonClient != nullptr) {
    Serial.println("[KEON] Disconnecting...");
    keonStop();
    delay(500);
    keonClient->disconnect();
    keonConnected = false;
    keonTxCharacteristic = nullptr;
    keonActive = false;
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
      keonActive = false;
    }
  }
}

void keonReconnect() {
  Serial.println("[KEON] Manual reconnect...");
  if (keonConnected) {
    keonDisconnect();
  }
  delay(1000);
  keonConnect();
}

String keonGetLastMAC() {
  return lastConnectedMAC;
}

void keonSetMAC(const char* mac) {
  lastConnectedMAC = String(mac);
  Serial.printf("[KEON] MAC set to: %s (requires recompile)\n", mac);
}

bool keonStopAtPosition(uint8_t position) {
  if (position > 99) position = 99;
  keonMove(50, 0);
  keonMove(50, 0);
  keonMove(position, 0);
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// CORE 0 TICK - SIMPEL ZOALS ESP32-C3!
// ═══════════════════════════════════════════════════════════════════════════

void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  extern uint8_t g_speedStep;
  
  static bool wasRunning = false;
  
  // PAUSE
  if (paused) {
    if (wasRunning || keonActive) {
      Serial.println("[KEON CORE0] ⏸️ Paused");
      keonStop();
      wasRunning = false;
      keonActive = false;
    }
    return;
  }
  
  // START
  if (!wasRunning && !keonActive) {
    Serial.println("[KEON CORE0] ▶️ Starting");
    keonActive = true;
    wasRunning = true;
    
    keonCurrentLevel = g_speedStep;
    
    Serial.printf("[KEON CORE0] Level %u (pos %u)\n", 
                  keonCurrentLevel, KEON_LEVEL_POSITIONS[keonCurrentLevel]);
    
    keonMove(KEON_LEVEL_POSITIONS[keonCurrentLevel], 99);
    return;
  }
  
  // LEVEL CHANGE - Direct zoals ESP32-C3!
  if (keonActive && keonCurrentLevel != g_speedStep) {
    keonCurrentLevel = g_speedStep;
    
    Serial.printf("[KEON CORE0] → L%u (pos %u)\n",
                  keonCurrentLevel, KEON_LEVEL_POSITIONS[keonCurrentLevel]);
    
    keonMove(KEON_LEVEL_POSITIONS[keonCurrentLevel], 99);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// FREERTOS TASK
// ═══════════════════════════════════════════════════════════════════════════

void keonTask(void* parameter) {
  Serial.println("[KEON TASK] Started on Core 0");
  
  while(true) {
    keonCheckConnection();
    keonIndependentTick();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

static TaskHandle_t keonTaskHandle = NULL;

void keonStartTask() {
  if (keonTaskHandle != NULL) {
    Serial.println("[KEON TASK] Already running!");
    return;
  }
  
  xTaskCreatePinnedToCore(
    keonTask,
    "KeonTask",
    4096,
    NULL,
    1,
    &keonTaskHandle,
    0
  );
  
  Serial.println("[KEON TASK] Created on Core 0!");
}

void keonStopTask() {
  if (keonTaskHandle != NULL) {
    vTaskDelete(keonTaskHandle);
    keonTaskHandle = NULL;
    Serial.println("[KEON TASK] Stopped");
  }
}
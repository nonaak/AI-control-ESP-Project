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
  
  // KIES ER EEN - uncomment de regel die je wilt testen!
  
  // /1.06 - tussen /1.05 en /1.1
  /*
  const float frequencies[8] = {
    0.503f, 0.943f, 1.179f, 1.368f, 1.491f, 1.623f, 1.726f, 1.887f
  };
  */
  
  // /1.07 - tussen /1.05 en /1.1
  /*
  const float frequencies[8] = {
    0.498f, 0.935f, 1.168f, 1.355f, 1.477f, 1.607f, 1.710f, 1.869f
  };
  */
  
  // /1.08 - tussen /1.05 en /1.1
  /*
  const float frequencies[8] = {
    0.493f, 0.926f, 1.157f, 1.343f, 1.463f, 1.593f, 1.694f, 1.852f
  };
  */
  
  // /1.09 - tussen /1.05 en /1.1
  const float frequencies[8] = {
    0.489f, 0.917f, 1.147f, 1.330f, 1.450f, 1.578f, 1.679f, 1.835f
  };
  
  return frequencies[level];
}

/*
float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  // Gedeeld door 1.1 - bijna 1:1!
  const float frequencies[8] = {
    0.485f,  // L0: pos 0   (0.533 / 1.1)
    0.909f,  // L1: pos 30  (1.000 / 1.1)
    1.136f,  // L2: pos 45  (1.250 / 1.1)
    1.318f,  // L3: pos 55  (1.450 / 1.1)
    1.436f,  // L4: pos 65  (1.580 / 1.1)
    1.564f,  // L5: pos 75  (1.720 / 1.1)
    1.664f,  // L6: pos 85  (1.830 / 1.1)
    1.818f   // L7: pos 99  (2.000 / 1.1)
  };
  
  return frequencies[level];
}*/

/*
float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  // Gedeeld door 1.1 - bijna 1:1!
  const float frequencies[8] = {
    0.485f,  // L0: pos 0   (0.533 / 1.1)
    0.909f,  // L1: pos 30  (1.000 / 1.1)
    1.136f,  // L2: pos 45  (1.250 / 1.1)
    1.318f,  // L3: pos 55  (1.450 / 1.1)
    1.436f,  // L4: pos 65  (1.580 / 1.1)
    1.564f,  // L5: pos 75  (1.720 / 1.1)
    1.664f,  // L6: pos 85  (1.830 / 1.1)
    1.818f   // L7: pos 99  (2.000 / 1.1)
  };
  
  return frequencies[level];
}*/

/*
float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  // Gedeeld door 1.15 - fine tuning!
  const float frequencies[8] = {
    0.463f,  // L0: pos 0   (0.533 / 1.15)
    0.870f,  // L1: pos 30  (1.000 / 1.15)
    1.087f,  // L2: pos 45  (1.250 / 1.15)
    1.261f,  // L3: pos 55  (1.450 / 1.15)
    1.374f,  // L4: pos 65  (1.580 / 1.15)
    1.496f,  // L5: pos 75  (1.720 / 1.15)
    1.591f,  // L6: pos 85  (1.830 / 1.15)
    1.739f   // L7: pos 99  (2.000 / 1.15)
  };
  
  return frequencies[level];
}*/

/*
float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  // Gedeeld door 1.2
  const float frequencies[8] = {
    0.444f,  // L0: pos 0   (0.533 / 1.2)
    0.833f,  // L1: pos 30  (1.000 / 1.2)
    1.042f,  // L2: pos 45  (1.250 / 1.2)
    1.208f,  // L3: pos 55  (1.450 / 1.2)
    1.317f,  // L4: pos 65  (1.580 / 1.2)
    1.433f,  // L5: pos 75  (1.720 / 1.2)
    1.525f,  // L6: pos 85  (1.830 / 1.2)
    1.667f   // L7: pos 99  (2.000 / 1.2)
  };
  
  return frequencies[level];
}*/

/*
float keonGetStrokeFrequency(uint8_t level) {
  if (level > 7) level = 7;
  
  // TEST frequencies - 1:1 met Keon hardware
  const float frequencies[8] = {
    0.410f,  // L0: pos 0   (0.533 / 1.3)
    0.769f,  // L1: pos 30  (1.000 / 1.3)
    0.962f,  // L2: pos 45  (1.250 / 1.3)
    1.115f,  // L3: pos 55  (1.450 / 1.3)
    1.215f,  // L4: pos 65  (1.580 / 1.3)
    1.323f,  // L5: pos 75  (1.720 / 1.3)
    1.408f,  // L6: pos 85  (1.830 / 1.3)
    1.538f   // L7: pos 99  (2.000 / 1.3)
  };
  
  return frequencies[level];
}*/

/*
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
}*/
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
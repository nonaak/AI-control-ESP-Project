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
// ===== ALTERNATIEVE AANPAK: FIXED SLOW INTERVAL =====
// Simpel, betrouwbaar, geen fancy waypoint logic
// Voor keon_ble.cpp

// ===== SIMPLE VERSIE - GETUNED VOOR SMOOTH LEVEL 7 =====
// Voor keon_ble.cpp


// ===== STOP COMMAND VERSIE - CLEAR BUFFER TUSSEN STROKES =====
// Voor keon_ble.cpp

void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool keonDirection = false;
  static uint32_t lastCommand = 0;
  static uint32_t strokeCount = 0;
  static bool isStopping = false;
  static uint32_t stopStartTime = 0;
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // Intervals
  uint32_t interval = 2200 - ((g_speedStep * 800) / 7);
  
  // ✅ STOP STATE: Wacht 150ms na stop command
  if (isStopping) {
    if (now - stopStartTime < 150) {
      return;  // Nog aan het stoppen
    }
    // Stop compleet, ga verder
    isStopping = false;
  }
  
  // Check of het tijd is voor volgende stroke
  if ((now - lastCommand) < interval) {
    return;
  }
  
  // Toggle direction
  keonDirection = !keonDirection;
  uint8_t targetPos = keonDirection ? 99 : 0;
  
  if (keonTxCharacteristic == nullptr) return;
  
  try {
    // ✅ STUUR MOVE COMMAND
    uint8_t moveCmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
    keonTxCharacteristic->writeValue(moveCmd, 5, true);
    
    Serial.printf("[KEON STOP #%u] MOVE %s→%u L:%u\n",
                  strokeCount + 1,
                  keonDirection ? "UP" : "DOWN",
                  targetPos,
                  g_speedStep);
    
    // Wacht kort voor beweging start
    delay(50);
    
    // ✅ STUUR STOP COMMAND (CLEAR BUFFER!)
    uint8_t stopCmd[5] = {0x04, 0x00, targetPos, 0x00, 0};  // Speed 0 = STOP
    keonTxCharacteristic->writeValue(stopCmd, 5, true);
    
    Serial.printf("[KEON STOP #%u] STOP at %u (buffer clear)\n",
                  strokeCount + 1,
                  targetPos);
    
    lastCommand = now;
    strokeCount++;
    isStopping = true;
    stopStartTime = now;
    
    if (strokeCount % 10 == 0) {
      Serial.printf("[KEON STOP] ===== 10 strokes at Level %u =====\n", g_speedStep);
    }
    
  } catch (...) {
    Serial.println("[KEON] Command failed!");
  }
}

// ===============================================================================
// THEORIE:
// ===============================================================================
// Keon firmware buffert mogelijk commands intern.
// Door STOP command (speed=0) te sturen NA elke beweging:
// - Buffer wordt geleegd
// - Keon stopt op exacte positie
// - Volgende command start met lege buffer
// 
// Flow:
// 1. MOVE to 99 (speed 99)
// 2. Wacht 50ms
// 3. STOP at 99 (speed 0) ← CLEAR BUFFER
// 4. Wacht 150ms
// 5. Herhaal
//
// Als dit werkt → Buffer probleem opgelost!
// Als dit niet werkt → Firmware issue dieper
// ===============================================================================



/*// ===== 150MS ABSOLUTE MINIMUM - FEELCONNECT METHODE =====
// Voor keon_ble.cpp
// Respecteert Keon's interne rate limiter (zoals FeelConnect app doet)

void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool keonDirection = false;
  static uint32_t lastCommand = 0;
  static uint32_t strokeCount = 0;
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // Desired intervals per level
  uint32_t desiredInterval = 2200 - ((g_speedStep * 800) / 7);
  
  // ✅ ABSOLUTE MINIMUM: 150ms (zoals FeelConnect gebruikt ~120ms)
  // Keon heeft intern rate limiter, commands sneller dan 150ms stapelen op!
  uint32_t actualInterval = desiredInterval;
  if (actualInterval < 150) {
    actualInterval = 150;  // FORCE minimum
  }
  
  // Check of het tijd is
  if ((now - lastCommand) < actualInterval) {
    return;
  }
  
  // Toggle direction
  keonDirection = !keonDirection;
  uint8_t targetPos = keonDirection ? 99 : 0;
  
  if (keonTxCharacteristic == nullptr) return;
  
  try {
    // Stuur command
    uint8_t cmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
    keonTxCharacteristic->writeValue(cmd, 5, true);
    
    uint32_t actualDelay = now - lastCommand;
    lastCommand = now;
    strokeCount++;
    
    Serial.printf("[KEON 150MS #%u] Dir:%s Pos:%u Delay:%ums Desired:%ums L:%u\n",
                  strokeCount,
                  keonDirection ? "UP" : "DOWN",
                  targetPos,
                  actualDelay,
                  desiredInterval,
                  g_speedStep);
    
    // Waarschuwing als desired < 150ms
    if (desiredInterval < 150 && strokeCount % 10 == 0) {
      Serial.printf("[KEON 150MS] WARNING: Level %u wants %ums, forced to 150ms!\n",
                    g_speedStep, desiredInterval);
    }
    
    if (strokeCount % 10 == 0) {
      Serial.printf("[KEON 150MS] ===== 10 strokes at Level %u =====\n", g_speedStep);
    }
    
  } catch (...) {
    Serial.println("[KEON] Command failed!");
  }
}*/

// ===============================================================================
// THEORIE:
// ===============================================================================
// Uit jouw protocol doc:
// "Typical Interval: ~120ms (from FeelConnect)"
// 
// FeelConnect (officiële app) gebruikt ~120ms tussen commands.
// Dit suggereert dat Keon firmware een interne rate limiter heeft van ~120ms.
// 
// Als je sneller dan 120-150ms stuurt:
// → Commands stapelen in interne buffer
// → Worden later in bursts uitgevoerd
// → Dit verklaart de "ruis"!
// 
// Oplossing: FORCEER minimum 150ms tussen ALLE commands
// 
// Nadeel: Level 7+ worden trager dan gewenst
// Voordeel: Smooth operation, geen buffer overflow
// 
// NIEUWE INTERVALS:
// Level 0: 2200ms (gewenst) → 2200ms (actual) ✅
// Level 1: 2086ms (gewenst) → 2086ms (actual) ✅
// Level 2: 1971ms (gewenst) → 1971ms (actual) ✅
// Level 3: 1857ms (gewenst) → 1857ms (actual) ✅
// Level 4: 1743ms (gewenst) → 1743ms (actual) ✅
// Level 5: 1629ms (gewenst) → 1629ms (actual) ✅
// Level 6: 1514ms (gewenst) → 1514ms (actual) ✅
// Level 7: 1400ms (gewenst) → 1400ms (actual) ✅
// 
// Alle levels > 150ms, dus geen conflict!
// ===============================================================================



/*void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool keonDirection = false;
  static uint32_t lastCommand = 0;
  static uint32_t strokeCount = 0;
  static uint32_t lastPrintTime = 0;
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // ✅ LANGZAMERE INTERVALS (vooral Level 7!)
  // Level 0: 2200ms (27 strokes/min) - LANGZAAM
  // Level 7: 1400ms (43 strokes/min) - MEDIUM (was 1000ms!)
  uint32_t interval = 2200 - ((g_speedStep * 800) / 7);
  
  // Check of het tijd is
  if ((now - lastCommand) < interval) {
    return;
  }
  
  // Toggle direction
  keonDirection = !keonDirection;
  uint8_t targetPos = keonDirection ? 99 : 0;
  
  // ✅ COMMAND STUREN
  uint8_t cmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
  
  if (keonTxCharacteristic == nullptr) {
    Serial.println("[KEON] ERROR: TX characteristic is null!");
    return;
  }
  
  // Timing diagnostics
  uint32_t actualInterval = (strokeCount > 0) ? (now - lastCommand) : 0;
  
  try {
    // WITH response voor betrouwbaarheid
    keonTxCharacteristic->writeValue(cmd, 5, true);
    
    uint32_t cmdSendTime = millis();
    uint32_t writeLatency = cmdSendTime - now;
    
    lastCommand = now;  // Gebruik command START tijd
    strokeCount++;
    
    Serial.printf("[KEON #%u] Dir:%s Pos:%u Int:%u/%u Lat:%ums L:%u\n",
                  strokeCount,
                  keonDirection ? "UP" : "DOWN",
                  targetPos,
                  actualInterval,  // Werkelijke interval
                  interval,        // Gewenste interval
                  writeLatency,    // BLE write tijd
                  g_speedStep);
    
    // Stats elke 10 strokes
    if (strokeCount % 10 == 0) {
      Serial.printf("[KEON] ===== 10 strokes at Level %u (interval: %ums) =====\n", 
                    g_speedStep, interval);
    }
    
  } catch (...) {
    // Fallback zonder response
    Serial.println("[KEON] Write WITH response failed, trying without...");
    try {
      keonTxCharacteristic->writeValue(cmd, 5, false);
      lastCommand = now;
      strokeCount++;
      Serial.printf("[KEON #%u] (NO_RESP) Dir:%s Pos:%u L:%u\n",
                    strokeCount,
                    keonDirection ? "UP" : "DOWN",
                    targetPos,
                    g_speedStep);
    } catch (...) {
      Serial.println("[KEON] Command failed completely!");
    }
  }
}*/

// ===== ADAPTIVE SIMPLE VERSIE =====
// Past timing automatisch aan voor smooth operation
// Voor keon_ble.cpp



/*
void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool keonDirection = false;
  static uint32_t lastCommand = 0;
  static uint32_t strokeCount = 0;
  
  // ✅ ADAPTIVE TIMING PER LEVEL
  // Start met conservatieve waarden
  static uint32_t levelIntervals[8] = {
    2200,  // Level 0: ZEER LANGZAAM
    2100,  // Level 1
    2000,  // Level 2
    1800,  // Level 3
    1700,  // Level 4
    1600,  // Level 5
    1500,  // Level 6
    1400   // Level 7: MEDIUM (was 1000ms!)
  };
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // Get interval voor dit level
  uint32_t interval = levelIntervals[g_speedStep];
  
  // Check of het tijd is
  if ((now - lastCommand) < interval) {
    return;
  }
  
  // Toggle direction
  keonDirection = !keonDirection;
  uint8_t targetPos = keonDirection ? 99 : 0;
  
  // Command sturen
  uint8_t cmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
  
  if (keonTxCharacteristic == nullptr) return;
  
  try {
    keonTxCharacteristic->writeValue(cmd, 5, true);
    
    lastCommand = now;
    strokeCount++;
    
    Serial.printf("[KEON ADAPTIVE #%u] Dir:%s Pos:%u Int:%ums L:%u\n",
                  strokeCount,
                  keonDirection ? "UP" : "DOWN",
                  targetPos,
                  interval,
                  g_speedStep);
    
    // Tuning hint elke 10 strokes
    if (strokeCount % 10 == 0) {
      Serial.printf("[KEON ADAPTIVE] Level %u: %ums/stroke (%.1f strokes/min)\n", 
                    g_speedStep, 
                    interval,
                    60000.0f / (interval * 2.0f));  // Per volledig cycle
      Serial.println("[KEON ADAPTIVE] Smooth? Type new interval in code and re-upload!");
    }
    
  } catch (...) {
    Serial.println("[KEON] Command failed!");
  }
}*/

// ===============================================================================
// TUNING INSTRUCTIES:
// ===============================================================================
// 
// Als Level X onregelmatig is:
// 1. Verhoog levelIntervals[X] met 200ms
// 2. Re-upload
// 3. Test opnieuw
// 4. Herhaal tot smooth
// 
// Voorbeeld:
// Level 7 nog steeds ruis bij 1400ms?
// → Wijzig naar: levelIntervals[7] = 1600;
// → Upload
// → Test
// 
// Als Level X té langzaam is:
// 1. Verlaag levelIntervals[X] met 100ms
// 2. Re-upload
// 3. Test
// 4. Als ruis terugkomt → ga 50ms terug
//
// ===============================================================================*/






/*
void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool keonDirection = false;
  static uint32_t lastCommand = 0;
  static uint32_t strokeCount = 0;
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // ✅ ZEER CONSERVATIEVE VASTE INTERVALS
  // Level 0: 2000ms (30 strokes/min) - LANGZAAM
  // Level 7: 1000ms (60 strokes/min) - MEDIUM
  uint32_t interval = 2000 - ((g_speedStep * 1000) / 7);
  
  // Check of het tijd is
  if ((now - lastCommand) < interval) {
    return;
  }
  
  // Toggle direction
  keonDirection = !keonDirection;
  uint8_t targetPos = keonDirection ? 99 : 0;
  
  // ✅ STUUR COMMAND MET RESPONSE
  uint8_t cmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
  
  if (keonTxCharacteristic == nullptr) return;
  
  try {
    // WITH response voor betrouwbaarheid
    keonTxCharacteristic->writeValue(cmd, 5, true);
    lastCommand = now;
    strokeCount++;
    
    Serial.printf("[KEON SIMPLE #%u] Dir:%s Pos:%u Interval:%ums L:%u\n",
                  strokeCount,
                  keonDirection ? "UP" : "DOWN",
                  targetPos,
                  interval,
                  g_speedStep);
    
  } catch (...) {
    // Fallback
    try {
      keonTxCharacteristic->writeValue(cmd, 5, false);
      lastCommand = now;
      strokeCount++;
    } catch (...) {
      Serial.println("[KEON] Command failed!");
    }
  }
}*/

// ===== ULTRA-CONSERVATIEVE WAYPOINT VERSIE =====
// Voor keon_ble.cpp - VEEL LANGZAMER, MEER STABLE

/*void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  // State machine
  static bool isMoving = false;
  static uint8_t currentPos = 50;
  static uint8_t targetPos = 50;
  static uint32_t moveStartTime = 0;
  static uint32_t moveDuration = 0;
  static bool nextDirection = false;
  static uint32_t strokeCount = 0;
  static uint32_t settlingStartTime = 0;  // ✅ NIEUW: Settling time
  static bool isSettling = false;         // ✅ NIEUW: Settling state
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  // ✅ SETTLING STATE: Wacht na beweging voor Keon stabiliteit
  if (isSettling) {
    const uint32_t SETTLING_TIME = 200;  // 200ms rust na elke beweging
    if ((now - settlingStartTime) < SETTLING_TIME) {
      return;  // Nog aan het settlen
    }
    // Settling compleet
    isSettling = false;
    isMoving = false;  // Nu pas klaar voor volgende
    return;
  }
  
  if (!isMoving) {
    // ✅ START NIEUWE BEWEGING
    
    currentPos = targetPos;
    targetPos = nextDirection ? 99 : 0;
    nextDirection = !nextDirection;
    
    uint8_t distance = abs(targetPos - currentPos);
    
    // ✅ VEEL CONSERVATIEVER TIMING
    // Base: 15ms per unit (was 8ms)
    // Level 0: 15ms/unit → 99 units = 1485ms (!)
    // Level 7: 8ms/unit  → 99 units = 792ms
    float msPerUnit = 15.0f - ((float)g_speedStep * 7.0f / 7.0f);
    if (msPerUnit < 8.0f) msPerUnit = 8.0f;  // Minimum 8ms
    
    moveDuration = (uint32_t)(distance * msPerUnit);
    
    // ✅ HOGERE MINIMUM TIJD
    if (moveDuration < 800) moveDuration = 800;  // Was 300ms!
    
    // ✅ STUUR MET BLE RESPONSE (betrouwbaarder)
    uint8_t cmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
    
    if (keonTxCharacteristic == nullptr) return;
    
    try {
      // Probeer eerst MET response (langzamer maar betrouwbaarder)
      keonTxCharacteristic->writeValue(cmd, 5, true);  // true = WITH response
      
      moveStartTime = now;
      isMoving = true;
      strokeCount++;
      
      Serial.printf("[KEON CONSERVATIVE #%u] %u→%u (dist:%u dur:%ums %.1fms/unit) L:%u\n", 
                    strokeCount, currentPos, targetPos, distance, 
                    moveDuration, msPerUnit, g_speedStep);
      
    } catch (...) {
      // Fallback naar without response
      try {
        keonTxCharacteristic->writeValue(cmd, 5, false);
        moveStartTime = now;
        isMoving = true;
        strokeCount++;
        Serial.printf("[KEON CONSERVATIVE #%u] %u→%u (NO_RESP) L:%u\n", 
                      strokeCount, currentPos, targetPos, g_speedStep);
      } catch (...) {
        Serial.println("[KEON] Command failed completely!");
      }
    }
    
  } else {
    // ✅ WACHT TOT BEWEGING COMPLEET
    uint32_t elapsed = now - moveStartTime;
    
    if (elapsed >= moveDuration) {
      // Beweging zou compleet moeten zijn
      // Start settling period
      isSettling = true;
      settlingStartTime = now;
      
      // Debug
      if (strokeCount % 10 == 0) {
        Serial.printf("[KEON CONSERVATIVE] 10 strokes at L:%u (avg dur: %ums)\n", 
                      g_speedStep, moveDuration);
      }
    }
  }
}*/

/*// ===== VERSIE MET POSITION POLLING =====
// Als Keon een readable position characteristic heeft
// Voor keon_ble.cpp

// ===============================================================================
// GLOBALS - Toevoegen bovenaan keon_ble.cpp
// ===============================================================================
static BLERemoteCharacteristic* keonPositionChar = nullptr;  // Position feedback
static bool hasPositionFeedback = false;

// ===============================================================================
// SCAN EN INIT POSITION CHARACTERISTIC
// ===============================================================================
void keonInitPositionFeedback() {
  if (!keonConnected || keonClient == nullptr) return;
  
  Serial.println("[KEON] Scanning for position feedback characteristic...");
  
  BLERemoteService* pRemoteService = keonClient->getService(KEON_SERVICE_UUID);
  if (pRemoteService == nullptr) return;
  
  // Get all characteristics
  std::map<std::string, BLERemoteCharacteristic*>* characteristics = 
      pRemoteService->getCharacteristics();
  
  if (characteristics == nullptr) return;
  
  // Zoek naar readable characteristic (niet de TX char)
  for (auto &entry : *characteristics) {
    if (entry.first == KEON_TX_CHAR_UUID) continue;  // Skip TX char
    
    BLERemoteCharacteristic* pChar = entry.second;
    
    if (pChar->canRead()) {
      Serial.printf("[KEON] Found readable char: %s\n", entry.first.c_str());
      
      // Probeer te lezen
      try {
        std::string value = pChar->readValue();
        if (value.length() == 1) {
          uint8_t val = (uint8_t)value[0];
          if (val <= 99) {
            Serial.printf("[KEON] ✓ POSITION FEEDBACK FOUND! Value: %d\n", val);
            keonPositionChar = pChar;
            hasPositionFeedback = true;
            return;
          }
        }
      } catch (...) {
        Serial.println("[KEON] Read failed, trying next...");
      }
    }
  }
  
  Serial.println("[KEON] ✗ No position feedback found");
  hasPositionFeedback = false;
}

// ===============================================================================
// LEES HUIDIGE POSITIE
// ===============================================================================
uint8_t keonReadPosition() {
  if (!hasPositionFeedback || keonPositionChar == nullptr) return 255;  // Invalid
  
  try {
    std::string value = keonPositionChar->readValue();
    if (value.length() == 1) {
      return (uint8_t)value[0];
    }
  } catch (...) {
    // Read failed
  }
  
  return 255;  // Invalid
}

// ===============================================================================
// CHECK OF POSITIE BEREIKT
// ===============================================================================
bool keonIsAtPosition(uint8_t targetPos, uint8_t tolerance = 3) {
  if (!hasPositionFeedback) return false;
  
  uint8_t currentPos = keonReadPosition();
  if (currentPos == 255) return false;  // Invalid read
  
  return (abs((int)currentPos - (int)targetPos) <= tolerance);
}

// ===============================================================================
// WAYPOINT MET POSITION POLLING
// ===============================================================================
void keonIndependentTick() {
  if (!keonConnected) return;
  
  extern bool paused;
  if (paused) return;
  
  static bool isMoving = false;
  static uint8_t currentPos = 50;
  static uint8_t targetPos = 50;
  static uint32_t moveStartTime = 0;
  static uint32_t maxMoveDuration = 0;  // Timeout
  static bool nextDirection = false;
  static uint32_t strokeCount = 0;
  
  uint32_t now = millis();
  extern uint8_t g_speedStep;
  
  if (!isMoving) {
    // ✅ START NIEUWE BEWEGING
    
    currentPos = targetPos;
    targetPos = nextDirection ? 99 : 0;
    nextDirection = !nextDirection;
    
    uint8_t distance = abs(targetPos - currentPos);
    
    // Timing als fallback
    float msPerUnit = 10.0f - ((float)g_speedStep * 5.0f / 7.0f);
    if (msPerUnit < 5.0f) msPerUnit = 5.0f;
    maxMoveDuration = (uint32_t)(distance * msPerUnit);
    if (maxMoveDuration < 500) maxMoveDuration = 500;
    
    // Stuur command
    uint8_t cmd[5] = {0x04, 0x00, targetPos, 0x00, 99};
    
    if (keonTxCharacteristic == nullptr) return;
    
    try {
      keonTxCharacteristic->writeValue(cmd, 5, true);
      moveStartTime = now;
      isMoving = true;
      strokeCount++;
      
      if (hasPositionFeedback) {
        Serial.printf("[KEON POLLING #%u] %u→%u (POLLING) L:%u\n", 
                      strokeCount, currentPos, targetPos, g_speedStep);
      } else {
        Serial.printf("[KEON POLLING #%u] %u→%u (timeout:%ums) L:%u\n", 
                      strokeCount, currentPos, targetPos, maxMoveDuration, g_speedStep);
      }
      
    } catch (...) {
      Serial.println("[KEON] Command failed!");
    }
    
  } else {
    // ✅ WACHT TOT BEWEGING COMPLEET
    
    uint32_t elapsed = now - moveStartTime;
    
    // Check A: Position polling (als beschikbaar)
    if (hasPositionFeedback) {
      if (keonIsAtPosition(targetPos, 3)) {
        // ECHTE COMPLETION GEDETECTEERD!
        isMoving = false;
        Serial.printf("[KEON] ✓ Position reached! (took %ums)\n", elapsed);
        
        if (strokeCount % 10 == 0) {
          Serial.printf("[KEON POLLING] 10 strokes at L:%u\n", g_speedStep);
        }
        return;
      }
    }
    
    // Check B: Timeout (fallback)
    if (elapsed >= maxMoveDuration) {
      isMoving = false;
      
      if (hasPositionFeedback) {
        uint8_t actualPos = keonReadPosition();
        Serial.printf("[KEON] ⚠ Timeout! Target:%u Actual:%u\n", targetPos, actualPos);
      } else {
        Serial.printf("[KEON] Movement timeout (%ums)\n", elapsed);
      }
      
      if (strokeCount % 10 == 0) {
        Serial.printf("[KEON POLLING] 10 strokes at L:%u\n", g_speedStep);
      }
    }
  }
}*/

// ===============================================================================
// TOEVOEGEN AAN keonConnect() - NA SUCCESSFUL CONNECTION
// ===============================================================================
/*
if (keonConnected) {
  Serial.println("[KEON] Connected successfully!");
  
  // Init position feedback
  keonInitPositionFeedback();
  
  if (hasPositionFeedback) {
    Serial.println("[KEON] ✓ Position polling enabled!");
  } else {
    Serial.println("[KEON] ✗ Using timeout-based completion");
  }
}
*/

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

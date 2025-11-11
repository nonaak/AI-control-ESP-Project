#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>

// ===============================================================================
// KEON BLE PROTOCOL CONSTANTS
// ===============================================================================

// Keon MAC address (configureren met jouw device)
#define KEON_MAC_ADDRESS "ac:67:b2:25:42:5a"

// BLE Service & Characteristic UUIDs (reverse-engineered via Wireshark)
#define KEON_SERVICE_UUID "00001900-0000-1000-8000-00805f9b34fb"
#define KEON_TX_CHAR_UUID "00001902-0000-1000-8000-00805f9b34fb"

// ===============================================================================
// KEON COMMAND STRUCTURE
// ===============================================================================

// Move command: [0x04][0x00][POSITION][0x00][SPEED]
// Position: 0-99 (0=bottom, 99=top, 4-5cm stroke)
// Speed: 0-99 (0=stopped, 99=maximum speed)
struct KeonMoveCommand {
  uint8_t cmd = 0x04;         // Command ID for movement
  uint8_t reserved1 = 0x00;
  uint8_t position = 50;      // 0-99 (maps to animation positions)
  uint8_t reserved2 = 0x00;
  uint8_t speed = 50;         // 0-99 (maps to animation speed)
  
  uint8_t* getBytes() {
    static uint8_t data[5];
    data[0] = cmd;
    data[1] = reserved1;
    data[2] = position;
    data[3] = reserved2;
    data[4] = speed;
    return data;
  }
};

// Stop command
struct KeonStopCommand {
  uint8_t cmd = 0x00;  // Stop command
  
  uint8_t* getBytes() {
    static uint8_t data[1];
    data[0] = cmd;
    return data;
  }
};

// ===============================================================================
// CONNECTION STATE & TIMING
// ===============================================================================

// Connection parameters
#define KEON_CONNECTION_TIMEOUT_MS 5000
#define KEON_COMMAND_DELAY_MS 200      // Minimum delay between commands
#define KEON_RECONNECT_INTERVAL_MS 5000

// ===============================================================================
// GLOBAL STATE VARIABLES
// ===============================================================================

extern BLEClient* keonClient;
extern BLERemoteCharacteristic* keonTxCharacteristic;
extern bool keonConnected;
extern uint8_t keonCurrentPosition;
extern uint8_t keonCurrentSpeed;

// ===============================================================================
// FUNCTION DECLARATIONS
// ===============================================================================

// Connection management
void keonInit();
bool keonConnect();
void keonDisconnect();
bool keonIsConnected();

// Movement control
bool keonMove(uint8_t position, uint8_t speed);
bool keonMoveSlow(uint8_t position);       // 33% speed
bool keonMoveMedium(uint8_t position);     // 66% speed
bool keonMoveFast(uint8_t position);       // 99% speed
bool keonStop();
void keonParkToBottom();                   // Park at bottom (pause state)

// Animation synchronization
// Convert HoofdESP animation state to Keon position/speed
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp);

// Connection state check
void keonCheckConnection();
void keonReconnect();

// Debug & utility
void keonPrintStatus();

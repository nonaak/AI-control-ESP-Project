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
bool keonMoveSlow(uint8_t position);
bool keonMoveMedium(uint8_t position);
bool keonMoveFast(uint8_t position);
bool keonStop();
void keonParkToBottom();

// Animation synchronization
// Sync Keon to real sleeve position from animation
//void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, float sleevePercentage);
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp);

// Connection state check (call this in loop!)
void keonCheckConnection();
void keonReconnect();

// MAC address management
String keonGetLastMAC();
void keonSetMAC(const char* mac);

// Debug & utility
void keonPrintStatus();

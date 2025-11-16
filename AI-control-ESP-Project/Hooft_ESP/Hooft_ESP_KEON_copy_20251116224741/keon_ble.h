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

// ===============================================================================
// 🚀 NIEUWE ONAFHANKELIJKE KEON CONTROL
// ===============================================================================

/**
 * ✅ KEON INDEPENDENT TICK - 100% ONAFHANKELIJK VAN ANIMATIE!
 * 
 * Deze functie maakt de Keon COMPLEET los van de animatie.
 * 
 * FEATURES:
 * - Eigen timing gebaseerd op speedStep (0-7)
 * - Eigen direction toggle (NIET van animatie!)
 * - Level 0 = langzaam (1000ms = 60 strokes/min)
 * - Level 7 = snel (200ms = 300 strokes/min)
 * - Volle strokes altijd (0 ↔ 99)
 * - GEEN ruis meer van animatie!
 * 
 * GEBRUIK:
 * - Roep aan vanuit uiTick() elke frame
 * - Gebruikt g_speedStep (van Nunchuk OF Body ESP)
 * - Stopt automatisch bij paused = true
 * 
 * PRIORITEIT:
 * - Nunchuk joystick → g_speedStep → Keon (HOOGSTE)
 * - Body ESP AI → g_speedStep → Keon (als Nunchuk idle)
 * - Animatie → GEEN invloed op Keon!
 */
void keonIndependentTick();

// ===============================================================================
// OUDE FUNCTIE - BEHOUDEN VOOR BODY ESP AI CONTROL
// ===============================================================================

/**
 * ⚠️ LEGACY: Sync Keon with animation OR Body ESP stress levels
 * 
 * GEBRUIKT DOOR:
 * - espnow_comm.cpp -> syncKeonToStressLevel() 
 * - Body ESP AI control via stress levels
 * 
 * NIET VERWIJDEREN - Body ESP heeft dit nodig!
 * 
 * Parameters:
 * - speedStep: Current speed level (0-7)
 * - speedSteps: Total speed levels (should be 8)
 * - isMovingUp: Direction (true = up, false = down)
 */
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp);

// ===============================================================================
// CONNECTION STATE & UTILITY
// ===============================================================================

// Connection state check (call this in loop!)
void keonCheckConnection();
void keonReconnect();

// MAC address management
String keonGetLastMAC();
void keonSetMAC(const char* mac);

// Debug & utility
void keonPrintStatus();

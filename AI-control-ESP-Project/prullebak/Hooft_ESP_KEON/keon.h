#pragma once
#include <Arduino.h>
#include <BLEDevice.h>

// ===============================================================================
// KEON BLE CONFIGURATION
// ===============================================================================

// Keon MAC address (update with your device's MAC)
#define KEON_MAC_ADDRESS "f1:e0:12:a3:75:cd"

// Keon BLE service and characteristic UUIDs
#define KEON_SERVICE_UUID "88f80580-0000-01e6-aace-0002a5d5c51b"
#define KEON_TX_CHAR_UUID "88f80581-0000-01e6-aace-0002a5d5c51b"

// Timing constants
#define KEON_COMMAND_DELAY_MS 200       // Delay between BLE commands (rate limiting only)
#define KEON_CONNECTION_TIMEOUT_MS 5000 // Connection attempt timeout

// ===============================================================================
// GLOBAL STATE
// ===============================================================================

extern bool keonConnected;
extern uint8_t keonCurrentPosition;
extern uint8_t keonCurrentSpeed;

// ===============================================================================
// INITIALIZATION
// ===============================================================================

/**
 * Initialize Keon BLE system
 * Must be called once during setup()
 */
void keonInit();

// ===============================================================================
// CONNECTION MANAGEMENT
// ===============================================================================

/**
 * Connect to Keon via BLE
 * WARNING: BLOCKING function! Takes 500ms-1s to complete
 * Only call from user-triggered actions (menu), NOT from loop()!
 * 
 * @return true if connection successful, false otherwise
 */
bool keonConnect();

/**
 * Disconnect from Keon
 * Sends stop command before disconnecting
 */
void keonDisconnect();

/**
 * Check if Keon is currently connected
 * 
 * @return true if connected, false otherwise
 */
bool keonIsConnected();

/**
 * Check connection state (ultra-lightweight)
 * NO AUTO-RECONNECT! Manual reconnect only via menu
 * Safe to call from loop() - takes <1ms
 */
void keonCheckConnection();

/**
 * Manual reconnect attempt
 * WARNING: BLOCKING function!
 */
void keonReconnect();

// ===============================================================================
// MOVEMENT CONTROL
// ===============================================================================

/**
 * Move Keon to position with speed
 * 
 * @param position Target position (0-99, 0=bottom, 99=top)
 * @param speed Movement speed (0-99, 0=stopped, 99=max)
 * @return true if command sent successfully
 */
bool keonMove(uint8_t position, uint8_t speed);

/**
 * Stop at specific position
 * 
 * @param position Position to stop at (0-99)
 * @return true if command sent successfully
 */
bool keonStopAtPosition(uint8_t position);

/**
 * Stop at current position
 * 
 * @return true if command sent successfully
 */
bool keonStop();

/**
 * Convenience functions for predefined speeds
 */
bool keonMoveSlow(uint8_t position);    // 33% speed
bool keonMoveMedium(uint8_t position);  // 66% speed
bool keonMoveFast(uint8_t position);    // 99% speed

// ===============================================================================
// ANIMATION SYNCHRONIZATION
// ===============================================================================

/**
 * Park Keon at bottom when animation paused
 * Called when C button triggers pause
 */
void keonParkToBottom();

/**
 * Sync Keon movement with HoofdESP animation
 * 
 * Velocity/Direction Mapping:
 * - isMovingUp = true  (velEMA < 0, moving UP)    → Keon naar BOVEN (99)
 * - isMovingUp = false (velEMA >= 0, moving DOWN) → Keon naar BENEDEN (0)
 * - Speed follows HoofdESP animation speed step (0-7 → 0-99)
 * 
 * @param speedStep Current animation speed step (0-7)
 * @param speedSteps Total speed steps (typically 8)
 * @param isMovingUp Animation direction (true=up, false=down)
 */
void keonSyncToAnimation(uint8_t speedStep, uint8_t speedSteps, bool isMovingUp);

// ===============================================================================
// DEBUG & UTILITY
// ===============================================================================

/**
 * Print Keon status information to serial
 */
void keonPrintStatus();

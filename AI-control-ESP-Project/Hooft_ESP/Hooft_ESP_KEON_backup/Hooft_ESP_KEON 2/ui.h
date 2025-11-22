#pragma once
#include <Arduino.h>

// ========== FORWARD DECLARATIONS ==========
void uiInit();
void uiTick();

// ========== GLOBAL STATE (exposed for ESP-NOW sync) ==========
extern bool paused;
extern float phase;
extern bool arrowFull;
extern bool vibeState;

// ========== SPEED CONTROL (exposed for ESP-NOW) ==========
extern uint8_t g_speedStep;  // Current speed step (0-7)

// ========== KEON STATE (exposed for connection popup) ==========
extern bool keonConnected;
extern bool solaceConnected;
extern bool motionConnected;
extern bool bodyConnected;

// ========== SMERING (exposed for vacuum.cpp) ==========
extern uint16_t g_targetStrokes;
extern float g_lubeHold_s;
extern float g_startLube_s;

// ========== HELPER FUNCTIONS (exposed for ESP-NOW) ==========
float getSleevePercentage();  // Get current sleeve position as percentage (0-100)
void resetPauseState();       // Reset pause state after Body ESP resume command
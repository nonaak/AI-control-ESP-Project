#pragma once
#include <Arduino.h>

void uiInit();
void uiTick();

// Global variables accessible by ESP-NOW (declared in ui.cpp)
extern uint8_t g_speedStep;      // Current speed step (0 to SPEED_STEPS-1)
extern uint16_t g_targetStrokes; // Target stroke count
extern float g_lubeHold_s;       // Lube hold time in seconds
extern float g_startLube_s;      // Start-Lubric time in seconds

// Device connection status
extern bool keonConnected;
extern bool solaceConnected;
extern bool motionConnected;
extern bool bodyConnected;

// Pause status (C-knop)
extern bool paused;

// Sleeve synchronisatie
extern float phase;  // Animatie fase voor sleeve berekening
float getSleevePercentage();

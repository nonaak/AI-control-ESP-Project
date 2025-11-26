#pragma once
#include <Arduino.h>
class Arduino_GFX;

// System settings menu events
// System settings menu events
enum SystemSettingsEvent : uint8_t { 
  SSE_NONE=0, 
  SSE_BACK,              // Terug naar hoofdmenu
  SSE_ROTATE_SCREEN,     // Scherm 180° draaien
  SSE_COLORS,            // Kleurinstellingen (placeholder voor toekomst)
  SSE_FORMAT_SD,         // Format SD kaart
  SSE_TOGGLE_MENU,       // Toggle touch menu buttons
  SSE_TOGGLE_PARAMS,     // Toggle touch parameters
  SSE_TOGGLE_EMERGENCY   // Toggle touch emergency pause
};

// System settings menu interface
void systemSettings_begin(Arduino_GFX* gfx);
SystemSettingsEvent systemSettings_poll();
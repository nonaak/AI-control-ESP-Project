#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// System settings menu events
enum SystemSettingsEvent : uint8_t { 
  SSE_NONE=0, 
  SSE_BACK,           // Terug naar hoofdmenu
  SSE_ROTATE_SCREEN,  // Scherm 180° draaien
  SSE_COLORS,         // Kleurinstellingen (placeholder voor toekomst)
  SSE_FORMAT_SD       // Format SD kaart
};

// System settings menu interface
void systemSettings_begin(TFT_eSPI* gfx);
SystemSettingsEvent systemSettings_poll();
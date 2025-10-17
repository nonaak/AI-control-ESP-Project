#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// AI Event Config events
enum EventConfigEvent : uint8_t { 
  ECE_NONE=0, 
  ECE_BACK,         // Terug naar AI Overrule menu
  ECE_SAVE,         // Opslaan configuratie
  ECE_RESET,        // Reset naar defaults
  ECE_EDIT_EVENT,   // Bewerk gebeurtenis naam
  ECE_NEXT_PAGE,    // Volgende pagina events
  ECE_PREV_PAGE     // Vorige pagina events
};

// Event configuratie structuur (opgeslagen in EEPROM)
struct AIEventConfig {
  char eventNames[8][64];  // Max 8 events, 64 chars per naam
  uint32_t magic;          // Validatie magic number
};

// AI Event Config interface
void aiEventConfig_begin(TFT_eSPI* gfx);
EventConfigEvent aiEventConfig_poll();
void aiEventConfig_loadFromEEPROM();
void aiEventConfig_saveToEEPROM();
void aiEventConfig_resetToDefaults();

// Externe toegang tot configuratie
extern AIEventConfig aiEventConfig;
const char* aiEventConfig_getName(uint8_t eventType);
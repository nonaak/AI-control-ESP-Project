#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Overrule menu events
enum OverruleEvent : uint8_t { 
  OE_NONE=0, 
  OE_BACK,          // Terug naar menu
  OE_TOGGLE_AI,     // AI overrule aan/uit
  OE_SAVE,          // Settings opslaan
  OE_RESET,         // Reset naar defaults
  OE_ANALYZE,       // AI Data Analyse
  OE_CONFIG         // AI Event Configuratie
};

// Overrule menu interface
void overrule_begin(TFT_eSPI* gfx);
OverruleEvent overrule_poll();

// AI overrule functions
void toggleAIOverrule();
bool isAIOverruleEnabled();
void saveMLAutonomyToConfig();

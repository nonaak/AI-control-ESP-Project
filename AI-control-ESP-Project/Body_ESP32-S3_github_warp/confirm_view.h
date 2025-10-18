#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Confirmation dialog events
enum ConfirmEvent : uint8_t { 
  CONF_NONE=0, 
  CONF_YES,      // Ja gekozen
  CONF_NO        // Nee gekozen
};

// Confirmation dialog interface
void confirm_begin(TFT_eSPI* gfx, const char* title, const char* message);
ConfirmEvent confirm_poll();
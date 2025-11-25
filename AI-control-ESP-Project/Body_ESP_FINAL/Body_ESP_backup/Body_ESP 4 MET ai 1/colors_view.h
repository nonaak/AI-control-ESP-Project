#pragma once
#include <Arduino.h>
class Arduino_GFX;

// Color settings events
enum ColorEvent : uint8_t { 
  CE_NONE=0, 
  CE_BACK,          // Terug naar system settings
  CE_COLOR_CHANGE   // Kleur is gewijzigd
};

// Color settings interface
void colors_begin(Arduino_GFX* gfx);
ColorEvent colors_poll();
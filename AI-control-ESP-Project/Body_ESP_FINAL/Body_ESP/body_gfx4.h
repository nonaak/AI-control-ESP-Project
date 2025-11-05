#pragma once
#include <Arduino.h>
#include "body_config.h"

class Arduino_GFX; 
extern Arduino_GFX *body_gfx;

// 🔥 NIEUW: Rendering control flag (voor popup pause)
extern volatile bool g4_pauseRendering;

enum { G4_HART=0, G4_HUID=1, G4_TEMP=2, G4_ADEMHALING=3, /* G4_OXY=4, */ G4_HOOFDESP=4, G4_ZUIGEN=5, G4_TRIL=6, G4_NUM=7 };  // OXY verwijderd

// Body GFX4 interface with HoofdESP styling
void body_gfx4_begin();
void body_gfx4_clear();
void body_gfx4_setLabel(uint8_t ch, const char* name);
void body_gfx4_pushSample(uint8_t ch, float value, bool mark=false);
void body_gfx4_drawButtons(bool recording, bool playing, bool menuActive, bool overruleActive, bool recDisabled=false);
void body_gfx4_drawStatus(const char* text);

// Touch cooldown management
void body_gfx4_setCooldown(uint32_t mainScreenMs, uint32_t menuMs);
bool body_gfx4_canTouch(bool isMainScreen);
void body_gfx4_registerTouch(bool isMainScreen);
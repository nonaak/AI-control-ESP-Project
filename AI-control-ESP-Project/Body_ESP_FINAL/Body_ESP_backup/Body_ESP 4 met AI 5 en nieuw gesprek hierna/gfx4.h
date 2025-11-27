#pragma once
#include <Arduino.h>
class Arduino_GFX; extern Arduino_GFX *gfx;

enum { G4_HART=0, G4_TEMP=1, G4_HUID=2, G4_OXY=3, G4_MACHINE=4, G4_NUM=5 };

void gfx4_begin();
void gfx4_clear();
void gfx4_setLabel(uint8_t ch, const char* name);
void gfx4_pushSample(uint8_t ch, float value, bool mark=false);
void gfx4_drawButtons(bool recording, bool playing, bool menuActive, bool overruleActive, bool recDisabled=false);
void gfx4_drawStatus(const char* text);

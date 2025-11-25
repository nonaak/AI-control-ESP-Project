#pragma once
#include <Arduino.h>
class Arduino_GFX;

enum CalEvent : uint8_t { CE_NONE=0, CE_BACK };

void cal_begin(Arduino_GFX* gfx);
CalEvent cal_poll();

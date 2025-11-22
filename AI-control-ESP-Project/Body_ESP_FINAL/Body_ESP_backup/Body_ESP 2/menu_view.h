#pragma once
#include <Arduino.h>
class Arduino_GFX;

enum MenuEvent : uint8_t { ME_NONE=0, ME_SENSOR_SETTINGS, ME_OVERRULE, ME_SYSTEM_SETTINGS, ME_PLAYLIST, ME_ML_TRAINING, ME_BACK };

void menu_begin(Arduino_GFX* gfx);
MenuEvent menu_poll();

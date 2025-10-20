#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

enum MenuEvent : uint8_t { ME_NONE=0, ME_SENSOR_SETTINGS, ME_OVERRULE, ME_SYSTEM_SETTINGS, ME_PLAYLIST, ME_ML_TRAINING, ME_BACK };

void menu_begin(TFT_eSPI* gfx);
MenuEvent menu_poll();

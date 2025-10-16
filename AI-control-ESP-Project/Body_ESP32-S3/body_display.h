#pragma once
#include <Arduino.h>
#include "TFT_eSPI.h"  // T-HMI gebruikt TFT_eSPI in plaats van Arduino_GFX
#include "body_fonts.h"
#include "body_config.h"

// T-HMI display objecten (direct TFT_eSPI gebruik) 
class Arduino_GFX;  // Forward declaration
extern TFT_eSPI tft;  // Main display object from .ino
#define body_gfx ((TFT_eSPI*)(&tft))  // Direct TFT_eSPI gebruik

// T-HMI pinout (pin definities zijn al in main .ino)

// Geometrie linker/rechter paneel
extern const int L_PANE_W, L_PANE_H, L_PANE_X, L_PANE_Y;
extern const int L_PAD, L_HEADER_H;
extern const int L_CANVAS_W, L_CANVAS_H, L_CANVAS_X, L_CANVAS_Y;
extern const int R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H;

// API voor Body UI
void drawBodySpeedBarTop(const char* title);
void drawLeftFrame();
void drawBodySensorHeader(float heartRate, float temperature);

// Body sensor weergave helpers
void drawHeartRateGraph(float currentHR, float* history, int historySize);
void drawTemperatureBar(float temp, float minTemp, float maxTemp);
void drawGSRIndicator(float gsrLevel, float maxLevel);
void drawAIOverruleStatus(bool active, float trustOverride, float sleeveOverride);
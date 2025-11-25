#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "body_fonts.h"
#include "body_config.h"

// Externe GFX objecten voor SC01 Plus (RGB Parallel)
extern Arduino_GFX    *body_gfx;  // Generic type for compatibility
extern Arduino_Canvas *body_cv;

// SC01 Plus display specs (geen aparte pins nodig voor RGB parallel)
#define GFX_BL 45   // Backlight pin

// Screen dimensions voor SC01 Plus (480x320)
extern const int SCR_W;  // 480
extern const int SCR_H;  // 320

// Geometrie linker/rechter paneel (aangepast voor grotere resolutie)
extern const int L_PANE_W, L_PANE_H, L_PANE_X, L_PANE_Y;
extern const int L_PAD, L_HEADER_H;
extern const int L_CANVAS_W, L_CANVAS_H, L_CANVAS_X, L_CANVAS_Y;
extern const int R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H;

// Backlight control
void setBacklight(uint8_t brightness);

// API voor Body UI
void drawBodySpeedBarTop(const char* title);
void drawLeftFrame();
void drawBodySensorHeader(float heartRate, float temperature);

// Body sensor weergave helpers
void drawHeartRateGraph(float currentHR, float* history, int historySize);
void drawTemperatureBar(float temp, float minTemp, float maxTemp);
void drawGSRIndicator(float gsrLevel, float maxLevel);
void drawAIOverruleStatus(bool active, float trustOverride, float sleeveOverride);
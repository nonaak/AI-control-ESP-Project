#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "body_fonts.h"
#include "body_config.h"

// Externe GFX objecten
extern Arduino_GFX     *body_gfx;
extern Arduino_Canvas  *body_cv;

// Pinout/rotatiewaarden voor CYD display
extern const int LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO, LCD_RST, LCD_ROT, LCD_BL;

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
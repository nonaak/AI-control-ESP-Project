#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <canvas/Arduino_Canvas.h>
#include "fonts.h"
#include "config.h"

// Externe GFX objecten
extern Arduino_GFX     *gfx;
extern Arduino_Canvas  *cv;

// Pinout/rotatiewaarden worden in display.cpp gezet
extern const int LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO, LCD_RST, LCD_ROT, LCD_BL;

// Geometrie linker/rechter paneel (waardes in display.cpp)
extern const int L_PANE_W, L_PANE_H, L_PANE_X, L_PANE_Y;
extern const int L_PAD, L_HEADER_H;
extern const int L_CANVAS_W, L_CANVAS_H, L_CANVAS_X, L_CANVAS_Y;
extern const int R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H;

// API voor UI
void drawSpeedBarTop(uint8_t step, uint8_t stepsTotal);
void drawLeftFrame();
void drawVacArrowHeader(bool filledUp);

// Animatie-/shape helpers die UI gebruikt
void drawSleeveFixedTop(int capY, uint16_t colTan);
void drawRodFromCap_Vinside_NoSeam(int capY, int drawBaselineY, uint16_t rodCol, uint16_t edgeCol);

// Vibe lightning effects
void drawVibeLightning(bool leftSide);
void drawZigzagSegment(int x1, int y1, int x2, int y2, uint16_t mainColor, uint16_t glowColor);

// Suction symbol effects
void drawSuctionSymbol(bool leftSide);

// External states
extern bool vibeState;
extern bool suctionState;

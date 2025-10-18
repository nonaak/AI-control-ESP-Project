#include "system_settings_view.h"
//#include <Arduino_GFX_Library.h>
#include "TFT_eSPI.h"
#include "view_colors.h"
#include "input_touch.h"

//#include "TFT_eSPI.h"

// TFT_eSPI color definitions (vervangen Arduino_GFX)
//#define BLACK   0x0000
//#define WHITE   0xFFFF
//#define RED     0xF800
//#define GREEN   0x07E0
//#define BLUE    0x001F
//#define CYAN    0x07FF
//#define MAGENTA 0xF81F
//#define YELLOW  0xFFE0

static TFT_eSPI* g = nullptr;

// Eenvoudige kleuren zoals andere menus
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_ROTATE=0xFFE0;     // Geel voor rotatie
static const uint16_t COL_BTN_COLORS=0x07FF;     // Cyaan voor kleuren
static const uint16_t COL_BTN_FORMAT=0xF800;     // Rood voor format
static const uint16_t COL_BTN_CALIBRATE=0xF81F;  // Magenta voor calibrate
static const uint16_t COL_BTN_BACK=0x001F;       // Blauw voor terug

static int16_t SCR_W = 320, SCR_H = 240;
static uint32_t lastTouchMs = 0;
static const uint16_t SYSTEM_COOLDOWN_MS = 800;  // Consistente cooldown

// Button rectangles - menu stijl
static int16_t bxRotateX,bxRotateY,bxRotateW,bxRotateH;
static int16_t bxColorsX,bxColorsY,bxColorsW,bxColorsH;
static int16_t bxFormatX,bxFormatY,bxFormatW,bxFormatH;
static int16_t bxCalibrateX,bxCalibrateY,bxCalibrateW,bxCalibrateH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

static inline bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

// Eenvoudige button functie zoals andere menus
static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  // Button met afgeronde hoeken
  g->fillRoundRect(x, y, w, h, 8, COL_BG);  // Background
  g->drawRoundRect(x, y, w, h, 8, COL_FR);  // Outer border
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);  // Inner border
  
  // Fill button with color
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  // Button text with better centering
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  // Better text centering calculation zoals playlist
  int16_t x1, y1; 
  uint16_t tw, th;
  // Estimate text bounds based on text size 1 (6px per char width, 8px height)
  tw = strlen(txt) * 6;
  th = 8;
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;  // Better vertical centering
  g->setCursor(textX, textY);
  g->print(txt);
}

void systemSettings_begin(TFT_eSPI* gfx) {
    g = gfx;
    SCR_W = g->width();
    SCR_H = g->height();
    
    // Eenvoudige achtergrond zoals andere menus
    g->fillScreen(COL_BG);
    g->setTextColor(COL_TX);
    g->setTextSize(2);
    
    // Center title perfect in scherm - FIXED
    int16_t x1, y1;
    uint16_t tw, th;
    // Estimate text bounds based on text size 2 (12px per char width, 16px height)
    tw = strlen("Systeem") * 12;
    th = 16;
    g->setCursor((SCR_W - tw) / 2, 25);
    g->print("Systeem");
    
    // Eenvoudige status info
    g->setTextSize(1);
    g->setCursor(20, 55);
    g->print("Display & SD kaart");
    
    // Knoppen layout - FIXED voor schermhoogte
    int16_t btnW = 160;  // Medium grootte
    int16_t btnH = 26;   // Smaller om 5 knoppen op scherm te passen
    int16_t btnX = (SCR_W - btnW) / 2;  // Gecentreerd
    int16_t startY = 85;  // Begin na status
    int16_t gap = 5;     // Smaller gap voor 5 knoppen
    
    // 5 knoppen: Rotatie, Kleuren, Format, Calibrate, Terug
    bxRotateX = btnX; bxRotateY = startY; bxRotateW = btnW; bxRotateH = btnH;
    bxColorsX = btnX; bxColorsY = startY + btnH + gap; bxColorsW = btnW; bxColorsH = btnH;
    bxFormatX = btnX; bxFormatY = startY + (btnH + gap) * 2; bxFormatW = btnW; bxFormatH = btnH;
    bxCalibrateX = btnX; bxCalibrateY = startY + (btnH + gap) * 3; bxCalibrateW = btnW; bxCalibrateH = btnH;
    bxBackX = btnX; bxBackY = startY + (btnH + gap) * 4; bxBackW = btnW; bxBackH = btnH;
    
    // Teken knoppen
    drawButton(bxRotateX, bxRotateY, bxRotateW, bxRotateH, "SCHERM ROTATIE", COL_BTN_ROTATE);
    drawButton(bxColorsX, bxColorsY, bxColorsW, bxColorsH, "KLEUREN", COL_BTN_COLORS);
    drawButton(bxFormatX, bxFormatY, bxFormatW, bxFormatH, "FORMAT SD", COL_BTN_FORMAT);
    drawButton(bxCalibrateX, bxCalibrateY, bxCalibrateW, bxCalibrateH, "CALIBRATE", COL_BTN_CALIBRATE);
    drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

SystemSettingsEvent systemSettings_poll() {
    // Touch-on-release systeem zoals hoofdscherm
    static bool wasTouching = false;
    static int16_t lastX = -1, lastY = -1;
    
    int16_t x, y;
    bool isTouching = inputTouchRead(x, y);
    
    // Sla coördinaten op tijdens touch
    if (isTouching) {
        lastX = x;
        lastY = y;
        wasTouching = true;
        return SSE_NONE;  // Nog niet triggeren tijdens aanraken
    }
    
    // Touch RELEASED - nu pas triggeren!
    if (wasTouching && !isTouching) {
        wasTouching = false;
        
        // Check cooldown
        uint32_t now = millis();
        if (now - lastTouchMs < SYSTEM_COOLDOWN_MS) return SSE_NONE;
        
        // Gebruik de laatst bekende coördinaten
        x = lastX;
        y = lastY;
        
        // DEBUG: Print alleen touch coordinaten
        Serial.printf("[SYSTEM] Touch: (%d,%d)\n", x, y);
    
    // Check button touches - eenvoudig menu style
    if (inRect(x, y, bxRotateX, bxRotateY, bxRotateW, bxRotateH)) {
        lastTouchMs = now;
        return SSE_ROTATE_SCREEN;
    }
    if (inRect(x, y, bxColorsX, bxColorsY, bxColorsW, bxColorsH)) {
        lastTouchMs = now;
        return SSE_COLORS;
    }
    if (inRect(x, y, bxFormatX, bxFormatY, bxFormatW, bxFormatH)) {
        lastTouchMs = now;
        return SSE_FORMAT_SD;
    }
    if (inRect(x, y, bxCalibrateX, bxCalibrateY, bxCalibrateW, bxCalibrateH)) {
        lastTouchMs = now;
        return SSE_CALIBRATE;
    }
    if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
        lastTouchMs = now;
        return SSE_BACK;
    }
    
    }
    
    return SSE_NONE;
}

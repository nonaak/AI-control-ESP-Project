#include "system_settings_view.h"
#include <Arduino_GFX_Library.h>
#include "input_touch.h"

static Arduino_GFX* g = nullptr;

// Eenvoudige kleuren zoals andere menus
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_ROTATE=0xFFE0;     // Geel voor rotatie
static const uint16_t COL_BTN_COLORS=0x07FF;     // Cyaan voor kleuren
static const uint16_t COL_BTN_FORMAT=0xF800;     // Rood voor format
static const uint16_t COL_BTN_BACK=0x001F;       // Blauw voor terug

static int16_t SCR_W = 320, SCR_H = 240;
static uint32_t lastTouchMs = 0;
static const uint16_t SYSTEM_COOLDOWN_MS = 800;  // Consistente cooldown

// Button rectangles - menu stijl
static int16_t bxRotateX,bxRotateY,bxRotateW,bxRotateH;
static int16_t bxColorsX,bxColorsY,bxColorsW,bxColorsH;
static int16_t bxFormatX,bxFormatY,bxFormatW,bxFormatH;
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
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;  // Better vertical centering
  g->setCursor(textX, textY);
  g->print(txt);
}

void systemSettings_begin(Arduino_GFX* gfx) {
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
    g->getTextBounds("Systeem", 0, 0, &x1, &y1, &tw, &th);
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
    
    // 5 knoppen: Rotatie, Kleuren, Format, Terug - passen nu op scherm
    bxRotateX = btnX; bxRotateY = startY; bxRotateW = btnW; bxRotateH = btnH;
    bxColorsX = btnX; bxColorsY = startY + btnH + gap; bxColorsW = btnW; bxColorsH = btnH;
    bxFormatX = btnX; bxFormatY = startY + (btnH + gap) * 2; bxFormatW = btnW; bxFormatH = btnH;
    bxBackX = btnX; bxBackY = startY + (btnH + gap) * 3; bxBackW = btnW; bxBackH = btnH;
    
    // Teken knoppen - KLEUREN TERUG!
    drawButton(bxRotateX, bxRotateY, bxRotateW, bxRotateH, "SCHERM ROTATIE", COL_BTN_ROTATE);
    drawButton(bxColorsX, bxColorsY, bxColorsW, bxColorsH, "KLEUREN", COL_BTN_COLORS);
    drawButton(bxFormatX, bxFormatY, bxFormatW, bxFormatH, "FORMAT SD", COL_BTN_FORMAT);
    drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

SystemSettingsEvent systemSettings_poll() {
    int16_t x, y;
    if (!inputTouchRead(x, y)) return SSE_NONE;
    
    // Check cooldown
    uint32_t now = millis();
    if (now - lastTouchMs < SYSTEM_COOLDOWN_MS) return SSE_NONE;
    
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
    if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
        lastTouchMs = now;
        return SSE_BACK;
    }
    
    return SSE_NONE;
}

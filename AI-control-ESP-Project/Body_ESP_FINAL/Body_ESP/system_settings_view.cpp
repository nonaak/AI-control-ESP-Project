#include "system_settings_view.h"
#include <Arduino_GFX_Library.h>
#include "input_touch.h"
#include "body_display.h"

static Arduino_GFX* g = nullptr;

// Eenvoudige kleuren zoals andere menus
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_ROTATE=0xFFE0;     // Geel voor rotatie
static const uint16_t COL_BTN_COLORS=0x07FF;     // Cyaan voor kleuren
static const uint16_t COL_BTN_FORMAT=0xF800;     // Rood voor format
static const uint16_t COL_BTN_TOUCH=0x07E0;      // Groen voor touch opties
static const uint16_t COL_BTN_BACK=0x001F;       // Blauw voor terug

static uint32_t lastTouchMs = 0;
static const uint16_t SYSTEM_COOLDOWN_MS = 800;

// Button rectangles
static int16_t bxRotateX,bxRotateY,bxRotateW,bxRotateH;
static int16_t bxColorsX,bxColorsY,bxColorsW,bxColorsH;
static int16_t bxFormatX,bxFormatY,bxFormatW,bxFormatH;
static int16_t bxMenuTouchX,bxMenuTouchY,bxMenuTouchW,bxMenuTouchH;
static int16_t bxParamsTouchX,bxParamsTouchY,bxParamsTouchW,bxParamsTouchH;
static int16_t bxEmergTouchX,bxEmergTouchY,bxEmergTouchW,bxEmergTouchH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

// Touch toggle states (extern - uit Body_ESP.ino)
extern bool touchMenuEnabled;
extern bool touchParamsEnabled;
extern bool touchEmergencyEnabled;

static inline bool inRect(int16_t x, int16_t y, int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

// Button tekenen met ON/OFF status
static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  g->fillRoundRect(x, y, w, h, 8, COL_BG);
  g->drawRoundRect(x, y, w, h, 8, COL_FR);
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  int16_t x1, y1; 
  uint16_t tw, th;
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;
  g->setCursor(textX, textY);
  g->print(txt);
}

// Toggle button met ON/OFF status
static void drawToggleButton(int16_t x, int16_t y, int16_t w, int16_t h, const char* txt, bool enabled) {
  g->fillRoundRect(x, y, w, h, 8, COL_BG);
  g->drawRoundRect(x, y, w, h, 8, COL_FR);
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);
  
  // Kleur: groen als ON, grijs als OFF
  uint16_t color = enabled ? 0x07E0 : 0x7BEF;
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  // Tekst + status
  char buf[40];
  snprintf(buf, sizeof(buf), "%s: %s", txt, enabled ? "ON" : "OFF");
  
  int16_t x1, y1; 
  uint16_t tw, th;
  g->getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;
  g->setCursor(textX, textY);
  g->print(buf);
}

void systemSettings_begin(Arduino_GFX* gfx) {
    g = gfx;
    
    g->fillScreen(COL_BG);
    g->setTextColor(COL_TX);
    g->setTextSize(2);
    
    // Center title
    int16_t x1, y1;
    uint16_t tw, th;
    g->getTextBounds("Systeem", 0, 0, &x1, &y1, &tw, &th);
    g->setCursor((SCR_W - tw) / 2, 15);
    g->print("Systeem");
    
    // Info
    g->setTextSize(1);
    g->setCursor(20, 40);
    g->print("Display & Touch Opties");
    
    // Knoppen layout - 7 knoppen op scherm
    int16_t btnW = 220;
    int16_t btnH = 24;
    int16_t btnX = (SCR_W - btnW) / 2;
    int16_t startY = 65;
    int16_t gap = 4;
    
    // 7 knoppen posities
    bxRotateX = btnX; bxRotateY = startY; bxRotateW = btnW; bxRotateH = btnH;
    bxColorsX = btnX; bxColorsY = startY + (btnH + gap) * 1; bxColorsW = btnW; bxColorsH = btnH;
    bxFormatX = btnX; bxFormatY = startY + (btnH + gap) * 2; bxFormatW = btnW; bxFormatH = btnH;
    
    // Touch opties (met toggle status)
    bxMenuTouchX = btnX; bxMenuTouchY = startY + (btnH + gap) * 3; bxMenuTouchW = btnW; bxMenuTouchH = btnH;
    bxParamsTouchX = btnX; bxParamsTouchY = startY + (btnH + gap) * 4; bxParamsTouchW = btnW; bxParamsTouchH = btnH;
    bxEmergTouchX = btnX; bxEmergTouchY = startY + (btnH + gap) * 5; bxEmergTouchW = btnW; bxEmergTouchH = btnH;
    
    bxBackX = btnX; bxBackY = startY + (btnH + gap) * 6; bxBackW = btnW; bxBackH = btnH;
    
    // Teken knoppen
    drawButton(bxRotateX, bxRotateY, bxRotateW, bxRotateH, "SCHERM ROTATIE", COL_BTN_ROTATE);
    drawButton(bxColorsX, bxColorsY, bxColorsW, bxColorsH, "KLEUREN", COL_BTN_COLORS);
    drawButton(bxFormatX, bxFormatY, bxFormatW, bxFormatH, "FORMAT SD", COL_BTN_FORMAT);
    
    // Touch toggle knoppen met status
    drawToggleButton(bxMenuTouchX, bxMenuTouchY, bxMenuTouchW, bxMenuTouchH, "Touch Menu", touchMenuEnabled);
    drawToggleButton(bxParamsTouchX, bxParamsTouchY, bxParamsTouchW, bxParamsTouchH, "Touch Params", touchParamsEnabled);
    drawToggleButton(bxEmergTouchX, bxEmergTouchY, bxEmergTouchW, bxEmergTouchH, "Touch Emergency", touchEmergencyEnabled);
    
    drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

SystemSettingsEvent systemSettings_poll() {
    int16_t x, y;
    if (!inputTouchRead(x, y)) return SSE_NONE;
    
    // Check cooldown
    uint32_t now = millis();
    if (now - lastTouchMs < SYSTEM_COOLDOWN_MS) return SSE_NONE;
    
    // Check button touches
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
    
    // Touch toggle buttons
    if (inRect(x, y, bxMenuTouchX, bxMenuTouchY, bxMenuTouchW, bxMenuTouchH)) {
        lastTouchMs = now;
        return SSE_TOGGLE_MENU;
    }
    if (inRect(x, y, bxParamsTouchX, bxParamsTouchY, bxParamsTouchW, bxParamsTouchH)) {
        lastTouchMs = now;
        return SSE_TOGGLE_PARAMS;
    }
    if (inRect(x, y, bxEmergTouchX, bxEmergTouchY, bxEmergTouchW, bxEmergTouchH)) {
        lastTouchMs = now;
        return SSE_TOGGLE_EMERGENCY;
    }
    
    if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
        lastTouchMs = now;
        return SSE_BACK;
    }
    
    return SSE_NONE;
}
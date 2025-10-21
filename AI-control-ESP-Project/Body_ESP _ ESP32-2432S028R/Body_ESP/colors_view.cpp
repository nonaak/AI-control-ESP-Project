#include "colors_view.h"
#include <Arduino_GFX_Library.h>
#include "input_touch.h"
#include "body_gfx4.h"

static Arduino_GFX* g = nullptr;

// Eenvoudige kleuren zoals andere menus
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_THEME1=0x001F;     // Blauw voor thema 1
static const uint16_t COL_BTN_THEME2=0x07E0;     // Groen voor thema 2  
static const uint16_t COL_BTN_THEME3=0xF81F;     // Magenta voor thema 3
static const uint16_t COL_BTN_BACK=0xF800;       // Rood voor terug

static int16_t SCR_W = 320, SCR_H = 240;
static uint32_t lastTouchMs = 0;
static const uint16_t COLORS_COOLDOWN_MS = 800;  // Consistente cooldown

// Button rectangles - horizontale layout
static int16_t bxTheme1X,bxTheme1Y,bxTheme1W,bxTheme1H;
static int16_t bxTheme2X,bxTheme2Y,bxTheme2W,bxTheme2H;
static int16_t bxTheme3X,bxTheme3Y,bxTheme3W,bxTheme3H;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

// Huidige kleurenthema
static int currentTheme = 0; // 0=Standaard, 1=Blauw, 2=Groen

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
  
  // Better text centering calculation
  int16_t x1, y1; 
  uint16_t tw, th;
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;  // Better vertical centering
  g->setCursor(textX, textY);
  g->print(txt);
}

void colors_begin(Arduino_GFX* gfx) {
    g = gfx;
    SCR_W = g->width();
    SCR_H = g->height();
    
    // Eenvoudige achtergrond zoals andere menus
    g->fillScreen(COL_BG);
    g->setTextColor(COL_TX);
    g->setTextSize(2);
    
    // Center title perfect in scherm
    int16_t x1, y1;
    uint16_t tw, th;
    g->getTextBounds("Kleuren", 0, 0, &x1, &y1, &tw, &th);
    g->setCursor((SCR_W - tw) / 2, 25);
    g->print("Kleuren");
    
    // Status info
    g->setTextSize(1);
    g->setCursor(20, 50);
    g->printf("Huidig thema: %s", 
        currentTheme == 0 ? "Standaard" : 
        currentTheme == 1 ? "Blauw" :
        "Groen");
    
    // Preview kleuren
    g->setTextColor(WHITE);
    g->setCursor(20, 70);
    g->print("Preview kleuren:");
    
    // Toon kleur voorbeelden
    int previewY = 85;
    int previewSize = 15;
    int previewSpacing = 25;
    
    // Standaard kleuren gebaseerd op thema
    uint16_t colors[3];
    switch(currentTheme) {
        case 1: // Blauw thema
            colors[0] = 0x001F; // Blauw
            colors[1] = 0x07FF; // Cyaan  
            colors[2] = 0x4210; // Donkerblauw
            break;
        case 2: // Groen thema
            colors[0] = 0x07E0; // Groen
            colors[1] = 0x07FF; // Cyaan
            colors[2] = 0x2104; // Donkergroen
            break;
        default: // Standaard (magenta/paars)
            colors[0] = 0xF81F; // Magenta
            colors[1] = 0xC618; // Licht paars
            colors[2] = 0x780F; // Donker magenta
            break;
    }
    
    for(int i = 0; i < 3; i++) {
        g->fillRect(20 + i * previewSpacing, previewY, previewSize, previewSize, colors[i]);
        g->drawRect(20 + i * previewSpacing, previewY, previewSize, previewSize, COL_FR);
    }
    
    // Knoppen layout - horizontaal naast elkaar
    int16_t btnW = 70;   // Smaller voor 4 knoppen naast elkaar
    int16_t btnH = 25;   // Standaard hoogte
    int16_t totalW = 4 * btnW + 3 * 8;  // 4 knoppen + 3 gaps van 8px
    int16_t startX = (SCR_W - totalW) / 2;  // Gecentreerd
    int16_t startY = 140;  // Begin na preview
    int16_t gap = 8;     // Gap tussen knoppen
    
    // 4 knoppen horizontaal: Standaard, Blauw, Groen, Terug
    bxTheme1X = startX; bxTheme1Y = startY; bxTheme1W = btnW; bxTheme1H = btnH;
    bxTheme2X = startX + btnW + gap; bxTheme2Y = startY; bxTheme2W = btnW; bxTheme2H = btnH;
    bxTheme3X = startX + 2 * (btnW + gap); bxTheme3Y = startY; bxTheme3W = btnW; bxTheme3H = btnH;
    bxBackX = startX + 3 * (btnW + gap); bxBackY = startY; bxBackW = btnW; bxBackH = btnH;
    
    // Teken knoppen met indicatie van huidige thema
    const char* theme1Text = currentTheme == 0 ? "●STAN" : "STAN";
    const char* theme2Text = currentTheme == 1 ? "●BLAUW" : "BLAUW";  
    const char* theme3Text = currentTheme == 2 ? "●GROEN" : "GROEN";
    
    drawButton(bxTheme1X, bxTheme1Y, bxTheme1W, bxTheme1H, theme1Text, 0x8410);  // Grijs
    drawButton(bxTheme2X, bxTheme2Y, bxTheme2W, bxTheme2H, theme2Text, COL_BTN_THEME1);
    drawButton(bxTheme3X, bxTheme3Y, bxTheme3W, bxTheme3H, theme3Text, COL_BTN_THEME2);
    drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

ColorEvent colors_poll() {
    int16_t x, y;
    if (!inputTouchRead(x, y)) return CE_NONE;
    
    // Check cooldown
    uint32_t now = millis();
    if (now - lastTouchMs < COLORS_COOLDOWN_MS) return CE_NONE;
    
    // Check button touches
    if (inRect(x, y, bxTheme1X, bxTheme1Y, bxTheme1W, bxTheme1H)) {
        lastTouchMs = now;
        currentTheme = 0; // Standaard
        colors_begin(g); // Refresh
        return CE_COLOR_CHANGE;
    }
    if (inRect(x, y, bxTheme2X, bxTheme2Y, bxTheme2W, bxTheme2H)) {
        lastTouchMs = now;
        currentTheme = 1; // Blauw
        colors_begin(g); // Refresh
        return CE_COLOR_CHANGE;
    }
    if (inRect(x, y, bxTheme3X, bxTheme3Y, bxTheme3W, bxTheme3H)) {
        lastTouchMs = now;
        currentTheme = 2; // Groen
        colors_begin(g); // Refresh
        return CE_COLOR_CHANGE;
    }
    if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
        lastTouchMs = now;
        return CE_BACK;
    }
    
    return CE_NONE;
}
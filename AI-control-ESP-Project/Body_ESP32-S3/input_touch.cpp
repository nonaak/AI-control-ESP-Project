#include "input_touch.h"
#include <Arduino.h>

#if ENABLE_TOUCH
  #include <SPI.h>
  #include <xpt2046.h>
  
  // T-HMI touch pins (uit T-HMI-master/examples/touch/pins.h)
  #define TOUCHSCREEN_SCLK_PIN (1)
  #define TOUCHSCREEN_MISO_PIN (4)
  #define TOUCHSCREEN_MOSI_PIN (3)
  #define TOUCHSCREEN_CS_PIN   (2)
  #define TOUCHSCREEN_IRQ_PIN  (9)
  
  static XPT2046 touch = XPT2046(SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN);
#endif

/* layout (moet kloppen met body_gfx4.cpp) - UPDATED voor nieuwe layout */
// Deze waarden moeten exact matchen met body_gfx4.cpp:
// BUTTON_Y = 200, BUTTON_W = 75, BUTTON_H = 35, BUTTON_SPACING = 80, BUTTON_START_X = 5
  // Touch zones exact gelijk aan visuele knoppen
static const int16_t BUTTON_START_X=5, BUTTON_Y=200, BUTTON_W=75, BUTTON_H=35, BUTTON_SPACING=80;

static TFT_eSPI* g=nullptr;
static int16_t SCR_W=320, SCR_H=240;
static bool touchRotated180 = false;  // Is touch 180° gedraaid?

static int16_t bx1,by,bw,bh; // Opnemen
static int16_t bx2,bx3,bx4;  // Afspelen, Menu, Overrule

static uint32_t lastTapMs=0;
static const uint16_t TAP_COOLDOWN_MS=400;   // Kortere cooldown voor betere responsiviteit

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}
static inline int16_t clamp16(int16_t v,int16_t lo,int16_t hi){
  if(v<lo) return lo; if(v>hi) return hi; return v;
}

// XPT2046 doet alle coordinate mapping zelf via setCal()

void inputTouchBegin(TFT_eSPI* gfx){
  g=gfx;
  if(g){ SCR_W=g->width(); SCR_H=g->height(); }

  // Touch zones exact zoals in body_gfx4.cpp
  bw = BUTTON_W;
  bh = BUTTON_H;
  by = BUTTON_Y;
  bx1 = BUTTON_START_X + 0 * BUTTON_SPACING;  // REC
  bx2 = BUTTON_START_X + 1 * BUTTON_SPACING;  // PLAY  
  bx3 = BUTTON_START_X + 2 * BUTTON_SPACING;  // MENU
  bx4 = BUTTON_START_X + 3 * BUTTON_SPACING;  // AI

#if ENABLE_TOUCH
  SPI.begin(TOUCHSCREEN_SCLK_PIN, TOUCHSCREEN_MISO_PIN, TOUCHSCREEN_MOSI_PIN);
  touch.begin(240, 320);
  // T-HMI factory kalibratie data (uit factory.ino)
  touch.setCal(285, 1788, 311, 1877, 240, 320); // Raw xmin, xmax, ymin, ymax, width, height
  touch.setRotation(3);  // Factory gebruikt rotatie 3
  Serial.println("[TOUCH] T-HMI XPT2046 touch system geïnitialiseerd met factory kalibratie");
#endif
}

void inputTouchSetRotated(bool rotated180) {
  touchRotated180 = rotated180;
}

bool inputTouchRead(int16_t &sx, int16_t &sy){
#if ENABLE_TOUCH
  if(!touch.pressed()) return false;

  // XPT2046 geeft al gefilterde coordinaten
  sx = touch.X();
  sy = touch.Y();
  
  // Clamp naar scherm grenzen
  sx = clamp16(sx, 0, SCR_W-1);
  sy = clamp16(sy, 0, SCR_H-1);
  
  // Als scherm 180° gedraaid is, roteer de touch coordinaten ook
  if (touchRotated180) {
    sx = (int16_t)(SCR_W-1 - sx);
    sy = (int16_t)(SCR_H-1 - sy);
  }
  
  return true;
#else
  (void)sx; (void)sy; return false;
#endif
}

bool inputTouchPoll(TouchEvent &ev){
  ev = {TE_NONE, -1, -1};

  int16_t x,y;
  if(!inputTouchRead(x,y)) return false;

  uint32_t now=millis();
  if(now - lastTapMs < TAP_COOLDOWN_MS) return false;

  // Debug: print alle touch events
  Serial.printf("[TOUCH] Touch at (%d,%d). Buttons: REC(%d,%d,%d,%d) PLAY(%d,%d,%d,%d) MENU(%d,%d,%d,%d) AI(%d,%d,%d,%d)\n", 
                x, y, bx1, by, bw, bh, bx2, by, bw, bh, bx3, by, bw, bh, bx4, by, bw, bh);

  if(inRect(x,y, bx1,by,bw,bh)){ 
    Serial.printf("[TOUCH] REC button pressed at (%d,%d)\n", x, y);
    ev={TE_TAP_REC, x,y}; lastTapMs=now; return true; 
  }
  if(inRect(x,y, bx2,by,bw,bh)){ 
    Serial.printf("[TOUCH] PLAY button pressed at (%d,%d)\n", x, y);
    ev={TE_TAP_PLAY, x,y}; lastTapMs=now; return true; 
  }
  if(inRect(x,y, bx3,by,bw,bh)){ 
    Serial.printf("[TOUCH] MENU button pressed at (%d,%d)\n", x, y);
    ev={TE_TAP_MENU, x,y}; lastTapMs=now; return true; 
  }
  if(inRect(x,y, bx4,by,bw,bh)){ 
    Serial.printf("[TOUCH] AI button pressed at (%d,%d)\n", x, y);
    ev={TE_TAP_OVERRULE, x,y}; lastTapMs=now; return true; 
  }

  Serial.printf("[TOUCH] Touch at (%d,%d) - no button hit\n", x, y);
  return false;
}

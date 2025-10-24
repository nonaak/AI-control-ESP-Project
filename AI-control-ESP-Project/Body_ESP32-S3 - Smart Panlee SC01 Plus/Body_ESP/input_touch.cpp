#include "input_touch.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#if ENABLE_TOUCH
  #include <SPI.h>
  #include <XPT2046_Touchscreen.h>
  static SPIClass touchSPI(HSPI);
  static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
#endif

/* layout (moet kloppen met body_gfx4.cpp) - UPDATED voor nieuwe layout */
// Deze waarden moeten exact matchen met body_gfx4.cpp:
// BUTTON_Y = 200, BUTTON_W = 75, BUTTON_H = 35, BUTTON_SPACING = 80, BUTTON_START_X = 5
static const int16_t BUTTON_START_X=5, BUTTON_Y=200, BUTTON_W=75, BUTTON_H=35, BUTTON_SPACING=80;

static Arduino_GFX* g=nullptr;
static int16_t SCR_W=320, SCR_H=240;
static bool touchRotated180 = false;  // Is touch 180° gedraaid?

static int16_t bx1,by,bw,bh; // Opnemen
static int16_t bx2,bx3,bx4;  // Afspelen, Menu, Overrule

static uint32_t lastTapMs=0;
static const uint16_t TAP_COOLDOWN_MS=800;   // Lange cooldown maar niet te lang (was 1200)

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}
static inline int16_t clamp16(int16_t v,int16_t lo,int16_t hi){
  if(v<lo) return lo; if(v>hi) return hi; return v;
}

/* ======== ruwe → schermcoords met simpele toggles ======== */
static bool mapRawToScreen(int rx, int ry, int16_t &sx, int16_t &sy){
  // median-voorbewerking is elders; hier alleen mappen
  long X = constrain((long)rx, (long)min(TOUCH_MAP_X1,TOUCH_MAP_X2), (long)max(TOUCH_MAP_X1,TOUCH_MAP_X2));
  long Y = constrain((long)ry, (long)min(TOUCH_MAP_Y1,TOUCH_MAP_Y2), (long)max(TOUCH_MAP_Y1,TOUCH_MAP_Y2));

  // naar 0..max mappen in RAW-oriëntatie
  int16_t ax = (int16_t)map(X, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, SCR_W-1);
  int16_t ay = (int16_t)map(Y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, SCR_H-1);

  // optioneel wisselen/spiegelen (fix voor gespiegeld/gedraaid gedrag)
#if SWAP_XY
  int16_t tmp=ax; ax=ay; ay=tmp;
#endif
#if FLIP_X
  ax = (int16_t)(SCR_W-1 - ax);
#endif
#if FLIP_Y
  ay = (int16_t)(SCR_H-1 - ay);
#endif

  // Als scherm 180° gedraaid is, roteer de touch coordinaten ook
  if (touchRotated180) {
    ax = (int16_t)(SCR_W-1 - ax);
    ay = (int16_t)(SCR_H-1 - ay);
  }

  sx = clamp16(ax, 0, SCR_W-1);
  sy = clamp16(ay, 0, SCR_H-1);
  return true;
}

void inputTouchBegin(Arduino_GFX* gfx){
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
  touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  ts.begin(touchSPI);
  // geen ts.setRotation nodig — we doen mapping zelf
#endif
}

void inputTouchSetRotated(bool rotated180) {
  touchRotated180 = rotated180;
}

bool inputTouchRead(int16_t &sx, int16_t &sy){
#if ENABLE_TOUCH
  if(!(ts.tirqTouched() || ts.touched())) return false;

  // median van 3 samples (stabiel)
  int16_t rx[3], ry[3]; int pz[3];
  auto median3 = [](int16_t a,int16_t b,int16_t c)->int16_t{
    if(a>b){int16_t t=a;a=b;b=t;} if(b>c){int16_t t=b;b=c;c=t;} if(a>b){int16_t t=a;a=b;b=t;} return b;
  };

  for(int i=0;i<3;i++){
    TS_Point p = ts.getPoint();
    rx[i] = p.x; ry[i] = p.y; pz[i] = p.z;
    delay(2);
  }
  int16_t rX = median3(rx[0],rx[1],rx[2]);
  int16_t rY = median3(ry[0],ry[1],ry[2]);
  int     pZ = median3(pz[0],pz[1],pz[2]);

  if(pZ < 50) return false; // te licht

  return mapRawToScreen(rX, rY, sx, sy);
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

  // Debug: print touch coordinates en button zones
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug > 500) {  // Debug elke 500ms
    Serial.printf("[TOUCH] Touch at (%d,%d). Buttons: REC(%d,%d,%d,%d) PLAY(%d,%d,%d,%d) MENU(%d,%d,%d,%d) AI(%d,%d,%d,%d)\n", 
                  x, y, bx1, by, bw, bh, bx2, by, bw, bh, bx3, by, bw, bh, bx4, by, bw, bh);
    lastDebug = millis();
  }

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

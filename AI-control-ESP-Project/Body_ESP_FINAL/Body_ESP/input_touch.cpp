#include "input_touch.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

// ========= FT6X36 I2C Touch Driver (SC01 Plus) =========
#if ENABLE_TOUCH
  #include <FT6X36.h>  // FT6X36 touch library (compatibel met FT5x06)
  static FT6X36* ts = nullptr;  // Touch sensor pointer (init in begin)
  static TPoint lastTouchPoint = {0, 0};  // Last touch coordinates
  static bool touchAvailable = false;     // Touch data available flag
  
  // Touch callback voor FT6X36 - ONLY TouchEnd to prevent spam
  static void touchCallback(TPoint point, TEvent e) {
    // Only process on lift up (release) to prevent spam
    if (e != TEvent::TouchEnd) return;
    
    lastTouchPoint = point;
    touchAvailable = true;
    Serial.printf("[TOUCH CALLBACK] x=%d, y=%d, event=TouchEnd\n", point.x, point.y);
  }
#endif

// ========= OUDE XPT2046 SPI Touch Driver (CYD) =========
// #if ENABLE_TOUCH
//   #include <SPI.h>
//   #include <XPT2046_Touchscreen.h>
//   static SPIClass touchSPI(HSPI);
//   static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
// #endif

/* layout (moet kloppen met body_gfx4.cpp) - UPDATED voor nieuwe layout */
// Deze waarden moeten exact matchen met body_gfx4.cpp:
// Voor 480x320: BUTTON_Y = 280, BUTTON_W = 112, BUTTON_H = 35, BUTTON_SPACING = 120, BUTTON_START_X = 5
static const int16_t BUTTON_START_X=5, BUTTON_Y=280, BUTTON_W=112, BUTTON_H=35, BUTTON_SPACING=120;

static Arduino_GFX* g=nullptr;
static int16_t SCR_W=480, SCR_H=320;  // SC01 Plus: 480x320 (landscape)
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

/* ======== FT6X36 touch coordinaten naar scherm mapping ======== */
static bool mapRawToScreen(int rx, int ry, int16_t &sx, int16_t &sy){
  // FT6X36 geeft directe pixel coordinaten (0-319 x, 0-479 y)
  // Geen ADC mapping nodig zoals bij XPT2046!
  
  int16_t ax = (int16_t)rx;
  int16_t ay = (int16_t)ry;

  // Wissel en spiegel volgens configuratie
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

// ========= OUDE XPT2046 mapping functie =========
// static bool mapRawToScreen(int rx, int ry, int16_t &sx, int16_t &sy){
//   // median-voorbewerking is elders; hier alleen mappen
//   long X = constrain((long)rx, (long)min(TOUCH_MAP_X1,TOUCH_MAP_X2), (long)max(TOUCH_MAP_X1,TOUCH_MAP_X2));
//   long Y = constrain((long)ry, (long)min(TOUCH_MAP_Y1,TOUCH_MAP_Y2), (long)max(TOUCH_MAP_Y1,TOUCH_MAP_Y2));
//
//   // naar 0..max mappen in RAW-oriëntatie
//   int16_t ax = (int16_t)map(X, TOUCH_MAP_X1, TOUCH_MAP_X2, 0, SCR_W-1);
//   int16_t ay = (int16_t)map(Y, TOUCH_MAP_Y1, TOUCH_MAP_Y2, 0, SCR_H-1);
//
//   // optioneel wisselen/spiegelen (fix voor gespiegeld/gedraaid gedrag)
// #if SWAP_XY
//   int16_t tmp=ax; ax=ay; ay=tmp;
// #endif
// #if FLIP_X
//   ax = (int16_t)(SCR_W-1 - ax);
// #endif
// #if FLIP_Y
//   ay = (int16_t)(SCR_H-1 - ay);
// #endif
//
//   // Als scherm 180° gedraaid is, roteer de touch coordinaten ook
//   if (touchRotated180) {
//     ax = (int16_t)(SCR_W-1 - ax);
//     ay = (int16_t)(SCR_H-1 - ay);
//   }
//
//   sx = clamp16(ax, 0, SCR_W-1);
//   sy = clamp16(ay, 0, SCR_H-1);
//   return true;
// }

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
  // FT6X36 I2C touch initialisatie
  Wire.begin(TOUCH_SDA, TOUCH_SCL);  // Start I2C op GPIO 6 (SDA) en 5 (SCL)
  Wire.setClock(400000);  // 400kHz I2C speed
  
  // FT6X36 constructor: FT6X36(TwoWire * wire, int8_t intPin)
  // Gebruik -1 voor interrupt = polling mode
  ts = new FT6X36(&Wire, -1);  // Geen interrupt, polling mode
  
  if (ts->begin()) {
    Serial.println("[TOUCH] FT6X36 touch controller initialized (polling mode)");
    // Registreer touch callback
    ts->registerTouchHandler(touchCallback);
  } else {
    Serial.println("[TOUCH] ERROR: FT6X36 not found!");
  }
#endif
}

// ========= OUDE XPT2046 initialisatie =========
// #if ENABLE_TOUCH
//   touchSPI.begin(TOUCH_SCK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
//   ts.begin(touchSPI);
//   // geen ts.setRotation nodig — we doen mapping zelf
// #endif

void inputTouchSetRotated(bool rotated180) {
  touchRotated180 = rotated180;
}

bool inputTouchRead(int16_t &sx, int16_t &sy){
#if ENABLE_TOUCH
  if (!ts) return false;  // Not initialized
  
  // POLLING MODE: Manually trigger touch processing
  ts->processTouch();  // Read I2C and fire callbacks
  ts->loop();          // Process events
  
  // Debug: check touched() status
  static uint32_t lastTouchCheck = 0;
  if (millis() - lastTouchCheck > 1000) {
    uint8_t touches = ts->touched();
    Serial.printf("[TOUCH DEBUG] touched() = %d, available = %d\n", touches, touchAvailable);
    lastTouchCheck = millis();
  }
  
  // Check of er touch data beschikbaar is
  if (!touchAvailable) return false;
  
  // Gebruik laatste touch point uit callback
  int16_t rX = lastTouchPoint.x;
  int16_t rY = lastTouchPoint.y;
  
  // Reset flag (single-shot)
  touchAvailable = false;
  
  // Check of coordinaten geldig zijn
  if (rX == 0 && rY == 0) return false;  // Geen geldige touch
  
  // Debug output (elke 500ms)
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug > 500) {
    Serial.printf("[TOUCH] Raw: (%d,%d)\n", rX, rY);
    lastDebug = millis();
  }
  
  return mapRawToScreen(rX, rY, sx, sy);
#else
  (void)sx; (void)sy; return false;
#endif
}

// ========= OUDE XPT2046 touch read functie =========
// bool inputTouchRead(int16_t &sx, int16_t &sy){
// #if ENABLE_TOUCH
//   if(!(ts.tirqTouched() || ts.touched())) return false;
//
//   // median van 3 samples (stabiel)
//   int16_t rx[3], ry[3]; int pz[3];
//   auto median3 = [](int16_t a,int16_t b,int16_t c)->int16_t{
//     if(a>b){int16_t t=a;a=b;b=t;} if(b>c){int16_t t=b;b=c;c=t;} if(a>b){int16_t t=a;a=b;b=t;} return b;
//   };
//
//   for(int i=0;i<3;i++){
//     TS_Point p = ts.getPoint();
//     rx[i] = p.x; ry[i] = p.y; pz[i] = p.z;
//     delay(2);
//   }
//   int16_t rX = median3(rx[0],rx[1],rx[2]);
//   int16_t rY = median3(ry[0],ry[1],ry[2]);
//   int     pZ = median3(pz[0],pz[1],pz[2]);
//
//   if(pZ < 50) return false; // te licht
//
//   return mapRawToScreen(rX, rY, sx, sy);
// #else
//   (void)sx; (void)sy; return false;
// #endif
// }

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

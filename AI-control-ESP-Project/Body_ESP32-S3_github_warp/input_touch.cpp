#include "input_touch.h"
#include "touch_calibration.h"
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
  // hier kan ik de touch knoppen verschuiven:
  //Voor MENU knop hoger/lager: verander BUTTON_Y=200 naar bijvoorbeeld BUTTON_Y=210 (lager op scherm)
  //Voor MENU knop links/rechts: verander BUTTON_START_X=5 naar bijvoorbeeld BUTTON_START_X=-10 (meer naar links)
//static const int16_t BUTTON_START_X=5, BUTTON_Y=200, BUTTON_W=75, BUTTON_H=35, BUTTON_SPACING=80;
static const int16_t BUTTON_START_X=5, BUTTON_Y=225, BUTTON_W=70, BUTTON_H=35, BUTTON_SPACING=70;

static TFT_eSPI* g=nullptr;
static int16_t SCR_W=320, SCR_H=240;
static bool touchRotated180 = false;  // Is touch 180° gedraaid?

static int16_t bx1,by,bw,bh; // Opnemen
static int16_t bx2,bx3,bx4;  // Afspelen, Menu, Overrule

static uint32_t lastTapMs=0;
static const uint16_t TAP_COOLDOWN_MS=200;   // Cooldown tussen touch events

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
  //touch.begin(320, 240);  // GEFIXED: was 240,320 maar scherm is 320x240
  //touch.begin(240, 320);  // Terug naar factory zoals T-HMI voorbeelden
  touch.begin(320, 240); // dat is wat Sonnet zij
  
  // Probeer opgeslagen kalibratie data te laden
  touchCalibration_loadFromEEPROM();
  
  if (touchCalibration_hasValidData()) {
    // Gebruik opgeslagen kalibratie waarden
    CalibrationData calData = touchCalibration_getData();
    //touch.setCal(calData.xmin, calData.xmax, calData.ymin, calData.ymax, 320, 240);  // GEFIXED: dimensies
    touch.setCal(calData.xmin, calData.xmax, calData.ymin, calData.ymax, 240, 320);  // Terug naar factory dimensies
    Serial.println("[TOUCH] T-HMI XPT2046 touch system geïnitialiseerd met opgeslagen kalibratie");
    Serial.printf("[TOUCH] Cal waarden: (%d, %d, %d, %d)\n", calData.xmin, calData.xmax, calData.ymin, calData.ymax);
  } else {
    // Gebruik default kalibratie data
    //touch.setCal(285, 1788, 280, 1846, 320, 240);  // GEFIXED: dimensies
    //touch.setCal(285, 1788, 280, 1846, 240, 320);  // Terug naar factory dimensies
    //touch.setCal(311, 1877, 285, 1788, 320, 240);
    touch.setCal(385, 1788, 341, 1877, 240, 320);
    Serial.println("[TOUCH] T-HMI XPT2046 touch system geïnitialiseerd met default kalibratie");
    Serial.println("[TOUCH] TIP: Gebruik Menu->Systeem->CALIBRATE voor betere touch precisie");
  }
  
  touch.setRotation(3);  // Factory gebruikt rotatie 3
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

  // Track touch state om alleen op RELEASE te triggeren
  static bool wasTouching = false;
  static int16_t lastX = -1, lastY = -1;
  
  int16_t x, y;
  bool isTouching = inputTouchRead(x, y);
  
  // Sla coördinaten op tijdens touch
  if (isTouching) {
    lastX = x;
    lastY = y;
    wasTouching = true;
    // DEBUG: Print alle touch events (ook tijdens vasthouden)
    static uint32_t lastDebugMs = 0;
    if (millis() - lastDebugMs > 100) {  // Elke 100ms
      Serial.printf("[TOUCH RAW] Live touch: (%d,%d)\n", x, y);
      lastDebugMs = millis();
    }
    return false;  // Nog niet triggeren tijdens aanraken
  }
  
  // Touch RELEASED - nu pas triggeren!
  if (wasTouching && !isTouching) {
    wasTouching = false;
    
    uint32_t now = millis();
    if (now - lastTapMs < TAP_COOLDOWN_MS) return false;
    
    // Gebruik de laatst bekende coördinaten
    x = lastX;
    y = lastY;

    // Debug: print alle touch events
    Serial.printf("[TOUCH] Touch released at (%d,%d). Buttons: REC(%d,%d,%d,%d) PLAY(%d,%d,%d,%d) MENU(%d,%d,%d,%d) AI(%d,%d,%d,%d)\n", 
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
  }
  
  return false;
}

// NIEUW: Reset de cooldown timer
// Roep deze functie aan elke keer als je van scherm wisselt
// om te voorkomen dat een touch op de oude positie direct een
// nieuwe actie triggert op het nieuwe scherm
void inputTouchResetCooldown() {
  lastTapMs = millis();
  Serial.println("[TOUCH] Cooldown reset voor schermwisseling");
}

// Ruwe touch data voor kalibratie (zonder mapping)
bool inputTouchReadRaw(int16_t &rawX, int16_t &rawY) {
#if ENABLE_TOUCH
  if (!touch.pressed()) return false;
  
  // Lees ruwe coordinaten ZONDER kalibratie mapping
  rawX = touch.RawX();
  rawY = touch.RawY();
  
  return true;
#else
  (void)rawX; (void)rawY; return false;
#endif
}

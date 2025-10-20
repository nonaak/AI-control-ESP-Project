#include "input_touch.h"
#include "touch_calibration.h"
#include <Arduino.h>

#if ENABLE_TOUCH
  #include <SPI.h>
  #include <xpt2046.h>
  
  // T-HMI touch pins
  #define TOUCHSCREEN_SCLK_PIN (1)
  #define TOUCHSCREEN_MISO_PIN (4)
  #define TOUCHSCREEN_MOSI_PIN (3)
  #define TOUCHSCREEN_CS_PIN   (2)
  #define TOUCHSCREEN_IRQ_PIN  (9)
  
  static XPT2046 touch = XPT2046(SPI, TOUCHSCREEN_CS_PIN, TOUCHSCREEN_IRQ_PIN);
#endif

// Touch zones EXACT zoals body_gfx4.cpp
static const int16_t BUTTON_START_X=5, BUTTON_Y=200, BUTTON_W=75, BUTTON_H=35, BUTTON_SPACING=80;

static TFT_eSPI* g=nullptr;
static int16_t SCR_W=320, SCR_H=240;
static bool touchRotated180 = false;

static int16_t bx1,by,bw,bh; // Opnemen
static int16_t bx2,bx3,bx4;  // Afspelen, Menu, Overrule

static uint32_t lastTapMs=0;
static const uint16_t TAP_COOLDOWN_MS=200;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}
static inline int16_t clamp16(int16_t v,int16_t lo,int16_t hi){
  if(v<lo) return lo; if(v>hi) return hi; return v;
}

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
  
  // T-HMI factory volgorde: begin → setCal → setRotation
  touch.begin(240, 320);
  touch.setCal(275, 1788, 325, 1860, 240, 320);  // v20a - werkende calibratie
  touch.setRotation(3);
  
  Serial.println("[TOUCH] T-HMI XPT2046 initialized with calibration v20a");
  Serial.printf("[TOUCH] Screen: %dx%d, Touch zones: Y=%d\n", SCR_W, SCR_H, by);
#endif
}

void inputTouchSetRotated(bool rotated180) {
  touchRotated180 = rotated180;
}

bool inputTouchRead(int16_t &sx, int16_t &sy){
#if ENABLE_TOUCH
  if(!touch.pressed()) return false;

  sx = touch.X();
  sy = touch.Y();
  
  sx = clamp16(sx, 0, SCR_W-1);
  sy = clamp16(sy, 0, SCR_H-1);
  
  if (touchRotated180) {
    sx = (int16_t)(SCR_W-1 - sx);
    sy = (int16_t)(SCR_H-1 - sy);
  }
  
  return true;
#else
  (void)sx; (void)sy; return false;
#endif
}

void inputTouchCorrectForViews(int16_t &x, int16_t &y) {
    // Y-correctie: bovenste helft van scherm heeft extra offset nodig
    if (y < 100) {
        y += 30;  // Vaste correctie van 30px voor bovenste buttons
    } else if (y < 150) {
        y += 15;  // Kleinere correctie voor midden buttons
    }
    
    // X-correctie: non-lineair patroon - rechts heeft meer offset
    if (x > 200) {
        int16_t overshoot = x - 200;
        x -= (overshoot * 15) / 100;
    }
}

bool inputTouchPoll(TouchEvent &ev){
  ev = {TE_NONE, -1, -1};

  static bool wasTouching = false;
  static int16_t lastX = -1, lastY = -1;
  
  int16_t x, y;
  bool isTouching = inputTouchRead(x, y);
  
  if (isTouching) {
    lastX = x;
    lastY = y;
    wasTouching = true;
    static uint32_t lastDebugMs = 0;
    if (millis() - lastDebugMs > 100) {
      Serial.printf("[TOUCH RAW] Live touch: (%d,%d)\n", x, y);
      lastDebugMs = millis();
    }
    return false;
  }
  
  if (wasTouching && !isTouching) {
    wasTouching = false;
    
    uint32_t now = millis();
    if (now - lastTapMs < TAP_COOLDOWN_MS) return false;
    
    x = lastX;
    y = lastY;

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

void inputTouchResetCooldown() {
  lastTapMs = millis();
  Serial.println("[TOUCH] Cooldown reset voor schermwisseling");
}

bool inputTouchReadRaw(int16_t &rawX, int16_t &rawY) {
#if ENABLE_TOUCH
  if (!touch.pressed()) return false;
  
  rawX = touch.RawX();
  rawY = touch.RawY();
  
  return true;
#else
  (void)rawX; (void)rawY; return false;
#endif
}
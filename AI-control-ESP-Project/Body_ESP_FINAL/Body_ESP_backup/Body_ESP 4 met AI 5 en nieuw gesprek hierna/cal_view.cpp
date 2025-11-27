#include "cal_view.h"
#include <Arduino_GFX_Library.h>
#include "input_touch.h"

static Arduino_GFX* g=nullptr;
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE, COL_DOT=0xF800, COL_BTN=0xFD20;  // Oranje voor terug knop
static int16_t SCR_W=320,SCR_H=240;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

// Touch cooldown voor calibration
static uint32_t lastCalTouchMs = 0;
static const uint16_t CAL_COOLDOWN_MS = 300;  // Korter voor calibratie

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}
static void drawBackButton(){
  int16_t margin=12; bxBackW=SCR_W-margin*2; bxBackH=36; bxBackX=margin; bxBackY=SCR_H-bxBackH-margin;
  g->fillRect(bxBackX,bxBackY,bxBackW,bxBackH,COL_BTN);
  g->drawRect(bxBackX,bxBackY,bxBackW,bxBackH,COL_FR);
  g->setTextColor(BLACK); g->setTextSize(2);
  int16_t tw=5*12; g->setCursor(bxBackX+(bxBackW-tw)/2, bxBackY+(bxBackH-16)/2); g->print("Terug");
}

void cal_begin(Arduino_GFX* gfx){
  g=gfx; SCR_W=g->width(); SCR_H=g->height();
  g->fillScreen(COL_BG);
  g->setTextColor(COL_TX); g->setTextSize(2);
  g->setCursor(12,12); g->print("Touch kalibratie");
  g->setTextSize(1);
  g->setCursor(12,34); g->print("Tik ergens op het scherm; dot volgt.");
  g->setCursor(12,46); g->print("Stel evt. MAP/ROT in input_touch.h bij.");
  drawBackButton();
}

CalEvent cal_poll(){
  int16_t x,y;
  if(inputTouchRead(x,y)){
    uint32_t now = millis();
    
    // Check back button first (with cooldown)
    if(inRect(x,y,bxBackX,bxBackY,bxBackW,bxBackH)) {
      if (now - lastCalTouchMs >= CAL_COOLDOWN_MS) {
        lastCalTouchMs = now;
        return CE_BACK;
      }
      return CE_NONE;
    }
    
    // Dot (geen cooldown voor calibratie dots)
    for(int dx=-3; dx<=3; ++dx)
      for(int dy=-3; dy<=3; ++dy){
        int16_t xx=x+dx, yy=y+dy;
        if(xx>=0 && xx<SCR_W && yy>=0 && yy<SCR_H) g->drawPixel(xx,yy,COL_DOT);
      }
    // Coords
    g->fillRect(12,60,160,14,COL_BG);
    g->setTextColor(COL_TX); g->setTextSize(1);
    g->setCursor(12,60); g->printf("x=%d  y=%d", x, y);
  }
  return CE_NONE;
}

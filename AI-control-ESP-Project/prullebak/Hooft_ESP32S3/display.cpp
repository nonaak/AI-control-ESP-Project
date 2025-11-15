#include <math.h>
#include "display.h"

// ==================== LOVYANGFX SETUP ====================
// Deze setup is SPECIFIEK voor ESP32-4827S043R (480x272 RGB)

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_RGB     _panel_instance;
  lgfx::Bus_RGB       _bus_instance;
  lgfx::Light_PWM     _light_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      cfg.pin_d0  = GPIO_NUM_8;   // B0
      cfg.pin_d1  = GPIO_NUM_3;   // B1
      cfg.pin_d2  = GPIO_NUM_46;  // B2
      cfg.pin_d3  = GPIO_NUM_9;   // B3
      cfg.pin_d4  = GPIO_NUM_1;   // B4
      cfg.pin_d5  = GPIO_NUM_5;   // G0
      cfg.pin_d6  = GPIO_NUM_6;   // G1
      cfg.pin_d7  = GPIO_NUM_7;   // G2
      cfg.pin_d8  = GPIO_NUM_15;  // G3
      cfg.pin_d9  = GPIO_NUM_16;  // G4
      cfg.pin_d10 = GPIO_NUM_4;   // G5
      cfg.pin_d11 = GPIO_NUM_45;  // R0
      cfg.pin_d12 = GPIO_NUM_48;  // R1
      cfg.pin_d13 = GPIO_NUM_47;  // R2
      cfg.pin_d14 = GPIO_NUM_21;  // R3
      cfg.pin_d15 = GPIO_NUM_14;  // R4

      cfg.pin_henable = GPIO_NUM_40;
      cfg.pin_vsync   = GPIO_NUM_41;
      cfg.pin_hsync   = GPIO_NUM_39;
      cfg.pin_pclk    = GPIO_NUM_42;
      cfg.freq_write  = 14000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch  = 43;
      
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch  = 12;

      cfg.pclk_active_neg = 1;
      cfg.de_idle_high    = 0;
      cfg.pclk_idle_high  = 0;

      _bus_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.memory_width  = 480;
      cfg.memory_height = 272;
      cfg.panel_width   = 480;
      cfg.panel_height  = 272;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = GPIO_NUM_2;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
    }

    _panel_instance.light(&_light_instance);
    _panel_instance.setBus(&_bus_instance);
    setPanel(&_panel_instance);
  }
};

// Create display instance
static LGFX tft;

// Wrapper voor compatibiliteit met je oude code
class DisplayWrapper {
public:
  void begin() { tft.begin(); tft.setBrightness(128); }
  void fillScreen(uint16_t color) { tft.fillScreen(color); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { tft.fillRect(x, y, w, h, color); }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { tft.drawRect(x, y, w, h, color); }
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) { tft.drawRoundRect(x, y, w, h, r, color); }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) { tft.fillRoundRect(x, y, w, h, r, color); }
  void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) { tft.drawTriangle(x0, y0, x1, y1, x2, y2, color); }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) { tft.fillTriangle(x0, y0, x1, y1, x2, y2, color); }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { tft.drawFastVLine(x, y, h, color); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { tft.drawFastHLine(x, y, w, color); }
  void setCursor(int16_t x, int16_t y) { tft.setCursor(x, y); }
  void setTextColor(uint16_t c, uint16_t bg) { tft.setTextColor(c, bg); }
  void setTextSize(uint8_t s) { tft.setTextSize(s); }
  void print(const char* str) { tft.print(str); }
  void getTextBounds(char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) { 
    tft.getTextBounds(str, x, y, x1, y1, w, h); 
  }
  void setFont(const void* f) { /* LovyanGFX gebruikt andere fonts */ }
  void drawPixel(int16_t x, int16_t y, uint16_t color) { tft.drawPixel(x, y, color); }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) { tft.drawLine(x0, y0, x1, y1, color); }
};

// Canvas wrapper
class CanvasWrapper {
private:
  LGFX_Sprite* sprite;
  int16_t _x, _y;
public:
  CanvasWrapper(int16_t w, int16_t h, DisplayWrapper* parent, int16_t x, int16_t y) : _x(x), _y(y) {
    sprite = new LGFX_Sprite(&tft);
    sprite->createSprite(w, h);
  }
  
  void begin() { sprite->fillScreen(0); }
  void fillScreen(uint16_t color) { sprite->fillScreen(color); }
  void flush() { sprite->pushSprite(_x, _y); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) { sprite->fillRect(x, y, w, h, color); }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color) { sprite->fillRoundRect(x, y, w, h, r, color); }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) { sprite->fillTriangle(x0, y0, x1, y1, x2, y2, color); }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { sprite->drawFastHLine(x, y, w, color); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) { sprite->drawPixel(x, y, color); }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) { sprite->drawLine(x0, y0, x1, y1, color); }
};

// Global instances
static DisplayWrapper gfx_wrapper;
DisplayWrapper *gfx = &gfx_wrapper;

// ==================== JOUW LAYOUT (480x272) ====================
const int L_PANE_W = 200;
const int L_PANE_H = 272;
const int L_PANE_X = 10;
const int L_PANE_Y = 0;

const int L_PAD      = 12;
const int L_HEADER_H = 70;

const int L_CANVAS_W = L_PANE_W - 2 * L_PAD;
const int L_CANVAS_H = 170;
const int L_CANVAS_X = L_PANE_X + L_PAD;
const int L_CANVAS_Y = L_PANE_Y + L_PAD + L_HEADER_H;

static CanvasWrapper cv_instance(L_CANVAS_W, L_CANVAS_H, gfx, L_CANVAS_X, L_CANVAS_Y);
CanvasWrapper *cv = &cv_instance;

const int R_WIN_X = L_PANE_X + L_PANE_W;
const int R_WIN_Y = 0;
const int R_WIN_W = 480 - R_WIN_X;
const int R_WIN_H = 272;

// ==================== JOUW CODE (onveranderd) ====================

static inline void HLineClamped(int x, int y, int w, uint16_t col){
  if (y < 0 || y >= L_CANVAS_H || w <= 0) return;
  int xx=x, ww=w;
  if (xx < 0) { ww += xx; xx = 0; }
  if (xx+ww > L_CANVAS_W) ww = L_CANVAS_W - xx;
  if (ww > 0) cv->drawFastHLine(xx, y, ww, col);
}

static inline void fillRectCenterClamped(int cx, int y, int w, int h, uint16_t col) {
  int x = cx - w/2;
  int yy = y, hh = h;
  if (yy < 0) { hh += yy; yy = 0; }
  if (yy + hh > L_CANVAS_H) hh = L_CANVAS_H - yy;
  if (hh > 0 && w > 0) cv->fillRect(x, yy, w, hh, col);
}

static inline void fillRoundRectCenterClamped(int cx, int y, int w, int h, int r, uint16_t col) {
  int x = cx - w/2;
  int yy = y, hh = h;
  if (yy < 0) { hh += yy; yy = 0; }
  if (yy + hh > L_CANVAS_H) hh = L_CANVAS_H - yy;
  if (hh > 0) cv->fillRoundRect(x, yy, w, hh, r, col);
}

static inline void fillQuad2(int x1,int y1,int x2,int y2,int x3,int y3,int x4,int y4,uint16_t c){
  cv->fillTriangle(x1,y1,x2,y2,x3,y3,c);
  cv->fillTriangle(x3,y3,x4,y4,x1,y1,c);
}

static inline void bandSegCentered(int xa,int ya,int xb,int yb,float wa,float wb,uint16_t col){
  float dx = xb-xa, dy = yb-ya;
  float L = sqrtf(dx*dx+dy*dy); if (L<1e-3f) return;
  float nx=-dy/L, ny=dx/L; float ha=0.5f*wa, hb=0.5f*wb;
  int axL=(int)roundf(xa+nx*ha), ayL=(int)roundf(ya+ny*ha);
  int axR=(int)roundf(xa-nx*ha), ayR=(int)roundf(ya-ny*ha);
  int bxL=(int)roundf(xb+nx*hb), byL=(int)roundf(yb+ny*hb);
  int bxR=(int)roundf(xb-nx*hb), byR=(int)roundf(yb-ny*hb);
  fillQuad2(axL,ayL,bxL,byL,bxR,byR,axR,ayR,col);
}

static inline void drawVEdges(int triBaseY, int triApexY, uint16_t col){
  int apexX=L_CANVAS_W/2;
  int apexY=max(0,triApexY - 7);
  int baseY=min(L_CANVAS_H-1, triBaseY + 11);
  int leftBaseX  = L_CANVAS_W/2 - (int)round(16*1.25f*1.70f*1.31f/2.0f);
  int rightBaseX = L_CANVAS_W/2 + (int)round(16*1.25f*1.70f*1.31f/2.0f);
  const float wStart=1.0f, wPeak=8.0f;
  bandSegCentered(leftBaseX, baseY, apexX, apexY, wStart, wPeak, col);
  bandSegCentered(rightBaseX, baseY, apexX, apexY, wStart, wPeak, col);
}

// Rest van je functies blijven EXACT HETZELFDE...
void drawVibeLightning(bool leftSide) {
  if (!vibeState) return;
  
  const int lightningX = leftSide ? 6 : (L_CANVAS_W - 6);
  const int startY = L_CANVAS_H / 2 + 5;
  const int endY = L_CANVAS_H - 10;
  const int height = endY - startY;
  const int segments = 8;
  const int zigWidth = 4;
  const uint16_t vibeColor = 0xFBE0;
  const uint16_t glowColor = 0x7800;
  
  for (int seg = 0; seg < segments; seg++) {
    float t1 = (float)seg / (float)segments;
    float t2 = (float)(seg + 1) / (float)segments;
    int y1 = startY + (int)(t1 * height);
    int y2 = startY + (int)(t2 * height);
    int x1, x2;
    if (seg % 2 == 0) {
      x1 = lightningX;
      x2 = lightningX + (leftSide ? -zigWidth : zigWidth);
    } else {
      x1 = lightningX + (leftSide ? -zigWidth : zigWidth);
      x2 = lightningX + (leftSide ? zigWidth/2 : -zigWidth/2);
    }
    drawZigzagSegment(x1, y1, x2, y2, vibeColor, glowColor);
  }
}

void drawZigzagSegment(int x1, int y1, int x2, int y2, uint16_t mainColor, uint16_t glowColor) {
  cv->drawLine(x1-1, y1, x2-1, y2, glowColor);
  cv->drawLine(x1+1, y1, x2+1, y2, glowColor);
  cv->drawLine(x1, y1-1, x2, y2-1, glowColor);
  cv->drawLine(x1, y1+1, x2, y2+1, glowColor);
  cv->drawLine(x1, y1, x2, y2, mainColor);
}

void drawSuctionSymbol(bool leftSide) {
  if (!suctionState) return;
  
  const int symbolX = leftSide ? 8 : (L_CANVAS_W - 8);
  const int topY = 10;
  const int bottomY = L_CANVAS_H / 2 - 5;
  const int height = bottomY - topY;
  const int curveWidth = 6;
  const uint16_t suctionColor = CFG.SPEEDBAR_BORDER;
  const uint16_t glowColor = 0x0410;
  
  for (int i = 0; i < height; i++) {
    float t = (float)i / (float)height;
    int y = topY + i;
    float curve = 4.0f * t * (1.0f - t);
    int offset = (int)(curve * curveWidth);
    
    if (leftSide) {
      cv->drawPixel(symbolX + offset - 1, y, glowColor);
      cv->drawPixel(symbolX + offset + 1, y, glowColor);
      cv->drawPixel(symbolX + offset, y - 1, glowColor);
      cv->drawPixel(symbolX + offset, y + 1, glowColor);
      cv->drawPixel(symbolX + offset, y, suctionColor);
    } else {
      cv->drawPixel(symbolX - offset - 1, y, glowColor);
      cv->drawPixel(symbolX - offset + 1, y, glowColor);
      cv->drawPixel(symbolX - offset, y - 1, glowColor);
      cv->drawPixel(symbolX - offset, y + 1, glowColor);
      cv->drawPixel(symbolX - offset, y, suctionColor);
    }
  }
}

void drawSpeedBarTop(uint8_t step, uint8_t stepsTotal){
  int barX = L_PANE_X;
  int barW = L_PANE_W;
  int barH = 10;
  int barY = max(2, L_PANE_Y - (barH + 3));

  gfx->fillRect(barX-2, barY-2, barW+4, barH+4, CFG.COL_BG);
  gfx->drawRect(barX, barY, barW, barH, CFG.SPEEDBAR_BORDER);

  if (stepsTotal < 2) stepsTotal = 2;
  float t = (float)step / (float)(stepsTotal - 1);
  int fillW = (int)round(t * (barW - 2));

  for (int x=0; x<fillW; ++x){
    float u = (float)x / max(1, fillW-1);
    uint8_t R = 0;
    uint8_t G = (uint8_t)roundf(120.0f + u * (255.0f - 120.0f));
    uint8_t B = (uint8_t)roundf(255.0f - u * (255.0f - 120.0f));
    uint16_t c = RGB565u8(R,G,B);
    gfx->drawFastVLine(barX+1+x, barY+1, barH-2, c);
  }

  gfx->setTextSize(1);
  gfx->setTextColor(CFG.COL_BRAND, CFG.COL_BG);
  const char *txt = "Draak";
  
  int16_t x1,y1; uint16_t w,h;
  gfx->getTextBounds((char*)txt, 0, 0, &x1, &y1, &w, &h);
  int desiredTy = barY - 14 + 6;
  if (desiredTy < 0) desiredTy = 0;
  int tx = barX + (barW - (int)w)/2;
  int ty = desiredTy;
  int clearTop = max(0, ty - 2);
  int clearH = max(0, (barY - 2) - clearTop);
  if (clearH > 0) gfx->fillRect(barX-2, clearTop, barW+4, clearH, CFG.COL_BG);
  gfx->setCursor(tx, ty);
  gfx->print(txt);
}

void drawLeftFrame(){
  gfx->drawRoundRect(L_PANE_X+0, L_PANE_Y+0, L_PANE_W, L_PANE_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(L_PANE_X+2, L_PANE_Y+2, L_PANE_W-4, L_PANE_H-4, 10, CFG.COL_FRAME);
  gfx->fillRect(L_PANE_X + L_PAD, L_PANE_Y + L_PAD, L_CANVAS_W, L_HEADER_H, CFG.COL_BG);
}

void drawVacArrowHeader(bool filledUp){
  int headerX = L_PANE_X + L_PAD;
  int headerY = L_PANE_Y + L_PAD;
  int headerW = L_CANVAS_W;
  int boxW = 30, boxH = 30;
  int boxX = headerX + (headerW - boxW)/2;
  int boxY = headerY + 2;

  gfx->fillRect(boxX-2, boxY-2, boxW+4, boxH+4, CFG.COL_BG);

  uint16_t col = CFG.COL_ARROW;
  uint16_t glow = CFG.COL_ARROW_GLOW;
  int ix=boxX+3, iy=boxY+3, iw=boxW-6, ih=boxH-6;
  int headH = max(6, (ih * 60) / 100);
  int stemH = ih - headH;
  int stemW = max(4, (iw * 34) / 100);
  int apexX = ix + iw/2;
  int apexY = iy;
  int baseY = iy + headH;
  int stemX = ix + (iw - stemW)/2;

  gfx->drawTriangle(ix-1, baseY-1, ix+iw+1, baseY-1, apexX, apexY-1, glow);
  gfx->drawTriangle(ix, baseY, ix+iw, baseY, apexX, apexY, glow);

  if (filledUp) {
    gfx->fillTriangle(ix, baseY, ix+iw, baseY, apexX, apexY, col);
    int inset = 4;
    gfx->drawTriangle(ix+inset, baseY-inset, ix+iw-inset, baseY-inset, apexX, apexY+inset, col);
    gfx->drawTriangle(ix+inset+2, baseY-inset-2, ix+iw-(inset+2), baseY-(inset+2), apexX, apexY+(inset+2), col);
    gfx->fillRoundRect(stemX, baseY, stemW, stemH, 2, col);
    int ventW = stemW-2; if (ventW<2) ventW=2;
    int ventX = stemX+1;
    gfx->fillRect(ventX, baseY + stemH/3 - 1, ventW, 1, CFG.COL_BG);
    gfx->fillRect(ventX, baseY + (2*stemH)/3, ventW, 1, CFG.COL_BG);
  } else {
    for (int k=0;k<2;k++){
      gfx->drawTriangle(ix+k, baseY-k, ix+iw-k, baseY-k, apexX, apexY+k, col);
    }
    gfx->drawRoundRect(stemX, baseY, stemW, stemH, 2, col);
    int inset = 4;
    gfx->drawTriangle(ix+inset, baseY-inset, ix+iw-inset, baseY-inset, apexX, apexY+inset, col);
  }
}

// Animatie shapes
static const float SCALE = 1.25f;
static const int BOTTOM_MARGIN = 4;
static const float ROD_THICKEN = 1.70f;
static const int BASE_SL_SHAFT_W = 30;
static const int BASE_SL_SHAFT_H = 68;
static const int BASE_SL_CAP_W = 50;
static const int BASE_SL_CAP_H = 32;
static const int BASE_SL_CAP_R = 10;
static const int BASE_TIP_TRI_W = 16;
static const int BASE_TIP_TRI_H = 12;
static const int BASE_TIP_RECT_W = 12;
static const int BASE_TIP_RECT_H = 12;
static const int SL_SHAFT_W = (int)round(BASE_SL_SHAFT_W * SCALE);
static const int SL_SHAFT_H_BASE = (int)round(BASE_SL_SHAFT_H * SCALE);
static const int SL_CAP_W = (int)round(BASE_SL_CAP_W * SCALE);
static const int SL_CAP_H = (int)round(BASE_SL_CAP_H * SCALE);
static const int SL_CAP_R = (int)round(BASE_SL_CAP_R * SCALE);
static const int TIP_TRI_W_RAW = (int)round(BASE_TIP_TRI_W * SCALE * ROD_THICKEN);
static const int TIP_TRI_H_RAW = (int)round(BASE_TIP_TRI_H * SCALE);
static const int TIP_RECT_W = (int)round(BASE_TIP_RECT_W * SCALE * ROD_THICKEN);
static const int TIP_RECT_MIN_H = (int)round(BASE_TIP_RECT_H * SCALE);
static const int V_INSET_IN_CAP = 6;
static const float V_WIDTH_SCALE = 1.31f;
static const float V_HEIGHT_SCALE = 0.74f;
static const int TIP_TRI_W = max(3, (int)round(TIP_TRI_W_RAW * V_WIDTH_SCALE));
static const int TIP_TRI_H_EFF = max(2, (int)round(TIP_TRI_H_RAW * V_HEIGHT_SCALE));
static const int SEAM_OVERLAP_PX = V_INSET_IN_CAP;

void drawSleeveFixedTop(int capY, uint16_t colTan) {
  const int cx = L_CANVAS_W/2;
  const int topY = 20;
  int shaftH = capY - topY; if (shaftH < 1) shaftH = 1;
  fillRectCenterClamped(cx, topY, SL_SHAFT_W, shaftH, colTan);
  fillRoundRectCenterClamped(cx, capY, SL_CAP_W, SL_CAP_H, SL_CAP_R, colTan);
  const int TRAP_H=17, TRAP_INSET_BASE=3, TRAP_TAPER_PX=4;
  int baseY = topY;
  int topTrapY = baseY - TRAP_H;
  int baseWTrap = max(4, SL_SHAFT_W - 2 * TRAP_INSET_BASE);
  int topWTrap = max(4, baseWTrap - 2 * TRAP_TAPER_PX);
  if (!(baseY <= 0 || topY >= L_CANVAS_H)) {
    int tY = topTrapY; if (tY < 0) tY = 0;
    int bY = baseY; if (bY > L_CANVAS_H) bY = L_CANVAS_H;
    int H = bY - tY;
    for (int yy = tY; yy < bY; ++yy) {
      float t = float(yy - tY) / float(H<=0?1:H);
      int w = (int)round((float)topWTrap + t * (float)(baseWTrap - topWTrap));
      int x = cx - w/2;
      HLineClamped(x, yy, w, colTan);
    }
  }
}

void drawRodFromCap_Vinside_NoSeam(int capY, int drawBaselineY, uint16_t rodCol, uint16_t edgeCol) {
  const int cx = L_CANVAS_W/2;
  const int SL_CAP_H = (int)round(BASE_SL_CAP_H * SCALE);
  const int capBottomY = capY + SL_CAP_H;
  const int rodTopY = capBottomY - V_INSET_IN_CAP;
  int rodH = (drawBaselineY - rodTopY);
  if (rodH < TIP_RECT_MIN_H) rodH = TIP_RECT_MIN_H;
  fillRectCenterClamped(cx, rodTopY, TIP_RECT_W, rodH, rodCol);

  const int overlapY = capBottomY - V_INSET_IN_CAP;
  const int sideW = max(0, (TIP_RECT_W - TIP_TRI_W) / 2);
  if (sideW > 0) {
    int leftMaskX = cx - TIP_RECT_W/2;
    int rightMaskX = cx + TIP_TRI_W/2;
    for (int yy=overlapY; yy<overlapY+V_INSET_IN_CAP; ++yy){
      HLineClamped(leftMaskX, yy, sideW, CFG.COL_TAN);
      HLineClamped(rightMaskX, yy, sideW, CFG.COL_TAN);
    }
  }
  const int triBaseY = capBottomY - V_INSET_IN_CAP;
  const int triApexY = triBaseY - TIP_TRI_H_EFF;
  int leftX = cx - TIP_TRI_W/2;
  int rightX = cx + TIP_TRI_W/2;
  cv->fillTriangle(leftX, triBaseY, rightX, triBaseY, cx, triApexY, rodCol);
  drawVEdges(triBaseY, triApexY, edgeCol);
}

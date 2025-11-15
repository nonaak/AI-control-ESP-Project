#include <math.h>
#include "display.h"

// ------------------ Display pinout ------------------
//const int LCD_DC   = 2;
//const int LCD_CS   = 15;
//const int LCD_SCK  = 14;
//const int LCD_MOSI = 13;
//const int LCD_MISO = GFX_NOT_DEFINED;
//const int LCD_RST  = -1;
//const int LCD_ROT  = 1;           // landscape
//const int LCD_BL   = 27;

// ------------------ GFX + Canvas ------------------
//Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, LCD_MISO);
//Arduino_GFX     *gfx = new Arduino_ST7789(bus, LCD_RST, LCD_ROT, true /*IPS*/);

// Jingcai ESP32-4827S043R RGB parallel setup
const int LCD_BL = 2;

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */);

Arduino_RPi_DPI_RGBPanel *gfx = new Arduino_RPi_DPI_RGBPanel(
    bus,
    480 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 43 /* hsync_back_porch */,
    272 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 12 /* vsync_back_porch */);

// Linker paneel
//const int L_PANE_W = 108;
//const int L_PANE_H = 196;
//const int L_PANE_X = 4;
//const int L_PANE_Y = 44;

// Binnenmarges + header (pijl-zone)
c//onst int L_PAD      = 6;
//const int L_HEADER_H = 36;   // ruimte voor pijl

// Animatie-canvas
//const int L_CANVAS_W = L_PANE_W - 2 * L_PAD;   // 96
//const int L_CANVAS_H = 136;

const int L_PANE_W = 200;
const int L_PANE_H = 272;
const int L_PANE_X = 10;
const int L_PANE_Y = 0;
const int L_PAD      = 12;
const int L_HEADER_H = 70;
const int L_CANVAS_W = L_PANE_W - 2 * L_PAD;   // 176
const int L_CANVAS_H = 170;
const int L_CANVAS_X = L_PANE_X + L_PAD;
const int L_CANVAS_Y = L_PANE_Y + L_PAD + L_HEADER_H;

Arduino_Canvas  *cv  = new Arduino_Canvas(L_CANVAS_W, L_CANVAS_H, gfx, L_CANVAS_X, L_CANVAS_Y);

// Rechter menu-paneel
const int R_WIN_X = L_PANE_X + L_PANE_W;
const int R_WIN_Y = 0;
const int R_WIN_W = 480 - R_WIN_X;  // was: 320
const int R_WIN_H = 272;             // was: 240
//const int R_WIN_W = 320 - R_WIN_X;
//const int R_WIN_H = 240;

// ---------- interne helpers ----------
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

// ------------- Vibe lightning zigzag (bottom half) -------------
void drawVibeLightning(bool leftSide) {
  if (!vibeState) return; // Only draw when Vibe is active
  
  // Position on left or right edge of canvas - BOTTOM HALF ONLY
  const int lightningX = leftSide ? 6 : (L_CANVAS_W - 6);
  const int startY = L_CANVAS_H / 2 + 5;  // Start in middle
  const int endY = L_CANVAS_H - 10;      // End near bottom
  const int height = endY - startY;
  
  // Zigzag parameters
  const int segments = 8;  // Number of zigzag segments
  const int zigWidth = 4;  // How far left/right the zigzag goes
  
  // Colors
  const uint16_t vibeColor = 0xFBE0;  // Light red
  const uint16_t glowColor = 0x7800;  // Dimmer red glow
  
  // Draw zigzag from top to bottom
  for (int seg = 0; seg < segments; seg++) {
    // Calculate start and end points of this segment
    float t1 = (float)seg / (float)segments;
    float t2 = (float)(seg + 1) / (float)segments;
    
    int y1 = startY + (int)(t1 * height);
    int y2 = startY + (int)(t2 * height);
    
    // Zigzag pattern: alternate left and right
    int x1, x2;
    if (seg % 2 == 0) {
      // Even segments: go from center to left/right
      x1 = lightningX;
      x2 = lightningX + (leftSide ? -zigWidth : zigWidth);
    } else {
      // Odd segments: go from left/right back to center-ish
      x1 = lightningX + (leftSide ? -zigWidth : zigWidth);
      x2 = lightningX + (leftSide ? zigWidth/2 : -zigWidth/2);
    }
    
    // Draw line segment with glow
    drawZigzagSegment(x1, y1, x2, y2, vibeColor, glowColor);
  }
  
  // Add some sparkle effect along the zigzag
  static uint32_t lastSparkle = 0;
  if (millis() - lastSparkle > 150) { // Update every 150ms
    lastSparkle = millis();
    
    // Add 2-3 random sparkle pixels along the zigzag path
    for (int i = 0; i < 3; i++) {
      int sparkleY = startY + random(height);
      int sparkleX = lightningX + random(-zigWidth-1, zigWidth+2);
      if (sparkleX >= 0 && sparkleX < L_CANVAS_W && sparkleY >= 0 && sparkleY < L_CANVAS_H) {
        cv->drawPixel(sparkleX, sparkleY, random(2) ? vibeColor : glowColor);
      }
    }
  }
}

// Helper function to draw a zigzag segment with glow
void drawZigzagSegment(int x1, int y1, int x2, int y2, uint16_t mainColor, uint16_t glowColor) {
  // Draw glow first (thicker line)
  cv->drawLine(x1-1, y1, x2-1, y2, glowColor);
  cv->drawLine(x1+1, y1, x2+1, y2, glowColor);
  cv->drawLine(x1, y1-1, x2, y2-1, glowColor);
  cv->drawLine(x1, y1+1, x2, y2+1, glowColor);
  
  // Draw main line
  cv->drawLine(x1, y1, x2, y2, mainColor);
}

// ------------- Suction symbol )( (top half) -------------
void drawSuctionSymbol(bool leftSide) {
  if (!suctionState) return; // Only draw when Suction is active
  
  // Position on left or right edge of canvas - TOP HALF ONLY
  const int symbolX = leftSide ? 8 : (L_CANVAS_W - 8);
  const int topY = 10;                    // Start near top
  const int bottomY = L_CANVAS_H / 2 - 5; // End before middle
  const int height = bottomY - topY;
  const int curveWidth = 6; // Width of the )( curves
  
  // Colors - gebruik cyaan voor suction
  const uint16_t suctionColor = CFG.SPEEDBAR_BORDER;  // Cyaan (0x07FF)
  const uint16_t glowColor = 0x0410;  // Donker cyaan glow
  
  // Draw curved lines to form )( shape
  for (int i = 0; i < height; i++) {
    float t = (float)i / (float)height; // 0 to 1
    int y = topY + i;
    
    // Calculate curve offset - parabolic curve
    float curve = 4.0f * t * (1.0f - t); // Peak at t=0.5
    int offset = (int)(curve * curveWidth);
    
    if (leftSide) {
      // Draw ')' shape - curve opening to the right
      // Add glow effect
      cv->drawPixel(symbolX + offset - 1, y, glowColor);
      cv->drawPixel(symbolX + offset + 1, y, glowColor);
      cv->drawPixel(symbolX + offset, y - 1, glowColor);
      cv->drawPixel(symbolX + offset, y + 1, glowColor);
      // Main pixel
      cv->drawPixel(symbolX + offset, y, suctionColor);
    } else {
      // Draw '(' shape - curve opening to the left  
      // Add glow effect
      cv->drawPixel(symbolX - offset - 1, y, glowColor);
      cv->drawPixel(symbolX - offset + 1, y, glowColor);
      cv->drawPixel(symbolX - offset, y - 1, glowColor);
      cv->drawPixel(symbolX - offset, y + 1, glowColor);
      // Main pixel
      cv->drawPixel(symbolX - offset, y, suctionColor);
    }
  }
}

// ------------- publieke tekenfuncties -------------
void drawSpeedBarTop(uint8_t step, uint8_t stepsTotal){
  int barX = L_PANE_X;
  int barW = L_PANE_W;
  int barH = 10;
  int barY = max(2, L_PANE_Y - (barH + 3));

  gfx->fillRect(barX-2, barY-2, barW+4, barH+4, CFG.COL_BG);
  gfx->drawRect(barX, barY, barW, barH, CFG.SPEEDBAR_BORDER);

  if (stepsTotal < 2) stepsTotal = 2;
  float t = (float)step / (float)(stepsTotal - 1); // 0..1
  int fillW = (int)round(t * (barW - 2));

  for (int x=0; x<fillW; ++x){
    float u = (float)x / max(1, fillW-1);
    uint8_t R = 0;
    uint8_t G = (uint8_t)roundf(120.0f + u * (255.0f - 120.0f));
    uint8_t B = (uint8_t)roundf(255.0f - u * (255.0f - 120.0f));
    uint16_t c = RGB565u8(R,G,B);
    gfx->drawFastVLine(barX+1+x, barY+1, barH-2, c);
  }

  // "Draak" net lager (+6), exact als monolith-versie
#if USE_ADAFRUIT_FONTS
  gfx->setFont(&FONT_ITEM);
  gfx->setTextSize(1);
#else
  gfx->setFont(nullptr);
  gfx->setTextSize(1);
#endif
  gfx->setTextColor(CFG.COL_BRAND, CFG.COL_BG);

  const char *txt = "Draak";
  int16_t x1,y1; uint16_t w,h;
  gfx->getTextBounds((char*)txt, 0, 0, &x1, &y1, &w, &h);

  int desiredTy = barY - 14 + 6;
  if (desiredTy < 0) desiredTy = 0;

  int tx = barX + (barW - (int)w)/2;
  int ty = desiredTy;

  int clearTop = max(0, ty - 2);
  int clearH   = max(0, (barY - 2) - clearTop);
  if (clearH > 0) gfx->fillRect(barX-2, clearTop, barW+4, clearH, CFG.COL_BG);

  gfx->setCursor(tx, ty);
  gfx->print(txt);
}

void drawLeftFrame(){
  gfx->drawRoundRect(L_PANE_X+0, L_PANE_Y+0, L_PANE_W,   L_PANE_H,   12, CFG.COL_FRAME2);
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

  uint16_t col   = CFG.COL_ARROW;
  uint16_t glow  = CFG.COL_ARROW_GLOW;
  int ix=boxX+3, iy=boxY+3, iw=boxW-6, ih=boxH-6;

  int headH = max(6, (ih * 60) / 100);
  int stemH = ih - headH;
  int stemW = max(4, (iw * 34) / 100);

  int apexX = ix + iw/2;
  int apexY = iy;
  int baseY = iy + headH;
  int stemX = ix + (iw - stemW)/2;

  // GLOW contour
  gfx->drawTriangle(ix-1, baseY-1, ix+iw+1, baseY-1, apexX, apexY-1, glow);
  gfx->drawTriangle(ix,   baseY,   ix+iw,   baseY,   apexX, apexY,   glow);

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

// ================== ANIMATIE SHAPE (exact gelijk) ==================
static const float SCALE          = 1.25f;
static const int   BOTTOM_MARGIN  = 4;
static const float ROD_THICKEN    = 1.70f;

static const int BASE_SL_SHAFT_W  = 30;
static const int BASE_SL_SHAFT_H  = 68;
static const int BASE_SL_CAP_W    = 50;
static const int BASE_SL_CAP_H    = 32;
static const int BASE_SL_CAP_R    = 10;

static const int BASE_TIP_TRI_W   = 16;
static const int BASE_TIP_TRI_H   = 12;
static const int BASE_TIP_RECT_W  = 12;
static const int BASE_TIP_RECT_H  = 12;

static const int SL_SHAFT_W      = (int)round(BASE_SL_SHAFT_W * SCALE);
static const int SL_SHAFT_H_BASE = (int)round(BASE_SL_SHAFT_H * SCALE);
static const int SL_CAP_W        = (int)round(BASE_SL_CAP_W   * SCALE);
static const int SL_CAP_H        = (int)round(BASE_SL_CAP_H   * SCALE);
static const int SL_CAP_R        = (int)round(BASE_SL_CAP_R   * SCALE);

static const int TIP_TRI_W_RAW   = (int)round(BASE_TIP_TRI_W  * SCALE * ROD_THICKEN);
static const int TIP_TRI_H_RAW   = (int)round(BASE_TIP_TRI_H  * SCALE);
static const int TIP_RECT_W      = (int)round(BASE_TIP_RECT_W * SCALE * ROD_THICKEN);
static const int TIP_RECT_MIN_H  = (int)round(BASE_TIP_RECT_H * SCALE);

static const int   V_INSET_IN_CAP  = 6;
static const float V_WIDTH_SCALE   = 1.31f;
static const float V_HEIGHT_SCALE  = 0.74f;
static const int   TIP_TRI_W       = max(3, (int)round(TIP_TRI_W_RAW * V_WIDTH_SCALE));
static const int   TIP_TRI_H_EFF   = max(2, (int)round(TIP_TRI_H_RAW * V_HEIGHT_SCALE));
static const int   SEAM_OVERLAP_PX = V_INSET_IN_CAP;

void drawSleeveFixedTop(int capY, uint16_t colTan) {
  const int cx = L_CANVAS_W/2;
  const int topY = 20;
  int shaftH = capY - topY; if (shaftH < 1) shaftH = 1;
  fillRectCenterClamped(cx, topY, SL_SHAFT_W, shaftH, colTan);
  fillRoundRectCenterClamped(cx, capY, SL_CAP_W, SL_CAP_H, SL_CAP_R, colTan);
  const int TRAP_H=17, TRAP_INSET_BASE=3, TRAP_TAPER_PX=4;
  int baseY     = topY;
  int topTrapY  = baseY - TRAP_H;
  int baseWTrap = max(4, SL_SHAFT_W - 2 * TRAP_INSET_BASE);
  int topWTrap  = max(4, baseWTrap - 2 * TRAP_TAPER_PX);
  // fillTaperTop inline:
  if (!(baseY <= 0 || topY >= L_CANVAS_H)) {
    int tY = topTrapY; if (tY < 0) tY = 0;
    int bY = baseY;    if (bY > L_CANVAS_H) bY = L_CANVAS_H;
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
  const int rodTopY    = capBottomY - V_INSET_IN_CAP;
  int rodH = (drawBaselineY - rodTopY);
  if (rodH < TIP_RECT_MIN_H) rodH = TIP_RECT_MIN_H;
  fillRectCenterClamped(cx, rodTopY, TIP_RECT_W, rodH, rodCol);

  const int overlapY = capBottomY - V_INSET_IN_CAP;
  const int sideW = max(0, (TIP_RECT_W - TIP_TRI_W) / 2);
  if (sideW > 0) {
    int leftMaskX  = cx - TIP_RECT_W/2;
    int rightMaskX = cx + TIP_TRI_W/2;
    for (int yy=overlapY; yy<overlapY+V_INSET_IN_CAP; ++yy){
      HLineClamped(leftMaskX, yy, sideW, CFG.COL_TAN);
      HLineClamped(rightMaskX, yy, sideW, CFG.COL_TAN);
    }
  }
  const int triBaseY = capBottomY - V_INSET_IN_CAP;
  const int triApexY = triBaseY - TIP_TRI_H_EFF;
  int leftX  = cx - TIP_TRI_W/2;
  int rightX = cx + TIP_TRI_W/2;
  cv->fillTriangle(leftX, triBaseY, rightX, triBaseY, cx, triApexY, rodCol);
  // randen
  drawVEdges(triBaseY, triApexY, edgeCol);
}

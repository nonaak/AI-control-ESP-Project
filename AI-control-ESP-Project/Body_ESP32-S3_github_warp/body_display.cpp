#include <math.h>
#include "body_display.h"

// T-HMI display: Direct TFT_eSPI gebruik (geen wrapper)
extern TFT_eSPI tft; // Gedefinieerd in main .ino

// Linker paneel (body sensor weergave)
const int L_PANE_W = 108;
const int L_PANE_H = 196;
const int L_PANE_X = 4;
const int L_PANE_Y = 44;

// Binnenmarges + header (sensor status zone)
const int L_PAD      = 6;
const int L_HEADER_H = 36;   // ruimte voor sensor status

// Body sensor canvas (T-HMI direkte drawing)
const int L_CANVAS_W = L_PANE_W - 2 * L_PAD;   // 96
const int L_CANVAS_H = 136;
const int L_CANVAS_X = L_PANE_X + L_PAD;
const int L_CANVAS_Y = L_PANE_Y + L_PAD + L_HEADER_H;

// T-HMI Canvas wrapper (simuleert Arduino_Canvas)
class TFT_Canvas_Wrapper {
public:
  void fillScreen(uint16_t color) {
    tft.fillRect(L_CANVAS_X, L_CANVAS_Y, L_CANVAS_W, L_CANVAS_H, color);
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    tft.drawRect(L_CANVAS_X + x, L_CANVAS_Y + y, w, h, color);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    tft.fillRect(L_CANVAS_X + x, L_CANVAS_Y + y, w, h, color);
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    tft.drawLine(L_CANVAS_X + x0, L_CANVAS_Y + y0, L_CANVAS_X + x1, L_CANVAS_Y + y1, color);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    tft.drawFastHLine(L_CANVAS_X + x, L_CANVAS_Y + y, w, color);
  }
  void setCursor(int16_t x, int16_t y) {
    tft.setCursor(L_CANVAS_X + x, L_CANVAS_Y + y);
  }
  void setTextColor(uint16_t color, uint16_t bg) {
    tft.setTextColor(color, bg);
  }
  void setTextSize(uint8_t size) {
    tft.setTextSize(size);
  }
  void setFont(const void* font) {
    // TFT_eSPI font API verschilt
  }
  void print(const char* text) {
    tft.print(text);
  }
  void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[256];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    tft.print(buffer);
  }
  void flush() {
    // TFT_eSPI schrijft direct, geen flush nodig
  }
};
static TFT_Canvas_Wrapper canvas_wrapper;
TFT_Canvas_Wrapper *body_cv = &canvas_wrapper;

// Rechter menu-paneel
const int R_WIN_X = L_PANE_X + L_PANE_W;
const int R_WIN_Y = 0;
const int R_WIN_W = 320 - R_WIN_X;
const int R_WIN_H = 240;

// Global config instantie
BodyConfig BODY_CFG;

// RGB565u8 is al gedefinieerd in body_config.h

// ---------- interne helpers ----------
static inline void HLineClamped(int x, int y, int w, uint16_t col){
  if (y < 0 || y >= L_CANVAS_H || w <= 0) return;
  int xx=x, ww=w;
  if (xx < 0) { ww += xx; xx = 0; }
  if (xx+ww > L_CANVAS_W) ww = L_CANVAS_W - xx;
  if (ww > 0) body_cv->drawFastHLine(xx, y, ww, col);
}
static inline void fillRectCenterClamped(int cx, int y, int w, int h, uint16_t col) {
  int x = cx - w/2;
  int yy = y, hh = h;
  if (yy < 0) { hh += yy; yy = 0; }
  if (yy + hh > L_CANVAS_H) hh = L_CANVAS_H - yy;
  if (hh > 0 && w > 0) body_cv->fillRect(x, yy, w, hh, col);
}

// ------------- publieke tekenfuncties -------------
void drawBodySpeedBarTop(const char* title){
  int barX = L_PANE_X;
  int barW = L_PANE_W;
  int barH = 10;
  int barY = max(2, L_PANE_Y - (barH + 3));

  body_gfx->fillRect(barX-2, barY-2, barW+4, barH+4, BODY_CFG.COL_BG);
  body_gfx->drawRect(barX, barY, barW, barH, BODY_CFG.COL_FRAME2);

  // Gradient fill om body monitoring status te tonen
  for (int x=0; x<barW-2; ++x){
    float u = (float)x / max(1, barW-3);
    uint8_t R = (uint8_t)roundf(255.0f - u * 100.0f);
    uint8_t G = (uint8_t)roundf(120.0f + u * (255.0f - 120.0f));
    uint8_t B = (uint8_t)roundf(120.0f + u * 50.0f);
    uint16_t c = RGB565u8(R,G,B);
    body_gfx->drawFastVLine(barX+1+x, barY+1, barH-2, c);
  }

  // Title text
#if USE_ADAFRUIT_FONTS
  body_gfx->setFont(&FONT_ITEM);
  body_gfx->setTextSize(1);
#else
  // TFT_eSPI gebruikt setTextFont in plaats van setFont
  body_gfx->setTextSize(1);
#endif
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);

  int16_t x1,y1; uint16_t w,h;
  // TFT_eSPI heeft geen getTextBounds, schatting gebruiken
  w = strlen(title) * 6;
  h = 8;

  int desiredTy = barY - 14 + 6;
  if (desiredTy < 0) desiredTy = 0;

  int tx = barX + (barW - (int)w)/2;
  int ty = desiredTy;

  int clearTop = max(0, ty - 2);
  int clearH   = max(0, (barY - 2) - clearTop);
  if (clearH > 0) body_gfx->fillRect(barX-2, clearTop, barW+4, clearH, BODY_CFG.COL_BG);

  body_gfx->setCursor(tx, ty);
  body_gfx->print(title);
}

void drawLeftFrame(){
  body_gfx->drawRoundRect(L_PANE_X+0, L_PANE_Y+0, L_PANE_W,   L_PANE_H,   12, BODY_CFG.COL_FRAME2);
  body_gfx->drawRoundRect(L_PANE_X+2, L_PANE_Y+2, L_PANE_W-4, L_PANE_H-4, 10, BODY_CFG.COL_FRAME);
  body_gfx->fillRect(L_PANE_X + L_PAD, L_PANE_Y + L_PAD, L_CANVAS_W, L_HEADER_H, BODY_CFG.COL_BG);
}

void drawBodySensorHeader(float heartRate, float temperature){
  int headerX = L_PANE_X + L_PAD;
  int headerY = L_PANE_Y + L_PAD;
  int headerW = L_CANVAS_W;

  body_gfx->fillRect(headerX-2, headerY-2, headerW+4, L_HEADER_H+4, BODY_CFG.COL_BG);

  // Hartslag indicator (links)
  int heartX = headerX + 8;
  int heartY = headerY + 8;
  body_gfx->fillCircle(heartX, heartY, 6, BODY_CFG.COL_HEART);
  // TFT_eSPI standaard font
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_gfx->setCursor(heartX + 12, heartY - 3);
  body_gfx->print(String(heartRate, 0));

  // Temperatuur indicator (rechts)
  int tempX = headerX + headerW - 40;
  int tempY = headerY + 8;
  body_gfx->fillCircle(tempX, tempY, 6, BODY_CFG.COL_TEMP);
  body_gfx->setCursor(tempX + 12, tempY - 3);
  body_gfx->print(String(temperature, 1));

  // AI status indicator (onder)
  extern bool aiOverruleActive;
  uint16_t aiColor = aiOverruleActive ? BODY_CFG.COL_AI : BODY_CFG.DOT_GREY;
  int aiX = headerX + headerW/2;
  int aiY = headerY + 24;
  body_gfx->fillCircle(aiX, aiY, 4, aiColor);
  body_gfx->setCursor(aiX - 8, aiY + 8);
  body_gfx->setTextSize(1);
  body_gfx->print(aiOverruleActive ? "AI" : "--");
}

void drawHeartRateGraph(float currentHR, float* history, int historySize){
  if (!history || historySize <= 0) return;
  
  body_cv->fillScreen(BODY_CFG.COL_BG);
  
  // Graph parameters
  int graphY = 10;
  int graphH = 60;
  float minHR = BODY_CFG.heartRateMin;
  float maxHR = BODY_CFG.heartRateMax;
  
  // Draw graph background
  body_cv->drawRect(2, graphY, L_CANVAS_W-4, graphH, BODY_CFG.COL_FRAME);
  
  // Draw heart rate history line
  for (int i = 1; i < historySize && i < L_CANVAS_W-6; i++) {
    if (history[i-1] > 0 && history[i] > 0) {
      int y1 = graphY + graphH - (int)((history[i-1] - minHR) / (maxHR - minHR) * graphH);
      int y2 = graphY + graphH - (int)((history[i] - minHR) / (maxHR - minHR) * graphH);
      if (y1 < graphY) y1 = graphY;
      if (y1 > graphY + graphH) y1 = graphY + graphH;
      if (y2 < graphY) y2 = graphY;
      if (y2 > graphY + graphH) y2 = graphY + graphH;
      body_cv->drawLine(i+2, y1, i+3, y2, BODY_CFG.COL_HEART);
    }
  }
  
  // Current HR value (large text)
  // TFT_eSPI canvas font
  body_cv->setTextSize(2);
  body_cv->setTextColor(BODY_CFG.COL_HEART, BODY_CFG.COL_BG);
  body_cv->setCursor(10, graphY + graphH + 15);
  body_cv->printf("HR: %.0f", currentHR);
  
  body_cv->flush();
}

void drawTemperatureBar(float temp, float minTemp, float maxTemp){
  int barX = 10;
  int barY = 90;
  int barW = L_CANVAS_W - 20;
  int barH = 12;
  
  // Background
  body_cv->fillRect(barX, barY, barW, barH, BODY_CFG.COL_BG);
  body_cv->drawRect(barX, barY, barW, barH, BODY_CFG.COL_FRAME);
  
  // Temperature fill
  float tempRatio = (temp - minTemp) / (maxTemp - minTemp);
  if (tempRatio < 0.0f) tempRatio = 0.0f;
  if (tempRatio > 1.0f) tempRatio = 1.0f;
  int fillW = (int)(tempRatio * (barW - 2));
  
  // Color gradient based on temperature
  uint16_t tempColor;
  if (temp < 36.5f) {
    tempColor = BODY_CFG.DOT_BLUE; // Cold
  } else if (temp > 37.5f) {
    tempColor = BODY_CFG.DOT_RED; // Hot
  } else {
    tempColor = BODY_CFG.COL_TEMP; // Normal
  }
  
  body_cv->fillRect(barX + 1, barY + 1, fillW, barH - 2, tempColor);
  
  // Temperature text
  body_cv->setFont(nullptr);
  body_cv->setTextSize(1);
  body_cv->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_cv->setCursor(barX, barY + barH + 5);
  body_cv->printf("TEMP: %.1f C", temp);
}

void drawGSRIndicator(float gsrLevel, float maxLevel){
  int indicatorX = 10;
  int indicatorY = 115;
  int indicatorW = L_CANVAS_W - 20;
  int indicatorH = 8;
  
  // Background
  body_cv->fillRect(indicatorX, indicatorY, indicatorW, indicatorH, BODY_CFG.COL_BG);
  body_cv->drawRect(indicatorX, indicatorY, indicatorW, indicatorH, BODY_CFG.COL_FRAME);
  
  // GSR level fill
  float gsrRatio = gsrLevel / maxLevel;
  if (gsrRatio < 0.0f) gsrRatio = 0.0f;
  if (gsrRatio > 1.0f) gsrRatio = 1.0f;
  int fillW = (int)(gsrRatio * (indicatorW - 2));
  
  // Color based on stress level
  uint16_t gsrColor;
  if (gsrRatio < 0.3f) {
    gsrColor = BODY_CFG.DOT_GREEN; // Low stress
  } else if (gsrRatio > 0.7f) {
    gsrColor = BODY_CFG.DOT_RED; // High stress
  } else {
    gsrColor = BODY_CFG.DOT_ORANGE; // Medium stress
  }
  
  body_cv->fillRect(indicatorX + 1, indicatorY + 1, fillW, indicatorH - 2, gsrColor);
  
  // GSR text
  body_cv->setFont(nullptr);
  body_cv->setTextSize(1);
  body_cv->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_cv->setCursor(indicatorX, indicatorY + indicatorH + 5);
  body_cv->printf("GSR: %.0f", gsrLevel);
}

void drawAIOverruleStatus(bool active, float trustOverride, float sleeveOverride){
  if (!active) return;
  
  int statusY = L_CANVAS_H - 15;
  body_cv->setFont(nullptr);
  body_cv->setTextSize(1);
  body_cv->setTextColor(BODY_CFG.COL_AI, BODY_CFG.COL_BG);
  body_cv->setCursor(5, statusY);
  body_cv->printf("AI: T%.0f%% S%.0f%%", trustOverride*100, sleeveOverride*100);
}
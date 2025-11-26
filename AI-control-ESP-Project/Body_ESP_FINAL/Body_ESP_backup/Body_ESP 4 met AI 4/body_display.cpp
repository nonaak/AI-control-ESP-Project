#include <math.h>
#include <Arduino_GFX_Library.h>
#include "body_display.h"

// Backlight
#define GFX_BL 45

// 8-bit parallel bus voor SC01 Plus
Arduino_DataBus *bus = new Arduino_ESP32LCD8(
    0   /* DC */, 
    GFX_NOT_DEFINED /* CS */, 
    47  /* WR */, 
    GFX_NOT_DEFINED /* RD */,
    9   /* D0 */, 
    46  /* D1 */, 
    3   /* D2 */, 
    8   /* D3 */, 
    18  /* D4 */, 
    17  /* D5 */, 
    16  /* D6 */, 
    15  /* D7 */
);

// ST7796 display driver (3.5" 480x320)
Arduino_GFX *body_gfx = new Arduino_ST7796(
    bus, 
    4    /* RST */, 
    1    /* rotation (landscape) */, 
    true /* IPS */
);

// Screen dimensions
const int SCR_W = 480;
const int SCR_H = 320;

// ... rest van je code blijft hetzelfde ...
//#include "body_display.h"

// ------------------ Display pinout voor SC01 Plus (RGB Parallel) ------------------
// SC01 Plus gebruikt ST7796 RGB parallel interface (480x320)
//#define GFX_BL 45   // Backlight

// RGB Parallel pins voor SC01 Plus
//Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    //40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
    //45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
    //5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
    //8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
    //0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
    //0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
    //1 /* pclk_active_neg */, 16000000 /* prefer_speed */);

//Arduino_RGB_Display *body_gfx = new Arduino_RGB_Display(
//Arduino_GFX *body_gfx = new Arduino_RGB_Display(
    //480 /* width */, 320 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);

// Screen dimensions voor SC01 Plus
//const int SCR_W = 480;
//const int SCR_H = 320;

// Linker paneel (body sensor weergave) - aangepast voor grotere resolutie
const int L_PANE_W = 160;
const int L_PANE_H = 280;
const int L_PANE_X = 10;
const int L_PANE_Y = 40;

// Binnenmarges + header (sensor status zone)
const int L_PAD      = 8;
const int L_HEADER_H = 48;   // ruimte voor sensor status

// Body sensor canvas
const int L_CANVAS_W = L_PANE_W - 2 * L_PAD;   // 144
const int L_CANVAS_H = 200;
const int L_CANVAS_X = L_PANE_X + L_PAD;
const int L_CANVAS_Y = L_PANE_Y + L_PAD + L_HEADER_H;

// Canvas moet NIET hier aangemaakt worden - body_gfx is nog niet ge-begin()'d!
// Canvas wordt nu aangemaakt in body_gfx4_begin()
Arduino_Canvas  *body_cv  = nullptr;  // Start als nullptr

// Rechter menu-paneel - aangepast voor 480x320
const int R_WIN_X = L_PANE_X + L_PANE_W + 10;
const int R_WIN_Y = 0;
const int R_WIN_W = 480 - R_WIN_X;
const int R_WIN_H = 320;

// Global config instantie
BodyConfig BODY_CFG;

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

// Backlight control
void setBacklight(uint8_t brightness) {
  pinMode(GFX_BL, OUTPUT);
  analogWrite(GFX_BL, brightness);
}

// ------------- publieke tekenfuncties -------------
void drawBodySpeedBarTop(const char* title){
  if (!body_gfx) return;  // Safety check
  int barX = L_PANE_X;
  int barW = L_PANE_W;
  int barH = 12;
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
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
#endif
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);

  int16_t x1,y1; uint16_t w,h;
  body_gfx->getTextBounds((char*)title, 0, 0, &x1, &y1, &w, &h);

  int desiredTy = barY - 16 + 6;
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
  if (!body_gfx) return;  // Safety check
  body_gfx->drawRoundRect(L_PANE_X+0, L_PANE_Y+0, L_PANE_W,   L_PANE_H,   12, BODY_CFG.COL_FRAME2);
  body_gfx->drawRoundRect(L_PANE_X+2, L_PANE_Y+2, L_PANE_W-4, L_PANE_H-4, 10, BODY_CFG.COL_FRAME);
  body_gfx->fillRect(L_PANE_X + L_PAD, L_PANE_Y + L_PAD, L_CANVAS_W, L_HEADER_H, BODY_CFG.COL_BG);
}

void drawBodySensorHeader(float heartRate, float temperature){
  if (!body_gfx) return;  // Safety check
  int headerX = L_PANE_X + L_PAD;
  int headerY = L_PANE_Y + L_PAD;
  int headerW = L_CANVAS_W;

  body_gfx->fillRect(headerX-2, headerY-2, headerW+4, L_HEADER_H+4, BODY_CFG.COL_BG);

  // Hartslag indicator (links)
  int heartX = headerX + 12;
  int heartY = headerY + 12;
  body_gfx->fillCircle(heartX, heartY, 8, BODY_CFG.COL_HEART);
  body_gfx->setFont(nullptr); 
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_gfx->setCursor(heartX + 16, heartY - 4);
  body_gfx->printf("%.0f", heartRate);

  // Temperatuur indicator (rechts)
  int tempX = headerX + headerW - 50;
  int tempY = headerY + 12;
  body_gfx->fillCircle(tempX, tempY, 8, BODY_CFG.COL_TEMP);
  body_gfx->setCursor(tempX + 16, tempY - 4);
  body_gfx->printf("%.1f", temperature);

  // AI status indicator (onder)
  extern bool aiOverruleActive;
  uint16_t aiColor = aiOverruleActive ? BODY_CFG.COL_AI : BODY_CFG.DOT_GREY;
  int aiX = headerX + headerW/2;
  int aiY = headerY + 32;
  body_gfx->fillCircle(aiX, aiY, 6, aiColor);
  body_gfx->setCursor(aiX - 10, aiY + 10);
  body_gfx->setTextSize(1);
  body_gfx->print(aiOverruleActive ? "AI" : "--");
}

void drawHeartRateGraph(float currentHR, float* history, int historySize){
  Serial.println("[DRAW] drawHeartRateGraph START");
  
  if (!history) {
    Serial.println("[DRAW] ERROR: history is NULL");
    return;
  }
  if (historySize <= 0) {
    Serial.println("[DRAW] ERROR: historySize <= 0");
    return;
  }
  if (!body_gfx) {
    Serial.println("[DRAW] ERROR: body_gfx is NULL!");
    return;
  }
  
  // Teken naar body_gfx in plaats van body_cv (canvas werkt niet wegens bus conflict)
  Serial.println("[DRAW] clearing graph area...");
  int graphAreaX = L_PANE_X + L_PAD;
  int graphAreaY = L_PANE_Y + L_PAD + L_HEADER_H + 5;
  body_gfx->fillRect(graphAreaX, graphAreaY, L_CANVAS_W, 130, BODY_CFG.COL_BG);
  Serial.println("[DRAW] clear done");
  
  // Graph parameters (relative to graph area)
  int localGraphY = 15;
  int graphH = 80;
  float minHR = BODY_CFG.heartRateMin;
  float maxHR = BODY_CFG.heartRateMax;
  
  // Absolute coordinates
  int absGraphX = graphAreaX + 2;
  int absGraphY = graphAreaY + localGraphY;
  
  // Draw graph background
  body_gfx->drawRect(absGraphX, absGraphY, L_CANVAS_W-4, graphH, BODY_CFG.COL_FRAME);
  
  // Draw heart rate history line
  for (int i = 1; i < historySize && i < L_CANVAS_W-6; i++) {
    if (history[i-1] > 0 && history[i] > 0) {
      int y1 = absGraphY + graphH - (int)((history[i-1] - minHR) / (maxHR - minHR) * graphH);
      int y2 = absGraphY + graphH - (int)((history[i] - minHR) / (maxHR - minHR) * graphH);
      if (y1 < absGraphY) y1 = absGraphY;
      if (y1 > absGraphY + graphH) y1 = absGraphY + graphH;
      if (y2 < absGraphY) y2 = absGraphY;
      if (y2 > absGraphY + graphH) y2 = absGraphY + graphH;
      body_gfx->drawLine(absGraphX + i, y1, absGraphX + i + 1, y2, BODY_CFG.COL_HEART);
    }
  }
  
  // Current HR value (large text)
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(2);
  body_gfx->setTextColor(BODY_CFG.COL_HEART, BODY_CFG.COL_BG);
  body_gfx->setCursor(graphAreaX + 10, absGraphY + graphH + 20);
  body_gfx->printf("HR: %.0f", currentHR);
  
  Serial.println("[DRAW] drawHeartRateGraph DONE");
}

void drawTemperatureBar(float temp, float minTemp, float maxTemp){
  if (!body_gfx) return;  // Safety check
  int graphAreaX = L_PANE_X + L_PAD;
  int graphAreaY = L_PANE_Y + L_PAD + L_HEADER_H + 5;
  int barX = graphAreaX + 10;
  int barY = graphAreaY + 120;
  int barW = L_CANVAS_W - 20;
  int barH = 16;
  
  // Background
  body_gfx->fillRect(barX, barY, barW, barH, BODY_CFG.COL_BG);
  body_gfx->drawRect(barX, barY, barW, barH, BODY_CFG.COL_FRAME);
  
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
  
  body_gfx->fillRect(barX + 1, barY + 1, fillW, barH - 2, tempColor);
  
  // Temperature text
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_gfx->setCursor(barX, barY + barH + 8);
  body_gfx->printf("TEMP: %.1f C", temp);
}

void drawGSRIndicator(float gsrLevel, float maxLevel){
  if (!body_gfx) return;  // Safety check
  int graphAreaX = L_PANE_X + L_PAD;
  int graphAreaY = L_PANE_Y + L_PAD + L_HEADER_H + 5;
  int indicatorX = graphAreaX + 10;
  int indicatorY = graphAreaY + 155;
  int indicatorW = L_CANVAS_W - 20;
  int indicatorH = 12;
  
  // Background
  body_gfx->fillRect(indicatorX, indicatorY, indicatorW, indicatorH, BODY_CFG.COL_BG);
  body_gfx->drawRect(indicatorX, indicatorY, indicatorW, indicatorH, BODY_CFG.COL_FRAME);
  
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
  
  body_gfx->fillRect(indicatorX + 1, indicatorY + 1, fillW, indicatorH - 2, gsrColor);
  
  // GSR text
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(0xFFFF, BODY_CFG.COL_BG);
  body_gfx->setCursor(indicatorX, indicatorY + indicatorH + 8);
  body_gfx->printf("GSR: %.0f", gsrLevel);
}

void drawAIOverruleStatus(bool active, float trustOverride, float sleeveOverride){
  if (!active || !body_gfx) return;  // Safety check
  
  int graphAreaX = L_PANE_X + L_PAD;
  int graphAreaY = L_PANE_Y + L_PAD + L_HEADER_H + 5;
  int statusY = graphAreaY + L_CANVAS_H - 20;
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(BODY_CFG.COL_AI, BODY_CFG.COL_BG);
  body_gfx->setCursor(graphAreaX + 5, statusY);
  body_gfx->printf("AI: T%.0f%% S%.0f%%", trustOverride*100, sleeveOverride*100);
}

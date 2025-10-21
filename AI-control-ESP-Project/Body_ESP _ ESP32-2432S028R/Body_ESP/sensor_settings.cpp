#include "sensor_settings.h"
#include <Arduino_GFX_Library.h>
#include <EEPROM.h>
#include "input_touch.h"

// Global config instance
SensorConfig sensorConfig;

static Arduino_GFX* g = nullptr;

// Eenvoudige kleuren zoals andere menus
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_HR=0xF81F;       // Magenta voor HR
static const uint16_t COL_BTN_TEMP=0x07E0;     // Groen voor temp
static const uint16_t COL_BTN_GSR=0xFFE0;      // Geel voor GSR
static const uint16_t COL_BTN_SAVE=0x001F;     // Blauw voor save
static const uint16_t COL_BTN_BACK=0xF800;     // Rood voor terug

static int16_t SCR_W=320, SCR_H=240;
static uint32_t lastTouchMs = 0;
static const uint16_t SETTINGS_COOLDOWN_MS = 800;  // Consistente cooldown

// Button rectangles - menu stijl
static int16_t bxHrX,bxHrY,bxHrW,bxHrH;
static int16_t bxTempX,bxTempY,bxTempW,bxTempH;
static int16_t bxGsrX,bxGsrY,bxGsrW,bxGsrH;
static int16_t bxSaveX,bxSaveY,bxSaveW,bxSaveH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

// Eenvoudige button functie zoals andere menus
static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  // Button met afgeronde hoeken
  g->fillRoundRect(x, y, w, h, 8, COL_BG);  // Background
  g->drawRoundRect(x, y, w, h, 8, COL_FR);  // Outer border
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);  // Inner border
  
  // Fill button with color
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  // Button text with better centering
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  // Better text centering calculation zoals playlist
  int16_t x1, y1; 
  uint16_t tw, th;
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;  // Better vertical centering
  g->setCursor(textX, textY);
  g->print(txt);
}

static void drawSmallButton(int16_t x,int16_t y,const char* txt, uint16_t color) {
  const int16_t w=30, h=20;
  g->fillRect(x,y,w,h,color);
  g->drawRect(x,y,w,h,COL_FR);
  g->setTextColor(BLACK);
  g->setTextSize(1);
  
  // Better text centering voor +/- buttons
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int16_t textX = x + (w - tw) / 2;
  int16_t textY = y + (h + th) / 2 - 2;
  g->setCursor(textX, textY);
  g->print(txt);
}

static void drawValueLine(int16_t y, const char* label, float value, float minVal, float maxVal, float step) {
  // Label - uitgebreid voor betere uitlijning
  g->setTextColor(WHITE);
  g->setTextSize(1);
  g->setCursor(8, y);
  g->print(label);
  
  // Value box - vergroot voor betere zichtbaarheid
  g->fillRect(130, y-4, 80, 18, 0x2104);  // Background, smaller box
  g->drawRect(130, y-4, 80, 18, COL_FR);  // Border
  g->setTextColor(WHITE);
  
  // Value text - RIGHT aligned in box voor consistentie
  char valueStr[16];
  sprintf(valueStr, "%.2f", value);
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds(valueStr, 0, 0, &x1, &y1, &tw, &th);
  int16_t valueX = 130 + 80 - tw - 4;  // Right align in 80px box with margin
  int16_t valueY = y - 4 + (18 + th) / 2 - 2;  // Center vertically
  g->setCursor(valueX, valueY);
  g->print(valueStr);
  
  // +/- Buttons - moved voor ruimte
  drawSmallButton(220, y-4, "-", COL_BTN_BACK);
  drawSmallButton(255, y-4, "+", COL_BTN_SAVE);
}

static void drawIntValueLine(int16_t y, const char* label, int value, int minVal, int maxVal, int step) {
  // Label - consistent met drawValueLine
  g->setTextColor(WHITE);
  g->setTextSize(1);
  g->setCursor(8, y);
  g->print(label);
  
  // Value box - consistent sizing
  g->fillRect(130, y-4, 80, 18, 0x2104);  // Background, smaller box  
  g->drawRect(130, y-4, 80, 18, COL_FR);  // Border
  g->setTextColor(WHITE);
  
  // Value text - RIGHT aligned zoals drawValueLine
  char valueStr[16];
  sprintf(valueStr, "%d", value);
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds(valueStr, 0, 0, &x1, &y1, &tw, &th);
  int16_t valueX = 130 + 80 - tw - 4;  // Right align in 80px box with margin
  int16_t valueY = y - 4 + (18 + th) / 2 - 2;  // Center vertically
  g->setCursor(valueX, valueY);
  g->print(valueStr);
  
  // +/- Buttons - consistent positioning
  drawSmallButton(220, y-4, "-", COL_BTN_BACK);
  drawSmallButton(255, y-4, "+", COL_BTN_SAVE);
}

static bool handleValueTouch(int16_t x, int16_t y) {
  int16_t lineY[] = {70, 90, 110, 130, 150, 170};  // Alle value lines
  
  for (int i = 0; i < 6; i++) {
    int16_t ly = lineY[i];
    if (y >= ly-4 && y <= ly+14) {
      if (i == 0) {  // Beat threshold
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          sensorConfig.hrThreshold = max(10000.0f, sensorConfig.hrThreshold - 1000.0f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          sensorConfig.hrThreshold = min(100000.0f, sensorConfig.hrThreshold + 1000.0f);
          return true;
        }
      }
      else if (i == 1) {  // Temp offset
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          sensorConfig.tempOffset = max(-10.0f, sensorConfig.tempOffset - 0.1f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          sensorConfig.tempOffset = min(10.0f, sensorConfig.tempOffset + 0.1f);
          return true;
        }
      }
      else if (i == 2) {  // Temp smoothing
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          sensorConfig.tempSmoothing = max(0.1f, sensorConfig.tempSmoothing - 0.1f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          sensorConfig.tempSmoothing = min(1.0f, sensorConfig.tempSmoothing + 0.1f);
          return true;
        }
      }
      else if (i == 3) {  // GSR baseline
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          sensorConfig.gsrBaseline = max(0.0f, sensorConfig.gsrBaseline - 10.0f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          sensorConfig.gsrBaseline = min(4095.0f, sensorConfig.gsrBaseline + 10.0f);
          return true;
        }
      }
      else if (i == 4) {  // GSR sensitivity
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          sensorConfig.gsrSensitivity = max(0.1f, sensorConfig.gsrSensitivity - 0.1f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          sensorConfig.gsrSensitivity = min(5.0f, sensorConfig.gsrSensitivity + 0.1f);
          return true;
        }
      }
      else if (i == 5) {  // GSR smoothing
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          sensorConfig.gsrSmoothing = max(0.05f, sensorConfig.gsrSmoothing - 0.05f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          sensorConfig.gsrSmoothing = min(0.5f, sensorConfig.gsrSmoothing + 0.05f);
          return true;
        }
      }
    }
  }
  return false;
}

void loadSensorConfig() {
  EEPROM.begin(512);
  SensorConfig loaded;
  EEPROM.get(0, loaded);
  
  if (loaded.magic == 0xDEADBEEF) {
    sensorConfig = loaded;
  } else {
    // First time - use defaults
    resetSensorConfig();
    saveSensorConfig();
  }
}

void saveSensorConfig() {
  sensorConfig.magic = 0xDEADBEEF;
  EEPROM.put(0, sensorConfig);
  EEPROM.commit();
}

void resetSensorConfig() {
  sensorConfig = SensorConfig();  // Reset to defaults
}

void sensorSettings_begin(Arduino_GFX* gfx) {
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  // Eenvoudige achtergrond
  g->fillScreen(COL_BG);
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  // Center title perfect in scherm
  int16_t x1, y1;
  uint16_t tw, th;
  g->getTextBounds("Sensor Settings", 0, 0, &x1, &y1, &tw, &th);
  g->setCursor((SCR_W - tw) / 2, 25);
  g->print("Sensor Settings");
  
  // Status info
  g->setTextSize(1);
  g->setCursor(20, 50);
  g->print("Kalibratie & drempelwaarden");
  
  // VALUE EDITORS - alle sensor instellingen
  g->setTextColor(WHITE);
  
  // HR Settings (LED Power weggehaald - PCB LED blijft uit)
  drawValueLine(70, "Beat Threshold:", sensorConfig.hrThreshold, 10000.0f, 100000.0f, 1000.0f);
  
  // Temperature Settings
  drawValueLine(90, "Temp Offset:", sensorConfig.tempOffset, -10.0f, 10.0f, 0.1f);
  drawValueLine(110, "Temp Smoothing:", sensorConfig.tempSmoothing, 0.1f, 1.0f, 0.1f);
  
  // GSR Settings  
  drawValueLine(130, "GSR Baseline:", sensorConfig.gsrBaseline, 0.0f, 4095.0f, 10.0f);
  drawValueLine(150, "GSR Sens.:", sensorConfig.gsrSensitivity, 0.1f, 5.0f, 0.1f);
  drawValueLine(170, "GSR Smoothing:", sensorConfig.gsrSmoothing, 0.05f, 0.5f, 0.05f);
  
  // Knoppen layout - horizontaal naast elkaar
  int16_t btnW = 120;  // Breder voor 2 knoppen
  int16_t btnH = 25;   // Standaard hoogte
  int16_t totalW = 2 * btnW + 15;  // 2 knoppen + gap van 15px
  int16_t startX = (SCR_W - totalW) / 2;  // Gecentreerd
  int16_t startY = 200;  // Begin na alle value editors
  int16_t gap = 15;    // Gap tussen knoppen
  
  // 2 knoppen horizontaal: Save, Terug
  bxSaveX = startX; bxSaveY = startY; bxSaveW = btnW; bxSaveH = btnH;
  bxBackX = startX + btnW + gap; bxBackY = startY; bxBackW = btnW; bxBackH = btnH;
  
  // Teken knoppen
  drawButton(bxSaveX, bxSaveY, bxSaveW, bxSaveH, "OPSLAAN", COL_BTN_SAVE);
  drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

SensorEvent sensorSettings_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return SE_NONE;
  
  // Check cooldown
  uint32_t now = millis();
  if (now - lastTouchMs < SETTINGS_COOLDOWN_MS) return SE_NONE;
  
  // Check button touches
  if (inRect(x, y, bxSaveX, bxSaveY, bxSaveW, bxSaveH)) {
    lastTouchMs = now;
    return SE_SAVE;
  }
  if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
    lastTouchMs = now;
    return SE_BACK;
  }
  
  // Check value adjustments - TERUGGEBRACHT
  if (handleValueTouch(x, y)) {
    lastTouchMs = now;
    // Redraw values (larger area voor alle settings)
    g->fillRect(0, 60, SCR_W, 140, COL_BG);
    sensorSettings_begin(g);
    return SE_NONE;
  }
  
  return SE_NONE;
}

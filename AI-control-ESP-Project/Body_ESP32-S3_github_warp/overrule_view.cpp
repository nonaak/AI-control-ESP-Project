#include "overrule_view.h"
//#include <Arduino_GFX_Library.h>
#include "TFT_eSPI.h"
#include "view_colors.h"
#include "input_touch.h"
#include "ai_overrule.h"
#include "advanced_stress_manager.h"

//#include "TFT_eSPI.h"

// TFT_eSPI color definitions (vervangen Arduino_GFX)
//#define BLACK   0x0000
//#define WHITE   0xFFFF
//#define RED     0xF800
//#define GREEN   0x07E0
//#define BLUE    0x001F
//#define CYAN    0x07FF
//#define MAGENTA 0xF81F
//#define YELLOW  0xFFE0

static TFT_eSPI* g = nullptr;
// Eenvoudige kleuren zoals andere menus
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_AI_TOGGLE=0xF81F;  // Magenta voor AI toggle
static const uint16_t COL_BTN_ANALYZE=0xFFE0;    // Geel voor analyse  
static const uint16_t COL_BTN_CONFIG=0x07E0;     // Groen voor config
static const uint16_t COL_BTN_SAVE=0x001F;       // Blauw voor save
static const uint16_t COL_BTN_BACK=0xF800;       // Rood voor terug

static int16_t SCR_W=320, SCR_H=240;
static uint32_t lastTouchMs = 0;
static const uint16_t OVERRULE_COOLDOWN_MS = 800;  // Consistente cooldown

// Button rectangles - menu stijl
static int16_t bxAiToggleX,bxAiToggleY,bxAiToggleW,bxAiToggleH;
static int16_t bxAnalyzeX,bxAnalyzeY,bxAnalyzeW,bxAnalyzeH;
static int16_t bxConfigX,bxConfigY,bxConfigW,bxConfigH;
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
  // Estimate text bounds based on text size 1 (6px per char width, 8px height)
  tw = strlen(txt) * 6;
  th = 8;
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
  // Estimate text bounds based on text size 1 (6px per char width, 8px height)
  tw = strlen(txt) * 6;
  th = 8;
  int16_t textX = x + (w - tw) / 2;
  int16_t textY = y + (h + th) / 2 - 2;
  g->setCursor(textX, textY);
  g->print(txt);
}

static void drawValueLine(int16_t y, const char* label, float value, float minVal, float maxVal, float step) {
  // Label - consistent met sensor settings
  g->setTextColor(WHITE);
  g->setTextSize(1);
  g->setCursor(8, y);
  g->print(label);
  
  // Value box - consistent sizing met sensor settings
  g->fillRect(130, y-4, 80, 18, 0x2104);
  g->drawRect(130, y-4, 80, 18, COL_FR);
  g->setTextColor(WHITE);
  
  // Value text - RIGHT aligned voor consistentie
  char valueStr[16];
  sprintf(valueStr, "%.2f", value);
  int16_t x1, y1;
  uint16_t tw, th;
  // Estimate text bounds based on text size 1 (6px per char width, 8px height)
  tw = strlen(valueStr) * 6;
  th = 8;
  int16_t valueX = 130 + 80 - tw - 4;  // Right align in 80px box with margin
  int16_t valueY = y - 4 + (18 + th) / 2 - 2;  // Center vertically
  g->setCursor(valueX, valueY);
  g->print(valueStr);
  
  // +/- Buttons - consistent positioning
  drawSmallButton(220, y-4, "-", 0xF800);
  drawSmallButton(255, y-4, "+", 0x07E0);
}

static bool handleValueTouch(int16_t x, int16_t y) {
  int16_t lineY[] = {65, 80, 95, 110, 125, 140};  // Alle 6 value lines - ML Eigenwil eerst
  
  for (int i = 0; i < 6; i++) {
    int16_t ly = lineY[i];
    if (y >= ly-4 && y <= ly+14) {
      if (i == 0) {  // ML Autonomy Control (nu eerst)
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          // Decrease ML Autonomy by 5%
          float current = stressManager.getMLAutonomyLevel();
          float newLevel = max(0.0f, current - 0.05f);
          stressManager.setMLAutonomyLevel(newLevel);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          // Increase ML Autonomy by 5%
          float current = stressManager.getMLAutonomyLevel();
          float newLevel = min(1.0f, current + 0.05f);
          stressManager.setMLAutonomyLevel(newLevel);
          return true;
        }
      }
      else if (i == 1) {  // HR Low threshold (was index 0)
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          aiConfig.hrLowThreshold = max(40.0f, aiConfig.hrLowThreshold - 5.0f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          aiConfig.hrLowThreshold = min(100.0f, aiConfig.hrLowThreshold + 5.0f);
          return true;
        }
      }
      else if (i == 2) {  // HR High threshold (was index 1)
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          aiConfig.hrHighThreshold = max(100.0f, aiConfig.hrHighThreshold - 5.0f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          aiConfig.hrHighThreshold = min(200.0f, aiConfig.hrHighThreshold + 5.0f);
          return true;
        }
      }
      else if (i == 3) {  // Temp threshold (was index 2)
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          aiConfig.tempHighThreshold = max(35.0f, aiConfig.tempHighThreshold - 0.1f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          aiConfig.tempHighThreshold = min(40.0f, aiConfig.tempHighThreshold + 0.1f);
          return true;
        }
      }
      else if (i == 4) {  // GSR threshold (was index 3)
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          aiConfig.gsrHighThreshold = max(100.0f, aiConfig.gsrHighThreshold - 50.0f);
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          aiConfig.gsrHighThreshold = min(1000.0f, aiConfig.gsrHighThreshold + 50.0f);
          return true;
        }
      }
      else if (i == 5) {  // AI Response Rate (placeholder)
        if (inRect(x, y, 220, ly-4, 30, 20)) {
          // Placeholder - future AI response rate decrease
          return true;
        }
        if (inRect(x, y, 255, ly-4, 30, 20)) {
          // Placeholder - future AI response rate increase
          return true;
        }
      }
    }
  }
  return false;
}

void overrule_begin(TFT_eSPI* gfx) {
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  // Eenvoudige achtergrond zoals andere menus
  g->fillScreen(COL_BG);
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  // Center title perfect in scherm - FIXED
  int16_t x1, y1;
  uint16_t tw, th;
  // Estimate text bounds based on text size 2 (12px per char width, 16px height)
  tw = strlen("AI Settings") * 12;
  th = 16;
  g->setCursor((SCR_W - tw) / 2, 25);
  g->print("AI Settings");
  
  // Compacte AI status - minder tekst
  g->setTextSize(1);
  g->setTextColor(WHITE);
  g->setCursor(20, 50);
  
  float mlLevel = stressManager.getMLAutonomyLevel();
  
  g->printf("AI:%s ML:autonoom %.0f%%", 
            aiConfig.enabled ? "ON" : "OFF", 
            mlLevel * 100.0f);
  
  // AI Parameters - ML Eigenwil eerst, dan de rest
  g->setTextColor(WHITE);
  
  // ML Autonomy Control - BOVENAAN
  float mlAutonomy = stressManager.getMLAutonomyLevel() * 100.0f;  // Convert to percentage
  drawValueLine(65, "ML Eigenwil:", mlAutonomy, 0.0f, 100.0f, 5.0f);  // ML Autonomy 0-100%
  
  // Daarna de andere instellingen
  drawValueLine(80, "HR Laag:", aiConfig.hrLowThreshold, 40.0f, 100.0f, 5.0f);
  drawValueLine(95, "HR Hoog:", aiConfig.hrHighThreshold, 100.0f, 200.0f, 5.0f);
  drawValueLine(110, "Temp Max:", aiConfig.tempHighThreshold, 35.0f, 40.0f, 0.1f);
  drawValueLine(125, "GSR Max:", aiConfig.gsrHighThreshold, 100.0f, 1000.0f, 50.0f);
  drawValueLine(140, "Response:", 0.5f, 0.1f, 1.0f, 0.1f);   // AI Response Rate (placeholder)
  
  // Knoppen layout - horizontaal naast elkaar, lager voor meer settings
  int16_t btnW = 90;   // Smaller voor 3 knoppen naast elkaar
  int16_t btnH = 25;   // Iets hoger
  int16_t totalW = 3 * btnW + 2 * 8;  // 3 knoppen + 2 gaps van 8px
  int16_t startX = (SCR_W - totalW) / 2;  // Gecentreerd
  int16_t startY = 170;  // Begin na alle value editors (6 lines)
  int16_t gap = 8;     // Gap tussen knoppen
  
  // 3 knoppen horizontaal: AI Toggle, Save, Terug
  bxAiToggleX = startX; bxAiToggleY = startY; bxAiToggleW = btnW; bxAiToggleH = btnH;
  bxSaveX = startX + btnW + gap; bxSaveY = startY; bxSaveW = btnW; bxSaveH = btnH;
  bxBackX = startX + 2 * (btnW + gap); bxBackY = startY; bxBackW = btnW; bxBackH = btnH;
  
  // Teken knoppen - UPDATED voor nieuwe layout
  const char* aiButtonText = aiConfig.enabled ? "AI UIT" : "AI AAN";
  drawButton(bxAiToggleX, bxAiToggleY, bxAiToggleW, bxAiToggleH, aiButtonText, COL_BTN_AI_TOGGLE);
  drawButton(bxSaveX, bxSaveY, bxSaveW, bxSaveH, "OPSLAAN", COL_BTN_SAVE);
  drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

OverruleEvent overrule_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return OE_NONE;
  
  // Check cooldown
  uint32_t now = millis();
  if (now - lastTouchMs < OVERRULE_COOLDOWN_MS) return OE_NONE;
  
  // Check button touches - UPDATED voor nieuwe layout
  if (inRect(x, y, bxAiToggleX, bxAiToggleY, bxAiToggleW, bxAiToggleH)) {
    lastTouchMs = now;
    return OE_TOGGLE_AI;
  }
  if (inRect(x, y, bxSaveX, bxSaveY, bxSaveW, bxSaveH)) {
    lastTouchMs = now;
    return OE_SAVE;
  }
  if (inRect(x, y, bxBackX, bxBackY, bxBackW, bxBackH)) {
    lastTouchMs = now;
    return OE_BACK;
  }
  
  // Check value adjustments - TERUGGEBRACHT
  if (handleValueTouch(x, y)) {
    lastTouchMs = now;
    // Redraw values (larger area voor 6 value lines + status)
    g->fillRect(0, 75, SCR_W, 110, COL_BG);
    overrule_begin(g);
    return OE_NONE;
  }
  
  return OE_NONE;
}

void toggleAIOverrule() {
  aiConfig.enabled = !aiConfig.enabled;
}

void saveMLAutonomyToConfig() {
  // Save current ML Autonomy level to global config
  BODY_CFG.mlAutonomyLevel = stressManager.getMLAutonomyLevel();
  Serial.printf("[AI] ML Autonomy saved: %.1f%%\n", BODY_CFG.mlAutonomyLevel * 100.0f);
}

bool isAIOverruleEnabled() {
  return aiConfig.enabled;
}
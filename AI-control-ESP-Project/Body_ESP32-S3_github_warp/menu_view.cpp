#include "menu_view.h"
#include "input_touch.h"
#include "TFT_eSPI.h"
#include "view_colors.h"

static TFT_eSPI* g=nullptr;

static const uint16_t COL_BG=TFT_BLACK, COL_FR=0xC618, COL_TX=TFT_WHITE;
static const uint16_t COL_BTN_OVERRULE = 0xF81F;
static const uint16_t COL_BTN_SENSORS = 0x07E0;
static const uint16_t COL_BTN_SYSTEM = 0xFFE0;
static const uint16_t COL_BTN_OPNAMES = 0xF800;
static const uint16_t COL_BTN_BACK = 0x001F;
static const uint16_t COL_BTN_ML_TRAINING = 0x09EE;

static int16_t SCR_W=320,SCR_H=240;

static int16_t bxOverruleX,bxOverruleY,bxOverruleW,bxOverruleH;
static int16_t bxSensorsX,bxSensorsY,bxSensorsW,bxSensorsH;
static int16_t bxSystemX,bxSystemY,bxSystemW,bxSystemH;
static int16_t bxOpnamesX,bxOpnamesY,bxOpnamesW,bxOpnamesH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;
static int16_t bxMLTrainingX,bxMLTrainingY,bxMLTrainingW,bxMLTrainingH;

static uint32_t lastMenuTouchMs = 0;
static const uint16_t MENU_COOLDOWN_MS = 800;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  g->fillRoundRect(x, y, w, h, 8, COL_BG);
  g->drawRoundRect(x, y, w, h, 8, COL_FR);
  g->drawRoundRect(x+2, y+2, w-4, h-4, 6, COL_FR);
  g->fillRoundRect(x+4, y+4, w-8, h-8, 4, color);
  
  g->setTextColor(TFT_BLACK);
  g->setTextSize(1);
  
  uint16_t tw = strlen(txt) * 6;
  uint16_t th = 8;
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;
  g->setCursor(textX, textY);
  g->print(txt);
}

void menu_begin(TFT_eSPI* gfx){
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  g->fillScreen(COL_BG);
  g->setTextColor(COL_TX);
  g->setTextSize(2);
  
  uint16_t tw = strlen("Menu") * 12;
  g->setCursor((SCR_W - tw) / 2, 25);
  g->print("Menu");
  
  // Knoppen layout - 6 knoppen verticaal
  int16_t btnW = 200;
  int16_t btnH = 26;
  int16_t btnX = (SCR_W - btnW) / 2;  // Gecentreerd = 60
  int16_t startY = 50;
  int16_t gap = 4;
  
  // Touch zones EXACT gelijk aan visuele buttons
  bxOverruleX = btnX; 
  bxOverruleY = startY; 
  bxOverruleW = btnW; 
  bxOverruleH = btnH;
  
  bxMLTrainingX = btnX; 
  bxMLTrainingY = startY + btnH + gap; 
  bxMLTrainingW = btnW; 
  bxMLTrainingH = btnH;
  
  bxSensorsX = btnX; 
  bxSensorsY = startY + (btnH + gap) * 2; 
  bxSensorsW = btnW; 
  bxSensorsH = btnH;
  
  bxSystemX = btnX; 
  bxSystemY = startY + (btnH + gap) * 3; 
  bxSystemW = btnW; 
  bxSystemH = btnH;
  
  bxOpnamesX = btnX; 
  bxOpnamesY = startY + (btnH + gap) * 4; 
  bxOpnamesW = btnW; 
  bxOpnamesH = btnH;
  
  bxBackX = btnX; 
  bxBackY = startY + (btnH + gap) * 5; 
  bxBackW = btnW; 
  bxBackH = btnH;
  
  // Teken alle knoppen
  drawButton(bxOverruleX, bxOverruleY, bxOverruleW, bxOverruleH, "AI", COL_BTN_OVERRULE);
  drawButton(bxMLTrainingX, bxMLTrainingY, bxMLTrainingW, bxMLTrainingH, "ML TRAIN", COL_BTN_ML_TRAINING);
  drawButton(bxSensorsX, bxSensorsY, bxSensorsW, bxSensorsH, "SENSORS", COL_BTN_SENSORS);
  drawButton(bxSystemX, bxSystemY, bxSystemW, bxSystemH, "SYSTEEM", COL_BTN_SYSTEM);
  drawButton(bxOpnamesX, bxOpnamesY, bxOpnamesW, bxOpnamesH, "OPNAMES", COL_BTN_OPNAMES);
  drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
  
  Serial.println("[MENU] Menu initialized - touch zones match visual buttons");
}

MenuEvent menu_poll(){
  static bool wasTouching = false;
  static int16_t lastX = -1, lastY = -1;
  
  int16_t x, y;
  bool isTouching = inputTouchRead(x, y);
  
  if (isTouching) {
    lastX = x;
    lastY = y;
    wasTouching = true;
    return ME_NONE;
  }
  
  if (wasTouching && !isTouching) {
    wasTouching = false;
    
    uint32_t now = millis();
    if (now - lastMenuTouchMs < MENU_COOLDOWN_MS) return ME_NONE;
    
    x = lastX;
    y = lastY;
    
    // Pas touch correctie toe (centrale helper functie)
    inputTouchCorrectForViews(x, y);
    
    Serial.printf("[MENU] Touch released at (%d,%d) [after correction]\n", x, y);
    
    if(inRect(x,y,bxOverruleX,bxOverruleY,bxOverruleW,bxOverruleH)) { 
      Serial.println("[MENU] → AI button");
      lastMenuTouchMs = now;
      return ME_OVERRULE;
    }
    if(inRect(x,y,bxMLTrainingX,bxMLTrainingY,bxMLTrainingW,bxMLTrainingH)) { 
      Serial.println("[MENU] → ML TRAINING button");
      lastMenuTouchMs = now;
      return ME_ML_TRAINING;
    }
    if(inRect(x,y,bxSensorsX,bxSensorsY,bxSensorsW,bxSensorsH)) { 
      Serial.println("[MENU] → SENSORS button");
      lastMenuTouchMs = now;
      return ME_SENSOR_SETTINGS;
    }
    if(inRect(x,y,bxSystemX,bxSystemY,bxSystemW,bxSystemH)) { 
      Serial.println("[MENU] → SYSTEEM button");
      lastMenuTouchMs = now;
      return ME_SYSTEM_SETTINGS;
    }
    if(inRect(x,y,bxOpnamesX,bxOpnamesY,bxOpnamesW,bxOpnamesH)) { 
      Serial.println("[MENU] → OPNAMES button");
      lastMenuTouchMs = now;
      return ME_PLAYLIST;
    }
    if(inRect(x,y,bxBackX,bxBackY,bxBackW,bxBackH)) { 
      Serial.println("[MENU] → TERUG button");
      lastMenuTouchMs = now;
      return ME_BACK;
    }
    
    Serial.printf("[MENU] Touch (%d,%d) - no button hit\n", x, y);
  }
  
  return ME_NONE;
}
#include "menu_view.h"
// FIXED: Verwijderd Arduino_GFX_Library.h (oude CYD code)
#include "input_touch.h"

static TFT_eSPI* g=nullptr;

// Eenvoudige kleuren zoals playlist
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_OVERRULE = 0xF81F; // Magenta voor AI
static const uint16_t COL_BTN_SENSORS = 0x07E0;  // Groen voor sensors
static const uint16_t COL_BTN_SYSTEM = 0xFFE0;   // Geel voor systeem
static const uint16_t COL_BTN_OPNAMES = 0xF800;  // Rood voor opnames
static const uint16_t COL_BTN_BACK = 0x001F;     // Blauw voor terug
static const uint16_t COL_BTN_ML_TRAINING = 0x09EE; // Blauw-cyaan voor ML Training

static int16_t SCR_W=320,SCR_H=240;

// Button rectangles - playlist stijl
static int16_t bxOverruleX,bxOverruleY,bxOverruleW,bxOverruleH;
static int16_t bxSensorsX,bxSensorsY,bxSensorsW,bxSensorsH;
static int16_t bxSystemX,bxSystemY,bxSystemW,bxSystemH;
static int16_t bxOpnamesX,bxOpnamesY,bxOpnamesW,bxOpnamesH;
static int16_t bxBackX,bxBackY,bxBackW,bxBackH;
static int16_t bxMLTrainingX,bxMLTrainingY,bxMLTrainingW,bxMLTrainingH;

// Touch cooldown voor menu
static uint32_t lastMenuTouchMs = 0;
static const uint16_t MENU_COOLDOWN_MS = 800;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

// Eenvoudige button functie zoals playlist
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
  
  // Knoppen layout - 6 knoppen
  int16_t btnW = 200;
  int16_t btnH = 26;
  int16_t btnX = (SCR_W - btnW) / 2;
  int16_t startY = 50;
  int16_t gap = 4;
  
  bxOverruleX = btnX; bxOverruleY = startY; bxOverruleW = btnW; bxOverruleH = btnH;
  bxMLTrainingX = btnX; bxMLTrainingY = startY + btnH + gap; bxMLTrainingW = btnW; bxMLTrainingH = btnH;
  bxSensorsX = btnX; bxSensorsY = startY + (btnH + gap) * 2; bxSensorsW = btnW; bxSensorsH = btnH;
  bxSystemX = btnX; bxSystemY = startY + (btnH + gap) * 3; bxSystemW = btnW; bxSystemH = btnH;
  bxOpnamesX = btnX; bxOpnamesY = startY + (btnH + gap) * 4; bxOpnamesW = btnW; bxOpnamesH = btnH;
  bxBackX = btnX; bxBackY = startY + (btnH + gap) * 5; bxBackW = btnW; bxBackH = btnH;
  
  drawButton(bxOverruleX, bxOverruleY, bxOverruleW, bxOverruleH, "AI", COL_BTN_OVERRULE);
  drawButton(bxMLTrainingX, bxMLTrainingY, bxMLTrainingW, bxMLTrainingH, "ML TRAIN", COL_BTN_ML_TRAINING);
  drawButton(bxSensorsX, bxSensorsY, bxSensorsW, bxSensorsH, "SENSORS", COL_BTN_SENSORS);
  drawButton(bxSystemX, bxSystemY, bxSystemW, bxSystemH, "SYSTEEM", COL_BTN_SYSTEM);
  drawButton(bxOpnamesX, bxOpnamesY, bxOpnamesW, bxOpnamesH, "OPNAMES", COL_BTN_OPNAMES);
  drawButton(bxBackX, bxBackY, bxBackW, bxBackH, "TERUG", COL_BTN_BACK);
}

MenuEvent menu_poll(){
  int16_t x,y; 
  if(!inputTouchRead(x,y)) return ME_NONE;
  
  uint32_t now = millis();
  if (now - lastMenuTouchMs < MENU_COOLDOWN_MS) return ME_NONE;
  
  if(inRect(x,y,bxOverruleX,bxOverruleY,bxOverruleW,bxOverruleH)) { 
    lastMenuTouchMs = now;
    return ME_OVERRULE;
  }
  if(inRect(x,y,bxSensorsX,bxSensorsY,bxSensorsW,bxSensorsH)) { 
    lastMenuTouchMs = now;
    return ME_SENSOR_SETTINGS;
  }
  if(inRect(x,y,bxSystemX,bxSystemY,bxSystemW,bxSystemH)) { 
    lastMenuTouchMs = now;
    return ME_SYSTEM_SETTINGS;
  }
  if(inRect(x,y,bxOpnamesX,bxOpnamesY,bxOpnamesW,bxOpnamesH)) { 
    lastMenuTouchMs = now;
    return ME_PLAYLIST;
  }
  if(inRect(x,y,bxMLTrainingX,bxMLTrainingY,bxMLTrainingW,bxMLTrainingH)) { 
    lastMenuTouchMs = now;
    return ME_ML_TRAINING;
  }
  if(inRect(x,y,bxBackX,bxBackY,bxBackW,bxBackH)) { 
    lastMenuTouchMs = now;
    return ME_BACK;
  }
  
  return ME_NONE;
}
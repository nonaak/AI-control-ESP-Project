/*
  ML TRAINING VIEW
  
  Menu voor ML Training met 4 knoppen:
  - Data Opnemen
  - Model Trainen  
  - Feedback (was: AI Annotatie)
  - Model Manager
*/

#ifndef ML_TRAINING_VIEW_H
#define ML_TRAINING_VIEW_H

#include <Arduino.h>

// Forward declarations
extern Arduino_GFX* body_gfx;
extern const int MENU_X;
extern const int MENU_Y;
extern const int MENU_W;
extern const int MENU_H;

// Menu item labels
const char* ML_TRAINING_ITEMS[] = {
  "Data Opnemen",
  "Model Trainen",
  "Feedback",      // Was: "AI Annotatie"
  "Model Manager"
};
const int ML_TRAINING_ITEM_COUNT = 4;

// Kleuren per knop
const uint16_t ML_TRAINING_COLORS[] = {
  0xF800,   // Rood - Data Opnemen (recording)
  0x07E0,   // Groen - Model Trainen
  0xF81F,   // Magenta - Feedback
  0x001F    // Blauw - Model Manager
};

void drawMLTrainingView() {
  // Titel
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_TITLE);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  body_gfx->setTextColor(0xF81F, 0x0000);  // Magenta
  
  int16_t x1, y1;
  uint16_t tw, th;
  const char* title = "ML TRAINING";
  body_gfx->getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  body_gfx->setCursor(MENU_X + (MENU_W - tw) / 2, MENU_Y + 35);
  body_gfx->print(title);
  
  // Knoppen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  const int BTN_W = MENU_W - 40;
  const int BTN_H = 42;
  const int BTN_SPACING = 8;
  const int START_X = MENU_X + 20;
  const int START_Y = MENU_Y + 60;
  
  for (int i = 0; i < ML_TRAINING_ITEM_COUNT; i++) {
    int btnY = START_Y + i * (BTN_H + BTN_SPACING);
    
    // Knop achtergrond
    body_gfx->fillRoundRect(START_X, btnY, BTN_W, BTN_H, 8, ML_TRAINING_COLORS[i]);
    body_gfx->drawRoundRect(START_X, btnY, BTN_W, BTN_H, 8, 0xFFFF);
    
    // Tekst
    uint16_t textColor = 0xFFFF;
    if (ML_TRAINING_COLORS[i] == 0x07E0 || ML_TRAINING_COLORS[i] == 0xFFE0) {
      textColor = 0x0000;  // Zwart op lichte kleuren
    }
    
    body_gfx->setTextColor(textColor, ML_TRAINING_COLORS[i]);
    body_gfx->getTextBounds(ML_TRAINING_ITEMS[i], 0, 0, &x1, &y1, &tw, &th);
    
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2 - x1, btnY + (BTN_H + th) / 2 - 2);
    #else
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2, btnY + (BTN_H - th) / 2 + th - 2);
    #endif
    
    body_gfx->print(ML_TRAINING_ITEMS[i]);
  }
  
  // Info tekst onderaan
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_SMALL);
  #else
    body_gfx->setTextSize(1);
  #endif
  
  body_gfx->setTextColor(0x7BEF, 0x0000);  // Grijs
  body_gfx->setCursor(MENU_X + 15, MENU_Y + MENU_H - 25);
  body_gfx->print("Feedback = levels corrigeren");
}

#endif // ML_TRAINING_VIEW_H

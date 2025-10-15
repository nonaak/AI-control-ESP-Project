#include "confirm_view.h"
#include <Arduino_GFX_Library.h>
#include "input_touch.h"

static TFT_eSPI* g = nullptr;
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_BTN_YES=0xF800;    // Rood voor JA (gevaarlijk)
static const uint16_t COL_BTN_NO=0x07E0;     // Groen voor NEE (veilig)
static const uint16_t COL_DIALOG_BG=0x2104;  // Donkergrijs voor dialog

static int16_t SCR_W=320, SCR_H=240;

// Button rectangles
static int16_t bxYesX,bxYesY,bxYesW,bxYesH;
static int16_t bxNoX,bxNoY,bxNoW,bxNoH;

// Touch cooldown voor confirmation
static uint32_t lastConfirmTouchMs = 0;
static const uint16_t CONFIRM_COOLDOWN_MS = 800;  // Lange cooldown voor dialogs

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color) {
  g->fillRect(x,y,w,h,color);
  g->drawRect(x,y,w,h,COL_FR);
  g->setTextColor(BLACK);
  g->setTextSize(1);  // Kleiner gemaakt van 2 naar 1
  
  // Handmatige centrering met experimentele offsets
  int16_t txtX, txtY;
  
  if (strcmp(txt, "JA") == 0) {
    // JA: handmatig gecentreerd, 5px naar links, 9px naar beneden
    txtX = x + w/2 - 6 - 5;   // Halve knop min halve tekstbreedte min 5px
    txtY = y + h/2 - 4 + 3 + 3 + 3;   // Halve knop min halve teksthoogte plus 9px naar beneden
    
    // Vetgedrukte tekst door dubbel printen met offset
    g->setCursor(txtX, txtY);
    g->print(txt);
    g->setCursor(txtX + 1, txtY);  // 1px offset voor vet effect
    g->print(txt);
  } else if (strcmp(txt, "NEE") == 0) {
    // NEE: handmatig gecentreerd, 9px naar links (5+2+2), 9px naar beneden
    txtX = x + w/2 - 9 - 5 - 2 - 2;   // Halve knop min halve tekstbreedte min 9px
    txtY = y + h/2 - 4 + 3 + 3 + 3;   // Halve knop min halve teksthoogte plus 9px naar beneden
    
    // Vetgedrukte tekst door dubbel printen met offset
    g->setCursor(txtX, txtY);
    g->print(txt);
    g->setCursor(txtX + 1, txtY);  // 1px offset voor vet effect
    g->print(txt);
  } else {
    // Fallback - normale tekst
    txtX = x + 10 - 5;
    txtY = y + h/2 - 4 + 3 + 3 + 3;  // Ook 3px verder naar beneden
    g->setCursor(txtX, txtY);
    g->print(txt);
  }
}

void confirm_begin(TFT_eSPI* gfx, const char* title, const char* message) {
  g = gfx;
  SCR_W = g->width();
  SCR_H = g->height();
  
  // Donkere achtergrond
  g->fillScreen(COL_BG);
  
  // Dialog box
  int16_t dialogW = SCR_W - 40;
  int16_t dialogH = 140;
  int16_t dialogX = 20;
  int16_t dialogY = (SCR_H - dialogH) / 2;
  
  g->fillRect(dialogX, dialogY, dialogW, dialogH, COL_DIALOG_BG);
  g->drawRect(dialogX, dialogY, dialogW, dialogH, COL_FR);
  
  // Title
  g->setTextColor(COL_TX);
  g->setTextSize(1);  // Kleinere tekst
  
  // Eenvoudige centrering voor title (size 1: ~6px per char), 15px naar links
  int16_t titleLen = strlen(title);
  int16_t titleWidth = titleLen * 6;
  int16_t titleX = dialogX + (dialogW - titleWidth) / 2 - 10 - 5;  // 15px naar links
  
  g->setCursor(titleX, dialogY + 10);
  g->print(title);
  
  // Message (kan meerdere regels zijn)
  g->setTextSize(1);
  g->setCursor(dialogX + 10, dialogY + 35);
  
  // Split message op spaties voor word wrapping
  String msg = String(message);
  int lineY = dialogY + 35;
  int lineX = dialogX + 10;
  int charCount = 0;
  int maxCharsPerLine = (dialogW - 20) / 6; // 6 pixels per char bij size 1
  
  for (int i = 0; i <= msg.length(); i++) {
    if (i == msg.length() || msg.charAt(i) == ' ' || charCount >= maxCharsPerLine) {
      if (i == msg.length()) {
        // Laatste woord
        String lastPart = msg.substring(i - charCount);
        g->setCursor(lineX, lineY);
        g->print(lastPart);
      } else if (charCount >= maxCharsPerLine) {
        // Nieuwe regel forceren
        lineY += 12;
        lineX = dialogX + 10;
        charCount = 0;
        i--; // Herhaal deze char op nieuwe regel
      } else {
        // Normale spatie
        charCount++;
      }
    } else {
      charCount++;
    }
  }
  
  // Buttons - groter gemaakt
  int16_t btnY = dialogY + dialogH - 55;
  int16_t btnW = 100;  // Was 80
  int16_t btnH = 45;   // Was 36
  int16_t gap = 15;    // Was 20, kleiner voor ruimte
  
  bxNoX = dialogX + gap; 
  bxNoY = btnY; 
  bxNoW = btnW; 
  bxNoH = btnH;
  
  bxYesX = dialogX + dialogW - btnW - gap; 
  bxYesY = btnY; 
  bxYesW = btnW; 
  bxYesH = btnH;
  
  // Teken buttons (NEE links (veilig), JA rechts (gevaarlijk))
  drawButton(bxNoX, bxNoY, bxNoW, bxNoH, "NEE", COL_BTN_NO);
  drawButton(bxYesX, bxYesY, bxYesW, bxYesH, "JA", COL_BTN_YES);
}

ConfirmEvent confirm_poll() {
  int16_t x, y;
  if (!inputTouchRead(x, y)) return CONF_NONE;
  
  // Check cooldown
  uint32_t now = millis();
  if (now - lastConfirmTouchMs < CONFIRM_COOLDOWN_MS) return CONF_NONE;
  
  if (inRect(x, y, bxYesX, bxYesY, bxYesW, bxYesH)) {
    lastConfirmTouchMs = now;
    return CONF_YES;
  }
  if (inRect(x, y, bxNoX, bxNoY, bxNoW, bxNoH)) {
    lastConfirmTouchMs = now;
    return CONF_NO;
  }
  
  return CONF_NONE;
}

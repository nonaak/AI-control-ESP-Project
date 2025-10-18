#include "body_gfx4.h"
#include "body_display.h"
#include "body_fonts.h"

// Forward declaration
void drawChannel(uint8_t ch);

// Channel data
struct Channel {
  char label[16];
  float samples[160];  // Full screen width samples
  int writeIndex;
  float minVal, maxVal;
  bool hasData;
  uint16_t color;
};

static Channel channels[G4_NUM];  // Nu 7 channels in plaats van 8
static char statusText[64] = "Gereed";

// Touch cooldown variables
static uint32_t mainScreenCooldownMs = 2000;  // 2 second cooldown for main screen
static uint32_t menuCooldownMs = 500;         // 0.5 second cooldown for menu
static uint32_t lastTouchTime = 0;            // Unified touch time

// Volledig scherm layout voor T-HMI (320x240) - VOOR 7 SENSOREN (zonder Oxy)
static const int GRAPH_Y = 5;
static const int GRAPH_H = 190;   // Totale grafiek ruimte
static const int GRAPH_W = 310;   // Volledige breedte
static const int GRAPH_X = 5;
static const int CHANNEL_H = 27;  // Voor 7 channels (190/7 ≈ 27) - MEER RUIMTE!

// Button layout - helemaal onderaan
static const int BUTTON_Y = 200;  // Onderaan
static const int BUTTON_W = 75;   // Groter
static const int BUTTON_H = 35;   // Groter voor touch
static const int BUTTON_SPACING = 80;  // Meer ruimte tussen buttons
static const int BUTTON_START_X = 5;   // Start vanaf links

void body_gfx4_begin() {
  // Initialize channels with HoofdESP colors
  for (int i = 0; i < G4_NUM; i++) {
    channels[i].writeIndex = 0;
    channels[i].minVal = 0;
    channels[i].maxVal = 100;
    channels[i].hasData = false;
    strcpy(channels[i].label, "---");
    
    for (int j = 0; j < 160; j++) {
      channels[i].samples[j] = 0;
    }
  }
  
  // Set colors - OXY VERWIJDERD, nummers aangepast
  channels[G4_HART].color = BODY_CFG.DOT_RED;         // Rood for heart
  channels[G4_HUID].color = 0xF8FB;                   // Licht huidkleur for skin
  channels[G4_TEMP].color = 0xFD60;                   // Licht-oranje for temperature
  channels[G4_ADEMHALING].color = BODY_CFG.DOT_GREEN; // Groen for breathing
  channels[G4_HOOFDESP].color = BODY_CFG.COL_BRAND;   // Paars for HoofdESP
  channels[G4_ZUIGEN].color = 0x87FF;                 // Licht-blauw for suction
  channels[G4_TRIL].color = 0xFB40;                   // Donker oranje for vibration
  
  body_gfx4_clear();
  
  // Draw initial buttons
  body_gfx4_drawButtons(false, false, false, false);
  Serial.println("[BODY] Initial buttons drawn (7 grafieken - geen Oxy)");
}

void body_gfx4_clear() {
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // Draw HoofdESP-style frame
  body_gfx->drawRoundRect(2, 2, 316, 236, 12, BODY_CFG.COL_FRAME2);
  body_gfx->drawRoundRect(4, 4, 312, 232, 10, BODY_CFG.COL_FRAME);
  
  Serial.println("[BODY] Screen cleared and frame drawn");
}

void body_gfx4_setLabel(uint8_t ch, const char* name) {
  if (ch >= G4_NUM) return;
  strncpy(channels[ch].label, name, sizeof(channels[ch].label) - 1);
  channels[ch].label[sizeof(channels[ch].label) - 1] = '\0';
}

void body_gfx4_pushSample(uint8_t ch, float value, bool mark) {
  if (ch >= G4_NUM) return;
  
  Channel& c = channels[ch];
  c.samples[c.writeIndex] = value;
  c.writeIndex = (c.writeIndex + 1) % 160;
  c.hasData = true;
  
  // Auto-scale
  if (!c.hasData || value < c.minVal) c.minVal = value;
  if (!c.hasData || value > c.maxVal) c.maxVal = value;
  
  // Ensure some range
  if (c.maxVal - c.minVal < 1.0f) {
    c.maxVal = c.minVal + 1.0f;
  }
  
  // Redraw this channel
  drawChannel(ch);
}

void drawChannel(uint8_t ch) {
  if (ch >= G4_NUM) return;
  
  Channel& c = channels[ch];
  if (!c.hasData) return;
  
  int y = GRAPH_Y + ch * CHANNEL_H;
  
  // Clear channel area
  body_gfx->fillRect(GRAPH_X, y, GRAPH_W, CHANNEL_H - 2, BODY_CFG.COL_BG);
  
  // Draw label with sierlijke fonts - each sensor keeps its own color
#if USE_ADAFRUIT_FONTS
  body_gfx->setFont(&FONT_ITEM);  // Sierlijk font
  body_gfx->setTextSize(1);  // Kleinere size voor Adafruit fonts
#else
  // TFT_eSPI standaard font
  body_gfx->setTextSize(2);  // Groter voor betere leesbaarheid
#endif
  body_gfx->setTextColor(c.color, BODY_CFG.COL_BG);  // Eigen kleur per sensor
#if USE_ADAFRUIT_FONTS
  body_gfx->setCursor(GRAPH_X + 2, y + 14);  // Hoger voor Adafruit fonts, meer ruimte
#else
  body_gfx->setCursor(GRAPH_X + 2, y + 4);   // Meer verticale ruimte
#endif
  body_gfx->print(c.label);
  
  // Numeric values removed - only show label and graph
  
  // Draw mini graph line - adjusted for labels only (no numeric values)
  int graphStartX = GRAPH_X + 100;  // Alleen ruimte voor labels
  int graphW = GRAPH_W - 100 - 10;
  int graphH = CHANNEL_H - 8;  // Meer hoogte voor grafiek
  int graphY = y + 4;
  
  // Draw graph background with light purple border
  uint16_t lightPurple = 0xC618;  // Licht paars
  body_gfx->drawRect(graphStartX, graphY, graphW, graphH, lightPurple);
  
  // Draw samples
  for (int i = 1; i < graphW && i < 160; i++) {
    int idx1 = (c.writeIndex + 160 - graphW + i - 1) % 160;
    int idx2 = (c.writeIndex + 160 - graphW + i) % 160;
    
    float val1 = c.samples[idx1];
    float val2 = c.samples[idx2];
    
    int y1 = graphY + graphH - (int)((val1 - c.minVal) / (c.maxVal - c.minVal) * (graphH - 2));
    int y2 = graphY + graphH - (int)((val2 - c.minVal) / (c.maxVal - c.minVal) * (graphH - 2));
    
    if (y1 < graphY + 1) y1 = graphY + 1;
    if (y1 > graphY + graphH - 1) y1 = graphY + graphH - 1;
    if (y2 < graphY + 1) y2 = graphY + 1;
    if (y2 > graphY + graphH - 1) y2 = graphY + graphH - 1;
    
    body_gfx->drawLine(graphStartX + i - 1, y1, graphStartX + i, y2, c.color);
  }
}

void body_gfx4_drawButtons(bool recording, bool playing, bool menuActive, bool overruleActive, bool recDisabled) {
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug > 5000) {  // Debug elke 5 seconden
    Serial.printf("[BODY] Drawing buttons at Y=%d, W=%d, H=%d\n", BUTTON_Y, BUTTON_W, BUTTON_H);
    lastDebug = millis();
  }
  
  // Button layout: [REC] [PLAY] [MENU] [AI]
  const char* labels[] = {"REC", "PLAY", "MENU", "AI"};
  bool states[] = {recording, playing, menuActive, overruleActive};
  uint16_t colors[] = {
    recording ? BODY_CFG.DOT_RED : BODY_CFG.DOT_GREY,
    playing ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_GREY, 
    menuActive ? BODY_CFG.COL_BRAND : BODY_CFG.DOT_GREY,
    overruleActive ? BODY_CFG.COL_AI : BODY_CFG.COL_BG  // AI: 0xC618 of zwart
  };
  
  for (int i = 0; i < 4; i++) {
    int x = BUTTON_START_X + i * BUTTON_SPACING;
    
    // Draw button with HoofdESP styling - groter en duidelijker
    body_gfx->fillRoundRect(x, BUTTON_Y, BUTTON_W, BUTTON_H, 8, BODY_CFG.COL_BG);
    body_gfx->drawRoundRect(x, BUTTON_Y, BUTTON_W, BUTTON_H, 8, BODY_CFG.COL_FRAME2);
    body_gfx->drawRoundRect(x+2, BUTTON_Y+2, BUTTON_W-4, BUTTON_H-4, 6, BODY_CFG.COL_FRAME);
    
    // Fill button if active
    if (states[i]) {
      body_gfx->fillRoundRect(x+4, BUTTON_Y+4, BUTTON_W-8, BUTTON_H-8, 4, colors[i]);
    }
    
    // Button text - groter voor betere zichtbaarheid
#if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
#else
    // TFT_eSPI standaard font
    body_gfx->setTextSize(2);  // Groter voor touch buttons
#endif
    
    if (i == 3) {  // AI knop
      if (states[i]) {
        // AI AAN: zwarte tekst
        body_gfx->setTextColor(BODY_CFG.COL_BG, BODY_CFG.COL_AI);
      } else {
        // AI UIT: 0xC618 tekst op zwart
        body_gfx->setTextColor(BODY_CFG.COL_AI, BODY_CFG.COL_BG);
      }
    } else {
      // Andere knoppen normaal
      uint16_t lightSkinTone = 0xFDFF;
      body_gfx->setTextColor(states[i] ? BODY_CFG.COL_BG : lightSkinTone, states[i] ? colors[i] : BODY_CFG.COL_BG);
    }
    
    // Center text in button - improved centering
    int16_t x1, y1; uint16_t w, h;
    // TFT_eSPI schatting voor text size 2
    w = strlen(labels[i]) * 12;  // Size 2 = 12px per char
    h = 16;  // Size 2 = 16px height
    int textX = x + (BUTTON_W - w)/2;
    int textY = BUTTON_Y + (BUTTON_H - h)/2 + h - 10;  // Text 10px hoger
    body_gfx->setCursor(textX, textY);
    body_gfx->print(labels[i]);
  }
}

void body_gfx4_drawStatus(const char* text) {
  // Status text disabled - meer ruimte voor grafieken
  // Serial output voor debugging
  if (text) {
    Serial.printf("[BODY] Status: %s\n", text);
  }
}

// Touch cooldown management
void body_gfx4_setCooldown(uint32_t mainScreenMs, uint32_t menuMs) {
  mainScreenCooldownMs = mainScreenMs;
  menuCooldownMs = menuMs;
}

bool body_gfx4_canTouch(bool isMainScreen) {
  uint32_t cooldown = isMainScreen ? mainScreenCooldownMs : menuCooldownMs;
  return (millis() - lastTouchTime) >= cooldown;
}

void body_gfx4_registerTouch(bool isMainScreen) {
  lastTouchTime = millis();
  // Visual feedback - kort flash effect
  if (isMainScreen) {
    // Flash hoofdscherm buttons area voor duidelijke bevestiging
    body_gfx->fillRect(0, 195, 320, 45, 0x4208);  // Grijs overlay
    delay(25);  // Korter om flikkeren te voorkomen
    Serial.println("[BODY] Touch registered - cooldown active");
  } else {
    // Menu feedback - subtiel
    Serial.println("[BODY] Menu touch registered");
  }
}
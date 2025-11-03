#include "body_gfx4.h"
#include "body_display.h"
#include "body_fonts.h"
#include "body_menu.h"

// 🔥 NIEUW: Global rendering pause flag
volatile bool g4_pauseRendering = false;

// Forward declaration
void drawChannel(uint8_t ch);

// Channel data
struct Channel {
  char label[16];
  float samples[240];  // Increased for wider screen
  int writeIndex;
  float minVal, maxVal;
  bool hasData;
  uint16_t color;
};

static Channel channels[G4_NUM];
static char statusText[64] = "Gereed";

// Touch cooldown variables
static uint32_t mainScreenCooldownMs = 2000;  // 2 second cooldown for main screen
static uint32_t menuCooldownMs = 500;         // 0.5 second cooldown for menu
static uint32_t lastTouchTime = 0;            // Unified touch time

// Dynamic layout - scales with SCR_W and SCR_H
// For SC01 Plus: SCR_W=480, SCR_H=320
static int GRAPH_Y = 30;  // Start 30px lager voor ruimte bovenaan (progress bar/tekst)
static int GRAPH_H;       // Calculated from SCR_H
static int GRAPH_W;       // Calculated from SCR_W
static int GRAPH_X = 5;
static int CHANNEL_H;     // Calculated from GRAPH_H

// Button layout - scales with screen size
static int BUTTON_Y;     // Calculated from SCR_H
static int BUTTON_W;     // Calculated from SCR_W
static int BUTTON_H = 35;
static int BUTTON_SPACING;  // Calculated from SCR_W
static int BUTTON_START_X = 5;

void body_gfx4_begin() {
  // Initialiseer canvas NU - body_gfx is al ge-begin()'d in setup()
  extern const int L_CANVAS_W, L_CANVAS_H, L_CANVAS_X, L_CANVAS_Y;
  if (!body_cv && body_gfx) {
    body_cv = new Arduino_Canvas(L_CANVAS_W, L_CANVAS_H, body_gfx, L_CANVAS_X, L_CANVAS_Y);
    // body_cv->begin();  // VERWIJDERD: veroorzaakt I80 bus conflict omdat body_gfx bus al bezit
    Serial.println("[BODY_GFX4] Canvas created (without begin)");
  } else if (!body_gfx) {
    Serial.println("[BODY_GFX4] ERROR: body_gfx not initialized!");
  } else if (body_cv) {
    Serial.println("[BODY_GFX4] Canvas already exists");
  }
  
  // Calculate dynamic layout based on screen size
  GRAPH_W = SCR_W - 10;                 // 470 voor SC01 Plus (480-10)
  GRAPH_H = SCR_H - 80;                 // 240 voor SC01 Plus (320-80: 30px boven, 50px onder)
  CHANNEL_H = GRAPH_H / G4_NUM;         // ~34 voor SC01 Plus (240/7)
  BUTTON_Y = SCR_H - 40;                // 280 voor SC01 Plus
  BUTTON_W = (SCR_W - 30) / 4;          // 112 voor SC01 Plus ((480-30)/4)
  BUTTON_SPACING = (SCR_W - 10) / 4;    // 117 voor SC01 Plus
  
  Serial.printf("[BODY_GFX4] Layout: SCR=%dx%d, GRAPH=%dx%d, BUTTON_Y=%d, BUTTON_W=%d\n", 
                SCR_W, SCR_H, GRAPH_W, GRAPH_H, BUTTON_Y, BUTTON_W);
  
  // Initialize channels with HoofdESP colors
  for (int i = 0; i < G4_NUM; i++) {
    channels[i].writeIndex = 0;
    channels[i].minVal = 0;
    channels[i].maxVal = 100;
    channels[i].hasData = false;
    strcpy(channels[i].label, "---");
    
    for (int j = 0; j < 240; j++) {
      channels[i].samples[j] = 0;
    }
  }
  
  // Set colors according to new specification
  channels[G4_HART].color = BODY_CFG.DOT_RED;       // Rood for heart
  channels[G4_HUID].color = 0xF8FB;                 // Licht huidkleur for skin
  channels[G4_TEMP].color = 0xFD60;                 // Licht-oranje for temperature
  channels[G4_ADEMHALING].color = BODY_CFG.DOT_GREEN; // Groen for breathing
  // channels[G4_OXY].color = 0xFFE0;                  // Geel for oxygen - VERWIJDERD
  channels[G4_HOOFDESP].color = BODY_CFG.COL_BRAND; // Paars for HoofdESP
  channels[G4_ZUIGEN].color = 0x87FF;               // Licht-blauw for suction
  channels[G4_TRIL].color = 0xFB40;                 // Donker oranje for vibration
  
  body_gfx4_clear();
  
  // Draw initial buttons
  body_gfx4_drawButtons(false, false, false, false);
  Serial.println("[BODY] Initial buttons drawn");
}

void body_gfx4_clear() {
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // Draw HoofdESP-style frame - scales with screen size
  body_gfx->drawRoundRect(2, 2, SCR_W-4, SCR_H-4, 12, BODY_CFG.COL_FRAME2);
  body_gfx->drawRoundRect(4, 4, SCR_W-8, SCR_H-8, 10, BODY_CFG.COL_FRAME);
  
  // Teken status tekst bovenaan (in ruimte voor progress bar)
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(1);
  #endif
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  body_gfx->setCursor(10, 8);
  body_gfx->print("AI Powered Body Monitor System");
  
  Serial.println("[BODY] Screen cleared and frame drawn");
}

void body_gfx4_setLabel(uint8_t ch, const char* name) {
  if (ch >= G4_NUM) return;
  strncpy(channels[ch].label, name, sizeof(channels[ch].label) - 1);
  channels[ch].label[sizeof(channels[ch].label) - 1] = '\0';
}

void body_gfx4_pushSample(uint8_t ch, float value, bool mark) {
  // 🔥 NIEUW: Stop rendering als gepauzeerd (tijdens popup)
  if (g4_pauseRendering) return;
  if (ch >= G4_NUM) return;
  
  Channel& c = channels[ch];
  c.samples[c.writeIndex] = value;
  c.writeIndex = (c.writeIndex + 1) % 240;
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
  body_gfx->setTextSize(1);  // 1 maat kleiner
#else
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(2);  // 1 maat kleiner
#endif
  body_gfx->setTextColor(c.color, BODY_CFG.COL_BG);  // Eigen kleur per sensor
#if USE_ADAFRUIT_FONTS
  body_gfx->setCursor(GRAPH_X + 2, y + 28);  // 10px lager dan 18
#else
  body_gfx->setCursor(GRAPH_X + 2, y + 17);  // 10px lager dan 7
#endif
  body_gfx->print(c.label);
  
  // Numeric values removed - only show label and graph
  
  // Draw mini graph line - adjusted for labels only (no numeric values)
  int graphStartX = GRAPH_X + 120;  // Meer ruimte voor labels op groter scherm
  int graphW = GRAPH_W - 120 - 10;
  int graphH = CHANNEL_H - 8;
  int graphY = y + 4;
  
  // Draw graph background with light purple border
  uint16_t lightPurple = 0xC618;  // Licht paars
  body_gfx->drawRect(graphStartX, graphY, graphW, graphH, lightPurple);
  
  // Draw samples - use more samples for wider screen
  int maxSamples = min(graphW, 240);
  for (int i = 1; i < maxSamples; i++) {
    int idx1 = (c.writeIndex + 240 - maxSamples + i - 1) % 240;
    int idx2 = (c.writeIndex + 240 - maxSamples + i) % 240;
    
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
    body_gfx->setFont(nullptr);
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
    
    // Center text in button - improved vertical centering
    int16_t x1, y1; uint16_t w, h;
    body_gfx->getTextBounds((char*)labels[i], 0, 0, &x1, &y1, &w, &h);
    int textX = x + (BUTTON_W - w)/2;
    int textY = BUTTON_Y + (BUTTON_H - h)/2 + h;  // Better vertical centering
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
  // Visual feedback - kort flash effect (scales with screen)
  if (isMainScreen) {
    // Flash hoofdscherm buttons area voor duidelijke bevestiging
    body_gfx->fillRect(0, SCR_H - 45, SCR_W, 45, 0x4208);  // Grijs overlay
    delay(25);  // Korter om flikkeren te voorkomen
    Serial.println("[BODY] Touch registered - cooldown active");
  } else {
    // Menu feedback - subtiel
    Serial.println("[BODY] Menu touch registered");
  }
}
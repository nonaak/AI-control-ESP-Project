#include <Arduino.h>
#include <limits.h>
#include <math.h>
#include <Wire.h>
#include <NintendoExtensionCtrl.h>

#include "fonts.h"
#include "config.h"
#include "ui.h"
#include "espnow_comm.h"
#include "vacuum.h"
#include "display.h"
#include "settings.h"
#include "keon.h"

// --------- C-knop events ----------
enum CEvent : uint8_t { CE_NONE=0, CE_SHORT=1, CE_LONG=2 };
static inline CEvent pollCEvent(bool cNow){
  static bool     cPrev=false;
  static uint32_t t0=0, lastEdge=0;
  const uint16_t LONG_MS=700, DEB_MS=35;
  uint32_t now=millis();
  if (cNow!=cPrev && (now-lastEdge)<DEB_MS) {
  } else if (cNow!=cPrev){
    lastEdge = now;
    if (cNow) t0=now;
    else {
      uint32_t held=now-t0;
      cPrev=cNow;
      if (held>=LONG_MS) return CE_LONG;
      else               return CE_SHORT;
    }
    cPrev=cNow;
  }
  return CE_NONE;
}

// --------- Z-knop events (ENTER/OK + lange druk voor debug LED) ----------
enum ZEvent : uint8_t { ZE_NONE=0, ZE_SHORT=1, ZE_LONG=2 };
static inline ZEvent pollZEvent(bool zNow){
  static bool     zPrev=false;
  static uint32_t t0=0, lastEdge=0;
  const uint16_t LONG_MS=3000, DEB_MS=35;  // 3 seconden voor lange druk
  uint32_t now=millis();
  if (zNow!=zPrev && (now-lastEdge)<DEB_MS) {
    // Debounce - ignore
  } else if (zNow!=zPrev){
    lastEdge = now;
    if (zNow) {
      t0=now;
      Serial.printf("[Z-POLL] Z pressed at %lu\n", now);
    } else {
      uint32_t held=now-t0;
      zPrev=zNow;
      Serial.printf("[Z-POLL] Z released after %lu ms\n", held);
      if (held>=LONG_MS) {
        Serial.println("[Z-POLL] -> LONG press detected!");
        return ZE_LONG;
      } else {
        Serial.println("[Z-POLL] -> SHORT press detected!");
        return ZE_SHORT;
      }
    }
    zPrev=zNow;
  }
  return ZE_NONE;
}

// Legacy Z-knop edge functie (behouden voor compatibiliteit)
static inline bool pollZEdge(bool zNow){
  ZEvent ze = pollZEvent(zNow);
  return (ze == ZE_SHORT);
}

// Z-knop Vibe detectie (dubbele klik voor Vibe)
enum ZClickType : uint8_t { Z_NONE=0, Z_SINGLE=1, Z_VIBE=2 };
static inline ZClickType pollZClick(bool zNow){
  static bool zPrev = false;
  static uint32_t firstClickTime = 0;
  static bool waitingForSecond = false;
  static uint32_t lastEdge = 0;
  const uint16_t DOUBLE_CLICK_MS = CFG.doubleClickTiming;  // Configureerbaar in config.h
  const uint16_t DEB_MS = 35;
  uint32_t now = millis();
  
  // Debounce
  if (zNow != zPrev && (now - lastEdge) < DEB_MS) {
    return Z_NONE;
  }
  
  if (zNow != zPrev) {
    lastEdge = now;
    
    if (!zNow) {  // Button released (click detected)
      if (!waitingForSecond) {
        // First click
        firstClickTime = now;
        waitingForSecond = true;
        Serial.println("[Z-CLICK] First click detected, waiting for second...");
      } else {
        // Second click binnen tijd
        if ((now - firstClickTime) <= DOUBLE_CLICK_MS) {
          waitingForSecond = false;
          Serial.println("[Z-VIBE] Vibe detected!");
          zPrev = zNow;
          return Z_VIBE;
        } else {
          // Te laat voor double click, nieuwe eerste click
          firstClickTime = now;
          Serial.println("[Z-CLICK] Second click too late, treating as new first click");
        }
      }
    }
    zPrev = zNow;
  }
  
  // Check timeout voor single click
  if (waitingForSecond && (now - firstClickTime) > DOUBLE_CLICK_MS) {
    waitingForSecond = false;
    Serial.println("[Z-CLICK] Single click confirmed (timeout)");
    return Z_SINGLE;
  }
  
  return Z_NONE;
}

// ================== Menu text helpers ==================
// Helper function to print text with clipping to avoid overlap with animation
void printClippedText(const char* text, int maxWidth = 0) {
  if (maxWidth <= 0) maxWidth = R_WIN_W - 25; // Default max width with margin
  
  String str = String(text);
  
  // Check if text fits - rough estimation (6 pixels per char for font size 1)
  int estimatedWidth = str.length() * 6;
  
  if (estimatedWidth <= maxWidth) {
    // Text fits - print normally
    gfx->print(text);
  } else {
    // Text too long - truncate with "..."
    int maxChars = (maxWidth - 18) / 6;  // Reserve space for "..."
    if (maxChars > 3) {
      String truncated = str.substring(0, maxChars) + "...";
      gfx->print(truncated);
    } else {
      gfx->print("...");
    }
  }
}

// Helper function to print formatted text with clipping
void printfClipped(const char* format, ...) {
  char buffer[128];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  printClippedText(buffer);
}

// ================== Menu / UI ==================
enum UIMode { MODE_ANIM=0, MODE_MENU=1 };
enum MenuPage { PAGE_MAIN=0, PAGE_SETTINGS=1, PAGE_COLORS=2, PAGE_VACUUM=3, PAGE_MOTION=4, PAGE_ESPNOW=5, PAGE_AUTO_VACUUM=6, PAGE_SMERING=7 };
static UIMode   uiMode = MODE_MENU;
static MenuPage currentPage = PAGE_MAIN;

static int   menuIdx  = 0;
static bool  menuEdit = false;
bool  paused   = true;

static bool  parkToBottom = false;
static float capY_draw    = 0.0f;

// bool keonConnected   = false;  // Now defined in keon.cpp
extern bool keonConnected;
bool solaceConnected = false;
bool motionConnected = false;
bool bodyConnected   = false;

// Vibe status voor M5StickC Plus (bit 3 in ESP-NOW flags)
bool vibeState = false;
bool suctionState = false;  // Suction symbol status

// Sleeve percentage voor Body_ESP synchronisatie
float getSleevePercentage() {
  // Bereken sleeve positie percentage gebaseerd op capY_draw
  const int BL = L_CANVAS_H - 4;
  const int MIN_ROD_VIS_IN = max(2, (int)round(12*1.25f) - 4);
  const int CAP_Y_OUT_BASE = 2;
  const int SL_CAP_H = (int)round(32*1.25f);
  const int CAP_Y_IN_BASE  = (BL - MIN_ROD_VIS_IN) - SL_CAP_H;
  const int CAP_Y_MID      = (CAP_Y_IN_BASE + CAP_Y_OUT_BASE) / 2;
  const float RANGE_SCALE  = 0.90f;
  int CAP_Y_IN  = (int)round(CAP_Y_MID + (CAP_Y_IN_BASE  - CAP_Y_MID) * RANGE_SCALE);
  int CAP_Y_OUT = (int)round(CAP_Y_MID + (CAP_Y_OUT_BASE - CAP_Y_MID) * RANGE_SCALE);
  
  // Huidige sleeve positie (uit animatie of parking)
  float s = 0.5f * (sinf(phase) + 1.0f);
  float ease = schlick_gain(CFG.easeGain, s);
  int capY_phase = (int)round((float)CAP_Y_IN + ease*(float)(CAP_Y_OUT - CAP_Y_IN));
  int capY = parkToBottom ? (int)round(capY_draw) : (!paused ? capY_phase : (int)round(capY_draw));
  
  // Converteer naar percentage: CAP_Y_IN = 0%, CAP_Y_OUT = 100%
  float percentage = ((float)(CAP_Y_IN - capY)) / (float)(CAP_Y_IN - CAP_Y_OUT) * 100.0f;
  return max(0.0f, min(100.0f, percentage));  // Clamp 0-100%
}

// // Suction status - currently tied to zuigen state
// bool suctionState = false;  // VERWIJDERD: dubbele definitie (al op regel 184)

// Connection popup variables
static bool connectionPopupOpen = false;
static int connectionDeviceIdx = -1; // 0=Keon, 1=Solace
static int connectionChoiceIdx = 0; // 0=Ja, 1=Nee

// Nunchuk help popup variables
static bool nunchukHelpPopupOpen = false;

// Connection attempt tracking
static bool connectionInProgress = false;
static uint32_t connectionStartTime = 0;
static const uint32_t CONNECTION_TIMEOUT_MS = 5000; // 5 second timeout

// Non-blocking connection attempt functions
static void startKeonConnection() {
  Serial.println("[CONNECTION] Starting Keon connection attempt...");
  connectionInProgress = true;
  connectionStartTime = millis();
  
  // Call real Keon connect in background
  extern bool keonConnect();
  keonConnect();
}

static bool checkKeonConnectionProgress() {
  if (!connectionInProgress) return false;
  
  uint32_t elapsed = millis() - connectionStartTime;
  
  // Give it time to connect
  if (elapsed < KEON_CONNECTION_TIMEOUT_MS) {
    return false;  // Still connecting
  }
  
  // Check result
  connectionInProgress = false;
  extern bool keonConnected;
  return keonConnected;
}

static void disconnectKeon() {
  if (keonConnected) {
    Serial.println("[CONNECTION] Disconnecting from Keon...");
    extern void keonDisconnect();
    keonDisconnect();
    keonConnected = false;
    Serial.println("[CONNECTION] Keon disconnected");
  }
}

static void startSolaceConnection() {
  Serial.println("[CONNECTION] Starting Solace connection attempt...");
  connectionInProgress = true;
  connectionStartTime = millis();
}

static bool checkSolaceConnectionProgress() {
  if (!connectionInProgress) return false;
  
  uint32_t elapsed = millis() - connectionStartTime;
  
  // Simulate connection process stages (Solace takes slightly longer)
  if (elapsed < 1200) {
    // Still connecting...
    return false;
  } else if (elapsed < 1400) {
    // Connection attempt completed
    connectionInProgress = false;
    
    // Simulate success/failure (75% success rate for Solace)
    bool success = (random(100) < 75);
    
    if (success) {
      Serial.println("[CONNECTION] Solace handshake successful");
      Serial.println("[CONNECTION] Solace connected successfully");
      return true;
    } else {
      Serial.println("[CONNECTION] Solace handshake failed");
      Serial.println("[CONNECTION] Solace connection failed - device not responding");
      return false;
    }
  }
  
  return false; // Still in progress
}

static void disconnectSolace() {
  if (solaceConnected) {
    Serial.println("[CONNECTION] Disconnecting from Solace...");
    // Send disconnect command to device
    Serial.println("[CONNECTION] Sending disconnect signal to Solace");
    delay(200); // Simulate disconnect time
    solaceConnected = false;
    Serial.println("[CONNECTION] Solace disconnected");
  }
}

// Nunchuk
static Nunchuk nchuk;
static bool    nkReady = false;

static void drawStatusDot(int x, int y, uint16_t col, int r=5){
  gfx->fillCircle(x, y, r, col);
  gfx->drawCircle(x, y, r, 0x0000);
}

static void setMenuFontTitle(){
#if USE_ADAFRUIT_FONTS
  gfx->setFont(&FONT_TITLE);
  gfx->setTextSize(1);
#else
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
#endif
}
static void setMenuFontItem(){
#if USE_ADAFRUIT_FONTS
  gfx->setFont(&FONT_ITEM);
  gfx->setTextSize(1);
#else
  gfx->setFont(nullptr);
  gfx->setTextSize(1);
#endif
}

// ================== Kleuren-menu items ==================
enum ColorItemType { C_RGB565, C_U8TRIPLE };
struct ColorItem { const char *label; ColorItemType type; uint16_t *p565; uint8_t *pr,*pg,*pb; };

uint16_t g_targetStrokes = 30;  // Removed static for ESP-NOW access
float    g_lubeHold_s    = 0.5f;  // Made extern for settings.cpp
float    g_startLube_s   = 0.5f;  // Made extern for settings.cpp

// NOTE: Persistent settings are now handled by settings.h/settings.cpp

static ColorItem COLORS[] = {
  {"Frame buiten",        C_RGB565,   &CFG.COL_FRAME2,  nullptr,nullptr,nullptr},
  {"Frame binnen",        C_RGB565,   &CFG.COL_FRAME,   nullptr,nullptr,nullptr},
  {"Sleeve set",          C_RGB565,   &CFG.COL_TAN,     nullptr,nullptr,nullptr},
  {"Rod langzaam",        C_U8TRIPLE, nullptr,          &CFG.rodSlowR,&CFG.rodSlowG,&CFG.rodSlowB},
  {"Rod snel",            C_U8TRIPLE, nullptr,          &CFG.rodFastR,&CFG.rodFastG,&CFG.rodFastB},
  {"Pijl",                C_RGB565,   &CFG.COL_ARROW,   nullptr,nullptr,nullptr},
  {"Pijl glow",           C_RGB565,   &CFG.COL_ARROW_GLOW, nullptr,nullptr,nullptr},
  {"Speedbar rand",       C_RGB565,   &CFG.SPEEDBAR_BORDER, nullptr,nullptr,nullptr},
  {"Brand (Draak)",       C_RGB565,   &CFG.COL_BRAND,   nullptr,nullptr,nullptr},
  {"Menu tekst",          C_RGB565,   &CFG.COL_MENU_PINK, nullptr,nullptr,nullptr}
};
static const int COLORS_COUNT = sizeof(COLORS)/sizeof(COLORS[0]);

static bool  colorEdit = false;
static uint8_t tempR, tempG, tempB;
static int   colorScroll = 0;
static bool paletteOpen = false;
static const int PAL_COLS = 16;
static const int PAL_ROWS = 8;
static int palX = 0, palY = 0;

static const int JX_LO = 70,  JX_HI = 180;
static const int JY_LO = 70,  JY_HI = 180;

static int  editingGlobalIdx = -1;
static bool backupIs565 = false;
static uint16_t backup565 = 0;
static uint8_t  backupR=0, backupG=0, backupB=0;

static inline int colorsTotalItems() { return 2 + COLORS_COUNT; }
static inline bool isControlRow(int gi){ return gi <= 1; }
static inline int  toColorIndex(int gi){ return gi - 2; }

static void applyTempToTarget_globalIndex(int gi){
  int idx = toColorIndex(gi);
  if (idx < 0 || idx >= COLORS_COUNT) return;
  ColorItem &ci = COLORS[idx];
  if (ci.type == C_RGB565) *ci.p565 = u8_to_RGB565(tempR,tempG,tempB);
  else { *ci.pr = tempR; *ci.pg = tempG; *ci.pb = tempB; }
}
static void beginColorEdit_globalIndex(int gi){
  int idx = toColorIndex(gi);
  if (idx < 0 || idx >= COLORS_COUNT) return;
  ColorItem &ci = COLORS[idx];

  if (ci.type == C_RGB565){
    backupIs565 = true;
    backup565   = *ci.p565;
    RGB565_to_u8(*ci.p565, tempR,tempG,tempB);
  } else {
    backupIs565 = false;
    backupR = *ci.pr; backupG = *ci.pg; backupB = *ci.pb;
    tempR = backupR; tempG = backupG; tempB = backupB;
  }

  float h,s,v; rgb2hsv_u8(tempR,tempG,tempB, h,s,v);
  palX = (int)roundf(h * (PAL_COLS-1));
  float vMin=0.35f, vMax=1.0f; float t = (v - vMin) / (vMax - vMin); if (t<0) t=0; if (t>1) t=1;
  palY = (PAL_ROWS-1) - (int)roundf(t*(PAL_ROWS-1));
  if (palX<0) palX=0; if (palX>PAL_COLS-1) palX=PAL_COLS-1;
  if (palY<0) palY=0; if (palY>PAL_ROWS-1) palY=PAL_ROWS-1;

  editingGlobalIdx = gi;
  colorEdit = true;
  paletteOpen = true;

  applyTempToTarget_globalIndex(editingGlobalIdx);
}
static void commitColorEdit_globalIndex(int gi){ 
  applyTempToTarget_globalIndex(gi); 
  saveAllSettings(); // Save color changes to flash
}
static void cancelColorEdit_globalIndex(int gi){
  int idx = toColorIndex(gi);
  if (idx < 0 || idx >= COLORS_COUNT) return;
  ColorItem &ci = COLORS[idx];
  if (backupIs565 && ci.type==C_RGB565) *ci.p565 = backup565;
  else if (!backupIs565 && ci.type==C_U8TRIPLE){ *ci.pr = backupR; *ci.pg = backupG; *ci.pb = backupB; }
}
static void resetColorsToDefault(){
  CFG.COL_BG       = DEF_COL_BG;
  CFG.COL_FRAME    = DEF_COL_FRAME;
  CFG.COL_FRAME2   = DEF_COL_FRAME2;
  CFG.COL_TAN      = DEF_COL_TAN;
  CFG.rodSlowR     = DEF_rodSlowR; CFG.rodSlowG = DEF_rodSlowG; CFG.rodSlowB = DEF_rodSlowB;
  CFG.rodFastR     = DEF_rodFastR; CFG.rodFastG = DEF_rodFastG; CFG.rodFastB = DEF_rodFastB;
  CFG.COL_ARROW    = DEF_COL_ARROW;
  CFG.COL_ARROW_GLOW = DEF_COL_GLOW;
  CFG.DOT_RED      = DEF_DOT_RED; CFG.DOT_BLUE=DEF_DOT_BLUE; CFG.DOT_GREEN=DEF_DOT_GREEN; CFG.DOT_GREY=DEF_DOT_GREY;
  CFG.SPEEDBAR_BORDER = DEF_SPEED_BORDER;
  CFG.COL_BRAND    = DEF_COL_BRAND;
  CFG.COL_MENU_PINK= DEF_MENU_PINK;
  saveAllSettings(); // Save color reset to flash
}

static int colorsVisibleRows(){
  const int topY = R_WIN_Y + 60;
  const int bottomHint = 14;
  const int LH=20;
  int usable = (R_WIN_H - (topY - R_WIN_Y)) - bottomHint;
  int vis = usable / LH; if (vis < 1) vis = 1; return vis;
}
static void ensureColorsScrollVisible(){
  int vis = colorsVisibleRows();
  int total = colorsTotalItems();
  if (menuIdx < colorScroll) colorScroll = menuIdx;
  if (menuIdx >= colorScroll + vis) colorScroll = menuIdx - (vis - 1);
  if (colorScroll < 0) colorScroll = 0;
  if (colorScroll > max(0, total - vis)) colorScroll = max(0, total - vis);
}

static void drawPalettePopup(){
  int pad=10;
  int bx = R_WIN_X + 8;
  int by = R_WIN_Y + 36;
  int bw = R_WIN_W - 16;
  int bh = R_WIN_H - 36 - 18;

  gfx->fillRoundRect(bx, by, bw, bh, 10, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 10, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 9,  CFG.COL_FRAME);

  int gw = bw - 2*pad;
  int gh = bh - 2*pad - 16;
  int gx = bx + pad;
  int gy = by + pad;

  int cellW = max(6, gw / PAL_COLS);
  int cellH = max(6, gh / PAL_ROWS);

  for (int r=0; r<PAL_ROWS; r++){
    for (int c=0; c<PAL_COLS; c++){
      float h = (float)c / (float)(PAL_COLS-1);
      float v = 0.35f + 0.65f * (float)(PAL_ROWS-1 - r) / (float)(PAL_ROWS-1);
      uint8_t rr,gg,bb; hsv2rgb_u8(h,1.0f,v, rr,gg,bb);
      uint16_t cc = u8_to_RGB565(rr,gg,bb);
      int cx = gx + c*cellW;
      int cy = gy + r*cellH;
      gfx->fillRect(cx, cy, cellW-1, cellH-1, cc);
    }
  }

  int sx = gx + palX*cellW;
  int sy = gy + palY*cellH;
  gfx->drawRect(sx-1, sy-1, cellW+1, cellH+1, CFG.COL_BRAND);

  uint16_t cur = u8_to_RGB565(tempR,tempG,tempB);
  gfx->drawRect(bx+pad, gy+gh+4, 40, 12, CFG.COL_FRAME2);
  gfx->fillRect(bx+pad+1, gy+gh+5, 38, 10, cur);

  gfx->setFont(nullptr); gfx->setTextSize(1);
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(bx+pad+48, gy+gh+14);
  gfx->print("JX/JY: kies   Z: OK   C: annuleer");
}

static void setMenuTitleAndItems(){
  // "INSTELLINGEN" en "MOTION BLEND" kleiner (item-font), exact zoals gevraagd
  if (currentPage == PAGE_SETTINGS || currentPage == PAGE_MOTION || currentPage == PAGE_AUTO_VACUUM || currentPage == PAGE_SMERING) setMenuFontItem();
  else                              setMenuFontTitle();

  gfx->setTextColor(0xFFFF, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y+36);
  if (currentPage == PAGE_MAIN)           gfx->print("MENU");
  else if (currentPage == PAGE_SETTINGS)  gfx->print("INSTELLINGEN");
  else if (currentPage == PAGE_COLORS)    gfx->print("KLEUREN");
  else if (currentPage == PAGE_VACUUM)    gfx->print("ZUIGEN");
  else if (currentPage == PAGE_MOTION)    gfx->print("MOTION BLEND");
  else if (currentPage == PAGE_ESPNOW)    gfx->print("ESP STATUS");
  else if (currentPage == PAGE_AUTO_VACUUM) gfx->print("AUTO VACUUM");
  else if (currentPage == PAGE_SMERING)   gfx->print("SMERING");
  else                                    gfx->print("MENU");

  setMenuFontItem();
}

static void drawEditPopupInMenu(){
  if (!(uiMode==MODE_MENU && menuEdit)) return;
  if (currentPage != PAGE_MAIN) return;
  if (menuIdx < 4 || menuIdx > 6) return;

  const int listY0 = R_WIN_Y + 60; const int LH = 20;
  int rowY = listY0 + menuIdx*LH;
  int bx = R_WIN_X + 8;
  int by = rowY - 14;
  int bw = R_WIN_W - 16;
  int bh = 26;

  if (by < R_WIN_Y + 8) by = R_WIN_Y + 8;
  if (by + bh > R_WIN_Y + R_WIN_H - 8) by = R_WIN_Y + R_WIN_H - 8 - bh;

  gfx->fillRoundRect(bx, by, bw, bh, 6, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 6, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 5, CFG.COL_FRAME);

  setMenuFontItem(); gfx->setTextColor(0xFFFF, CFG.COL_BG);
  const char* label = (menuIdx==4) ? "Pushes" : (menuIdx==5) ? "Lubrication" : "Start-Lubric.";
  char value[24];
  if (menuIdx==4) snprintf(value,sizeof(value),"%u",(unsigned)g_targetStrokes);
  else if(menuIdx==5) snprintf(value,sizeof(value),"%.1f s", g_lubeHold_s);
  else snprintf(value,sizeof(value),"%.1f s", g_startLube_s);

  gfx->setCursor(bx+10, by+17); gfx->print(label);
  gfx->setCursor(bx + bw - 10 - 60, by+17); gfx->print(value);

  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410,CFG.COL_BG);
  gfx->setCursor(bx + bw - 58, by + bh - 2); gfx->print("Y +/-  Z OK");
}

static void drawVacuumEditPopup(){
  if (!(uiMode==MODE_MENU && menuEdit)) return;
  if (currentPage != PAGE_VACUUM) return;
  if (menuIdx != 1) return;

  const int listY0 = R_WIN_Y + 60; const int LH = 20;
  int rowY = listY0 + menuIdx*LH;
  int bx = R_WIN_X + 8;
  int by = rowY - 14;
  int bw = R_WIN_W - 16;
  int bh = 26;

  if (by < R_WIN_Y + 8) by = R_WIN_Y + 8;
  if (by + bh > R_WIN_Y + R_WIN_H - 8) by = R_WIN_Y + R_WIN_H - 8 - bh;

  gfx->fillRoundRect(bx, by, bw, bh, 6, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 6, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 5, CFG.COL_FRAME);

  setMenuFontItem(); gfx->setTextColor(0xFFFF, CFG.COL_BG);
  const char* label;
  char value[32];
  
  // Alleen Vacuum Level edit nog mogelijk
  label = "Target Vacuum"; 
  snprintf(value, sizeof(value), "T:%.0f", abs(CFG.vacuumTargetMbar));

  gfx->setCursor(bx+10, by+17); gfx->print(label);
  gfx->setCursor(bx + bw - 10 - 60, by+17); gfx->print(value);

  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410,CFG.COL_BG);
  gfx->setCursor(bx + bw - 58, by + bh - 2); gfx->print("Y +/-  Z OK");
}

static void drawAutoVacuumEditPopup(){
  if (!(uiMode==MODE_MENU && menuEdit)) return;
  if (currentPage != PAGE_AUTO_VACUUM) return;
  if (menuIdx < 1 || menuIdx > 2) return;

  const int listY0 = R_WIN_Y + 60; const int LH = 20;
  int rowY = listY0 + menuIdx*LH;
  int bx = R_WIN_X + 8;
  int by = rowY - 14;
  int bw = R_WIN_W - 16;
  int bh = 26;

  if (by < R_WIN_Y + 8) by = R_WIN_Y + 8;
  if (by + bh > R_WIN_Y + R_WIN_H - 8) by = R_WIN_Y + R_WIN_H - 8 - bh;

  gfx->fillRoundRect(bx, by, bw, bh, 6, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 6, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 5, CFG.COL_FRAME);

  setMenuFontItem(); gfx->setTextColor(0xFFFF, CFG.COL_BG);
  const char* label;
  char value[32];
  
  switch(menuIdx){
    case 1: label = "Auto Vacuum"; snprintf(value, sizeof(value), "%s", CFG.vacuumAutoMode ? "Aan" : "Uit"); break;
    case 2: label = "Stop bij snelheid"; snprintf(value, sizeof(value), "  %d / %d", CFG.autoVacuumSpeedThreshold, CFG.SPEED_STEPS-1); break;
    default: label = "Error"; strcpy(value, "???"); break;
  }

  gfx->setCursor(bx+10, by+17); gfx->print(label);
  gfx->setCursor(bx + bw - 10 - 60, by+17); gfx->print(value);

  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410,CFG.COL_BG);
  gfx->setCursor(bx + bw - 58, by + bh - 2); gfx->print("Y +/-  Z OK");
}

static void drawSmeringEditPopup(){
  if (!(uiMode==MODE_MENU && menuEdit)) return;
  if (currentPage != PAGE_SMERING) return;
  if (menuIdx < 1 || menuIdx > 3) return;

  const int listY0 = R_WIN_Y + 60; const int LH = 20;
  int rowY = listY0 + menuIdx*LH;
  int bx = R_WIN_X + 8;
  int by = rowY - 14;
  int bw = R_WIN_W - 16;
  int bh = 26;

  if (by < R_WIN_Y + 8) by = R_WIN_Y + 8;
  if (by + bh > R_WIN_Y + R_WIN_H - 8) by = R_WIN_Y + R_WIN_H - 8 - bh;

  gfx->fillRoundRect(bx, by, bw, bh, 6, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 6, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 5, CFG.COL_FRAME);

  setMenuFontItem(); gfx->setTextColor(0xFFFF, CFG.COL_BG);
  const char* label;
  char value[32];
  
  switch(menuIdx){
    case 1: label = "Pushes"; snprintf(value, sizeof(value), "%u", (unsigned)g_targetStrokes); break;
    case 2: label = "Lubrication"; snprintf(value, sizeof(value), "%.1f s", g_lubeHold_s); break;
    case 3: label = "Start-Lubric."; snprintf(value, sizeof(value), "%.1f s", g_startLube_s); break;
    default: label = "Error"; strcpy(value, "???"); break;
  }

  gfx->setCursor(bx+10, by+17); gfx->print(label);
  gfx->setCursor(bx + bw - 10 - 60, by+17); gfx->print(value);

  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410,CFG.COL_BG);
  gfx->setCursor(bx + bw - 58, by + bh - 2); gfx->print("Y +/-  Z OK");
}

static void drawMotionEditPopup(){
  if (!(uiMode==MODE_MENU && menuEdit)) return;
  if (currentPage != PAGE_MOTION) return;
  if (menuIdx < 1 || menuIdx > 4) return;

  const int listY0 = R_WIN_Y + 60; const int LH = 20;
  int rowY = listY0 + menuIdx*LH;
  int bx = R_WIN_X + 8;
  int by = rowY - 14;
  int bw = R_WIN_W - 16;
  int bh = 26;

  if (by < R_WIN_Y + 8) by = R_WIN_Y + 8;
  if (by + bh > R_WIN_Y + R_WIN_H - 8) by = R_WIN_Y + R_WIN_H - 8 - bh;

  gfx->fillRoundRect(bx, by, bw, bh, 6, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 6, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 5, CFG.COL_FRAME);

  setMenuFontItem(); gfx->setTextColor(0xFFFF, CFG.COL_BG);
  const char* label;
  char value[32];
  
  switch(menuIdx){
    case 1: label = "Motion Blend"; snprintf(value, sizeof(value), "%s", CFG.motionBlendEnabled ? "Aan" : "Uit"); break;
    case 2: label = "Nunchuk Weight"; snprintf(value, sizeof(value), "%.0f%%", CFG.userSpeedWeight); break;
    case 3: label = "Motion Sensor Weight"; snprintf(value, sizeof(value), "%.0f%%", CFG.motionSpeedWeight); break;
    case 4: label = "Direction Sync"; snprintf(value, sizeof(value), "%s", CFG.motionDirectionSync ? "Aan" : "Uit"); break;
    default: label = "Error"; strcpy(value, "???"); break;
  }

  gfx->setCursor(bx+10, by+17); gfx->print(label);
  gfx->setCursor(bx + bw - 10 - 60, by+17); gfx->print(value);

  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410,CFG.COL_BG);
  gfx->setCursor(bx + bw - 58, by + bh - 2); gfx->print("Y +/-  Z OK");
}

static void drawConnectionPopup(){
  if (!connectionPopupOpen) return;
  
  const char* deviceName = (connectionDeviceIdx == 0) ? "Keon" : "Solace";
  bool isConnected = (connectionDeviceIdx == 0) ? keonConnected : solaceConnected;
  
  // Center popup on screen
  int bx = R_WIN_X + 20;
  int by = R_WIN_Y + 80;
  int bw = R_WIN_W - 40;
  int bh = connectionInProgress ? 100 : 80;  // Larger popup if connecting
  
  // Draw popup background
  gfx->fillRoundRect(bx, by, bw, bh, 8, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 8, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 7, CFG.COL_FRAME);
  
  // Title
  setMenuFontItem();
  gfx->setTextColor(0xFFFF, CFG.COL_BG);
  gfx->setCursor(bx + 10, by + 20);
  if (isConnected) {
    gfx->print("Verbreken met ");
  } else {
    gfx->print("Verbinden met ");
  }
  gfx->print(deviceName);
  gfx->print("?");
  
  // Show connection status or options
  if (connectionInProgress) {
    // Show connection progress
    setMenuFontItem();
    gfx->setTextColor(0xFFFF, CFG.COL_BG);
    gfx->setCursor(bx + 10, by + 45);
    gfx->print("Verbinden...");
    
    // Show animated dots
    static uint32_t lastDotUpdate = 0;
    static int dotCount = 0;
    if (millis() - lastDotUpdate > 300) {
      dotCount = (dotCount + 1) % 4;
      lastDotUpdate = millis();
    }
    
    gfx->setCursor(bx + 10, by + 65);
    for (int i = 0; i < dotCount; i++) {
      gfx->print(".");
    }
    
    // Connection timeout check
    if (millis() - connectionStartTime > CONNECTION_TIMEOUT_MS) {
      connectionInProgress = false;
      gfx->setTextColor(0xF800, CFG.COL_BG); // Red for failed
      gfx->setCursor(bx + 10, by + 85);
      gfx->print("Verbinding mislukt!");
    }
  } else {
    // Show normal options
    const int optY = by + 40;
    const int optSpacing = 60;
    
    for (int i = 0; i < 2; i++) {
      bool selected = (i == connectionChoiceIdx);
      uint16_t color = selected ? 0x07E0 : CFG.COL_MENU_PINK;
      
      gfx->setTextColor(color, CFG.COL_BG);
      gfx->setCursor(bx + 20 + i * optSpacing, optY);
      
      if (i == 0) gfx->print("Ja");
      else gfx->print("Nee");
    }
  }
  
  // Help text
  gfx->setFont(nullptr); gfx->setTextSize(1);
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(bx + 10, by + bh - 12);
  if (connectionInProgress) {
    gfx->print("Verbinden...   C: annuleer");
  } else {
    gfx->print("JX: kies   Z: OK   C: annuleer");
  }
}

static void drawNunchukHelpPopup(){
  if (!nunchukHelpPopupOpen) return;
  
  // Center popup op scherm
  int bx = R_WIN_X + 10;
  int by = R_WIN_Y + 30;
  int bw = R_WIN_W - 20;
  int bh = R_WIN_H - 60;
  
  // Popup achtergrond
  gfx->fillRoundRect(bx, by, bw, bh, 8, CFG.COL_BG);
  gfx->drawRoundRect(bx, by, bw, bh, 8, CFG.COL_FRAME2);
  gfx->drawRoundRect(bx+1, by+1, bw-2, bh-2, 7, CFG.COL_FRAME);
  
  // Title - kleinere font
  gfx->setFont(nullptr);
  gfx->setTextSize(2);
  gfx->setTextColor(CFG.COL_BRAND, CFG.COL_BG);
  gfx->setCursor(bx + 10, by + 20);
  gfx->print("Nunchuk Knoppen");
  
  // Help tekst - gebruik kleinere font
  gfx->setFont(nullptr);
  gfx->setTextSize(1);
  gfx->setTextColor(0xFFFF, CFG.COL_BG);
  int textY = by + 50;
  int lineH = 12;
  
  gfx->setCursor(bx + 10, textY); textY += lineH;
  gfx->print("JY Stick: Snelheid +/-");
  
  gfx->setCursor(bx + 10, textY); textY += lineH;
  gfx->print("JX Stick: Menu navigatie");
  
  gfx->setCursor(bx + 10, textY); textY += lineH + 4;
  gfx->print("Z Knop (grote knop):");
  
  gfx->setTextColor(CFG.COL_MENU_PINK, CFG.COL_BG);
  gfx->setCursor(bx + 20, textY); textY += lineH;
  gfx->print("- Menu: Bevestig/Open");
  
  gfx->setCursor(bx + 20, textY); textY += lineH;
  gfx->print("- Animatie: Zuigen toggle");
  
  gfx->setCursor(bx + 20, textY); textY += lineH;
  gfx->print("- Smering: Start lube");
  
  gfx->setTextColor(0xFFFF, CFG.COL_BG);
  gfx->setCursor(bx + 10, textY); textY += lineH + 4;
  gfx->print("C Knop (kleine knop):");
  
  gfx->setTextColor(CFG.COL_MENU_PINK, CFG.COL_BG);
  gfx->setCursor(bx + 20, textY); textY += lineH;
  gfx->print("- Kort: Pause/Menu toggle");
  
  gfx->setCursor(bx + 20, textY); textY += lineH;
  gfx->print("- Lang: Animatie/Menu wissel");
  
  // Help tekst onderaan
  gfx->setFont(nullptr); gfx->setTextSize(1);
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(bx + 10, by + bh - 12);
  gfx->print("C: sluiten");
}

static void drawColorsPage(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  setMenuTitleAndItems();

  setMenuFontItem();
  const int LH = 20;
  int y = R_WIN_Y + 60;
  int vis = colorsVisibleRows();
  int total = colorsTotalItems(); 
  ensureColorsScrollVisible();

  for (int i=0;i<vis;i++){
    int gi = colorScroll + i;
    if (gi >= total) break;

    bool sel = (uiMode==MODE_MENU && gi==menuIdx);
    uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
    gfx->setTextColor(col, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);

    if (isControlRow(gi)){
      gfx->print(gi==0 ? "< Terug" : "Default");
    } else {
      int idx = toColorIndex(gi);
      gfx->print(COLORS[idx].label);

      uint16_t c = (COLORS[idx].type==C_RGB565)
                    ? *COLORS[idx].p565
                    : u8_to_RGB565(*COLORS[idx].pr,*COLORS[idx].pg,*COLORS[idx].pb);
      int sw=24, sh=10;
      int sx = R_WIN_X + R_WIN_W - 10 - sw;
      int sy = y-10;
      gfx->fillRect(sx, sy, sw, sh, c);
      gfx->drawRect(sx, sy, sw, sh, CFG.COL_FRAME2);
    }
    y += LH;
  }

  if (colorScroll > 0) { gfx->setCursor(R_WIN_X + R_WIN_W - 18, R_WIN_Y + 52); gfx->print("^"); }
  if (colorScroll + vis < total) { gfx->setCursor(R_WIN_X + R_WIN_W - 18, R_WIN_Y + R_WIN_H - 28); gfx->print("v"); }

  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
  if (!paletteOpen) gfx->print("Y: scroll  Z: edit/open  C: menu");
  else              gfx->print("Palette: JX/JY kies  Z: OK  C: annuleren");

  if (paletteOpen) drawPalettePopup();
}

static void drawESPNowPage(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  setMenuTitleAndItems();
  setMenuFontItem();
  
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  // Menu items
  for (int i=0; i<1; i++){
    bool sel = (uiMode==MODE_MENU && i==menuIdx);
    uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
    gfx->setTextColor(col, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);
    
    switch(i){
      case 0: gfx->print("< Terug"); break;
    }
    y += LH;
  }
  
  // Real-time telemetrie sectie
  y += 10; // Extra ruimte
  gfx->setTextColor(0x8410, CFG.COL_BG); // Grijze kleur voor telemetrie
  gfx->setCursor(R_WIN_X+20, y);
  printClippedText("== ESP Status ==");
  y += LH;
  
  // Connection status - ESP-NOW modules
  gfx->setCursor(R_WIN_X+20, y);
  uint16_t pumpColor = pumpUnit_connected ? 0x07E0 : 0xF800; // Groen/Rood
  gfx->setTextColor(pumpColor, CFG.COL_BG);
  gfx->printf("Pump Unit: %s", pumpUnit_connected ? "ONLINE" : "OFFLINE");
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  uint16_t bodyColor = bodyESP_connected ? 0x07E0 : 0xF800; // Groen/Rood
  gfx->setTextColor(bodyColor, CFG.COL_BG);
  gfx->printf("Body ESP: %s", bodyESP_connected ? "ONLINE" : "OFFLINE");
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  uint16_t atomColor = motionConnected ? 0x07E0 : 0xF800; // Groen/Rood
  gfx->setTextColor(atomColor, CFG.COL_BG);
  gfx->printf("M5 Atom: %s", motionConnected ? "ONLINE" : "OFFLINE");
  y += LH;
  
  // Motion sync info
  y += 10;
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  printClippedText("== ESP Status ==");
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  if (motionConnected) {
    const char* dirStr = (currentMotionDir > 0) ? "UP" : (currentMotionDir < 0) ? "DOWN" : "STILL";
    gfx->printf("Motion: %s (%d%% speed)", dirStr, currentMotionSpeed);
  } else {
    gfx->print("Motion: NO DATA");
  }
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  extern uint8_t g_speedStep;
  gfx->printf("Animation Speed: %d/%d", g_speedStep, CFG.SPEED_STEPS-1);
  y += LH;
  
  // Hardware status sectie
  y += 10;
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  printClippedText("== Motion Control ==");
  y += LH;
  
  // Vacuum reading
  gfx->setCursor(R_WIN_X+20, y);
  float currentVacuumMbar = currentVacuumReading / 0.75f;  // Convert cmHg to mbar (1 mbar = 0.75 cmHg)
  gfx->printf("Vacuum: %.0f mbar", abs(currentVacuumMbar));
  y += LH;
  
  // Pump status
  gfx->setCursor(R_WIN_X+20, y);
  gfx->printf("Vac Pump: %s", vacuumPumpStatus ? "AAN" : "UIT");
  y += LH;
  
  // Lube pump status
  gfx->setCursor(R_WIN_X+20, y);
  gfx->printf("Lube Pump: %s", lubePumpStatus ? "AAN" : "UIT");
  y += LH;
  
  // Zuigen mode status - REMOVED to prevent overlap with animation
  
  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
  printClippedText("Y:sel Z:back C:menu");
}

static void drawVacuumPage(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  setMenuTitleAndItems();
  setMenuFontItem();
  
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  // Menu items
  for (int i=0; i<2; i++){
    bool sel = (uiMode==MODE_MENU && i==menuIdx);
    uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
    gfx->setTextColor(col, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);
    
    switch(i){
      case 0: gfx->print("< Terug"); break;
      case 1: { char b[64]; snprintf(b,sizeof(b),"T:%.0f Vacuum Level", abs(CFG.vacuumTargetMbar)); gfx->print(b); } break;
    }
    y += LH;
  }
  
  // Status info - simplified
  y += 10;
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  gfx->print("Druk Z-knop voor zuigen");
  y += LH;
  
  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
  printClippedText("Y:sel Z:edit C:menu");
  
  // Toon edit popup als we in edit mode zijn
  drawVacuumEditPopup();
}

static void drawAutoVacuumPage(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  setMenuTitleAndItems();
  setMenuFontItem();
  
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  // Menu items
  for (int i=0; i<3; i++){
    bool sel = (uiMode==MODE_MENU && i==menuIdx);
    uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
    gfx->setTextColor(col, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);
    
    switch(i){
      case 0: gfx->print("< Terug"); break;
      case 1: { char b[64]; snprintf(b,sizeof(b),"Auto Vacuum: %s", CFG.vacuumAutoMode ? "Aan" : "Uit"); gfx->print(b); } break;
      case 2: { char b[64]; snprintf(b,sizeof(b),"Stop bij snelheid:   %d / %d", CFG.autoVacuumSpeedThreshold, CFG.SPEED_STEPS-1); gfx->print(b); } break;
    }
    y += LH;
  }
  
  // Status info
  y += 10;
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  printClippedText("== Auto Vacuum ==");
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  extern uint8_t g_speedStep;
  bool speedDisabled = (g_speedStep >= CFG.autoVacuumSpeedThreshold);
  uint16_t statusColor = speedDisabled ? 0xF800 : 0x07E0; // Rood als uitgeschakeld door snelheid
  gfx->setTextColor(statusColor, CFG.COL_BG);
  
  if (!CFG.vacuumAutoMode) {
    gfx->print("AUTO VACUUM: UIT");
  } else if (speedDisabled) {
    gfx->printf("UIT (snelheid %d >= %d)", g_speedStep, CFG.autoVacuumSpeedThreshold);
  } else {
    gfx->printf("ACTIEF (snelheid %d < %d)", g_speedStep, CFG.autoVacuumSpeedThreshold);
  }
  y += LH;
  
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  gfx->print("Bij hoge snelheid kan vacuum");
  y += LH;
  gfx->setCursor(R_WIN_X+20, y);
  gfx->print("de animatie niet bijhouden.");
  y += LH;
  
  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
  printClippedText("Y:sel Z:edit C:menu");
  
  // Toon edit popup als we in edit mode zijn
  drawAutoVacuumEditPopup();
}

static void drawSmeringPage(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  setMenuTitleAndItems();
  setMenuFontItem();
  
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  // Menu items
  for (int i=0; i<4; i++){
    bool sel = (uiMode==MODE_MENU && i==menuIdx);
    uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
    gfx->setTextColor(col, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);
    
    switch(i){
      case 0: gfx->print("< Terug"); break;
      case 1: { char b[48]; snprintf(b,sizeof(b),"Pushes Lube at: %u", (unsigned)g_targetStrokes); gfx->print(b); } break;
      case 2: { char b[48]; snprintf(b,sizeof(b),"Lubrication: %.1f s", g_lubeHold_s); gfx->print(b); } break;
      case 3: { char b[48]; snprintf(b,sizeof(b),"Start-Lubric.: %.1f s", g_startLube_s); gfx->print(b); } break;
    }
    y += LH;
  }
  
  // Status info
  y += 10;
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  printClippedText("== Hardware ==");
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  extern uint32_t punchCount;
  uint32_t nextLubeAt = ((punchCount / g_targetStrokes) + 1) * g_targetStrokes;
  printfClipped("Push: %u (Next: %u)", punchCount, nextLubeAt);
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  printfClipped("Lube: %s", lubePumpStatus ? "AAN" : "UIT");
  y += LH;
  
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  gfx->print("Auto lube activeert bij");
  y += LH;
  gfx->setCursor(R_WIN_X+20, y);
  printfClipped("elke %u pushes.", (unsigned)g_targetStrokes);
  y += LH;
  
  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
  printClippedText("Y:sel Z:edit C:menu");
  
  // Toon edit popup als we in edit mode zijn
  drawSmeringEditPopup();
}

static void drawMotionPage(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  setMenuTitleAndItems();
  setMenuFontItem();
  
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  // Menu items
  for (int i=0; i<5; i++){
    bool sel = (uiMode==MODE_MENU && i==menuIdx);
    uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
    gfx->setTextColor(col, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);
    
    switch(i){
      case 0: gfx->print("< Terug"); break;
      case 1: { char b[64]; snprintf(b,sizeof(b),"Blend: %s", CFG.motionBlendEnabled ? "Aan" : "Uit"); gfx->print(b); } break;
      case 2: { char b[64]; snprintf(b,sizeof(b),"Nunchuk: %.0f%%", CFG.userSpeedWeight); gfx->print(b); } break;
      case 3: { char b[64]; snprintf(b,sizeof(b),"Motion Sensor: %.0f%%", CFG.motionSpeedWeight); gfx->print(b); } break;
      case 4: { char b[64]; snprintf(b,sizeof(b),"Direction Sync: %s", CFG.motionDirectionSync ? "Aan" : "Uit"); gfx->print(b); } break;
    }
    y += LH;
  }
  
  // Motion status info
  y += 10;
  gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, y);
  printClippedText("== Motion ==");
  y += LH;
  
  gfx->setCursor(R_WIN_X+20, y);
  uint16_t atomColor = m5atom_connected ? 0x07E0 : 0xF800;
  gfx->setTextColor(atomColor, CFG.COL_BG);
  gfx->printf("M5 Atom: %s", m5atom_connected ? "ONLINE" : "OFFLINE");
  y += LH;
  
  if (m5atom_connected) {
    gfx->setTextColor(0x8410, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, y);
    const char* dirStr = (currentMotionDir > 0) ? "UP" : (currentMotionDir < 0) ? "DOWN" : "STILL";
    printfClipped("Motion: %s (%d%% spd)", dirStr, currentMotionSpeed);
    y += LH;
    
    gfx->setCursor(R_WIN_X+20, y);
    extern uint8_t g_speedStep;
    printfClipped("Anim: %d/%d (%.0f%%+%.0f%%)", g_speedStep, CFG.SPEED_STEPS-1,
                CFG.userSpeedWeight, CFG.motionSpeedWeight);
  } else {
    gfx->setTextColor(0xFFE0, CFG.COL_BG); // Geel voor waarschuwing
    gfx->setCursor(R_WIN_X+20, y);
    printClippedText("Motion blend uit");
  }
  
  gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
  gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
  printClippedText("Y:sel Z:edit C:menu");
  
  // Toon edit popup als we in edit mode zijn
  drawMotionEditPopup();
}

static void drawRightMenu(){
  gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, CFG.COL_BG);
  gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, CFG.COL_FRAME2);
  gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, CFG.COL_FRAME);

  if (currentPage == PAGE_COLORS){ drawColorsPage(); return; }
  if (currentPage == PAGE_VACUUM){ drawVacuumPage(); return; }
  if (currentPage == PAGE_MOTION){ drawMotionPage(); return; }
  if (currentPage == PAGE_ESPNOW){ drawESPNowPage(); return; }
  if (currentPage == PAGE_AUTO_VACUUM){ drawAutoVacuumPage(); return; }
  if (currentPage == PAGE_SMERING){ drawSmeringPage(); return; }

  setMenuTitleAndItems();
  int y = R_WIN_Y + 60;
  const int LH = 20;
  const int DOT_X = R_WIN_X + R_WIN_W - 18;

  if (currentPage == PAGE_MAIN){
    for (int i=0;i<8;i++){
      bool sel = (uiMode==MODE_MENU && i==menuIdx);
      uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
      gfx->setTextColor(col, CFG.COL_BG);
      gfx->setCursor(R_WIN_X+20, y);

      switch(i){
        case 0: gfx->print("Keon"); drawStatusDot(DOT_X, y-8, keonConnected ? CFG.DOT_GREEN : CFG.DOT_RED, 4); break;
        case 1: gfx->print("Solace"); drawStatusDot(DOT_X, y-8, solaceConnected ? CFG.DOT_GREEN : CFG.DOT_RED, 4); break;
        case 2: gfx->print("Motion"); drawStatusDot(DOT_X, y-8, motionConnected ? CFG.DOT_GREEN : CFG.DOT_RED, 4); break;
        case 3: gfx->print("ESP Status"); drawStatusDot(DOT_X, y-8, (pumpUnit_connected && bodyESP_connected && motionConnected) ? CFG.DOT_GREEN : CFG.DOT_RED, 4); break;
        case 4: gfx->print("Smering"); break;
        case 5: gfx->print("Zuigen"); break;
        case 6: gfx->print("Auto Vacuum"); break;
        case 7: gfx->print("Instellingen"); break;
      }
      y += LH;
    }
    gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
    printClippedText("Y:sel Z:edit C:menu");
    drawEditPopupInMenu();
    drawConnectionPopup();
    drawNunchukHelpPopup();
  } else {
    for (int i=0;i<5;i++){
      bool sel = (uiMode==MODE_MENU && i==menuIdx);
      uint16_t col = sel ? 0x07E0 : CFG.COL_MENU_PINK;
      gfx->setTextColor(col, CFG.COL_BG);
      gfx->setCursor(R_WIN_X+20, y);
      if (i==0) gfx->print("< Terug");
      if (i==1) gfx->print("Motion Blend");
      if (i==2) gfx->print("Kleuren");
      if (i==3) gfx->print("Nunchuk Knoppen");
      if (i==4) gfx->print("Reset naar standaard");
      y += LH;
    }
    gfx->setFont(nullptr); gfx->setTextSize(1); gfx->setTextColor(0x8410, CFG.COL_BG);
    gfx->setCursor(R_WIN_X+20, R_WIN_Y + R_WIN_H - 10);
    printClippedText("Y:sel Z:enter C:menu");
    drawNunchukHelpPopup();
  }
}

// ---------------- Menu besturing ----------------
static void handleMenuY_colors(int jy){
  static uint32_t lastNav=0;
  const uint16_t NAV_MS=120;
  uint32_t now=millis();
  if (now - lastNav < NAV_MS) return;

  if (!paletteOpen){
    if (jy > JY_HI && menuIdx > 0) { menuIdx--; lastNav=now; drawRightMenu(); }
    else if (jy < JY_LO && menuIdx < colorsTotalItems()-1) { menuIdx++; lastNav=now; drawRightMenu(); }
    ensureColorsScrollVisible();
  } else {
    bool moved=false;
    if (jy > JY_HI && palY > 0)               { palY--; moved=true; }
    else if (jy < JY_LO && palY < PAL_ROWS-1) { palY++; moved=true; }
    if (moved){ lastNav=now; }
  }
}
static void handlePaletteX(int jx){
  static uint32_t lastNavX=0;
  const uint16_t NAV_MS=120;
  uint32_t now=millis();
  if (now - lastNavX < NAV_MS) return;

  bool moved=false;
  if (jx < JX_LO && palX > 0)              { palX--; moved=true; }
  else if (jx > JX_HI && palX < PAL_COLS-1){ palX++; moved=true; }

  if (moved){
    lastNavX = now;
    float h = (float)palX / (float)(PAL_COLS-1);
    float v = 0.35f + 0.65f * (float)(PAL_ROWS-1 - palY) / (float)(PAL_ROWS-1);
    uint8_t rr,gg,bb; hsv2rgb_u8(h,1.0f,v, rr,gg,bb);
    tempR=rr; tempG=gg; tempB=bb;
    applyTempToTarget_globalIndex(editingGlobalIdx);
    drawRightMenu();
  }
}
static void handleMenuY(int jy){
  if (uiMode != MODE_MENU) return;
  if (currentPage == PAGE_COLORS){ handleMenuY_colors(jy); return; }

  static uint32_t lastNav=0;
  const uint16_t NAV_MS=140;
  uint32_t now=millis();
  if (now - lastNav < NAV_MS) return;

  const int JY_LO_M=70, JY_HI_M=180;
  int maxIdx = (currentPage==PAGE_MAIN ? 7 : (currentPage==PAGE_SETTINGS ? 5 : (currentPage==PAGE_VACUUM ? 1 : (currentPage==PAGE_MOTION ? 4 : (currentPage==PAGE_ESPNOW ? 0 : (currentPage==PAGE_AUTO_VACUUM ? 2 : (currentPage==PAGE_SMERING ? 3 : 2)))))));

  if (!menuEdit) {
    if (jy > JY_HI_M && menuIdx > 0) { menuIdx--; drawRightMenu(); lastNav=now; return; }
    else if (jy < JY_LO_M && menuIdx < maxIdx) { menuIdx++; drawRightMenu(); lastNav=now; return; }
  } else if (currentPage == PAGE_SMERING) {
    bool changed=false;
    if (menuIdx == 1) {
      // Pushes/Lube target instelling
      bool strokesChanged = false;
      if (jy > JY_HI_M && g_targetStrokes < 999) { g_targetStrokes++; changed=true; strokesChanged=true; }
      if (jy < JY_LO_M && g_targetStrokes > 1)   { g_targetStrokes--; changed=true; strokesChanged=true; }
      
      if (strokesChanged) saveLubeSettings();
      
      // Reset punch counter met lange Y joystick press naar beneden
      static uint32_t longPressStart = 0;
      if (jy < JY_LO_M - 50) {  // Extra ver naar beneden
        if (longPressStart == 0) longPressStart = now;
        else if (now - longPressStart > 3000) {  // 3 seconden
          extern uint32_t punchCount;
          punchCount = 0;
          Serial.println("[RESET] Punch counter reset to 0");
          // Note: punch counter is not saved - only settings are persistent
          changed = true;
          longPressStart = 0;
        }
      } else {
        longPressStart = 0;
      }
    } else if (menuIdx == 2) {
      static float prevLubeHold = g_lubeHold_s;
      bool lubeChanged = false;
      if (jy > JY_HI_M) { g_lubeHold_s += 0.5f; if (g_lubeHold_s > 20.0f) g_lubeHold_s=20.0f; changed=true; lubeChanged=true; }
      if (jy < JY_LO_M) { g_lubeHold_s -= 0.5f; if (g_lubeHold_s < 0.5f) g_lubeHold_s=0.5f; changed=true; lubeChanged=true; }
      
      if (lubeChanged) saveLubeSettings();
      
      // Als Lubrication tijd verhoogd wordt, trigger normale lube shot
      if (changed && g_lubeHold_s > prevLubeHold) {
        extern void triggerLubeShot(float seconds); // Forward declaration  
        triggerLubeShot(g_lubeHold_s);
        Serial.printf("[UI] Lube shot geactiveerd voor %.1fs\n", g_lubeHold_s);
      }
      prevLubeHold = g_lubeHold_s;
    } else if (menuIdx == 3) {
      static float prevStartLube = g_startLube_s;
      bool startLubeChanged = false;
      if (jy > JY_HI_M) { g_startLube_s += 0.5f; if (g_startLube_s > 20.0f) g_startLube_s=20.0f; changed=true; startLubeChanged=true; }
      if (jy < JY_LO_M) { g_startLube_s -= 0.5f; if (g_startLube_s < 0.5f) g_startLube_s=0.5f; changed=true; startLubeChanged=true; }
      
      if (startLubeChanged) saveLubeSettings();
      
      // Als Start-Lubric tijd verhoogd wordt, trigger lube pump
      if (changed && g_startLube_s > prevStartLube) {
        extern void triggerStartLube(float seconds); // Forward declaration  
        triggerStartLube(g_startLube_s);
        Serial.printf("[UI] Start-Lubric geactiveerd voor %.1fs\n", g_startLube_s);
      }
      prevStartLube = g_startLube_s;
    }
    if (changed) {
      drawSmeringEditPopup(); 
      saveAllSettings();
    }
    lastNav = now;
  } else if (currentPage == PAGE_VACUUM) {
    bool changed=false;
    if (menuIdx == 1) {
      // Vacuum Level (1 tot 1000, stap 1 - intern opgeslagen als negatieve mbar)
      float vacLevel = abs(CFG.vacuumTargetMbar);  // Converteer naar positieve waarde voor display
      if (jy > JY_HI_M && vacLevel < 1000.0f) { vacLevel += 1.0f; changed=true; }
      if (jy < JY_LO_M && vacLevel > 1.0f) { vacLevel -= 1.0f; changed=true; }
      if (changed) CFG.vacuumTargetMbar = -vacLevel;  // Opslaan als negatieve waarde
    }
    
    if (changed) { 
      drawVacuumEditPopup(); 
      Serial.printf("[UI] Vacuum level changed to: %.1f mbar, saving...\n", CFG.vacuumTargetMbar);
      saveAllSettings(); // Save vacuum settings to flash
    }
    lastNav = now;
  } else if (currentPage == PAGE_MOTION) {
    bool changed=false;
    if (menuIdx == 1) {
      // Motion Blend toggle
      if (jy > JY_HI_M || jy < JY_LO_M) { CFG.motionBlendEnabled = !CFG.motionBlendEnabled; changed=true; }
    } else if (menuIdx == 2) {
      // Nunchuk Weight (0 tot 100%, stap 5%)
      if (jy > JY_HI_M) { CFG.userSpeedWeight += 5.0f; if (CFG.userSpeedWeight > 100.0f) CFG.userSpeedWeight=100.0f; changed=true; }
      if (jy < JY_LO_M) { CFG.userSpeedWeight -= 5.0f; if (CFG.userSpeedWeight < 0.0f) CFG.userSpeedWeight=0.0f; changed=true; }
    } else if (menuIdx == 3) {
      // Motion Sensor Weight (0 tot 100%, stap 5%)
      if (jy > JY_HI_M) { CFG.motionSpeedWeight += 5.0f; if (CFG.motionSpeedWeight > 100.0f) CFG.motionSpeedWeight=100.0f; changed=true; }
      if (jy < JY_LO_M) { CFG.motionSpeedWeight -= 5.0f; if (CFG.motionSpeedWeight < 0.0f) CFG.motionSpeedWeight=0.0f; changed=true; }
    } else if (menuIdx == 4) {
      // Direction Sync toggle
      if (jy > JY_HI_M || jy < JY_LO_M) { CFG.motionDirectionSync = !CFG.motionDirectionSync; changed=true; }
    }
    
    if (changed) { 
      drawMotionEditPopup();
      saveAllSettings(); // Save motion blend settings to flash
    }
    lastNav = now;
  } else if (currentPage == PAGE_AUTO_VACUUM) {
    bool changed=false;
    if (menuIdx == 1) {
      // Auto Vacuum toggle
      if (jy > JY_HI_M || jy < JY_LO_M) { 
        CFG.vacuumAutoMode = !CFG.vacuumAutoMode; 
        changed=true; 
        Serial.printf("[VACUUM] Auto Vacuum %s\n", CFG.vacuumAutoMode ? "INGESCHAKELD" : "UITGESCHAKELD");
      }
    } else if (menuIdx == 2) {
      // Speed Threshold (1 tot CFG.SPEED_STEPS-1)
      if (jy > JY_HI_M && CFG.autoVacuumSpeedThreshold < (CFG.SPEED_STEPS-1)) { 
        CFG.autoVacuumSpeedThreshold++; changed=true; 
      }
      if (jy < JY_LO_M && CFG.autoVacuumSpeedThreshold > 1) { 
        CFG.autoVacuumSpeedThreshold--; changed=true; 
      }
      if (changed) {
        Serial.printf("[AUTO-VACUUM] Speed threshold set to %d (>= this speed disables auto vacuum)\n", CFG.autoVacuumSpeedThreshold);
      }
    }
    
    if (changed) { 
      drawAutoVacuumEditPopup();
      saveAllSettings(); // Save auto vacuum settings to flash
    }
    lastNav = now;
  }
}

// ---------------- Nunchuk speed-steps ----------------
uint8_t g_speedStep = 0;  // Removed static for ESP-NOW access
static bool    upArmed = true, downArmed = true;
static const int JY_MID_LOW  = 110;
static const int JY_MID_HIGH = 146;

static void updateSpeedStepWithJoystick(int jy){
  if (uiMode != MODE_ANIM) return;
  if (jy > JY_HI && upArmed) { if (g_speedStep + 1 < CFG.SPEED_STEPS) g_speedStep++; upArmed=false; downArmed=true; }
  else if (jy < JY_LO && downArmed) { if (g_speedStep > 0) g_speedStep--; downArmed=false; upArmed=true; }
  else if (jy >= JY_MID_LOW && jy <= JY_MID_HIGH) { upArmed = downArmed = true; }
}

// ---------------- Animatie status ----------------
static const float TAU = 6.2831853f;
static uint32_t lastUs = 0;
float    phase=0.0f;
float    velEMA=0.0f;  // Made global for crispy unpause reset
static int      prevCapY = INT_MIN;

// ---------------- Setup/Loop wrappers ----------------
void uiInit() {
  Serial.begin(115200); delay(30);
  
  // Test flash storage first (can be disabled if it causes issues)
  #define ENABLE_FLASH_TEST 0
  #if ENABLE_FLASH_TEST
  testFlashStorage();
  #else
  Serial.println("[INIT] Flash test skipped - proceeding with settings load");
  #endif
  
  // Load persistent settings
  Serial.println("[INIT] About to call loadAllSettings()...");
  loadAllSettings();
  Serial.printf("[INIT] After loadAllSettings(), vacuumTargetMbar = %.1f\n", CFG.vacuumTargetMbar);

  gfx->begin();
  gfx->fillScreen(CFG.COL_BG);
// Backlight aan (LCD_BL is een const int, dus geen #ifdef gebruiken)
if (LCD_BL >= 0) {
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
}

  // // cv->begin() causes RGB panel slot conflict - commented for Jingcai RGB
  // cv->begin();
  cv->fillScreen(CFG.COL_BG); cv->flush();

  uiMode  = MODE_MENU;
  paused  = true;
  parkToBottom = false;
  currentPage = PAGE_MAIN;
  menuIdx = 0; colorScroll=0; paletteOpen=false; colorEdit=false;

  Wire.begin(21, 22); delay(10);
  nchuk.begin();
  for (int i=0;i<10 && !nkReady;i++){ nkReady = nchuk.connect(); if (!nkReady) delay(50); }
  
  // Initialize vacuum system
  vacuumInit();

  drawLeftFrame();
  drawRightMenu();
  phase = -1.5707963f;

  int CAP_Y_IN, CAP_Y_OUT, DRAW_BASELINE_Y;
  // zelfde berekening als monolith:
  const int BL = L_CANVAS_H - 4;
  const int MIN_ROD_VIS_IN = max(2, (int)round(12*1.25f) - 4);
  const int CAP_Y_OUT_BASE = 2;
  const int SL_CAP_H = (int)round(32*1.25f);
  const int CAP_Y_IN_BASE  = (BL - MIN_ROD_VIS_IN) - SL_CAP_H;
  const int CAP_Y_MID      = (CAP_Y_IN_BASE + CAP_Y_OUT_BASE) / 2;
  const float RANGE_SCALE  = 0.90f;
  CAP_Y_IN  = (int)round(CAP_Y_MID + (CAP_Y_IN_BASE  - CAP_Y_MID) * RANGE_SCALE);
  CAP_Y_OUT = (int)round(CAP_Y_MID + (CAP_Y_OUT_BASE - CAP_Y_MID) * RANGE_SCALE);
  DRAW_BASELINE_Y = (int)round(CAP_Y_MID + (BL - CAP_Y_MID) * RANGE_SCALE);

  capY_draw = (float)CAP_Y_IN;
  prevCapY  = INT_MIN;

  uint16_t rodCol = lerp_rgb565_u8(CFG.rodSlowR,CFG.rodSlowG,CFG.rodSlowB,
                                   CFG.rodFastR,CFG.rodFastG,CFG.rodFastB, 0.0f);
  cv->fillScreen(CFG.COL_BG);
  drawSleeveFixedTop((int)capY_draw, CFG.COL_TAN);
  drawRodFromCap_Vinside_NoSeam((int)capY_draw, DRAW_BASELINE_Y, rodCol, 0xE946);
  cv->flush();

  drawSpeedBarTop(g_speedStep, CFG.SPEED_STEPS);
  drawVacArrowHeader(false);
  lastUs = micros();
  g_speedStep = 0; upArmed = downArmed = true;
}

void uiTick() {
  bool cNow=false, zNow=false;
  int jy=128, jx=128;

  if (nkReady) {
    bool ok = nchuk.update();
    if (!ok) nkReady = nchuk.connect();
    jx = nchuk.joyX(); jy = nchuk.joyY();
    cNow = nchuk.buttonC(); zNow = nchuk.buttonZ();
    
    // Debug nunchuk button status (alleen als er een knop ingedrukt is)
    static bool lastZNow = false;
    if (zNow != lastZNow) {
      Serial.printf("[NUNCHUK] Z button: %s\n", zNow ? "PRESSED" : "RELEASED");
      lastZNow = zNow;
    }
  } else {
    static uint32_t lastTry=0;
    if (millis()-lastTry>1000){ 
      lastTry=millis(); 
      nkReady=nchuk.connect(); 
      Serial.printf("[NUNCHUK] Connection attempt: %s\n", nkReady ? "SUCCESS" : "FAILED");
    }
  }

  CEvent cev = pollCEvent(cNow);
  ZEvent zev = pollZEvent(zNow);  // Keep for legacy menu navigation
  ZClickType zClick = pollZClick(zNow);  // New click detection
  
  // Debug Z-click events
  if (zClick != Z_NONE) {
    Serial.printf("[Z-VIBE] Click type: %s\n", 
                  (zClick == Z_SINGLE) ? "SINGLE" : (zClick == Z_VIBE) ? "VIBE" : "NONE");
  }
  
  // Vibe toggle op dubbele klik
  if (zClick == Z_VIBE) {
    vibeState = !vibeState;
    Serial.printf("[Z-VIBE] Vibe toggle: %s -> sending bit 3 to M5StickC Plus\n", 
                  vibeState ? "ON" : "OFF");
    // De vibeState wordt automatisch verzonden via de reguliere ESP-NOW updates in sendM5AtomPumpColors()
  }
  
  bool zEdge = (zev == ZE_SHORT);  // Legacy compatibility voor menu navigatie

  // Handle popups FIRST before any other C button logic
  // Handle Nunchuk Help popup first
  if (nunchukHelpPopupOpen) {
    if (cev == CE_SHORT) { // C button - close popup
      nunchukHelpPopupOpen = false;
      drawRightMenu();
      return; // Stop processing other C button actions
    }
    return; // Don't process other menu logic while popup is open
  }
  
  if (paletteOpen && cev==CE_SHORT){
    cancelColorEdit_globalIndex(editingGlobalIdx);
    paletteOpen=false; colorEdit=false; editingGlobalIdx=-1;
    drawRightMenu();
  } else if (cev==CE_LONG){
    uiMode = (uiMode==MODE_MENU)? MODE_ANIM : MODE_MENU;
    menuEdit = false; colorEdit=false; paletteOpen=false;
    drawRightMenu();
  } else if (cev==CE_SHORT && !paletteOpen){
    if (!paused) {
      parkToBottom = true;
      // Park Keon at bottom when pausing
      extern void keonParkToBottom();
      keonParkToBottom();
    } else { 
      paused = false; parkToBottom = false; 
      // CRISPY FIX: Reset velEMA voor onmiddellijke auto vacuum response na unpause
      velEMA = 0.0f;
      Serial.println("[UNPAUSE] velEMA reset for crispy auto vacuum response");
    }
  }

  // Check connection progress if popup is open
  if (connectionPopupOpen && connectionInProgress) {
    if (connectionDeviceIdx == 0) {
      // Check Keon connection progress
      bool result = checkKeonConnectionProgress();
      if (!connectionInProgress) { // Connection attempt finished
        keonConnected = result;
        connectionPopupOpen = false;
        connectionDeviceIdx = -1;
        connectionChoiceIdx = 0;
        drawRightMenu();
        return;
      }
    } else if (connectionDeviceIdx == 1) {
      // Check Solace connection progress
      bool result = checkSolaceConnectionProgress();
      if (!connectionInProgress) { // Connection attempt finished
        solaceConnected = result;
        connectionPopupOpen = false;
        connectionDeviceIdx = -1;
        connectionChoiceIdx = 0;
        drawRightMenu();
        return;
      }
    }
    // Redraw popup to show progress
    drawRightMenu();
  }
  
  
  // Handle connection popup
  if (connectionPopupOpen) {
    if (zEdge) {
      if (connectionChoiceIdx == 0) { // "Ja" selected
        if (connectionDeviceIdx == 0) {
          if (keonConnected) {
            disconnectKeon();
          } else {
            // Start non-blocking connection attempt
            startKeonConnection();
            // Don't close popup yet - wait for connection result
            return;
          }
        } else if (connectionDeviceIdx == 1) {
          if (solaceConnected) {
            disconnectSolace();
          } else {
            // Start non-blocking connection attempt
            startSolaceConnection();
            // Don't close popup yet - wait for connection result
            return;
          }
        }
      }
      connectionPopupOpen = false;
      connectionDeviceIdx = -1;
      connectionChoiceIdx = 0;
      drawRightMenu();
    } else if (cev == CE_SHORT) { // C button - cancel
      connectionPopupOpen = false;
      connectionDeviceIdx = -1;
      connectionChoiceIdx = 0;
      drawRightMenu();
    } else {
      // Handle JX input for popup navigation
      static uint32_t lastPopupNav = 0;
      const uint16_t POPUP_NAV_MS = 200;
      uint32_t now = millis();
      if (now - lastPopupNav >= POPUP_NAV_MS) {
        if (jx < JX_LO && connectionChoiceIdx == 1) {
          connectionChoiceIdx = 0; // Move to "Ja"
          lastPopupNav = now;
          drawRightMenu();
        } else if (jx > JX_HI && connectionChoiceIdx == 0) {
          connectionChoiceIdx = 1; // Move to "Nee"
          lastPopupNav = now;
          drawRightMenu();
        }
      }
    }
    return; // Don't process other menu logic while popup is open
  }

  if (uiMode==MODE_MENU) {
    if (zEdge) {
      if (currentPage == PAGE_COLORS){
        if (!paletteOpen) {
          if (menuIdx == 0){ currentPage = PAGE_SETTINGS; menuIdx = 1; colorScroll=0; drawRightMenu(); }
          else if (menuIdx == 1){ resetColorsToDefault(); drawRightMenu(); }
          else { beginColorEdit_globalIndex(menuIdx); drawRightMenu(); }
        } else {
          commitColorEdit_globalIndex(editingGlobalIdx);
          paletteOpen=false; colorEdit=false; editingGlobalIdx=-1;
          drawRightMenu();
        }
      } else if (!menuEdit) {
        if (currentPage == PAGE_MAIN) {
          if (menuIdx == 0) { // Keon
            connectionPopupOpen = true;
            connectionDeviceIdx = 0;
            connectionChoiceIdx = 0;
            drawRightMenu();
          } else if (menuIdx == 1) { // Solace
            connectionPopupOpen = true;
            connectionDeviceIdx = 1;
            connectionChoiceIdx = 0;
            drawRightMenu();
          } else if (menuIdx == 3) { currentPage = PAGE_ESPNOW; menuIdx = 0; drawRightMenu(); }  // ESP Status
          else if (menuIdx == 4) { currentPage = PAGE_SMERING; menuIdx = 0; drawRightMenu(); }  // Smering
          else if (menuIdx == 5) { currentPage = PAGE_VACUUM; menuIdx = 0; drawRightMenu(); }  // Zuigen
          else if (menuIdx == 6) { currentPage = PAGE_AUTO_VACUUM; menuIdx = 0; drawRightMenu(); }  // Auto Vacuum
          else if (menuIdx == 7) { currentPage = PAGE_SETTINGS; menuIdx = 1; drawRightMenu(); }
        } else if (currentPage == PAGE_SETTINGS) {
          if (menuIdx == 0) { currentPage = PAGE_MAIN; menuIdx = 7; drawRightMenu(); }
          else if (menuIdx == 1) { currentPage = PAGE_MOTION; menuIdx = 0; drawRightMenu(); }  // Motion Blend
          else if (menuIdx == 2) { currentPage = PAGE_COLORS; menuIdx = 0; colorEdit=false; colorScroll=0; drawRightMenu(); }  // Kleuren
          else if (menuIdx == 3) {
            // Open Nunchuk Help popup
            nunchukHelpPopupOpen = true;
            drawRightMenu();
          }
          else if (menuIdx == 4) {
            // Reset alle instellingen naar standaard
            Serial.println("[SETTINGS] Resetting all settings to default values...");
            resetAllSettingsToDefault();
            Serial.println("[SETTINGS] All settings have been reset to default");
            drawRightMenu();
          }
        } else if (currentPage == PAGE_SMERING) {
          if (menuIdx == 0) { currentPage = PAGE_MAIN; menuIdx = 4; drawRightMenu(); }  // Terug naar main menu Smering item
          else if (menuIdx == 1 || menuIdx == 2 || menuIdx == 3) { menuEdit = true; drawRightMenu(); }
        } else if (currentPage == PAGE_VACUUM) {
          if (menuIdx == 0) { currentPage = PAGE_MAIN; menuIdx = 5; drawRightMenu(); }  // Terug naar main menu Zuigen item
          else if (menuIdx == 1) { menuEdit = true; drawRightMenu(); }
        } else if (currentPage == PAGE_MOTION) {
          if (menuIdx == 0) { currentPage = PAGE_SETTINGS; menuIdx = 2; drawRightMenu(); }
          else if (menuIdx == 1 || menuIdx == 2 || menuIdx == 3 || menuIdx == 4) { menuEdit = true; drawRightMenu(); }
        } else if (currentPage == PAGE_ESPNOW) {
          if (menuIdx == 0) { currentPage = PAGE_MAIN; menuIdx = 3; drawRightMenu(); }  // Terug naar main menu ESP Status item
        } else if (currentPage == PAGE_AUTO_VACUUM) {
          if (menuIdx == 0) { currentPage = PAGE_MAIN; menuIdx = 6; drawRightMenu(); }  // Terug naar main menu Auto Vacuum item
          else if (menuIdx == 1 || menuIdx == 2) { menuEdit = true; drawRightMenu(); }
        }
      } else {
        // Verlaten van menu edit mode met Z-knop
        if (currentPage == PAGE_SMERING && menuIdx == 2) {
          // Normale Lube: trigger lube shot bij Z-knop (verlaten edit)
          extern void triggerLubeShot(float seconds);
          triggerLubeShot(g_lubeHold_s);
          Serial.printf("[UI] Lube shot Z-knop: Lube geactiveerd voor %.1fs\n", g_lubeHold_s);
        } else if (currentPage == PAGE_SMERING && menuIdx == 3) {
          // Start-Lubric: trigger lube bij Z-knop (verlaten edit)
          extern void triggerStartLube(float seconds);
          triggerStartLube(g_startLube_s);
          Serial.printf("[UI] Start-Lubric Z-knop: Lube geactiveerd voor %.1fs\n", g_startLube_s);
        } else if (currentPage == PAGE_VACUUM) {
          // Save vacuum settings when exiting edit mode
          Serial.printf("[UI] Exiting vacuum edit mode, vacuumTargetMbar: %.1f mbar, saving...\n", CFG.vacuumTargetMbar);
          saveAllSettings();
          Serial.println("[UI] Vacuum settings saved on edit exit");
        } else if (currentPage == PAGE_AUTO_VACUUM) {
          // Save auto vacuum settings when exiting edit mode
          Serial.printf("[UI] Exiting auto vacuum edit mode, autoVacuumSpeedThreshold: %d, saving...\n", CFG.autoVacuumSpeedThreshold);
          saveAllSettings();
          Serial.println("[UI] Auto vacuum settings saved on edit exit");
        } else if (currentPage == PAGE_SMERING) {
          // Save smering settings when exiting edit mode
          Serial.println("[UI] Exiting smering edit mode, saving...");
          saveAllSettings();
          Serial.println("[UI] Smering settings saved on edit exit");
        }
        menuEdit = false; drawRightMenu();
      }
    }

    if (currentPage == PAGE_COLORS){
      handleMenuY_colors(jy);
      if (paletteOpen) {
        handlePaletteX(jx);
        float h = (float)palX / (float)(PAL_COLS-1);
        float v = 0.35f + 0.65f * (float)(PAL_ROWS-1 - palY) / (float)(PAL_ROWS-1);
        uint8_t rr,gg,bb; hsv2rgb_u8(h,1.0f,v, rr,gg,bb);
        if (rr!=tempR || gg!=tempG || bb!=tempB){
          tempR=rr; tempG=gg; tempB=bb;
          applyTempToTarget_globalIndex(editingGlobalIdx);
          drawRightMenu();
        }
      }
    } else {
      handleMenuY(jy);
    }
  }

  // ================= ANIMATIE EERST =================
  // We need to calculate animation first to get updated phase for auto vacuum
  updateSpeedStepWithJoystick(jy);
  drawSpeedBarTop(g_speedStep, CFG.SPEED_STEPS);

  uint32_t nowUs = micros();
  float dt = (nowUs - lastUs) / 1e6f;
  lastUs = nowUs;

  // Calculate step01 for animation speed and rod color
  float step01 = (CFG.SPEED_STEPS<=1)?0.0f : (float)g_speedStep/(float)(CFG.SPEED_STEPS-1);
  float instF = CFG.MIN_SPEED_HZ + step01 * (CFG.MAX_SPEED_HZ - CFG.MIN_SPEED_HZ);

  if (!paused && !parkToBottom) {
    phase += TAU * instF * dt * CFG.ANIM_SPEED_FACTOR;
    if (phase > TAU) phase -= TAU;
  }
  
  // ================= AUTO VACUUM LOGIC (after animation) =================
  // BACK TO SIMPLE velEMA method - this actually worked before!
  // We need to calculate velEMA first from the actual animation movement
  
  // Animation rendering (moved from later in code)
  float s = 0.5f * (sinf(phase) + 1.0f);
  float ease = schlick_gain(CFG.easeGain, s);
  
  // Calculate actual animation position
  const int BL = L_CANVAS_H - 4;
  const int MIN_ROD_VIS_IN = max(2, (int)round(12*1.25f) - 4);
  const int CAP_Y_OUT_BASE = 2;
  const int SL_CAP_H = (int)round(32*1.25f);
  const int CAP_Y_IN_BASE  = (BL - MIN_ROD_VIS_IN) - SL_CAP_H;
  const int CAP_Y_MID      = (CAP_Y_IN_BASE + CAP_Y_OUT_BASE) / 2;
  const float RANGE_SCALE  = 0.90f;
  int CAP_Y_IN  = (int)round(CAP_Y_MID + (CAP_Y_IN_BASE  - CAP_Y_MID) * RANGE_SCALE);
  int CAP_Y_OUT = (int)round(CAP_Y_MID + (CAP_Y_OUT_BASE - CAP_Y_MID) * RANGE_SCALE);
  
  int capY_phase = (int)round((float)CAP_Y_IN + ease*(float)(CAP_Y_OUT - CAP_Y_IN));
  int capY = parkToBottom ? (int)round(capY_draw) : (!paused ? capY_phase : (int)round(capY_draw));
  
  // Calculate velocity from actual animation
  extern int prevCapY;
  extern float velEMA;
  float vel = 0.0f; 
  if (prevCapY != INT_MIN) vel = (float)(capY - prevCapY);
  prevCapY = capY;
  velEMA = (1.0f - CFG.velEMAalpha)*velEMA + CFG.velEMAalpha*vel;
  
  // SIMPLE: Use velocity for direction (like it worked before)
  bool goingUp = (velEMA < 0.0f);
  
  
  // DEBUG: Track goingUp state changes and system state
  static bool prevGoingUp = false;
  static uint32_t lastDebugTime = 0;
  static uint32_t lastStatusTime = 0;
  
  // Show status every 10 seconds (reduced spam)
  if (millis() - lastStatusTime > 10000) {
    const char* modeStr = (uiMode == MODE_ANIM) ? "ANIM" : "MENU";
    const char* stateStr = paused ? "PAUSED" : "RUNNING";
    extern uint32_t punchCount;
    Serial.printf("[STATUS] %s/%s, pushCount=%u, speedStep=%d\n", 
                  modeStr, stateStr, punchCount, g_speedStep);
    lastStatusTime = millis();
  }
  
  // Removed frequent debug to reduce spam
  prevGoingUp = goingUp;
  
  // PUSH COUNTING - Faster edge detection for crispy response
  static bool prevArrowFull = false;
  static uint32_t lastEdgeTime = 0;
  const uint32_t EDGE_DEBOUNCE_MS = 100;  // Faster response (was 200ms)
  
  bool arrowFullEdge = (goingUp && !prevArrowFull);  // Rising edge: empty -> full
  uint32_t now = millis();
  
  // Count pushes alleen wanneer niet gepauzeerd
  if (!paused && arrowFullEdge && (now - lastEdgeTime > EDGE_DEBOUNCE_MS)) {
    extern uint32_t punchCount;
    punchCount++;
    lastEdgeTime = now;
    
    // Check if we need to trigger automatic lube
    if (punchCount > 0 && (punchCount % g_targetStrokes) == 0) {
      extern void triggerLubeShot(float seconds);
      triggerLubeShot(g_lubeHold_s);
      Serial.printf("[AUTO-LUBE] Push #%u reached target %u - Lube shot for %.1fs\n", 
                    punchCount, (unsigned)g_targetStrokes, g_lubeHold_s);
    }
    
    Serial.printf("[PUSH] Count: %u (Next lube at: %u), velEMA=%.3f\n", 
                  punchCount, ((punchCount / g_targetStrokes) + 1) * g_targetStrokes, velEMA);
    
    // Update menu display if we're in menu mode (to show updated count)
    if (uiMode == MODE_MENU && (currentPage == PAGE_MAIN || currentPage == PAGE_SMERING)) {
      drawRightMenu();
    }
  }
  
  // Auto vacuum (volgt animatie) alleen wanneer niet gepauzeerd
  static bool lastArrowFull = false;
  
  if (!paused) {
    // Simple auto vacuum: based on velocity direction
    bool currentArrowState = goingUp;
    
    // Only update actual vacuum control if auto vacuum is enabled
    extern bool pompUnitZuigActive;
    bool speedDisablesAutoVac = (g_speedStep >= CFG.autoVacuumSpeedThreshold);
    bool autoVacAllowed = CFG.vacuumAutoMode && !pompUnitZuigActive && !speedDisablesAutoVac;
    
    // Debug speed threshold state changes
    static bool prevSpeedDisabled = false;
    if (speedDisablesAutoVac != prevSpeedDisabled) {
      Serial.printf("[AUTO-VAC] Speed threshold %s: speed=%d, threshold=%d\n", 
                    speedDisablesAutoVac ? "TRIGGERED" : "CLEARED", g_speedStep, CFG.autoVacuumSpeedThreshold);
      prevSpeedDisabled = speedDisablesAutoVac;
    }
    
    // Update arrowFull ONLY if auto vacuum is allowed
    if (autoVacAllowed) {
      // Simple: arrowFull follows animation direction
      arrowFull = currentArrowState;
      
      if (arrowFull != lastArrowFull) {
        extern void sendImmediateArrowUpdate();
        sendImmediateArrowUpdate();
        Serial.printf("[AUTO-VAC] arrow_full=%d, velEMA=%.3f\n", arrowFull, velEMA);
      }
    } else {
      // Auto vacuum disabled - don't update arrowFull, don't send vacuum commands
      if (currentArrowState != lastArrowFull) { // Still track state changes for debug
        if (!CFG.vacuumAutoMode) {
          Serial.printf("[AUTO-VAC] SKIPPED (disabled): would be arrow_full=%d\n", currentArrowState);
        } else if (pompUnitZuigActive) {
          Serial.printf("[AUTO-VAC] SKIPPED (zuig active): would be arrow_full=%d\n", currentArrowState);
        } else if (speedDisablesAutoVac) {
          Serial.printf("[AUTO-VAC] SKIPPED (speed %d >= %d): would be arrow_full=%d\n", g_speedStep, CFG.autoVacuumSpeedThreshold, currentArrowState);
        }
      }
    }
    
    lastArrowFull = currentArrowState;  // Always track for edge detection
  }
  // Voor pauze: arrowFull behoudt vorige waarde
  
  prevArrowFull = goingUp;  // Remember state for next edge detection
  
  // Z button direct handling - zuigen reageert meteen op indrukken
  bool allowVacuumControl = (uiMode == MODE_ANIM);
  static bool prevZPressed = false;
  static bool zToggleCommandSent = false;
  
  // Zuigen control met single click en cooldown
  static uint32_t lastZuigenCommand = 0;
  const uint32_t ZUIGEN_COOLDOWN_MS = 1000;  // 1 seconde cooldown
  // Hergebruik bestaande 'now' variabele
  
  if (allowVacuumControl && zClick == Z_SINGLE) {
    // Single click in animatie modus - check cooldown
    if ((now - lastZuigenCommand) >= ZUIGEN_COOLDOWN_MS) {
      Serial.println("[Z-SINGLE-CLICK] Sending TOGGLE_ZUIGEN command to Pump Unit");
      extern void sendToggleZuigenCommand();
      sendToggleZuigenCommand();
      lastZuigenCommand = now;
    } else {
      uint32_t remaining = ZUIGEN_COOLDOWN_MS - (now - lastZuigenCommand);
      Serial.printf("[Z-SINGLE-CLICK] Zuigen cooldown active, wait %lu ms\n", remaining);
    }
  } else if (uiMode == MODE_MENU && zClick == Z_SINGLE) {
    // Debug: Z-knop gedrukt in menu modus - geen zuig actie
    Serial.println("[Z-SINGLE-CLICK] Pressed in MENU mode - using for navigation only");
  }
  
  // Zuigen logic is now entirely on Pump Unit - no local vacuumTick needed
  
  // Update ESP-NOW connection status indicators
  bodyConnected = bodyESP_connected;
  motionConnected = m5atom_connected;  // M5Atom connection status
  // Note: keonConnected, solaceConnected blijven zoals ze zijn
  
  // Sync suctionState with pompUnitZuigActive for visual indicators
  extern bool pompUnitZuigActive;
  suctionState = pompUnitZuigActive;
  
  // Sync Keon animation if connected
  extern void syncKeonToAnimation();
  syncKeonToAnimation();
  
  // ================= CONTINUE ANIMATIE RENDERING =================
  // Animation calculations moved to auto vacuum section above
  
  // Handle parkToBottom special case
  int DRAW_BASELINE_Y = (int)round(CAP_Y_MID + (BL - CAP_Y_MID) * RANGE_SCALE);
  if (parkToBottom) {
    float targetY=(float)CAP_Y_IN;
    float rangeAbs=fabsf((float)CAP_Y_OUT - (float)CAP_Y_IN);
    float vMag=max(28.0f, rangeAbs * instF * 1.6f);
    float dir=(capY_draw < targetY) ? +1.0f : -1.0f;
    capY_draw += dir * vMag * dt;
    if ((dir > 0.0f && capY_draw >= targetY) || (dir < 0.0f && capY_draw <= targetY)) {
      capY_draw=targetY; parkToBottom=false; paused=true; phase=-1.5707963f;
    }
    capY = (int)round(capY_draw);
  } else {
    if (!paused) capY_draw = (float)capY;
  }
  
  int drawBase = DRAW_BASELINE_Y;
  
  // Visual arrow logic - using variables defined earlier

  // Use step01 calculated earlier for rod color
  uint16_t rodCol = lerp_rgb565_u8(CFG.rodSlowR,CFG.rodSlowG,CFG.rodSlowB,
                                   CFG.rodFastR,CFG.rodFastG,CFG.rodFastB, step01);

  cv->fillScreen(CFG.COL_BG);
  drawSleeveFixedTop(capY, CFG.COL_TAN);
  drawRodFromCap_Vinside_NoSeam(capY, drawBase, rodCol, 0xE946);
  
  // Draw Vibe lightning effects when active (bottom half)
  drawVibeLightning(true);  // Left side
  drawVibeLightning(false); // Right side
  
  // Draw Suction symbols when active (top half)
  drawSuctionSymbol(true);  // Left side )(
  drawSuctionSymbol(false); // Right side )(
  
  cv->flush();

  drawLeftFrame();
  // Simple visual arrow based on animation direction
  bool vacFilled = goingUp;
  drawVacArrowHeader(vacFilled);
  
  // Debug: Show vacuum state when it changes
  static bool lastVacFilled = false;
  if (vacFilled != lastVacFilled) {
    Serial.printf("[VISUAL ARROW] vacFilled: %d->%d (arrowFull=%d)\n", 
                  lastVacFilled, vacFilled, arrowFull);
    lastVacFilled = vacFilled;
  }

  if (uiMode==MODE_MENU && menuEdit && currentPage==PAGE_MAIN) drawEditPopupInMenu();

  delay(16);
}

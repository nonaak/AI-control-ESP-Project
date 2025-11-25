#include "ai_training_view.h"
#include <Arduino_GFX_Library.h>
#include <SD.h>
#include "input_touch.h"

static Arduino_GFX* g = nullptr;

// Kleuren
static const uint16_t COL_BG=BLACK, COL_FR=0xC618, COL_TX=WHITE;
static const uint16_t COL_DATA_BG=0x4208;  // Lichter grijs voor betere zichtbaarheid
static const uint16_t COL_QUESTION=0xFFE0;    // Geel voor vraag
static const uint16_t COL_GRAPH=0x07FF;       // Cyaan voor grafiek
static const uint16_t COL_BTN_SELECTED=0x07E0; // Groen voor geselecteerd

// Feedback button kleuren (12 verschillende kleuren)
static const uint16_t feedbackColors[] = {
  0xE71C,  // FB_NIKS (RUSTIG) - Lichter grijs
  0x051F,  // FB_SLAP - Helder blauw
  0xC618,  // FB_MIDDEL - Licht paars
  0x07E0,  // FB_STIJF - Groen
  0x1BEF,  // FB_TE_SOFT - Licht blauwgroen
  0xFFE0,  // FB_FIJN - Geel
  0xFC00,  // FB_TE_HEFTIG - Licht oranje
  0xFCE0,  // FB_BIJNA - Licht roze
  0xF81F,  // FB_EDGE - Magenta
  0x07FF,  // FB_KLAAR - Cyaan
  0xF800,  // FB_STOP - Rood (emergency kleur)
  0xDEDB   // FB_SKIP - Licht beige/crème
};

static const char* feedbackLabels[] = {
  "NIKS", "SLAP", "MIDDEL", "STIJF", "TE SOFT", 
  "FIJN", "TE HEFTIG", "BIJNA", "EDGE", "KLAAR", "STOP", "SKIP"
};

static int16_t SCR_W=320, SCR_H=240;
static uint32_t lastTouchMs = 0;
static const uint16_t TRAINING_COOLDOWN_MS = 800;

// Training state
static String currentFile = "";
static TrainingPoint currentDataPoint;
static int currentSampleIndex = 0;
static int totalSamples = 0;
static bool trainingComplete = false;
static FeedbackType selectedFeedback = FB_NIKS;
static bool needsRedraw = true;  // Anti-flicker: alleen hertekenen als nodig

// Button areas voor feedback (4x3 grid voor 12 knoppen)
static int16_t feedbackBtns[12][4]; // x,y,w,h voor elke button
static int16_t btnKlaarX, btnKlaarY, btnBackX, btnBackY;
static const int16_t BTN_W_NEXT = 160, BTN_W_BACK = 80, BTN_H = 22;  // VOLGEND EVENT nog breder
static const int16_t FEEDBACK_BTN_W = 65, FEEDBACK_BTN_H = 18;  // Iets kleiner voor 11 knoppen

// Data arrays voor visualisatie - uitgebreid met alle data
#define MAX_GRAPH_POINTS 100
static float graphHR[MAX_GRAPH_POINTS];
static float graphTrust[MAX_GRAPH_POINTS];
static float graphSleeve[MAX_GRAPH_POINTS];
static float graphAdem[MAX_GRAPH_POINTS];
static float graphZuigen[MAX_GRAPH_POINTS];  // Was graphTril
static float graphGSR[MAX_GRAPH_POINTS];
static int graphPointCount = 0;

static inline bool inRect(int16_t x,int16_t y,int16_t rx,int16_t ry,int16_t rw,int16_t rh){
  return (x>=rx && x<rx+rw && y>=ry && y<ry+rh);
}

// Custom kleine tekst functie (pixel-based) - 5x7 pixels voor optimale leesbaarheid
static void drawSmallText(int16_t x, int16_t y, const char* text, uint16_t color) {
  // 5x7 pixel font - optimale balans tussen grootte en leesbaarheid
  // Elke karakter is 5 pixels breed, 7 pixels hoog, 1 pixel spacing
  
  int16_t curX = x;
  
  while (*text) {
    char c = *text;
    
    // Teken karakter per karakter met pixels (5x7)
    switch (c) {
      // LETTERS - 5x7 pixels voor betere leesbaarheid
      case 'A': case 'a':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'D': case 'd':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color);
        break;
      case 'G': case 'g':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color);
        break;
      case 'H': case 'h':
        g->drawPixel(curX, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'I': case 'i':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+2, y+3, color);
        g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX+2, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'J': case 'j':
        g->drawPixel(curX+4, y, color);
        g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color);
        break;
      case 'E': case 'e':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color);
        g->drawPixel(curX, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'N': case 'n':
        g->drawPixel(curX, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+1, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+2, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+3, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'M': case 'm':
        g->drawPixel(curX, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+1, y+1, color); g->drawPixel(curX+3, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+2, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'V': case 'v':
        g->drawPixel(curX, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX+1, y+4, color); g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+3, y+5, color);
        g->drawPixel(curX+2, y+6, color);
        break;
      case 'B': case 'b':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color);
        break;
      case 'W': case 'w':
        g->drawPixel(curX, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+2, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+3, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'F': case 'f':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color);
        g->drawPixel(curX, y+5, color);
        g->drawPixel(curX, y+6, color);
        break;
      case 'K': case 'k':
        g->drawPixel(curX, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+3, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case 'P': case 'p':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color);
        g->drawPixel(curX, y+5, color);
        g->drawPixel(curX, y+6, color);
        break;
      case 'L': case 'l':
        g->drawPixel(curX, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color);
        g->drawPixel(curX, y+3, color);
        g->drawPixel(curX, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color); g->drawPixel(curX+3, y+5, color);
        break;
      case 'O': case 'o':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+3, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case 'R': case 'r':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+3, y+5, color);
        break;
      case 'S': case 's':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case 'T': case 't':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color); g->drawPixel(curX+4, y, color);
        g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+2, y+3, color);
        g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX+2, y+5, color);
        g->drawPixel(curX+2, y+6, color);
        break;
      case 'U': case 'u':
        g->drawPixel(curX, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+3, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case 'Z': case 'z':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+1, y+3, color);
        g->drawPixel(curX+1, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color); g->drawPixel(curX+3, y+5, color);
        break;
      
      // CIJFERS 0-9 - 5x7 pixels
      case '0':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+4, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+4, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+4, y+5, color);
        g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color);
        break;
      case '1':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+2, y+3, color);
        g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX+2, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case '2':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+4, y+1, color);
        g->drawPixel(curX+4, y+2, color);
        g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX+1, y+5, color);
        g->drawPixel(curX, y+6, color); g->drawPixel(curX+1, y+6, color); g->drawPixel(curX+2, y+6, color); g->drawPixel(curX+3, y+6, color); g->drawPixel(curX+4, y+6, color);
        break;
      case '3':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case '4':
        g->drawPixel(curX, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color); g->drawPixel(curX+3, y+2, color);
        g->drawPixel(curX+2, y+3, color);
        g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX+2, y+5, color);
        break;
      case '5':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case '6':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case '7':
        g->drawPixel(curX, y, color); g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+1, y+3, color);
        g->drawPixel(curX+1, y+4, color);
        g->drawPixel(curX+1, y+5, color);
        break;
      case '8':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX+1, y+2, color); g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
      case '9':
        g->drawPixel(curX+1, y, color); g->drawPixel(curX+2, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+3, y+1, color);
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+3, y+2, color);
        g->drawPixel(curX+1, y+3, color); g->drawPixel(curX+2, y+3, color); g->drawPixel(curX+3, y+3, color);
        g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX+1, y+5, color); g->drawPixel(curX+2, y+5, color);
        break;
        
      // SPECIALE KARAKTERS - 5x7
      case ':': 
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+2, y+4, color);
        break;
      case '.':
        g->drawPixel(curX+2, y+6, color);
        break;
      case '/':
        g->drawPixel(curX+3, y, color);
        g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+1, y+3, color);
        g->drawPixel(curX+1, y+4, color);
        g->drawPixel(curX, y+5, color);
        break;
      case '(':
        g->drawPixel(curX+1, y, color);
        g->drawPixel(curX, y+1, color);
        g->drawPixel(curX, y+2, color);
        g->drawPixel(curX, y+3, color);
        g->drawPixel(curX, y+4, color);
        g->drawPixel(curX+1, y+5, color);
        break;
      case ')':
        g->drawPixel(curX+1, y, color);
        g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+2, y+2, color);
        g->drawPixel(curX+2, y+3, color);
        g->drawPixel(curX+2, y+4, color);
        g->drawPixel(curX+1, y+5, color);
        break;
      case '%':
        g->drawPixel(curX, y, color); g->drawPixel(curX+3, y, color);
        g->drawPixel(curX, y+1, color); g->drawPixel(curX+2, y+1, color);
        g->drawPixel(curX+1, y+2, color);
        g->drawPixel(curX+1, y+3, color);
        g->drawPixel(curX, y+4, color); g->drawPixel(curX+3, y+4, color);
        g->drawPixel(curX, y+5, color); g->drawPixel(curX+3, y+5, color);
        break;
      case ' ':
        // Spatie - geen pixels
        break;
      default:
        // Onbekend karakter - kleine rechthoek
        g->drawPixel(curX, y+2, color); g->drawPixel(curX+1, y+2, color);
        g->drawPixel(curX, y+3, color); g->drawPixel(curX+1, y+3, color);
        break;
    }
    
    curX += 6; // 5 pixels breed + 1 pixel spacing
    text++;
  }
}

// Twee verschillende button functies
static void drawButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color, bool selected = false) {
  // Voor feedback knoppen - gebruik custom kleine tekst
  // Fix donkere kleuren - gebruik lichtere achtergrond
  uint16_t bgColor = color;
  if (color == 0x0000 || color == 0x001F || color == 0x8000) {
    bgColor = 0xCE59; // Lichtgrijs voor donkere knoppen
  }
  
  g->fillRoundRect(x, y, w, h, 4, bgColor);
  if (selected) {
    g->drawRoundRect(x, y, w, h, 4, COL_BTN_SELECTED);
    g->drawRoundRect(x+1, y+1, w-2, h-2, 3, COL_BTN_SELECTED);
  } else {
    g->drawRoundRect(x, y, w, h, 4, COL_FR);
  }
  
  // Custom kleine tekst voor feedback knoppen - zwarte tekst op lichte achtergrond
  int16_t textWidth = strlen(txt) * 6;
  int16_t textX = x + (w - textWidth) / 2;
  int16_t textY = y + (h - 7) / 2;
  drawSmallText(textX, textY, txt, BLACK);
}

static void drawNavButton(int16_t x,int16_t y,int16_t w,int16_t h,const char* txt, uint16_t color, bool selected = false) {
  // Voor navigatie knoppen - gebruik normale GFX tekst
  g->fillRoundRect(x, y, w, h, 4, color);
  if (selected) {
    g->drawRoundRect(x, y, w, h, 4, COL_BTN_SELECTED);
    g->drawRoundRect(x+1, y+1, w-2, h-2, 3, COL_BTN_SELECTED);
  } else {
    g->drawRoundRect(x, y, w, h, 4, COL_FR);
  }
  
  // Normale GFX tekst voor navigatie
  g->setTextColor(BLACK);
  g->setTextSize(1);
  int16_t x1, y1; 
  uint16_t tw, th;
  g->getTextBounds((char*)txt, 0, 0, &x1, &y1, &tw, &th);
  int textX = x + (w - tw)/2;
  int textY = y + (h + th)/2 - 2;
  g->setCursor(textX, textY);
  g->print(txt);
}

static void drawMiniGraph(int16_t x, int16_t y, int16_t w, int16_t h, float* data, int count, uint16_t color, const char* label) {
  // Graph background - lichter voor betere zichtbaarheid
  g->fillRect(x, y, w, h, COL_DATA_BG);
  g->drawRect(x, y, w, h, COL_FR);
  
  // Label altijd tekenen
  g->setTextColor(color);
  g->setTextSize(1);
  g->setCursor(x + 2, y + 2);
  g->print(label);
  
  if (count < 2) {
    // Toon "Geen data" als er onvoldoende punten zijn
    g->setTextColor(0x7BEF);  // Grijs
    g->setCursor(x + 2, y + h/2);
    g->print("Geen data");
    return;
  }
  
  // Find min/max voor scaling
  float minVal = data[0], maxVal = data[0];
  for (int i = 1; i < count; i++) {
    if (data[i] < minVal) minVal = data[i];
    if (data[i] > maxVal) maxVal = data[i];
  }
  
  if (maxVal <= minVal) maxVal = minVal + 1.0f;
  
  // Draw filled area graph (veel mooier dan lijnen)
  int16_t baseY = y + h - 3;  // Basis lijn
  
  for (int i = 0; i < count; i++) {
    float normalizedVal = (data[i] - minVal) / (maxVal - minVal);
    int16_t barX = x + 2 + (i * (w - 4)) / count;
    int16_t barTop = y + h - 3 - (int16_t)(normalizedVal * (h - 16));
    int16_t barWidth = max(1, (w - 4) / count);
    int16_t barHeight = baseY - barTop;
    
    if (barHeight > 0) {
      // Gevulde balk met gradient effect (donkerder aan de rand)
      g->fillRect(barX, barTop, barWidth, barHeight, color);
      if (barWidth > 1) {
        // Lichte rand voor diepte effect
        uint16_t darkColor = ((color >> 1) & 0x7DEF); // Donkerder maken
        g->drawRect(barX, barTop, barWidth, barHeight, darkColor);
      }
    }
  }
  
  // Trendlijn over de balken heen (dunne lijn voor smoothing)
  for (int i = 1; i < count; i++) {
    float y1 = (data[i-1] - minVal) / (maxVal - minVal);
    float y2 = (data[i] - minVal) / (maxVal - minVal);
    
    int16_t x1 = x + 2 + ((i-1) * (w - 4)) / count;
    int16_t x2 = x + 2 + (i * (w - 4)) / count;
    int16_t py1 = y + h - 3 - (int16_t)(y1 * (h - 16));
    int16_t py2 = y + h - 3 - (int16_t)(y2 * (h - 16));
    
    g->drawLine(x1, py1, x2, py2, WHITE);  // Witte trendlijn
  }
  
  // Current value (label al getekend bovenaan)
  g->setTextColor(color);
  g->setCursor(x + 2, y + h - 12);
  g->printf("%.1f", data[count-1]);
}

static bool loadNextDataPoint() {
  if (currentSampleIndex >= totalSamples) {
    trainingComplete = true;
    return false;
  }
  
  File file = SD.open("/" + currentFile);
  if (!file) return false;
  
  // Skip header
  file.readStringUntil('\n');
  
  // Skip to current sample
  for (int i = 0; i < currentSampleIndex; i++) {
    if (!file.available()) {
      file.close();
      return false;
    }
    file.readStringUntil('\n');
  }
  
  // Read current sample
  if (!file.available()) {
    file.close();
    return false;
  }
  
  String line = file.readStringUntil('\n');
  file.close();
  
  if (line.length() < 5) return false;
  
  // Parse CSV: Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause,Adem,Tril
  int commaIndex[11];
  int commaCount = 0;
  for (int i = 0; i < line.length() && commaCount < 11; i++) {
    if (line.charAt(i) == ',') {
      commaIndex[commaCount++] = i;
    }
  }
  
  if (commaCount >= 11) {  // We hebben nu 12 velden
    // Parse alle CSV velden: Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause,Adem,Tril
    currentDataPoint.timeMs = line.substring(0, commaIndex[0]).toInt();
    currentDataPoint.heartRate = line.substring(commaIndex[0] + 1, commaIndex[1]).toFloat();
    currentDataPoint.temperature = line.substring(commaIndex[1] + 1, commaIndex[2]).toFloat();
    currentDataPoint.gsr = line.substring(commaIndex[2] + 1, commaIndex[3]).toFloat();
    currentDataPoint.oxygen = line.substring(commaIndex[3] + 1, commaIndex[4]).toFloat();
    currentDataPoint.beat = (line.substring(commaIndex[4] + 1, commaIndex[5]).toInt() != 0);
    currentDataPoint.trustSpeed = line.substring(commaIndex[5] + 1, commaIndex[6]).toFloat();
    currentDataPoint.sleeveSpeed = line.substring(commaIndex[6] + 1, commaIndex[7]).toFloat();
    currentDataPoint.suctionLevel = line.substring(commaIndex[7] + 1, commaIndex[8]).toFloat();
    currentDataPoint.pauseTime = line.substring(commaIndex[8] + 1, commaIndex[9]).toFloat();
    currentDataPoint.breathingRate = line.substring(commaIndex[9] + 1, commaIndex[10]).toFloat();
    currentDataPoint.vibrationLevel = line.substring(commaIndex[10] + 1).toFloat();
    
    currentDataPoint.feedback = FB_NIKS;
    currentDataPoint.trained = false;
    
    // Add to graphs - alle data
    if (graphPointCount < MAX_GRAPH_POINTS) {
      graphHR[graphPointCount] = currentDataPoint.heartRate;
      graphTrust[graphPointCount] = currentDataPoint.trustSpeed;
      graphSleeve[graphPointCount] = currentDataPoint.sleeveSpeed;
      graphAdem[graphPointCount] = currentDataPoint.breathingRate;
      graphZuigen[graphPointCount] = currentDataPoint.suctionLevel;  // Was vibrationLevel
      graphGSR[graphPointCount] = currentDataPoint.gsr;
      graphPointCount++;
    }
    
    currentSampleIndex++;
    return true;
  }
  
  return false;
}

static int countTotalSamples(const String& filename) {
  File file = SD.open("/" + filename);
  if (!file) return 0;
  
  // Skip header
  file.readStringUntil('\n');
  
  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 5) count++;
  }
  file.close();
  
  return count;
}

// Intelligente event beschrijving genereren
static String generateEventDescription(const TrainingPoint& dataPoint) {
  String description = "";
  float time_min = dataPoint.timeMs / 60000.0f;
  
  // Analyse van de sensors
  bool hr_high = (dataPoint.heartRate > 100);
  bool hr_low = (dataPoint.heartRate < 60);
  bool temp_high = (dataPoint.temperature > 37.5f);
  bool gsr_high = (dataPoint.gsr > 300);
  bool oxy_low = (dataPoint.oxygen < 94);
  bool trust_fast = (dataPoint.trustSpeed > 15);
  bool sleeve_fast = (dataPoint.sleeveSpeed > 15);
  bool breathing_low = (dataPoint.breathingRate < 40);
  
  description = "Tijd " + String(time_min, 1) + " min: ";
  
  // Detecteer patronen en genereer beschrijving
  if (hr_high && gsr_high && trust_fast) {
    description += "Je hartslag gaat omhoog, je huid gaat omhoog en je stroke input zet je sneller";
  } else if (breathing_low && gsr_high && hr_high && oxy_low) {
    description += "Je ademt niet meer even, je huid en hart gaat omhoog en oxy gaat omlaag";
  } else if (hr_high && temp_high) {
    description += "Je hartslag en temperatuur stijgen - mogelijk opwinding";
  } else if (gsr_high && hr_low) {
    description += "Hoge huidgeleiding maar lage hartslag - mogelijk ontspanning";
  } else if (trust_fast && sleeve_fast) {
    description += "Machine draait op hoge snelheid - intensief moment";
  } else if (hr_high) {
    description += "Je hartslag gaat omhoog - activiteit detectie";
  } else if (gsr_high) {
    description += "Hoge huidgeleiding - stress/opwinding indicator";
  } else if (oxy_low) {
    description += "Lage zuurstof - ademhalingsverandering";
  } else if (temp_high) {
    description += "Temperatuur stijgt - lichaamsreactie";
  } else {
    description += "Normale sensor waarden - rustig moment";
  }
  
  return description;
}

void aiTraining_begin(Arduino_GFX* gfx, const String& filename) {
  g = gfx;
  //SCR_W = g->width();
  //SCR_H = g->height();
  
  currentFile = filename;
  currentSampleIndex = 0;
  totalSamples = countTotalSamples(filename);
  trainingComplete = false;
  selectedFeedback = FB_NIKS;
  graphPointCount = 0;
  
  // Setup feedback button grid (4x3 voor 11 knoppen + 1 lege plek)
  int16_t gridCols = 4;
  int16_t gridRows = 3;
  int16_t gridTotalW = gridCols * FEEDBACK_BTN_W + (gridCols - 1) * 5;  // 4 buttons + 3 gaps
  int16_t gridStartX = (SCR_W - gridTotalW) / 2;
  int16_t gridStartY = 140;  // Lager - meer ruimte voor tekst bovenaan
  int16_t gridSpaceX = FEEDBACK_BTN_W + 5;  // Button width + smaller gap
  int16_t gridSpaceY = FEEDBACK_BTN_H + 3;   // Button height + smaller gap
  
  for (int i = 0; i < 12; i++) {
    int row = i / gridCols;
    int col = i % gridCols;
    feedbackBtns[i][0] = gridStartX + col * gridSpaceX;
    feedbackBtns[i][1] = gridStartY + row * gridSpaceY;
    feedbackBtns[i][2] = FEEDBACK_BTN_W;
    feedbackBtns[i][3] = FEEDBACK_BTN_H;
  }
  
  // Navigation buttons - verschillende breedtes
  int16_t navButtonsY = SCR_H - BTN_H - 5;
  int16_t totalNavWidth = BTN_W_BACK + BTN_W_NEXT + 20;  // Verschillende breedtes + gap
  int16_t navStartX = (SCR_W - totalNavWidth) / 2;
  
  btnBackX = navStartX;
  btnBackY = navButtonsY;
  btnKlaarX = navStartX + BTN_W_BACK + 20;
  btnKlaarY = navButtonsY;
  
  // Load eerste data point
  loadNextDataPoint();
  needsRedraw = true;  // Eerste keer tekenen
}

TrainingResult aiTraining_poll() {
  // Anti-flicker: alleen hertekenen als nodig
  if (!needsRedraw) {
    // Alleen input afhandelen
    int16_t x, y;
    if (!inputTouchRead(x, y)) return TR_NONE;
    
    uint32_t now = millis();
    if (now - lastTouchMs < TRAINING_COOLDOWN_MS) return TR_NONE;
    
    // Handle feedback button clicks
    for (int i = 0; i < 12; i++) {
      if (inRect(x, y, feedbackBtns[i][0], feedbackBtns[i][1], feedbackBtns[i][2], feedbackBtns[i][3])) {
        lastTouchMs = now;
        if (selectedFeedback != (FeedbackType)i) {
          selectedFeedback = (FeedbackType)i;
          needsRedraw = true;  // Herteken voor nieuwe selectie
        }
        return TR_NONE;
      }
    }
    
    // Handle navigation buttons
    if (inRect(x, y, btnKlaarX, btnKlaarY, BTN_W_NEXT, BTN_H)) {
      lastTouchMs = now;
      currentDataPoint.feedback = selectedFeedback;
      currentDataPoint.trained = true;
      
      if (!loadNextDataPoint()) {
        trainingComplete = true;
      }
      selectedFeedback = FB_NIKS;
      needsRedraw = true;  // Herteken voor nieuwe data
      return TR_NONE;
    }
    
    if (inRect(x, y, btnBackX, btnBackY, BTN_W_BACK, BTN_H)) {
      lastTouchMs = now;
      return TR_BACK;
    }
    
    // Handle completion screen buttons
    if (trainingComplete) {
      if (inRect(x, y, btnKlaarX, btnKlaarY, BTN_W_NEXT, BTN_H)) {
        lastTouchMs = now;
        return TR_COMPLETE;
      }
      if (inRect(x, y, btnBackX, btnBackY, BTN_W_BACK, BTN_H)) {
        lastTouchMs = now;
        return TR_BACK;
      }
    }
    
    return TR_NONE;
  }
  
  // Draw de interface (alleen als nodig)
  needsRedraw = false;  // Reset flag
  g->fillScreen(COL_BG);
  
  // Progress met GFX font
  String progressText = String(currentSampleIndex) + "/" + String(totalSamples) + 
                       " (" + String((float)currentSampleIndex * 100.0f / totalSamples, 0) + "%)";
  g->setTextSize(1);
  g->setTextColor(0x07FF); // Cyaan
  int16_t x1, y1; uint16_t w1, h1;
  g->getTextBounds(progressText, 0, 0, &x1, &y1, &w1, &h1);
  g->setCursor((SCR_W - w1) / 2, 15);
  g->print(progressText);
  
  if (trainingComplete) {
    // Training voltooid scherm
    g->setTextColor(COL_QUESTION);
    g->setTextSize(2);
    String completeText = "Training Voltooid!";
    int16_t x1, y1; uint16_t tw, th;
    g->getTextBounds(completeText, 0, 0, &x1, &y1, &tw, &th);
    g->setCursor((SCR_W - tw) / 2, 50);
    g->print(completeText);
    
    g->setTextColor(COL_TX);
    g->setTextSize(1);
    String infoText = String(totalSamples) + " samples getraind";
    g->getTextBounds(infoText, 0, 0, &x1, &y1, &tw, &th);
    g->setCursor((SCR_W - tw) / 2, 80);
    g->print(infoText);
    
    String saveText = "Kies OPSLAAN om model te bewaren";
    g->getTextBounds(saveText, 0, 0, &x1, &y1, &tw, &th);
    g->setCursor((SCR_W - tw) / 2, 95);
    g->print(saveText);
    
    drawNavButton(btnKlaarX, btnKlaarY, BTN_W_NEXT, BTN_H, "OPSLAAN", COL_BTN_SELECTED);
    drawNavButton(btnBackX, btnBackY, BTN_W_BACK, BTN_H, "TERUG", 0xF800);
    
    return TR_NONE;
  }
  
  // AI vraag over het event (max 2 regels)
  String aiQuestion = "Als: Hart " + String((int)currentDataPoint.heartRate) + 
                     " BPM, temp " + String(currentDataPoint.temperature, 1) + "C, GSR " + 
                     String((int)currentDataPoint.gsr) + ", O2 " + String((int)currentDataPoint.oxygen) + 
                     "%, trust " + String(currentDataPoint.trustSpeed, 1) + ", sleeve " + 
                     String(currentDataPoint.sleeveSpeed, 1) + "?";
  
  g->setTextSize(1);
  g->setTextColor(COL_QUESTION);
  
  // Probeer op 1 regel te houden, anders splitsen
  if (aiQuestion.length() > 38) {  // Korter maken voor betere verdeling
    // Zoek goede splitsing voor meer gelijke verdeling
    int splitPos = aiQuestion.lastIndexOf(' ', 35);
    if (splitPos > 15) {
      String line1 = aiQuestion.substring(0, splitPos);
      String line2 = aiQuestion.substring(splitPos + 1);
      
      // Teken regel 1
      int16_t x1, y1; uint16_t w1, h1;
      g->getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
      g->setCursor((SCR_W - w1) / 2, 32);
      g->print(line1);
      
      // Teken regel 2
      int16_t x2, y2; uint16_t w2, h2;
      g->getTextBounds(line2, 0, 0, &x2, &y2, &w2, &h2);
      g->setCursor((SCR_W - w2) / 2, 48);
      g->print(line2);
    } else {
      // Kan niet splitsen, gebruik 1 regel
      int16_t x2, y2; uint16_t w2, h2;
      g->getTextBounds(aiQuestion, 0, 0, &x2, &y2, &w2, &h2);
      g->setCursor((SCR_W - w2) / 2, 32);
      g->print(aiQuestion);
    }
  } else {
    // Past op 1 regel
    int16_t x2, y2; uint16_t w2, h2;
    g->getTextBounds(aiQuestion, 0, 0, &x2, &y2, &w2, &h2);
    g->setCursor((SCR_W - w2) / 2, 32);
    g->print(aiQuestion);
  }
  
  // AI antwoord - "Antwoord:" in groen, event naam in knop kleur
  String answerPrefix = "Antwoord: ";
  // Voor nu gebruiken we een dummy event naam - later vervangen door AI voorspelling
  String eventName = feedbackLabels[selectedFeedback]; // Gebruik geselecteerde feedback
  if (selectedFeedback == FB_NIKS) {
    eventName = "RUSTIG"; // Default AI gok
  }
  String fullAnswer = answerPrefix + eventName;
  
  // Bereken positie voor centrering van hele tekst
  g->setTextSize(1);
  int16_t fx, fy; uint16_t fw, fh;
  g->getTextBounds(fullAnswer, 0, 0, &fx, &fy, &fw, &fh);
  int16_t startX = (SCR_W - fw) / 2;
  
  // Teken "Antwoord:" in groen
  g->setTextColor(0x07E0);  // Groen
  g->setCursor(startX, 70);
  g->print(answerPrefix);
  
  // Bereken positie voor event naam
  int16_t px, py; uint16_t pw, ph;
  g->getTextBounds(answerPrefix, 0, 0, &px, &py, &pw, &ph);
  
  // Zoek welk event dit is en gebruik die kleur
  uint16_t eventColor = 0xFFFF; // Default wit
  for (int i = 0; i < 12; i++) {
    if (String(feedbackLabels[i]) == eventName) {
      eventColor = feedbackColors[i];
      break;
    }
  }
  
  // Teken event naam in knop kleur
  g->setTextColor(eventColor);
  g->setCursor(startX + pw, 70);
  g->print(eventName);
  
  // Sensor data - 3 regels met hoofdscherm kleuren
  g->setTextSize(0);  // Kleiner lettertype
  
  // Regel 1: HART (rood), HUID (roze)
  String hartText = "HART:" + String((int)currentDataPoint.heartRate);
  String huidText = " HUID:" + String((int)currentDataPoint.gsr);
  
  // Bereken totale breedte voor centrering
  String fullLine1 = hartText + huidText;
  int16_t fx1, fy1; uint16_t fw1, fh1;
  g->getTextBounds(fullLine1, 0, 0, &fx1, &fy1, &fw1, &fh1);
  int16_t startX1 = (SCR_W - fw1) / 2;
  int16_t currentX1 = startX1;
  
  // HART in rood
  g->setTextColor(0xF800);
  g->setCursor(currentX1, 97);
  g->print(hartText);
  int16_t tx1, ty1; uint16_t tw1, th1;
  g->getTextBounds(hartText, 0, 0, &tx1, &ty1, &tw1, &th1);
  currentX1 += tw1;
  
  // HUID in roze
  g->setTextColor(0xF81F);
  g->setCursor(currentX1, 97);
  g->print(huidText);
  
  // Regel 2: TEMP (oranje), ADEM (groen), OXY (geel)
  String tempText = "TEMP:" + String(currentDataPoint.temperature, 1);
  String ademText = " ADEM:" + String(currentDataPoint.breathingRate, 1);
  String oxyText = " OXY:" + String((int)currentDataPoint.oxygen) + "%";
  
  // Bereken totale breedte voor centrering
  String fullLine2 = tempText + ademText + oxyText;
  int16_t fx2, fy2; uint16_t fw2, fh2;
  g->getTextBounds(fullLine2, 0, 0, &fx2, &fy2, &fw2, &fh2);
  int16_t startX2 = (SCR_W - fw2) / 2;
  int16_t currentX2 = startX2;
  
  // TEMP in oranje
  g->setTextColor(0xFD20);
  g->setCursor(currentX2, 113);
  g->print(tempText);
  int16_t tx2, ty2; uint16_t tw2, th2;
  g->getTextBounds(tempText, 0, 0, &tx2, &ty2, &tw2, &th2);
  currentX2 += tw2;
  
  // ADEM in groen
  g->setTextColor(0x07E0);
  g->setCursor(currentX2, 113);
  g->print(ademText);
  g->getTextBounds(ademText, 0, 0, &tx2, &ty2, &tw2, &th2);
  currentX2 += tw2;
  
  // OXY in geel
  g->setTextColor(0xFFE0);
  g->setCursor(currentX2, 113);
  g->print(oxyText);
  
  // Regel 3: SnelH (paars), ZUIGEN (licht blauw), Vibe (oranje)
  String snelhText = "SnelH:" + String((int)currentDataPoint.sleeveSpeed);
  String zuigenText = " ZUIGEN:" + String((currentDataPoint.suctionLevel > 0.5) ? "AAN" : "UIT");
  String vibeText = " Vibe:" + String((currentDataPoint.vibrationLevel > 0.5) ? "AAN" : "UIT");
  
  // Bereken totale breedte voor centrering
  String fullLine3 = snelhText + zuigenText + vibeText;
  int16_t fx3, fy3; uint16_t fw3, fh3;
  g->getTextBounds(fullLine3, 0, 0, &fx3, &fy3, &fw3, &fh3);
  int16_t startX3 = (SCR_W - fw3) / 2;
  int16_t currentX3 = startX3;
  
  // SNELH in paars
  g->setTextColor(0xF81F);
  g->setCursor(currentX3, 129);
  g->print(snelhText);
  int16_t tx3, ty3; uint16_t tw3, th3;
  g->getTextBounds(snelhText, 0, 0, &tx3, &ty3, &tw3, &th3);
  currentX3 += tw3;
  
  // ZUIGEN in licht blauw
  g->setTextColor(0x87FF);
  g->setCursor(currentX3, 129);
  g->print(zuigenText);
  g->getTextBounds(zuigenText, 0, 0, &tx3, &ty3, &tw3, &th3);
  currentX3 += tw3;
  
  // VIBE in oranje
  g->setTextColor(0xFD20);
  g->setCursor(currentX3, 129);
  g->print(vibeText);
  
  // Feedback knoppen (4x3 grid voor 12 knoppen)
  for (int i = 0; i < 12; i++) {
    bool selected = (selectedFeedback == (FeedbackType)i);
    drawButton(feedbackBtns[i][0], feedbackBtns[i][1], feedbackBtns[i][2], feedbackBtns[i][3], 
               feedbackLabels[i], feedbackColors[i], selected);
  }
  
  // Navigatie knoppen (verschillende breedtes) - normale GFX tekst
  drawNavButton(btnKlaarX, btnKlaarY, BTN_W_NEXT, BTN_H, "VOLGEND EVENT", COL_BTN_SELECTED);
  drawNavButton(btnBackX, btnBackY, BTN_W_BACK, BTN_H, "TERUG", 0xF800);
  
  return TR_NONE;  // Input wordt al bovenaan afgehandeld
}

bool aiTraining_saveModel(int modelSlot) {
  // TODO: Implementeer model opslag naar EEPROM/Flash
  // Voor nu alleen debug output
  Serial.printf("[AI] Training model opslaan naar slot %d\n", modelSlot + 1);
  return true;
}
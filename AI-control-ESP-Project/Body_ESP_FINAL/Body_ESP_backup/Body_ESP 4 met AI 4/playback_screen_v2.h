/*
  PLAYBACK SCREEN V2 - Herontworpen voor duidelijke visualisatie
  
  ═══════════════════════════════════════════════════════════════════════════
  Layout (480x320):
  ═══════════════════════════════════════════════════════════════════════════
  
  ┌─────────────────────────────────────────────────────────────────────────┐
  │ ▶ sessie_20241124.anl                              125.3s / 1847.2s    │  Y: 0-28
  ├────────────────────┬────────────────────────────────────────────────────┤
  │                    │                                                    │
  │   ┌──────────┐     │  ♥ HR ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~    │
  │   │          │     │  🌡 Temp ──────────────────────────────────────    │
  │   │    4     │     │  ⚡ GSR ∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿∿    │  Y: 28-220
  │   │ VERHOOGD │     │                                                    │
  │   │          │     │  📊 LEVEL ▁▂▃▄▅▄▃▄▅▆▇▆▅▄▃▄▅▆▇█▇▆▅▄▃▂▁▂▃▄▅▆    │
  │   └──────────┘     │                                                    │
  │                    │                                                    │
  │   AI: 5 → Jij: 4   │                                                    │
  │   Correctie: -1    │                                                    │
  │                    │                                                    │
  ├────────────────────┴────────────────────────────────────────────────────┤
  │ [▓▓▓▓▓▓▓▓░░░░░░░░░░░░░░░░░░░░░░░░░░░░]  [-] 100% [+]                   │  Y: 220-250
  │      ▲  ▲     ▲           ▲                                             │
  │      └──┴─────┴───────────┴─ Markers                                    │
  ├─────────────────────────────────────────────────────────────────────────┤
  │                                                                          │
  │  [  STOP  ]    [  PAUZE  ]    [ ◀ -10s ]    [ +10s ▶ ]    [ AI-ACTIE ]  │  Y: 250-320
  │                                                                          │
  └─────────────────────────────────────────────────────────────────────────┘
  
  X: 0-130 = Level paneel
  X: 130-480 = Grafieken (via body_gfx4 of custom)
*/

#ifndef PLAYBACK_SCREEN_V2_H
#define PLAYBACK_SCREEN_V2_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         LAYOUT CONSTANTEN
// ═══════════════════════════════════════════════════════════════════════════

// Scherm dimensies
#define PB_SCREEN_W         480
#define PB_SCREEN_H         320

// Titel balk
#define PB_TITLE_H          28
#define PB_TITLE_Y          0

// Level paneel (links)
#define PB_LEVEL_PANEL_X    0
#define PB_LEVEL_PANEL_W    125
#define PB_LEVEL_PANEL_Y    PB_TITLE_H
#define PB_LEVEL_PANEL_H    185

// Grafieken (rechts)
#define PB_GRAPH_X          (PB_LEVEL_PANEL_W + 5)
#define PB_GRAPH_W          (PB_SCREEN_W - PB_GRAPH_X)
#define PB_GRAPH_Y          PB_TITLE_H
#define PB_GRAPH_H          190

// Afspeelbalk
#define PB_PROGRESS_Y       218
#define PB_PROGRESS_H       28
#define PB_PROGRESS_X       5
#define PB_PROGRESS_W       380
#define PB_SPEED_X          (PB_PROGRESS_X + PB_PROGRESS_W + 5)

// Knoppen
#define PB_BUTTON_Y         252
#define PB_BUTTON_H         38
#define PB_BUTTON_W         90
#define PB_BUTTON_SPACING   5

// ═══════════════════════════════════════════════════════════════════════════
//                         KLEUREN PER STRESS LEVEL
// ═══════════════════════════════════════════════════════════════════════════

// Stress level kleuren (van groen naar rood) - gedefinieerd in playback_screen_v2.cpp
extern const uint16_t STRESS_COLORS[8];

// Level labels (kort) - gedefinieerd in playback_screen_v2.cpp
extern const char* STRESS_LABELS_SHORT[8];

// ═══════════════════════════════════════════════════════════════════════════
//                         ANNOTATIE MARKER STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct PlaybackMarker {
  float timestamp;        // Seconden in recording
  int level;              // Stress level (0-7)
  bool isAI;              // true = AI voorspelling, false = user annotatie
  bool isEdge;            // Edge moment marker
};

#define MAX_VISIBLE_MARKERS 50

// ═══════════════════════════════════════════════════════════════════════════
//                         PLAYBACK SCREEN CLASS
// ═══════════════════════════════════════════════════════════════════════════

class PlaybackScreenV2 {
public:
  PlaybackScreenV2();
  
  // ─── Setup ───
  void begin(Arduino_GFX* gfx);
  
  // ─── State Updates ───
  void setFilename(const char* filename);
  void setProgress(float currentTime, float totalTime);
  void setSpeed(float speedPercent);
  void setPaused(bool paused);
  
  // ─── Level Updates ───
  void setCurrentLevel(int level);              // Huidig stress level (0-7)
  void setAIPrediction(int aiLevel);            // Wat AI voorspelde
  void setUserAnnotation(int userLevel);        // Wat gebruiker koos
  
  // ─── Sensor Updates ───
  void setSensorValues(float hr, float temp, float gsr);
  
  // ─── Markers ───
  void clearMarkers();
  void addMarker(float timestamp, int level, bool isAI, bool isEdge = false);
  
  // ─── Level History (voor level grafiek) ───
  void pushLevelSample(int level);
  
  // ─── Drawing ───
  void drawStaticElements();    // Eenmalig: frame, labels
  void drawDynamicElements();   // Elke frame: levels, progress, sensors
  void drawLevelPanel();        // Level indicator links
  void drawProgressBar();       // Afspeelbalk met markers
  void drawButtons(int selectedIdx);  // Knoppen onderaan
  void drawTitleBar();          // Titel met bestandsnaam en tijd
  void drawLevelGraph();        // Level over tijd grafiek
  
  // ─── Touch ───
  int handleTouch(int x, int y);  // Returns: -1=geen hit, 0-4=knop index
  
  // ─── Encoder Navigatie ───
  void setSelectedButton(int idx);
  int getSelectedButton() { return selectedButtonIdx; }
  int getButtonCount() { return 5; }  // STOP, PLAY, -10s, +10s, AI-ACTIE
  
private:
  Arduino_GFX* gfx;
  
  // State
  char filename[64];
  float currentTime;
  float totalTime;
  float speed;
  bool isPaused;
  
  // Levels
  int currentLevel;
  int aiPredictedLevel;
  int userAnnotatedLevel;
  
  // Sensors
  float hr, temp, gsr;
  
  // Markers voor afspeelbalk
  PlaybackMarker markers[MAX_VISIBLE_MARKERS];
  int markerCount;
  
  // Level history voor grafiek (laatste 100 samples)
  int8_t levelHistory[100];
  int levelHistoryIdx;
  bool levelHistoryFull;
  
  // UI state
  int selectedButtonIdx;
  bool staticDrawn;
  
  // ─── Helpers ───
  uint16_t getLevelColor(int level);
  void drawCenteredText(const char* text, int x, int y, int w, uint16_t color, uint16_t bg);
};

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBAL INSTANCE
// ═══════════════════════════════════════════════════════════════════════════

extern PlaybackScreenV2 playbackScreen;

#endif // PLAYBACK_SCREEN_V2_H

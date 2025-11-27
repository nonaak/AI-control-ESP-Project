/*
  ML INTEGRATION - KOPPELT ALLES AAN ELKAAR
  
  ═══════════════════════════════════════════════════════════════════════════
  Dit bestand verbindt:
  - ml_training.h/cpp      (live feedback via nunchuk)
  - ml_annotation.h/cpp    (playback feedback via GEVOEL knop)
  - playback_screen_v2     (nieuw playback scherm)
  - ai_bijbel.h            (autonomy regels)
  - ml_stress_analyzer     (stress level berekening)
  ═══════════════════════════════════════════════════════════════════════════
  
  FLOW OVERZICHT:
  
  ┌─────────────────────────────────────────────────────────────────────────┐
  │                         DATA VERZAMELEN                                 │
  ├─────────────────────────────────────────────────────────────────────────┤
  │                                                                         │
  │   LIVE SESSIE                        PLAYBACK                           │
  │   ───────────                        ────────                           │
  │   Nunchuk correctie                  GEVOEL knop                        │
  │        ↓                                  ↓                             │
  │   ml_logUserCorrection()             ml_addPlaybackAnnotation()         │
  │        ↓                                  ↓                             │
  │   feedback.csv                       sessie.ann                         │
  │        └──────────────┬──────────────────┘                              │
  │                       ↓                                                 │
  │              /ml_training/ folder                                       │
  │                       ↓                                                 │
  │              ml_trainModel()                                            │
  │                       ↓                                                 │
  │                 model.bin                                               │
  │                       ↓                                                 │
  │              ML maakt beslissingen!                                     │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘
*/

#ifndef ML_INTEGRATION_H
#define ML_INTEGRATION_H

#include <Arduino.h>
#include "ml_trainer.h"
#include "ml_annotation.h"
#include "ml_stress_analyzer.h"
#include "ai_bijbel.h"
#include "playback_screen_v2.h"

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBALE STATE
// ═══════════════════════════════════════════════════════════════════════════

struct MLIntegrationState {
  // Modus
  bool isLiveSession;           // true = live opname, false = playback
  bool isPlaybackMode;          // Playback actief?
  
  // Live sessie
  bool liveSessionActive;
  uint32_t liveSessionStart;
  int liveEdgeCount;
  
  // Playback
  String currentPlaybackFile;
  float playbackPosition;       // Seconden
  float playbackDuration;       // Totale lengte
  
  // Huidige sensor waarden (voor beide modi)
  float currentHR;
  float currentTemp;
  float currentGSR;
  
  // Huidige levels
  int currentStressLevel;       // Berekend door ML (0-7)
  int aiPredictedLevel;         // Wat AI voorspelde
  int userCorrectedLevel;       // Wat gebruiker koos (-1 = geen correctie)
  
  // Training stats
  int totalFeedbackSamples;
  int totalAnnotations;
  bool modelTrained;
  float modelAccuracy;
};

extern MLIntegrationState mlState;

// ═══════════════════════════════════════════════════════════════════════════
//                         INITIALISATIE
// ═══════════════════════════════════════════════════════════════════════════

// Roep dit aan in setup()
void mlIntegration_begin();

// ═══════════════════════════════════════════════════════════════════════════
//                         LIVE SESSIE FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

// Start live sessie met ML tracking
void mlIntegration_startLiveSession();

// Stop live sessie
void mlIntegration_stopLiveSession();

// Update sensors (roep elke loop aan)
void mlIntegration_updateSensors(float hr, float temp, float gsr);

// Log nunchuk correctie (roep aan vanuit ESP-NOW handler)
void mlIntegration_logNunchukCorrection(int aiLevel, int userLevel);

// Log edge event
void mlIntegration_logEdge();

// Log orgasme (beëindigt sessie)
void mlIntegration_logOrgasme();

// ═══════════════════════════════════════════════════════════════════════════
//                         PLAYBACK FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

// Start playback van een bestand
void mlIntegration_startPlayback(const char* filename);

// Stop playback
void mlIntegration_stopPlayback();

// Update playback positie en sensor waarden
void mlIntegration_updatePlayback(float timestamp, float hr, float temp, float gsr, int aiLevel);

// Log GEVOEL knop feedback
void mlIntegration_logGevoelFeedback(int userLevel);

// Check of er al feedback is op huidige positie
int mlIntegration_getExistingFeedback();  // Returns -1 als geen

// ═══════════════════════════════════════════════════════════════════════════
//                         AI BESLISSINGEN
// ═══════════════════════════════════════════════════════════════════════════

// Krijg optimaal level (gebruikt ML als getraind, anders rule-based)
int mlIntegration_getOptimalLevel(uint8_t autonomyPercent);

// Check of AI naar level mag (AI Bijbel regels)
bool mlIntegration_aiMagNaarLevel(uint8_t autonomyPercent, int level);

// ═══════════════════════════════════════════════════════════════════════════
//                         TRAINING
// ═══════════════════════════════════════════════════════════════════════════

// Combineer alle feedback bronnen en train model
bool mlIntegration_trainModel();

// Krijg training statistieken
void mlIntegration_getStats(int* feedbackCount, int* annotationCount, 
                             bool* modelTrained, float* accuracy);

// ═══════════════════════════════════════════════════════════════════════════
//                         UI HELPERS
// ═══════════════════════════════════════════════════════════════════════════

// Teken playback scherm (wrapper voor playback_screen_v2)
void mlIntegration_drawPlaybackScreen(Arduino_GFX* gfx, int selectedButton);

// Handle touch op playback scherm
int mlIntegration_handlePlaybackTouch(int x, int y);

// Handle encoder op playback scherm
void mlIntegration_handlePlaybackEncoder(int direction);
void mlIntegration_handlePlaybackEncoderPress();

#endif // ML_INTEGRATION_H

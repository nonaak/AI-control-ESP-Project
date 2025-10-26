#pragma once
#include <Arduino.h>
class Arduino_GFX;

// AI Training events
enum TrainingEvent : uint8_t { 
  TR_NONE=0, 
  TR_BACK,          // Terug naar AI analyze
  TR_COMPLETE       // Training voltooid
};

// Alias voor duidelijkere functie signatures
typedef TrainingEvent TrainingResult;

// Feedback categories voor AI training
enum FeedbackType : uint8_t {
  FB_NIKS = 0,      // Niks bijzonders
  FB_SLAP,          // Te slap
  FB_MIDDEL,        // Middel
  FB_STIJF,         // Stijf  
  FB_TE_SOFT,       // Te soft
  FB_FIJN,          // Fijn
  FB_TE_HEFTIG,     // Te heftig
  FB_BIJNA,         // Bijna (edge)
  FB_EDGE,          // Edge
  FB_KLAAR,         // Klaar/voltooid
  FB_STOP,          // Stop/emergency
  FB_SKIP           // Skip/geen label
};

// Training data point - uitgebreid met alle sensor data
struct TrainingPoint {
  uint32_t timeMs;
  float heartRate;
  float temperature;
  float gsr;              // Skin conductance
  float oxygen;
  bool beat;
  float trustSpeed;
  float sleeveSpeed;
  float suctionLevel;
  float pauseTime;
  float breathingRate;    // Ademhaling data
  float vibrationLevel;   // Tril data
  FeedbackType feedback;
  bool trained;
};

// AI Training interface
void aiTraining_begin(Arduino_GFX* gfx, const String& filename);
TrainingResult aiTraining_poll();
bool aiTraining_saveModel(int modelSlot);  // 0-4 voor Model 1-5
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// AI Analyse events
enum AnalyzeEvent : uint8_t { 
  AE_NONE=0, 
  AE_BACK,          // Terug naar AI Overrule menu
  AE_SELECT_FILE,   // Selecteer bestand voor analyse
  AE_START_ANALYSIS,// Start analyse proces
  AE_NEXT_PAGE,     // Volgende pagina resultaten
  AE_PREV_PAGE,     // Vorige pagina resultaten
  AE_TRAIN          // Start AI training
};

// AI Analyse interface
void aiAnalyze_begin(TFT_eSPI* gfx);
AnalyzeEvent aiAnalyze_poll();
void aiAnalyze_showResults(const char* filename);
String aiAnalyze_getSelectedFile();
void aiAnalyze_setSelectedFile(const String& filename);

// Event data voor timeline
struct EventData {
  uint32_t timeMs;     // Tijd van gebeurtenis
  uint8_t eventType;   // Type gebeurtenis (0-7)
  float severity;      // Ernst van gebeurtenis (0.0-1.0)
};

// Analyse resultaten structuur
struct AnalysisResult {
  char filename[32];
  uint32_t totalSamples;
  uint32_t totalDurationMs;  // Totale opname duur
  float avgHeartRate;
  float avgTrustSpeed;
  float avgSleeveSpeed;
  float maxTrustSpeed;
  float maxSleeveSpeed;
  uint32_t highHREvents;
  uint32_t lowHREvents;
  uint32_t highTempEvents;
  uint32_t highGSREvents;
  float recommendedTrustReduction;
  float recommendedSleeveReduction;
  char analysis[256];  // Tekstuele analyse
  
  // Timeline gebeurtenissen
  EventData events[50];  // Max 50 gebeurtenissen
  uint8_t eventCount;
};

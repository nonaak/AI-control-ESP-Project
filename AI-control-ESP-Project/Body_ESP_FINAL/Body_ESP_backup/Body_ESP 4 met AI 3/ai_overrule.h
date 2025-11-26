#pragma once
#include <Arduino.h>

// AI Overrule systeem structuur
struct AIOverruleConfig {
  bool enabled = false;           // AI overrule aan/uit
  float hrLowThreshold = 60.0f;   // Lage hartslag threshold
  float hrHighThreshold = 140.0f; // Hoge hartslag threshold
  float tempHighThreshold = 37.5f;// Hoge temperatuur threshold
  float gsrHighThreshold = 500.0f;// Hoge GSR threshold
  float trustReduction = 0.8f;    // Trust reductie factor
  float sleeveReduction = 0.7f;   // Sleeve reductie factor
  float recoveryRate = 0.02f;     // Recovery rate per seconde
  uint32_t activationDelay = 5000; // Delay voordat overrule actief wordt (ms)
};

// Externe variabelen (gedefinieerd in Body_ESP.ino)
extern AIOverruleConfig aiConfig;
extern bool aiOverruleActive;
extern uint32_t lastAIUpdate;
extern float currentTrustOverride;
extern float currentSleeveOverride;
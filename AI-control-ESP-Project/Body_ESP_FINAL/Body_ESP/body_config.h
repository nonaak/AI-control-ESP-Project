#pragma once
#include <Arduino.h>
#include <math.h>
#include "config.h"  // Eenvoudige gebruikersinstellingen

// ---- Body Config struct + globale instantie ----
struct BodyConfig {
  // Display instellingen
  uint16_t COL_BG        = 0x0000;
  uint16_t COL_FRAME     = 0xF968;
  uint16_t COL_FRAME2    = 0xF81F;
  uint16_t COL_TAN       = 0xEA8E;

  // Sensor kleuren
  uint16_t COL_HEART     = 0xF800; // Rood voor hartslag
  uint16_t COL_TEMP      = 0x07E0; // Groen voor temperatuur
  uint16_t COL_GSR       = 0x001F; // Blauw voor GSR
  uint16_t COL_AI        = 0xF81F; // AI kleur (magenta)
  
  uint16_t DOT_RED       = 0xF800;
  uint16_t DOT_BLUE      = 0x001F;
  uint16_t DOT_GREEN     = 0x07E0;
  uint16_t DOT_GREY      = 0x8410;
  uint16_t DOT_ORANGE    = 0xFD20;

  uint16_t COL_BRAND     = 0xF81F;
  uint16_t COL_MENU_PINK = ((255 & 0xF8) << 8) | ((140 & 0xFC) << 3) | (220 >> 3);
  
  // Body sensor instellingen (uit config.h)
  float heartRateMin = HARTSLAG_MIN;
  float heartRateMax = HARTSLAG_MAX;
  float tempMin = TEMPERATUUR_MIN;
  float tempMax = TEMPERATUUR_MAX;
  float gsrMax = GSR_MAX;
  
  // Graph instellingen
  int heartRateHistorySize = 50;
  uint32_t sensorUpdateInterval = 1000; // ms
  
  // Vacuum grafiek instellingen (uit config.h)
  float vacuumGraphMaxMbar = VACUUM_MAX_MBAR;
  
  // === Animatie snelheid ===
  float    MIN_SPEED_HZ  = 0.22f;    // Laagste animatie frequentie (Hz) - langzaamste snelheid
  float    MAX_SPEED_HZ  = 3.00f;    // Hoogste animatie frequentie (Hz) - snelste snelheid
  uint8_t  SPEED_STEPS   = 8;        // Aantal snelheidsstappen (1-20) - meer = fijnere controle
  float    SNELH_SPEED_FACTOR = 1.43f; // SNELH grafiek animatie snelheid (hoger = snellere golf)
  
  // AI Overrule defaults (uit config.h)
  bool aiEnabled = AI_ENABLED;
  float hrLowThreshold = AI_HARTSLAG_LAAG;
  float hrHighThreshold = AI_HARTSLAG_HOOG;
  float tempHighThreshold = AI_TEMP_HOOG;
  float gsrHighThreshold = AI_GSR_HOOG;
  float trustReduction = AI_TRUST_REDUCTIE;
  float sleeveReduction = AI_SLEEVE_REDUCTIE;
  float recoveryRate = AI_HERSTEL_SNELHEID;
  
  // === ADVANCED STRESS MANAGEMENT SYSTEM ===
  
  // Stress Level Timings (uit config.h)
  uint32_t stressLevel0Minutes = STRESS_LVL0_MINUTEN;
  uint32_t stressLevel1Minutes = STRESS_LVL1_MINUTEN;
  uint32_t stressLevel2Minutes = STRESS_LVL2_MINUTEN;
  uint32_t stressLevel3Minutes = STRESS_LVL3_MINUTEN;
  uint32_t stressLevel4Seconds = STRESS_LVL4_SECONDEN;
  uint32_t stressLevel5Seconds = STRESS_LVL5_SECONDEN;
  uint32_t stressLevel6Seconds = STRESS_LVL6_SECONDEN;
  
  // Stress Change Detection (uit config.h)
  float stressChangeRustig = STRESS_CHANGE_RUSTIG;
  float stressChangeNormaal = STRESS_CHANGE_NORMAAL;
  float stressChangeSnel = STRESS_CHANGE_SNEL;
  float stressChangeHeelSnel = STRESS_CHANGE_HEEL_SNEL;
  
  // AI Integration Settings (uit config.h)
  bool mlStressEnabled = AI_STRESS_ENABLED;
  float mlConfidenceThreshold = AI_CONFIDENCE_THRESHOLD;
  uint32_t mlUpdateIntervalMs = AI_UPDATE_INTERVAL_MS;
  bool mlTrainingMode = AI_TRAINING_MODE;
  bool autoRecordSessions = AUTO_RECORD_SESSIONS;
  
  // AI Autonomie (uit config.h)
  float mlAutonomyLevel = AI_AUTONOMIE_LEVEL;
  float mlOverrideConfidenceThreshold = AI_OVERRIDE_CONFIDENCE;
  bool mlCanSkipLevels = AI_KAN_LEVELS_OVERSLAAN;
  bool mlCanIgnoreTimers = AI_KAN_TIMERS_NEGEREN;
  bool mlCanEmergencyOverride = AI_KAN_EMERGENCY_OVERRIDE;
  
  // AI Learning (uit config.h)
  float mlLearningRate = AI_LEER_SNELHEID;
  uint32_t mlMinSessionsBeforeAutonomy = AI_MIN_SESSIES_VOOR_AUTONOMIE;
  float mlUserFeedbackWeight = AI_USER_FEEDBACK_WEIGHT;
  
  // Stress Algorithm (uit config.h)
  float stressHistoryWeight = STRESS_HISTORY_WEIGHT;
  uint32_t stressHistoryWindowMs = STRESS_HISTORY_WINDOW_MS;
  float bioStressSensitivity = BIO_STRESS_SENSITIVITY;
  
  // MultiFunPlayer (uit config.h)
  String mfpPcIP = MFP_PC_IP;
  uint16_t mfpPort = MFP_PORT;
  String mfpPath = MFP_PATH;
  bool mfpAutoConnect = MFP_AUTO_CONNECT;
  uint32_t mfpReconnectInterval = MFP_RECONNECT_INTERVAL;
  bool mfpMLIntegration = MFP_AI_INTEGRATIE;
  
  // Animatie configuratie (uit config.h)
  float trustAnimFactor = TRUST_ANIM_SNELHEID;
  float trustAnimAmplitude = TRUST_ANIM_HOOGTE;
  float vibeAnimSpeed = VIBE_ANIM_SNELHEID;
  float gsrScaleFactor = GSR_SCHAAL_FACTOR;
  uint32_t sensorReadInterval = SENSOR_INTERVAL_MS;
};
extern BodyConfig BODY_CFG;

// ---- Kleur/ease helpers (inline, exact gelijk aan HoofdESP) ----
inline uint16_t RGB565u8(uint8_t r, uint8_t g, uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
inline void RGB565_to_u8(uint16_t c, uint8_t &r,uint8_t &g,uint8_t &b){
  r = (c>>8)&0xF8; r |= r>>5;
  g = (c>>3)&0xFC; g |= g>>6;
  b = (c<<3)&0xF8; b |= b>>5;
}
inline uint16_t u8_to_RGB565(uint8_t r,uint8_t g,uint8_t b){
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
inline uint16_t lerp_rgb565_u8(uint8_t r1,uint8_t g1,uint8_t b1,
                               uint8_t r2,uint8_t g2,uint8_t b2,float t){
  if(t<0) t=0; if(t>1) t=1;
  float rf=r1+(r2-r1)*t, gf=g1+(g2-g1)*t, bf=b1+(b2-b1)*t;
  return RGB565u8((uint8_t)roundf(rf),(uint8_t)roundf(gf),(uint8_t)roundf(bf));
}
inline float schlick_bias(float b, float x){
  return x / ((1.0f/b - 2.0f)*(1.0f - x) + 1.0f);
}
inline float schlick_gain(float g, float x){
  if (x < 0.5f) return 0.5f * schlick_bias(1.0f - g, 2.0f * x);
  else          return 1.0f - 0.5f * schlick_bias(1.0f - g, 2.0f - 2.0f * x);
}

// HSV/RGB helpers
inline void hsv2rgb_u8(float h, float s, float v, uint8_t &R, uint8_t &G, uint8_t &B){
  if (s<=0.0f){ uint8_t g=(uint8_t)roundf(v*255.0f); R=G=B=g; return; }
  h = fmodf(fabsf(h), 1.0f) * 6.0f;
  int i = (int)floorf(h);
  float f = h - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - s*f);
  float t = v * (1.0f - s*(1.0f-f));
  float rf, gf, bf;
  switch(i){
    case 0: rf=v; gf=t; bf=p; break;
    case 1: rf=q; gf=v; bf=p; break;
    case 2: rf=p; gf=v; bf=t; break;
    case 3: rf=p; gf=q; bf=v; break;
    case 4: rf=t; gf=p; bf=v; break;
    default: rf=v; gf=p; bf=q; break;
  }
  R=(uint8_t)roundf(rf*255.0f);
  G=(uint8_t)roundf(gf*255.0f);
  B=(uint8_t)roundf(bf*255.0f);
}
inline void rgb2hsv_u8(uint8_t R,uint8_t G,uint8_t B,float &h,float &s,float &v){
  float r=R/255.0f, g=G/255.0f, b=B/255.0f;
  float mx=fmaxf(r,fmaxf(g,b)), mn=fminf(r,fminf(g,b));
  float d = mx-mn;
  v = mx;
  s = (mx<=0.0f)?0.0f:(d/mx);
  if (d<=1e-6f){ h=0.0f; return; }
  if (mx==r)      h = fmodf(((g-b)/d),6.0f);
  else if(mx==g)  h = ((b-r)/d)+2.0f;
  else            h = ((r-g)/d)+4.0f;
  h/=6.0f; if(h<0) h+=1.0f;
}
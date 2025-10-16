#pragma once
#include <Arduino.h>
#include <math.h>

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
  
  // Body sensor instellingen
  float heartRateMin = 50.0f;
  float heartRateMax = 200.0f;
  float tempMin = 35.0f;
  float tempMax = 40.0f;
  float gsrMax = 1000.0f;
  
  // Graph instellingen
  int heartRateHistorySize = 50;
  uint32_t sensorUpdateInterval = 1000; // ms
  
  // Vacuum grafiek instellingen voor zuigen modus
  float vacuumGraphMaxMbar = 10.0f;  // Maximale mbar waarde voor vacuum grafiek (0-10 mbar bereik)
                                     // Deze waarde bepaalt de top van de grafiek schaal
                                     // 0 mbar = onderkant grafiek, vacuumGraphMaxMbar = bovenkant grafiek
                                     // Wijzig deze waarde om het vacuum bereik aan te passen
  
  // === Animatie snelheid ===
  float    MIN_SPEED_HZ  = 0.22f;    // Laagste animatie frequentie (Hz) - langzaamste snelheid
  float    MAX_SPEED_HZ  = 3.00f;    // Hoogste animatie frequentie (Hz) - snelste snelheid
  uint8_t  SPEED_STEPS   = 8;        // Aantal snelheidsstappen (1-20) - meer = fijnere controle
  float    SNELH_SPEED_FACTOR = 1.43f; // SNELH grafiek animatie snelheid (hoger = snellere golf)
  
  // AI Overrule defaults (van ai_overrule.h)
  bool aiEnabled = true;
  float hrLowThreshold = 60.0f;
  float hrHighThreshold = 140.0f;
  float tempHighThreshold = 38.5f;
  float gsrHighThreshold = 800.0f;
  float trustReduction = 0.3f;    // 30% reductie bij risico
  float sleeveReduction = 0.4f;   // 40% reductie bij risico
  float recoveryRate = 0.02f;     // 2% herstel per update
  
  // === ADVANCED STRESS MANAGEMENT SYSTEM ===
  
  // Stress Level Timings (configureerbaar)
  uint32_t stressLevel0Minutes = 5;    // Stress 0: wachttijd in minuten
  uint32_t stressLevel1Minutes = 3;    // Stress 1: wachttijd in minuten  
  uint32_t stressLevel2Minutes = 3;    // Stress 2: wachttijd in minuten
  uint32_t stressLevel3Minutes = 2;    // Stress 3: wachttijd in minuten
  uint32_t stressLevel4Seconds = 30;   // Stress 4: reactietijd in seconden
  uint32_t stressLevel5Seconds = 20;   // Stress 5: reactietijd in seconden  
  uint32_t stressLevel6Seconds = 15;   // Stress 6: reactietijd in seconden
  
  // Stress Change Detection Thresholds
  float stressChangeRustig = 0.3f;     // Rustige verandering per minuut
  float stressChangeNormaal = 0.8f;    // Normale verandering per minuut
  float stressChangeSnel = 1.5f;       // Snelle verandering per minuut
  float stressChangeHeelSnel = 3.0f;   // Heel snelle verandering per minuut
  
  // ML Integration Settings  
  bool mlStressEnabled = true;         // ML stress prediction aan/uit
  float mlConfidenceThreshold = 0.7f;  // Minimale ML confidence voor acties
  uint32_t mlUpdateIntervalMs = 5000;  // ML update interval in milliseconden
  bool mlTrainingMode = false;         // ML training data collection mode
  bool autoRecordSessions = true;      // Automatically record CSV files for ALL sessions
  
  // ===== ML AUTONOMY CONFIGURATION =====
  float mlAutonomyLevel = 0.3f;        // 0.0-1.0: Hoeveel vrijheid ML krijgt (30% default)
  float mlOverrideConfidenceThreshold = 0.50f; // ML confidence drempel voor rule override (lager = makkelijker override)
  bool mlCanSkipLevels = true;         // ML mag stress levels overslaan
  bool mlCanIgnoreTimers = true;       // ML mag timers negeren
  bool mlCanEmergencyOverride = true;  // ML mag emergency beslissingen nemen
  
  // ML Learning & Adaptation
  float mlLearningRate = 0.1f;         // Hoe snel ML zich aanpast (0.0-1.0)
  uint32_t mlMinSessionsBeforeAutonomy = 0; // Minimum sessies voordat autonomie actief wordt (0 = direct actief)
  float mlUserFeedbackWeight = 0.8f;   // Gewicht van gebruiker feedback in learning (0.0-1.0)
  
  // Stress Algorithm Parameters
  float stressHistoryWeight = 0.3f;    // Gewicht van stress historie (0.0-1.0)
  uint32_t stressHistoryWindowMs = 60000; // Stress historie venster (1 minuut)
  float bioStressSensitivity = 1.0f;   // Gevoeligheid biometrische stress detectie
  
  // ===== MULTIFUNPLAYER CONFIGURATIE =====
  // Pas deze waarden aan voor jouw setup:
  String mfpPcIP = "192.168.2.3";     // ← VERVANG MET JE PC IP ADRES (cmd → ipconfig)
  uint16_t mfpPort = 8080;              // ← MULTIFUNPLAYER WEBSOCKET POORT
  String mfpPath = "/";                 // ← WEBSOCKET PATH (meestal "/")
  bool mfpAutoConnect = true;           // ← AUTOMATISCH VERBINDEN BIJ OPSTARTEN
  uint32_t mfpReconnectInterval = 5000; // ← HERVERBIND INTERVAL (5 seconden)
  bool mfpMLIntegration = true;         // ← ML INTEGRATIE VOOR FUNSCRIPTS
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
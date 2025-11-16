#pragma once
#include <Arduino.h>
#include <math.h>

// ---- Config struct + globale instantie ----
struct Config {
  // === Boot/startup gedrag ===
  bool     BOOT_TO_MENU  = true;     // Start in menu mode (false = direct animatie)
  bool     START_PAUSED  = true;     // Start gepauzeerd (false = direct running)

  // === Animatie snelheid ===
  float    MIN_SPEED_HZ  = 0.22f;    // Laagste animatie frequentie (Hz) - langzaamste snelheid
  float    MAX_SPEED_HZ  = 1.17f;    // Hoogste animatie frequentie (Hz) - snelste snelheid
  uint8_t  SPEED_STEPS   = 7;        // Aantal snelheidsstappen (1-20) - meer = fijnere controle
  float    ANIM_SPEED_FACTOR = 1.0f; // Animatie snelheid factor (hoger = snellere animatie)

  // === Animatie smoothing ===
  float    easeGain      = 0.35f;    // Ease curve sterkte (0.0-1.0) - hoger = meer curve
  float    velEMAalpha   = 0.15f;    // Snelheid smoothing (0.01-0.5) - lager = vloeiender

  // === Display kleuren (RGB565 format) ===
  uint16_t COL_BG        = 0x0000;   // Achtergrond kleur (zwart)
  uint16_t COL_FRAME     = 0xF968;   // Frame rand kleur (licht paars)
  uint16_t COL_FRAME2    = 0xF81F;   // Frame accent kleur (magenta)
  uint16_t COL_TAN       = 0xEA8E;   // Sleeve/huid kleur (beige)

  // === Rod kleuren (RGB888 format) ===
  uint8_t  rodSlowR=255, rodSlowG=120, rodSlowB=190;  // Rod kleur bij lage snelheid (licht paars)
  uint8_t  rodFastR=255, rodFastG= 50, rodFastB=140;  // Rod kleur bij hoge snelheid (donker paars)

  // === UI element kleuren ===
  uint16_t COL_ARROW     = 0xF81F;   // Pijl kleur (magenta)
  uint16_t COL_ARROW_GLOW= 0x780F;   // Pijl glow effect (donker magenta)
  uint16_t DOT_RED       = 0xF800;   // Status dot rood
  uint16_t DOT_BLUE      = 0x001F;   // Status dot blauw  
  uint16_t DOT_GREEN     = 0x07E0;   // Status dot groen
  uint16_t DOT_GREY      = 0x8410;   // Status dot grijs

  uint16_t SPEEDBAR_BORDER = 0x07FF; // Snelheids balk rand (cyaan)
  uint16_t COL_BRAND     = 0xF81F;   // Brand/logo kleur (magenta)

  uint16_t COL_MENU_PINK = ((255 & 0xF8) << 8) | ((140 & 0xFC) << 3) | (220 >> 3); // Menu tekst kleur (roze)

  // === Legacy vacuum (gebruik vacuumTargetMbar) ===
  float vacTarget = -30.0f;        // DEPRECATED - gebruik vacuumTargetMbar
  
  // === Vacuum/zuigen instellingen ===
  float vacuumTargetMbar = -30.0f; // Vacuum doeldruk in mbar (negatief = zuigen, -10 tot -100)
  float vacuumHoldTime = 600.0f;   // Max vacuum tijd in seconden (veiligheid, 60-3600)
  bool  vacuumAutoMode = true;     // Auto vacuum volgt animatie (true/false)
  
  // === Motion feedback van M5Atom ===
  bool  motionBlendEnabled = true; // M5Atom movement feedback aan/uit
  float userSpeedWeight = 90.0f;   // Nunchuk invloed op snelheid (0-100%)
  float motionSpeedWeight = 10.0f; // M5Atom invloed op snelheid (0-100%)
  bool  motionDirectionSync = true;// Animatie richting volgt M5Atom beweging
  
  // === Auto vacuum gedrag ===
  uint8_t autoVacuumSpeedThreshold = 6; // Stop auto vacuum bij snelheid >= X (1-10)
  
  // === Z-knop nunchuk instellingen ===
  uint16_t doubleClickTiming = 800;     // Tijd tussen clicks voor Vibe (300-1500ms)
};
extern Config CFG;

// ---- Defaults (snapshot) ----
extern const uint16_t DEF_COL_BG;
extern const uint16_t DEF_COL_FRAME;
extern const uint16_t DEF_COL_FRAME2;
extern const uint16_t DEF_COL_TAN;
extern const uint8_t  DEF_rodSlowR, DEF_rodSlowG, DEF_rodSlowB;
extern const uint8_t  DEF_rodFastR, DEF_rodFastG, DEF_rodFastB;
extern const uint16_t DEF_COL_ARROW;
extern const uint16_t DEF_COL_GLOW;
extern const uint16_t DEF_DOT_RED, DEF_DOT_BLUE, DEF_DOT_GREEN, DEF_DOT_GREY;
extern const uint16_t DEF_SPEED_BORDER;
extern const uint16_t DEF_COL_BRAND;
extern const uint16_t DEF_MENU_PINK;

// ---- Kleur/ease helpers (inline, exact gelijk) ----
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

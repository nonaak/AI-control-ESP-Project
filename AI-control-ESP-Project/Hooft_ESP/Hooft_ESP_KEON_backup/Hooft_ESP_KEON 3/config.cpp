#include "config.h"

// Globale config-instantie
Config CFG = {
  .vacTarget = -30.0f  // ← nieuw veld voor zuigen
};

// Defaults exact gelijk aan monolith
const uint16_t DEF_COL_BG       = 0x0000;
const uint16_t DEF_COL_FRAME    = 0xF968;
const uint16_t DEF_COL_FRAME2   = 0xF81F;
const uint16_t DEF_COL_TAN      = 0xEA8E;
const uint8_t  DEF_rodSlowR     = 255, DEF_rodSlowG=120, DEF_rodSlowB=190;
const uint8_t  DEF_rodFastR     = 255, DEF_rodFastG= 50, DEF_rodFastB=140;
const uint16_t DEF_COL_ARROW    = 0xF81F;
const uint16_t DEF_COL_GLOW     = 0x780F;
const uint16_t DEF_DOT_RED      = 0xF800;
const uint16_t DEF_DOT_BLUE     = 0x001F;
const uint16_t DEF_DOT_GREEN    = 0x07E0;
const uint16_t DEF_DOT_GREY     = 0x8410;
const uint16_t DEF_SPEED_BORDER = 0x07FF;
const uint16_t DEF_COL_BRAND    = 0xF81F;
const uint16_t DEF_MENU_PINK    = ((255 & 0xF8) << 8) | ((140 & 0xFC) << 3) | (220 >> 3);

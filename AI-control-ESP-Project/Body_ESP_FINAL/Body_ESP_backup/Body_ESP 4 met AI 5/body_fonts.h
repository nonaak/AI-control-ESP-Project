#pragma once

// Fonts & keuzes (gelijk aan HoofdESP)
//#define USE_ADAFRUIT_FONTS 0
#define USE_ADAFRUIT_FONTS 1
#define USE_EXOTIC_FONT    1    // ← DEZE OP 0 ZETTEN!

#if USE_ADAFRUIT_FONTS
  #if USE_EXOTIC_FONT
    #include "Fonts/FreeSerifItalic18pt7b.h"   // titel
    #include "Fonts/FreeSerifItalic9pt7b.h"    // items (kleiner)
    #define FONT_TITLE  FreeSerifItalic18pt7b
    #define FONT_ITEM   FreeSerifItalic9pt7b
  #else
    #include "Fonts/FreeSans9pt7b.h"
    #include "Fonts/FreeSans12pt7b.h"
    #define FONT_TITLE  FreeSans9pt7b
    #define FONT_ITEM   FreeSans9pt7b
  #endif
  // Kleinere font voor knoppen
  #include "Fonts/FreeSansBold9pt7b.h"
  #define FONT_BUTTON FreeSansBold9pt7b
#endif

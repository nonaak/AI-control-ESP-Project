#pragma once

// Fonts & keuzes (ongewijzigd)
#define USE_ADAFRUIT_FONTS 1
#define USE_EXOTIC_FONT    1

#if USE_ADAFRUIT_FONTS
  #if USE_EXOTIC_FONT
    #include <Fonts/FreeSerifItalic18pt7b.h>   // titel
    #include <Fonts/FreeSerifItalic9pt7b.h>    // items (kleiner)
    #define FONT_TITLE  FreeSerifItalic18pt7b
    #define FONT_ITEM   FreeSerifItalic9pt7b
  #else
    #include <Fonts/FreeSans12pt7b.h>
    #include <Fonts/FreeSans9pt7b.h>
    #define FONT_TITLE  FreeSans12pt7b
    #define FONT_ITEM   FreeSans9pt7b
  #endif
#endif

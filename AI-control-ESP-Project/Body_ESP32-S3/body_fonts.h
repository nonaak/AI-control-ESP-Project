#pragma once

// T-HMI Fonts (TFT_eSPI gebruikt andere font API)
// Voor nu gebruiken we standaard TFT_eSPI fonts
#define USE_ADAFRUIT_FONTS 0  // Uitgeschakeld voor TFT_eSPI
#define USE_EXOTIC_FONT    0  // Uitgeschakeld voor TFT_eSPI

// TFT_eSPI font defines (standaard fonts)
#define FONT_TITLE  nullptr  // TFT_eSPI standaard font
#define FONT_ITEM   nullptr  // TFT_eSPI standaard font

// Als je later Adafruit fonts wilt gebruiken in TFT_eSPI:
// 1. Kopieer .h files naar je project folder
// 2. Include ze hier direct
// 3. TFT_eSPI kan Adafruit GFX fonts gebruiken met setFreeFont()

// Voorbeeld voor toekomstig gebruik:
// #include "FreeSerifItalic18pt7b.h"  // lokale kopie
// #define FONT_TITLE  &FreeSerifItalic18pt7b

#pragma once
#include <Arduino.h>

/* ========= Touch inschakelen ========= */
#ifndef ENABLE_TOUCH
#define ENABLE_TOUCH 1
#endif

// ========= FT5x06 (SC01 Plus) op I2C =========
// SC01 Plus gebruikt FT5x06 touch controller via I2C
#if ENABLE_TOUCH
#ifndef TOUCH_SDA
#define TOUCH_SDA  6   // GPIO 6 = I2C SDA
#endif
#ifndef TOUCH_SCL
#define TOUCH_SCL  5   // GPIO 5 = I2C SCL
#endif
#ifndef TOUCH_INT
#define TOUCH_INT  7   // GPIO 7 = Touch interrupt
#endif
#ifndef TOUCH_RST
#define TOUCH_RST  -1  // Geen reset pin
#endif
#endif // ENABLE_TOUCH

// ========= XPT2046 (CYD) op HSPI - OUDE CODE =========
// Dit zijn de standaardpinnen voor ESP32-2432S028(R) boards
// #if ENABLE_TOUCH
// #ifndef TOUCH_CS
// #define TOUCH_CS   33
// #endif
// #ifndef TOUCH_IRQ
// #define TOUCH_IRQ  36
// #endif
// #ifndef TOUCH_SCK
// #define TOUCH_SCK  25
// #endif
// #ifndef TOUCH_MOSI
// #define TOUCH_MOSI 32
// #endif
// #ifndef TOUCH_MISO
// #define TOUCH_MISO 39
// #endif
// #endif // ENABLE_TOUCH

/* ========= Kalibratie / oriëntatie =========
   FT6X36 geeft directe pixel coordinaten
   SC01 Plus: 480x320 display (landscape, rotation 1)
   Touch natuurlijk: 320x480 (portrait)
   
   Als tikken gespiegeld/gdraaid lijken:
   - SWAP_XY  : 0/1 — wissel x en y om
   - FLIP_X   : 0/1 — spiegel horizontaal
   - FLIP_Y   : 0/1 — spiegel verticaal
   
   TESTED & WORKING!
   Manual test resultaat: SWAP_XY=1, FLIP_X=0, FLIP_Y=1
   = Perfect voor 90° landscape (USB rechts)
*/
#ifndef SWAP_XY
#define SWAP_XY 1  // Wissel X en Y (90° rotatie) - WORKING
#endif
#ifndef FLIP_X
#define FLIP_X  0  // GEEN X flip - WORKING
#endif
#ifndef FLIP_Y
#define FLIP_Y  1  // Y flip - WORKING
#endif

// ========= OUDE XPT2046 mapping - NIET MEER NODIG =========
// /* Ruwe XPT2046-waarden (zoals jouw werkende code) */
// #ifndef TOUCH_MAP_X1
// #define TOUCH_MAP_X1 4000
// #endif
// #ifndef TOUCH_MAP_X2
// #define TOUCH_MAP_X2 100
// #endif
// #ifndef TOUCH_MAP_Y1
// #define TOUCH_MAP_Y1 100
// #endif
// #ifndef TOUCH_MAP_Y2
// #define TOUCH_MAP_Y2 4000
// #endif

/* ======== Forward display type ======== */
class Arduino_GFX;

/* ======== Bottom-bar events ======== */
enum TouchEventKind : uint8_t { TE_NONE=0, TE_TAP_REC=1, TE_TAP_PLAY=2, TE_TAP_MENU=3, TE_TAP_OVERRULE=4 };
struct TouchEvent { TouchEventKind kind; int16_t x; int16_t y; };

/* ======== API ======== */
void inputTouchBegin(Arduino_GFX* gfx);
void inputTouchSetRotated(bool rotated180);     // zet touch 180° rotatie aan/uit
bool inputTouchRead(int16_t &sx, int16_t &sy);  // raw → schermcoord
bool inputTouchPoll(TouchEvent &ev);            // herkent onder-knoppen

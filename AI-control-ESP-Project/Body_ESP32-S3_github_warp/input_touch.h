#ifndef INPUT_TOUCH_H
#define INPUT_TOUCH_H

#include <TFT_eSPI.h>

/* ========= Touch inschakelen ========= */
#ifndef ENABLE_TOUCH
#define ENABLE_TOUCH 1
#endif

/* ======== Touch events ======== */
enum TouchEventType : uint8_t { 
  TE_NONE=0, 
  TE_TAP_REC, 
  TE_TAP_PLAY, 
  TE_TAP_MENU, 
  TE_TAP_OVERRULE
};

struct TouchEvent { 
  TouchEventType kind;  // Was 'type', nu 'kind' voor backwards compatibility
  int16_t x; 
  int16_t y; 
};

/* ======== API ======== */
void inputTouchBegin(TFT_eSPI* gfx);
void inputTouchSetRotated(bool rotated180);
bool inputTouchRead(int16_t &sx, int16_t &sy);
bool inputTouchReadRaw(int16_t &rawX, int16_t &rawY);
bool inputTouchPoll(TouchEvent &ev);
void inputTouchResetCooldown();

/**
 * Corrigeer touch coordinaten voor view-specifieke mapping problemen
 * Roep deze functie aan in elke view na inputTouchRead() en voor button checks
 */
void inputTouchCorrectForViews(int16_t &x, int16_t &y);

#endif
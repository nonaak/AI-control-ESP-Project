#pragma once
#include <Arduino.h>

// CYD on-board RGB (discreet) – active-LOW
#ifndef PIN_LED_R
#define PIN_LED_R 4
#endif
#ifndef PIN_LED_G
#define PIN_LED_G 16
#endif
#ifndef PIN_LED_B
#define PIN_LED_B 17
#endif

// Sommige varianten hebben andere polariteit, maar CYD is meestal active-LOW
#ifndef RGB_ACTIVE_LOW
#define RGB_ACTIVE_LOW 1
#endif

// Zet de LED gegarandeerd uit
inline void rgbOffInit(){
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
#if RGB_ACTIVE_LOW
  digitalWrite(PIN_LED_R, HIGH);
  digitalWrite(PIN_LED_G, HIGH);
  digitalWrite(PIN_LED_B, HIGH);
#else
  digitalWrite(PIN_LED_R, LOW);
  digitalWrite(PIN_LED_G, LOW);
  digitalWrite(PIN_LED_B, LOW);
#endif
}

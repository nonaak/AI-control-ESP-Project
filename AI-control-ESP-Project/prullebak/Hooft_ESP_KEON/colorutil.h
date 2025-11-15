
#pragma once
#include <stdint.h>

static inline uint16_t u8_to_RGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline void rgb2hsv_u8(uint8_t r, uint8_t g, uint8_t b, uint8_t &h, uint8_t &s, uint8_t &v) {
  h = 0; s = 0; v = r > g ? (r > b ? r : b) : (g > b ? g : b);
}
static inline void hsv2rgb_u8(uint8_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
  r = v; g = v; b = v;
}

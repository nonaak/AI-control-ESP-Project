#include "config.h"
#if ENABLE_MCP9808

#include <Wire.h>
#include "mcp9808.h"

static const uint8_t MCP9808_ADDR = 0x1F;  // Correct I2C adres
bool  mcpPresent = false;
float mcpC_raw   = NAN;
float mcpC_cal   = NAN;
float mcpC_smooth= NAN;
static uint32_t mcpLast  = 0;
static const uint32_t MCP_PERIOD_MS = 500;

static bool mcp9808_readC(float &outC) {
  Wire.beginTransmission(MCP9808_ADDR);
  Wire.write(0x05);
  if (Wire.endTransmission(false) != 0) return false; // repeated start
  if (Wire.requestFrom((int)MCP9808_ADDR, 2) != 2) return false;
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();
  int16_t t13 = ((msb & 0x1F) << 8) | lsb;
  bool neg = msb & 0x10;
  if (neg) t13 = t13 - 8192;
  outC = t13 * 0.0625f;
  return true;
}

void mcp9808_tryInitOnce() {
  if (mcpPresent) return;
  Wire.beginTransmission(MCP9808_ADDR);
  if (Wire.endTransmission() == 0) mcpPresent = true;
}

void mcp9808_tick() {
  if (!mcpPresent) return;
  uint32_t now = millis();
  if (now - mcpLast < MCP_PERIOD_MS) return;
  mcpLast = now;

  float c;
  if (mcp9808_readC(c)) {
    mcpC_raw = c;
    mcpC_cal = c + MCP9808_CAL_OFFSET_C;
    if (isnan(mcpC_smooth)) mcpC_smooth = mcpC_cal;
    mcpC_smooth = MCP9808_SMOOTH_A * mcpC_cal + (1.0f - MCP9808_SMOOTH_A) * mcpC_smooth;
  }
}
#endif

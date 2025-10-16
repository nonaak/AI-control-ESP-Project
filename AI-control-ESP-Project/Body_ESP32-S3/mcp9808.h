#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_MCP9808
extern bool  mcpPresent;
extern float mcpC_raw;
extern float mcpC_smooth;

void mcp9808_tryInitOnce();
void mcp9808_tick();
#else
// Stubs bij uitgeschakeld MCP9808 zodat hoofdcode ongewijzigd kan blijven
static inline void mcp9808_tryInitOnce() {}
static inline void mcp9808_tick()       {}
#endif

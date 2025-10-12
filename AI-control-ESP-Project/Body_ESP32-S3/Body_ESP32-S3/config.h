#pragma once

/* ========= UITBREIDINGSSCHAKELAAR ========= */
#define ENABLE_MCP9808  0   // 0 = volledig uit (geen I2C reads, geen UI); 1 = aan

/* ========= MCP9808 KALIBRATIE (voor later menu) =========
   - MCP9808_CAL_OFFSET_C: offset die bij de gemeten temperatuur wordt opgeteld
   - MCP9808_SMOOTH_A: smoothing factor (0..1), hogere waarde = trager
*/
#define MCP9808_CAL_OFFSET_C  0.00f
#define MCP9808_SMOOTH_A      0.2f

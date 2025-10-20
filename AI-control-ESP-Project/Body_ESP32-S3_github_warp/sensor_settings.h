#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Sensor settings events
enum SensorEvent : uint8_t { 
  SE_NONE=0, 
  SE_BACK,          // Terug naar menu
  SE_SAVE,          // Settings opslaan
  SE_RESET          // Reset naar defaults
};

// Sensor settings interface
void sensorSettings_begin(TFT_eSPI* gfx);
void sensorSettings_reset();  // Reset to page 0 when first opening
SensorEvent sensorSettings_poll();

// Settings structure (opgeslagen in EEPROM)
struct SensorConfig {
  // MAX30102 Heartrate settings
  float hrThreshold = 50000.0f;      // Beat detection threshold
  uint8_t hrLedPower = 0x2F;         // LED power (0x00-0xFF)
  
  // MCP9808 Temperature settings  
  float tempOffset = 0.0f;           // Temperature offset in °C
  float tempSmoothing = 0.2f;        // Smoothing factor (0.0-1.0)
  
  // GSR (Skin conductance) settings
  float gsrBaseline = 512.0f;        // Baseline reading
  float gsrSensitivity = 1.0f;       // Sensitivity multiplier
  float gsrSmoothing = 0.1f;         // GSR smoothing factor (0.0-1.0)
  
  // ESP32 Communication settings
  uint16_t commBaudrate = 115200;    // Serial baud rate
  uint8_t commTimeout = 100;         // Communication timeout (ms)
  
  // Validation
  uint32_t magic = 0xDEADBEEF;       // Magic number for validation
};

// Settings functions
void loadSensorConfig();              // Laad settings uit EEPROM
void saveSensorConfig();              // Sla settings op in EEPROM  
void resetSensorConfig();             // Reset naar defaults
extern SensorConfig sensorConfig;     // Global config
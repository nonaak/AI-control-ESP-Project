/*
  Factory Reset System for Body ESP
  
  Provides complete reset functionality to clear all settings,
  ML data, and EEPROM when preparing the final device
*/

#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

#include <Arduino.h>
#include <Wire.h>
#include "body_config.h"
#include "advanced_stress_manager.h"

// ===== Configuration =====
#define FACTORY_RESET_PIN         0     // GPIO 0 (Boot button)
#define FACTORY_RESET_HOLD_TIME   3000  // Hold for 3 seconds
#define ML_EEPROM_ADDR           0x57   // DS3231 EEPROM address
#define ML_EEPROM_SIZE           32768  // 32KB EEPROM size

// ===== Reset Types =====
enum ResetType {
  RESET_NONE = 0,
  RESET_SETTINGS_ONLY,    // Only reset configuration to defaults
  RESET_ML_DATA_ONLY,     // Only clear ML training data and models
  RESET_EEPROM_ONLY,      // Only clear EEPROM data
  RESET_COMPLETE          // Complete factory reset (everything)
};

// ===== Factory Reset Class =====
class FactoryReset {
private:
  bool resetInProgress;
  uint32_t buttonPressStart;
  bool buttonPressed;
  
  // Private methods
  void resetSettings();
  void resetMLData();
  void resetEEPROM();
  void showResetProgress(int percentage, const String& status);
  
public:
  // Constructor
  FactoryReset();
  
  // Initialization
  void begin();
  
  // Main functions
  void checkResetButton();           // Call in main loop
  bool isResetInProgress() const { return resetInProgress; }
  
  // Reset functions
  void performReset(ResetType type = RESET_COMPLETE);
  void performQuickReset();          // Fast reset without EEPROM wipe
  
  // Status
  void printResetStatus();
  
  // Recovery functions
  void createBackup();               // Create backup before reset
  void restoreFromBackup();          // Restore from backup
};

// ===== Global Instance =====
extern FactoryReset factoryReset;

// ===== Convenience Functions =====
void checkFactoryReset();             // Call in setup()
void factoryResetLoop();              // Call in main loop

#endif // FACTORY_RESET_H
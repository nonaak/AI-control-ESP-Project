#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Persistent settings functions
void loadLubeSettings();
void saveLubeSettings();

// Complete configuration settings functions
void loadAllSettings();
void saveAllSettings();
void resetAllSettingsToDefault();
void testFlashStorage(); // Test function for flash diagnostics

#include "settings.h"
#include "config.h"
#include "ui.h"
#include <Preferences.h>

// Test function to verify flash storage works
void testFlashStorage() {
  Serial.println("[FLASH-TEST] Starting flash storage test...");
  Serial.flush(); // Ensure message is sent
  
  try {
    Preferences testPrefs;
    Serial.println("[FLASH-TEST] Created Preferences object");
    Serial.flush();
    
    bool openResult = testPrefs.begin("test", false);
    Serial.printf("[FLASH-TEST] begin() result: %s\n", openResult ? "SUCCESS" : "FAILED");
    Serial.flush();
    
    if (!openResult) {
      Serial.println("[FLASH-TEST] FAILED: Cannot open test namespace");
      return;
    }
    
    // Write test value
    float testValue = -42.5f;
    Serial.printf("[FLASH-TEST] About to write value: %.1f\n", testValue);
    Serial.flush();
    
    size_t written = testPrefs.putFloat("testFloat", testValue);
    Serial.printf("[FLASH-TEST] Wrote %.1f, bytes written: %u\n", testValue, written);
    Serial.flush();
    
    // Read it back immediately
    Serial.println("[FLASH-TEST] About to read value back...");
    Serial.flush();
    
    float readValue = testPrefs.getFloat("testFloat", -999.0f);
    Serial.printf("[FLASH-TEST] Read back: %.1f\n", readValue);
    Serial.flush();
    
    if (readValue == testValue) {
      Serial.println("[FLASH-TEST] SUCCESS: Flash read/write working correctly");
    } else {
      Serial.printf("[FLASH-TEST] FAILED: Values don't match! Expected %.1f, got %.1f\n", testValue, readValue);
    }
    
    Serial.println("[FLASH-TEST] Cleaning up...");
    Serial.flush();
    
    testPrefs.end();
    
    // Clean up - open again and clear
    if (testPrefs.begin("test", false)) {
      testPrefs.clear();
      testPrefs.end();
      Serial.println("[FLASH-TEST] Cleanup completed");
    } else {
      Serial.println("[FLASH-TEST] WARNING: Could not clean up test data");
    }
    
  } catch (...) {
    Serial.println("[FLASH-TEST] EXCEPTION CAUGHT during flash test!");
  }
  
  Serial.println("[FLASH-TEST] Test function completed");
  Serial.flush();
}
// External references to settings variables (declared in ui.cpp)
extern uint16_t g_targetStrokes;
extern float g_lubeHold_s;
extern float g_startLube_s;

// Preferences object
static Preferences prefs;

// Load lube settings from flash
void loadLubeSettings() {
  prefs.begin("lube_settings", true);
  g_targetStrokes = prefs.getUInt("targetStrokes", 30);      // Default 30
  g_lubeHold_s = prefs.getFloat("lubeHold", 0.5f);          // Default 0.5s
  g_startLube_s = prefs.getFloat("startLube", 0.5f);        // Default 0.5s
  prefs.end();
  Serial.printf("[SETTINGS] Loaded: pushes=%u, lube=%.1fs, start=%.1fs\n", 
                (unsigned)g_targetStrokes, g_lubeHold_s, g_startLube_s);
}

// Save lube settings to flash
void saveLubeSettings() {
  prefs.begin("lube_settings", false);
  prefs.putUInt("targetStrokes", g_targetStrokes);
  prefs.putFloat("lubeHold", g_lubeHold_s);
  prefs.putFloat("startLube", g_startLube_s);
  prefs.end();
  Serial.printf("[SETTINGS] Saved: pushes=%u, lube=%.1fs, start=%.1fs\n", 
                (unsigned)g_targetStrokes, g_lubeHold_s, g_startLube_s);
}

// Load all configuration settings from flash
void loadAllSettings() {
  Serial.println("[FLASH] Starting loadAllSettings...");
  
  if (!prefs.begin("config", true)) { // Read-only mode
    Serial.println("[FLASH] ERROR: Failed to open Preferences for reading!");
    Serial.println("[FLASH] Using default values");
    return;
  }
  Serial.println("[FLASH] Preferences opened successfully for reading");
  
  // Basic settings
  CFG.BOOT_TO_MENU = prefs.getBool("bootToMenu", true);
  CFG.START_PAUSED = prefs.getBool("startPaused", true);
  
  // Speed settings
  CFG.MIN_SPEED_HZ = prefs.getFloat("minSpeedHz", 0.22f);
  CFG.MAX_SPEED_HZ = prefs.getFloat("maxSpeedHz", 3.00f);
  CFG.SPEED_STEPS = prefs.getUChar("speedSteps", 8);
  
  // Animation settings
  CFG.easeGain = prefs.getFloat("easeGain", 0.35f);
  CFG.velEMAalpha = prefs.getFloat("velEMAalpha", 0.15f);
  
  // Color settings - use same short keys as save
  CFG.COL_BG = prefs.getUShort("bg", 0x0000);
  CFG.COL_FRAME = prefs.getUShort("fr", 0xF968);
  CFG.COL_FRAME2 = prefs.getUShort("fr2", 0xF81F);
  CFG.COL_TAN = prefs.getUShort("tan", 0xEA8E);
  
  // Rod colors - compact keys
  CFG.rodSlowR = prefs.getUChar("rsr", 255);
  CFG.rodSlowG = prefs.getUChar("rsg", 120);
  CFG.rodSlowB = prefs.getUChar("rsb", 190);
  CFG.rodFastR = prefs.getUChar("rfr", 255);
  CFG.rodFastG = prefs.getUChar("rfg", 50);
  CFG.rodFastB = prefs.getUChar("rfb", 140);
  
  // UI colors - compact keys
  CFG.COL_ARROW = prefs.getUShort("arr", 0xF81F);
  CFG.COL_ARROW_GLOW = prefs.getUShort("glow", 0x780F);
  CFG.DOT_RED = prefs.getUShort("dr", 0xF800);
  CFG.DOT_BLUE = prefs.getUShort("db", 0x001F);
  CFG.DOT_GREEN = prefs.getUShort("dg", 0x07E0);
  CFG.DOT_GREY = prefs.getUShort("dgy", 0x8410);
  CFG.SPEEDBAR_BORDER = prefs.getUShort("sb", 0x07FF);
  CFG.COL_BRAND = prefs.getUShort("brand", 0xF81F);
  CFG.COL_MENU_PINK = prefs.getUShort("pink", ((255 & 0xF8) << 8) | ((140 & 0xFC) << 3) | (220 >> 3));
  
  // Vacuum settings
  float storedValue = prefs.getFloat("vacTarget2", -999.0f); // Use shorter key name to avoid conflicts
  if (storedValue == -999.0f) {
    Serial.println("[FLASH] vacuumTargetMbar key NOT FOUND in flash, using default -30.0");
    CFG.vacuumTargetMbar = -30.0f;
  } else {
    CFG.vacuumTargetMbar = storedValue;
    Serial.printf("[FLASH] vacuumTargetMbar found in flash: %.1f\n", storedValue);
  }
  
  CFG.vacTarget = prefs.getFloat("vacTarget", -30.0f);
  CFG.vacuumHoldTime = prefs.getFloat("vacuumHoldTime", 2.0f);
  CFG.vacuumAutoMode = prefs.getBool("vacuumAutoMode", true);
  
  Serial.printf("[SETTINGS] LOADED vacuumTargetMbar: %.1f mbar from flash\n", CFG.vacuumTargetMbar);
  
  // Store the loaded value to verify it doesn't get corrupted
  float loadedVacuumValue = CFG.vacuumTargetMbar;
  
  // Motion blend settings
  CFG.motionBlendEnabled = prefs.getBool("motionBlendEnabled", true);
  CFG.userSpeedWeight = prefs.getFloat("userSpeedWeight", 90.0f);
  CFG.motionSpeedWeight = prefs.getFloat("motionSpeedWeight", 10.0f);
  CFG.motionDirectionSync = prefs.getBool("motionDirectionSync", true);
  
  // Auto Vacuum settings
  CFG.autoVacuumSpeedThreshold = prefs.getUChar("autoVacSpeedThr", 6);
  
  prefs.end();
  
  // Also load lube settings
  loadLubeSettings();
  
  Serial.println("[SETTINGS] All configuration loaded from flash");
  
  // Verify vacuum value wasn't corrupted during loading process
  if (CFG.vacuumTargetMbar != loadedVacuumValue) {
    Serial.printf("[MEMORY] WARNING: vacuumTargetMbar corrupted! Was %.1f, now %.1f\n", loadedVacuumValue, CFG.vacuumTargetMbar);
  } else {
    Serial.printf("[MEMORY] vacuumTargetMbar integrity OK: %.1f\n", CFG.vacuumTargetMbar);
  }
  
  Serial.printf("[CONFIG] Speed: %.2f-%.2f Hz, Steps: %d\n", CFG.MIN_SPEED_HZ, CFG.MAX_SPEED_HZ, CFG.SPEED_STEPS);
  Serial.printf("[CONFIG] Vacuum: %.0f mbar, Auto: %s, Speed Threshold: %d\n", abs(CFG.vacuumTargetMbar), CFG.vacuumAutoMode ? "ON" : "OFF", CFG.autoVacuumSpeedThreshold);
  Serial.printf("[CONFIG] Motion Blend: %s (User: %.0f%%, Motion: %.0f%%)\n", 
                CFG.motionBlendEnabled ? "ON" : "OFF", CFG.userSpeedWeight, CFG.motionSpeedWeight);
}

// Save all configuration settings to flash
void saveAllSettings() {
  Serial.printf("[FLASH] Starting saveAllSettings, vacuumTargetMbar=%.1f\n", CFG.vacuumTargetMbar);
  
  if (!prefs.begin("config", false)) { // Write mode
    Serial.println("[FLASH] ERROR: Failed to open Preferences for writing!");
    return;
  }
  Serial.println("[FLASH] Preferences opened successfully for writing");
  
  // Basic settings
  prefs.putBool("bootToMenu", CFG.BOOT_TO_MENU);
  prefs.putBool("startPaused", CFG.START_PAUSED);
  
  // Speed settings
  prefs.putFloat("minSpeedHz", CFG.MIN_SPEED_HZ);
  prefs.putFloat("maxSpeedHz", CFG.MAX_SPEED_HZ);
  prefs.putUChar("speedSteps", CFG.SPEED_STEPS);
  
  // Animation settings
  prefs.putFloat("easeGain", CFG.easeGain);
  prefs.putFloat("velEMAalpha", CFG.velEMAalpha);
  
  // Color settings - use short key names to save flash space
  prefs.putUShort("bg", CFG.COL_BG);
  prefs.putUShort("fr", CFG.COL_FRAME);
  prefs.putUShort("fr2", CFG.COL_FRAME2);
  prefs.putUShort("tan", CFG.COL_TAN);
  
  // Rod colors - compact keys
  prefs.putUChar("rsr", CFG.rodSlowR);
  prefs.putUChar("rsg", CFG.rodSlowG);
  prefs.putUChar("rsb", CFG.rodSlowB);
  prefs.putUChar("rfr", CFG.rodFastR);
  prefs.putUChar("rfg", CFG.rodFastG);
  prefs.putUChar("rfb", CFG.rodFastB);
  
  // UI colors - compact keys
  prefs.putUShort("arr", CFG.COL_ARROW);
  prefs.putUShort("glow", CFG.COL_ARROW_GLOW);
  prefs.putUShort("dr", CFG.DOT_RED);
  prefs.putUShort("db", CFG.DOT_BLUE);
  prefs.putUShort("dg", CFG.DOT_GREEN);
  prefs.putUShort("dgy", CFG.DOT_GREY);
  prefs.putUShort("sb", CFG.SPEEDBAR_BORDER);
  prefs.putUShort("brand", CFG.COL_BRAND);
  prefs.putUShort("pink", CFG.COL_MENU_PINK);
  
  // Vacuum settings
  size_t bytesWritten;
  bytesWritten = prefs.putFloat("vacTarget", CFG.vacTarget);
  Serial.printf("[FLASH] vacTarget write: %u bytes\n", bytesWritten);
  
  bytesWritten = prefs.putFloat("vacTarget2", CFG.vacuumTargetMbar);
  Serial.printf("[FLASH] vacTarget2 write: %u bytes (value=%.1f)\n", bytesWritten, CFG.vacuumTargetMbar);
  
  if (bytesWritten == 0) {
    Serial.println("[FLASH] ERROR: vacTarget2 write failed! Investigating...");
    
    // Check available space
    size_t freeBytes = prefs.freeEntries();
    Serial.printf("[FLASH] Free entries in namespace: %u\n", freeBytes);
    
    // Try to remove the key and write again
    Serial.println("[FLASH] Attempting to remove existing key and retry...");
    bool removed = prefs.remove("vacTarget2");
    Serial.printf("[FLASH] Key removal result: %s\n", removed ? "SUCCESS" : "FAILED");
    
    // Retry write
    size_t retryBytes = prefs.putFloat("vacTarget2", CFG.vacuumTargetMbar);
    Serial.printf("[FLASH] Retry write: %u bytes\n", retryBytes);
  }
  
  bytesWritten = prefs.putFloat("vacuumHoldTime", CFG.vacuumHoldTime);
  Serial.printf("[FLASH] vacuumHoldTime write: %u bytes\n", bytesWritten);
  
  bytesWritten = prefs.putBool("vacuumAutoMode", CFG.vacuumAutoMode);
  Serial.printf("[FLASH] vacuumAutoMode write: %u bytes\n", bytesWritten);
  
  Serial.printf("[SETTINGS] SAVED vacuumTargetMbar: %.1f mbar to flash\n", CFG.vacuumTargetMbar);
  
  // Motion blend settings
  prefs.putBool("motionBlendEnabled", CFG.motionBlendEnabled);
  prefs.putFloat("userSpeedWeight", CFG.userSpeedWeight);
  prefs.putFloat("motionSpeedWeight", CFG.motionSpeedWeight);
  prefs.putBool("motionDirectionSync", CFG.motionDirectionSync);
  
  // Auto Vacuum settings
  prefs.putUChar("autoVacSpeedThr", CFG.autoVacuumSpeedThreshold);
  
  prefs.end();
  
  // Also save lube settings
  saveLubeSettings();
  
  Serial.println("[SETTINGS] All configuration saved to flash");
}

// Reset all settings to default values
void resetAllSettingsToDefault() {
  // Clear the config preferences namespace
  prefs.begin("config", false);
  prefs.clear();
  prefs.end();
  
  // Clear the lube settings namespace
  prefs.begin("lube_settings", false);
  prefs.clear();
  prefs.end();
  
  // Reset CFG struct to defaults by reinitializing it
  CFG = Config();
  
  // Reset lube settings to defaults
  g_targetStrokes = 30;
  g_lubeHold_s = 0.5f;
  g_startLube_s = 0.5f;
  
  Serial.println("[SETTINGS] All settings reset to defaults");
  
  // Save the default values
  saveAllSettings();
}

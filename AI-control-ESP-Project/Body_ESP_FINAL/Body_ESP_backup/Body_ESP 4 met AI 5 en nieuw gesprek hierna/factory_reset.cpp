/*
  Factory Reset System Implementation for Body ESP
  Uses GPIO 0 (Boot button) on ESP32-2432S028R (CYD)
*/

#include "factory_reset.h"

// Global instance
FactoryReset factoryReset;

// ===== Constructor =====
FactoryReset::FactoryReset() {
  resetInProgress = false;
  buttonPressStart = 0;
  buttonPressed = false;
}

// ===== Initialization =====
void FactoryReset::begin() {
  pinMode(FACTORY_RESET_PIN, INPUT_PULLUP);
  Serial.println("[RESET] Factory Reset system initialized (GPIO 0 - Boot button)");
  Serial.println("[RESET] Hold Boot button for 3 seconds during operation for factory reset");
}

// ===== Main Loop Function =====
void FactoryReset::checkResetButton() {
  if (resetInProgress) return;
  
  bool currentButtonState = (digitalRead(FACTORY_RESET_PIN) == LOW);
  
  if (currentButtonState && !buttonPressed) {
    // Button just pressed
    buttonPressed = true;
    buttonPressStart = millis();
    Serial.println("[RESET] Boot button pressed - hold for 3 seconds for factory reset...");
  }
  else if (!currentButtonState && buttonPressed) {
    // Button released
    buttonPressed = false;
    uint32_t pressDuration = millis() - buttonPressStart;
    
    if (pressDuration < FACTORY_RESET_HOLD_TIME) {
      Serial.printf("[RESET] Button released too early (%d ms < %d ms)\\n", 
                    pressDuration, FACTORY_RESET_HOLD_TIME);
    }
  }
  else if (currentButtonState && buttonPressed) {
    // Button being held - check if long enough
    uint32_t pressDuration = millis() - buttonPressStart;
    
    if (pressDuration >= FACTORY_RESET_HOLD_TIME) {
      Serial.println("[RESET] Factory reset triggered!");
      performReset(RESET_COMPLETE);
    } else {
      // Show countdown every 500ms
      if ((millis() % 500) < 50) {
        uint32_t remaining = FACTORY_RESET_HOLD_TIME - pressDuration;
        Serial.printf("[RESET] Hold for %d more ms...\\n", remaining);
      }
    }
  }
}

// ===== Reset Implementation =====
void FactoryReset::performReset(ResetType type) {
  resetInProgress = true;
  
  Serial.println("\\n=====================================");
  Serial.println("  BODY ESP FACTORY RESET STARTED");  
  Serial.println("=====================================");
  
  switch(type) {
    case RESET_COMPLETE:
      Serial.println("[RESET] Complete factory reset - clearing ALL data...");
      resetSettings();
      resetMLData(); 
      resetEEPROM();
      break;
      
    case RESET_SETTINGS_ONLY:
      Serial.println("[RESET] Settings reset only...");
      resetSettings();
      break;
      
    case RESET_ML_DATA_ONLY:
      Serial.println("[RESET] ML data reset only...");
      resetMLData();
      break;
      
    case RESET_EEPROM_ONLY:
      Serial.println("[RESET] EEPROM reset only...");
      resetEEPROM();
      break;
      
    default:
      Serial.println("[RESET] Unknown reset type!");
      resetInProgress = false;
      return;
  }
  
  Serial.println("\\n=====================================");
  Serial.println("  FACTORY RESET COMPLETE!");
  Serial.println("  Restarting in 3 seconds...");
  Serial.println("=====================================\\n");
  
  delay(3000);
  ESP.restart();
}

void FactoryReset::performQuickReset() {
  Serial.println("[RESET] Quick reset - settings and ML data only...");
  resetSettings();
  resetMLData();
  Serial.println("[RESET] Quick reset complete - restarting...");
  delay(1000);
  ESP.restart();
}

// ===== Individual Reset Functions =====
void FactoryReset::resetSettings() {
  Serial.println("[RESET] Resetting configuration to defaults...");
  
  // Reset Body Config to defaults
  BODY_CFG.mlAutonomyLevel = 0.3f;              // 30% default autonomie
  BODY_CFG.mlStressEnabled = true;
  BODY_CFG.mlTrainingMode = false;
  BODY_CFG.autoRecordSessions = true;           // Auto-record ALL sessions by default
  BODY_CFG.mlConfidenceThreshold = 0.7f;
  BODY_CFG.stressLevel0Minutes = 5;
  BODY_CFG.stressLevel1Minutes = 3;
  BODY_CFG.stressLevel2Minutes = 3;
  BODY_CFG.stressLevel3Minutes = 2;
  BODY_CFG.stressLevel4Seconds = 30;
  BODY_CFG.stressLevel5Seconds = 20;
  BODY_CFG.stressLevel6Seconds = 15;
  
  // Reset biometric thresholds
  BODY_CFG.heartRateMin = 50.0f;
  BODY_CFG.heartRateMax = 200.0f;
  BODY_CFG.tempMin = 35.0f;
  BODY_CFG.tempMax = 40.0f;
  BODY_CFG.gsrMax = 1000.0f;
  
  Serial.println("[RESET] Configuration reset complete");
}

void FactoryReset::resetMLData() {
  Serial.println("[RESET] Resetting ML training data and models...");
  
  // Reset stress manager
  stressManager.resetMLAutonomy();
  
  // Reset ML autonomy to default
  stressManager.setMLAutonomyLevel(BODY_CFG.mlAutonomyLevel);
  
  Serial.println("[RESET] ML data reset complete");
}

void FactoryReset::resetEEPROM() {
  Serial.println("[RESET] Erasing EEPROM (32KB)...");
  Serial.println("[RESET] This will take approximately 30 seconds...");
  
  showResetProgress(0, "Starting EEPROM erase");
  
  uint16_t bytesPerChunk = 64;  // Write in 64-byte chunks for efficiency
  uint16_t totalChunks = ML_EEPROM_SIZE / bytesPerChunk;
  
  for (uint16_t chunk = 0; chunk < totalChunks; chunk++) {
    uint16_t startAddr = chunk * bytesPerChunk;
    
    // Erase chunk
    for (uint16_t i = 0; i < bytesPerChunk; i++) {
      uint16_t addr = startAddr + i;
      
      Wire.beginTransmission(ML_EEPROM_ADDR);
      Wire.write(addr >> 8);     // High byte
      Wire.write(addr & 0xFF);   // Low byte  
      Wire.write(0xFF);          // Clear value (EEPROM erased state)
      
      if (Wire.endTransmission() != 0) {
        Serial.printf("[RESET] EEPROM write error at address 0x%04X\\n", addr);
      }
      
      delayMicroseconds(5000);   // EEPROM write time
    }
    
    // Progress update every 10 chunks (~6.25%)
    if (chunk % 10 == 0) {
      int percentage = (chunk * 100) / totalChunks;
      showResetProgress(percentage, String("Erasing... ") + String(chunk * bytesPerChunk) + "/" + String(ML_EEPROM_SIZE));
      
      // Allow ESP to process other tasks
      yield();
    }
  }
  
  showResetProgress(100, "EEPROM erase complete");
  Serial.println("[RESET] EEPROM reset complete");
}

void FactoryReset::showResetProgress(int percentage, const String& status) {
  Serial.printf("[RESET] %s (%d%%)\\n", status.c_str(), percentage);
  
  // Progress bar
  if (percentage % 10 == 0 || percentage == 100) {
    Serial.print("[RESET] Progress: [");
    for (int i = 0; i < 20; i++) {
      if (i < (percentage / 5)) {
        Serial.print("=");
      } else if (i == (percentage / 5)) {
        Serial.print(">");
      } else {
        Serial.print(" ");
      }
    }
    Serial.printf("] %d%%\\n", percentage);
  }
}

// ===== Status Functions =====
void FactoryReset::printResetStatus() {
  Serial.println("[RESET] === Factory Reset Status ===");
  Serial.printf("Reset Pin: GPIO %d\\n", FACTORY_RESET_PIN);
  Serial.printf("Hold Time Required: %d ms\\n", FACTORY_RESET_HOLD_TIME);
  Serial.printf("Reset In Progress: %s\\n", resetInProgress ? "Yes" : "No");
  Serial.printf("Button Pressed: %s\\n", buttonPressed ? "Yes" : "No");
  
  if (buttonPressed) {
    uint32_t pressDuration = millis() - buttonPressStart;
    Serial.printf("Current Press Duration: %d ms\\n", pressDuration);
    uint32_t remaining = (pressDuration < FACTORY_RESET_HOLD_TIME) ? 
                        (FACTORY_RESET_HOLD_TIME - pressDuration) : 0;
    Serial.printf("Remaining Hold Time: %d ms\\n", remaining);
  }
  
  Serial.println("[RESET] ================================");
}

// ===== Backup Functions =====
void FactoryReset::createBackup() {
  Serial.println("[RESET] Creating configuration backup...");
  // TODO: Implement backup to specific EEPROM section
  Serial.println("[RESET] Backup feature not yet implemented");
}

void FactoryReset::restoreFromBackup() {
  Serial.println("[RESET] Restoring from backup...");
  // TODO: Implement restore from backup
  Serial.println("[RESET] Restore feature not yet implemented"); 
}

// ===== Convenience Functions =====
void checkFactoryReset() {
  // Call this in setup()
  factoryReset.begin();
  
  // Check if reset pin is already held during boot
  if (digitalRead(FACTORY_RESET_PIN) == LOW) {
    Serial.println("[RESET] Boot button held during startup - checking for reset...");
    delay(100);  // Debounce
    
    if (digitalRead(FACTORY_RESET_PIN) == LOW) {
      Serial.println("[RESET] Boot button confirmed held - starting reset countdown...");
      
      // Countdown from 3 seconds
      for (int i = 3; i > 0; i--) {
        Serial.printf("[RESET] Factory reset in %d seconds (release button to cancel)...\\n", i);
        delay(1000);
        
        if (digitalRead(FACTORY_RESET_PIN) == HIGH) {
          Serial.println("[RESET] Reset cancelled - button released");
          return;
        }
      }
      
      Serial.println("[RESET] Performing factory reset NOW!");
      factoryReset.performReset(RESET_COMPLETE);
    }
  }
}

void factoryResetLoop() {
  // Call this in main loop()
  factoryReset.checkResetButton();
}
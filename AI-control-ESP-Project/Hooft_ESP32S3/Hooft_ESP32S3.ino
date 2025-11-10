// HoofdESP - Centralized ESP-NOW Control Hub with Motion Sync
#include "ui.h"
#include "espnow_comm.h"

void setup() {
  // Initialize UI first
  uiInit();
  
  // Initialize ESP-NOW communication
  initESPNow();
  
  Serial.println("[SYSTEM] HoofdESP ready - ESP-NOW system online");
  Serial.println("[SYSTEM] MAC: E4:65:B8:7A:85:E4 - Channel: 4");
  Serial.println("[SYSTEM] Motion sync enabled - M5Atom controls animation");
}

void loop() {
  // UI handling (existing functionality)
  uiTick();
  
  // ESP-NOW communication handling
  checkCommunicationTimeouts();
  updateVacuumControl();
  sendPumpControlMessages();
  sendStatusUpdates();
  performSafetyChecks();
  
  // Motion sync - M5Atom motion controls animation speed
  syncMotionToAnimation();
  
  // Small delay to prevent overwhelming the system
  delay(1);
}


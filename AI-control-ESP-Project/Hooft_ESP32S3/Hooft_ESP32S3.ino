// HoofdESP - Centralized ESP-NOW Control Hub with Motion Sync
#include "ui.h"
#include "espnow_comm.h"
#include "keon.h"

void setup() {
  // Initialize UI first
  uiInit();
  
  // Initialize Keon BLE controller
  Serial.println("[SYSTEM] Initializing Keon BLE...");
  keonInit();
  
  // Initialize ESP-NOW communication
  initESPNow();
  
  Serial.println("[SYSTEM] HoofdESP ready - ESP-NOW system online");
  Serial.println("[SYSTEM] MAC: E4:65:B8:7A:85:E4 - Channel: 4");
  Serial.println("[SYSTEM] Motion sync enabled - M5Atom controls animation");
  Serial.println("[SYSTEM] Keon BLE available for connection");
}

void loop() {
  // UI handling (existing functionality)
  uiTick();
  
  // Keon BLE handling
  keonCheckConnection();
  // Keon animation sync will be handled in uiTick() or sendStatusUpdates()
  
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


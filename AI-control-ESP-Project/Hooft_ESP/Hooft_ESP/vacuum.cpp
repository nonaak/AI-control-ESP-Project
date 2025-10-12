// vacuum.cpp – regelt "zuigen" state machine met ESP-NOW integratie

#include "vacuum.h"
#include "config.h"
#include "espnow_comm.h"
#include <Arduino.h>

VacuumState g_vacState = VAC_IDLE;  // Remove static for web access
uint32_t g_vacStateTime = 0;        // Remove static for web access

void vacuumInit() {
  g_vacState = VAC_IDLE;
  g_vacStateTime = millis();
}

void vacuumTick(bool goingUp, bool zPressed, bool zPressed_edge, float vacSensor) {
  // DEPRECATED: Vacuum control is now handled by Pump Unit state machine
  // This function is kept for compatibility but does minimal work
  
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug > 10000) { // Every 10 seconds
    lastDebug = millis();
    Serial.printf("[VACUUM DEBUG] PumpUnit zuig active: %s, Target=%.0f mbar\n", 
                  pompUnitZuigActive ? "TRUE" : "FALSE", abs(CFG.vacuumTargetMbar));
    Serial.printf("[VACUUM DEBUG] Current vacuum: %.1f cmHg, Auto mode: %s\n",
                  currentVacuumReading, CFG.vacuumAutoMode ? "AAN" : "UIT");
  }
  
  // Update local state based on Pump Unit feedback
  if (pompUnitZuigActive && g_vacState == VAC_IDLE) {
    g_vacState = VAC_ZUIGEN_ACTIEF;
    g_vacStateTime = millis();
    Serial.println("[VACUUM] Pump Unit activated zuig mode");
  } else if (!pompUnitZuigActive && g_vacState != VAC_IDLE) {
    g_vacState = VAC_IDLE;
    g_vacStateTime = millis();
    Serial.println("[VACUUM] Pump Unit deactivated zuig mode");
  }
}

void vacuumCancel() {
  // DEPRECATED: Vacuum control is now handled by Pump Unit
  // This function is kept for compatibility but state is managed by Pump Unit
  g_vacState = VAC_IDLE;
  g_vacStateTime = millis();
  Serial.println("[VACUUM] Local vacuum state reset - actual control handled by Pump Unit");
}

bool vacuumIsActive() {
  // Return status based on Pump Unit feedback
  return pompUnitZuigActive;
}

bool vacuumIsHolding() {
  // Return status based on Pump Unit feedback
  return pompUnitZuigActive;
}

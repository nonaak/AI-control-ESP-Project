// vacuum.h – regelt "zuigen" state machine (lokaal, zonder ESP-NOW)

#pragma once

#include <Arduino.h>

// === States ===
enum VacuumState {
  VAC_IDLE = 0,
  VAC_ZUIGEN_ACTIEF,
  VAC_ZUIGEN_BEHOUD
};

void vacuumInit();
void vacuumTick(bool goingUp, bool zPressed, bool zPressed_edge, float vacSensor);
void vacuumCancel();

bool vacuumIsActive();
bool vacuumIsHolding();

// Expose vacuum state for web interface
extern VacuumState g_vacState;
extern uint32_t g_vacStateTime;

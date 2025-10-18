#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Touch kalibratie functies
void touchCalibration_begin(TFT_eSPI* gfx);
bool touchCalibration_poll();  // true = kalibratie klaar
void touchCalibration_loadFromEEPROM();
bool touchCalibration_hasValidData();

// Kalibratie waarden ophalen
struct CalibrationData {
  int16_t xmin, xmax, ymin, ymax;
  bool valid;
};

CalibrationData touchCalibration_getData();
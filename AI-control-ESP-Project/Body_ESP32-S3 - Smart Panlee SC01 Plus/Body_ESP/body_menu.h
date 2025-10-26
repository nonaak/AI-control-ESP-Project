#pragma once
#include <Arduino.h>

// Menu modes
enum BodyMenuMode { 
  BODY_MODE_SENSORS = 0,  // Main sensor display
  BODY_MODE_MENU = 1      // Menu mode
};

// Menu pages
enum BodyMenuPage { 
  BODY_PAGE_MAIN = 0, 
  BODY_PAGE_AI_SETTINGS = 1, 
  BODY_PAGE_SENSOR_CAL = 2,
  BODY_PAGE_RECORDING = 3,
  BODY_PAGE_ESP_STATUS = 4
};

// Menu functions
void bodyMenuInit();
void bodyMenuTick();
void bodyMenuDraw();
void bodyMenuHandleTouch(int16_t x, int16_t y, bool pressed);
void bodyMenuHandleButton(bool cPressed, bool zPressed);
void bodyMenuSetVariablePointers(uint16_t* bpm, float* tempValue, float* gsrValue, 
                                 bool* aiOverruleActive, float* currentTrustOverride, float* currentSleeveOverride,
                                 bool* isRecording, bool* espNowInitialized, float* trustSpeed, float* sleeveSpeed,
                                 uint32_t* lastCommTime, uint32_t* samplesRecorded);

// Menu state
extern BodyMenuMode bodyMenuMode;
extern BodyMenuPage bodyMenuPage;
extern int bodyMenuIdx;
extern bool bodyMenuEdit;
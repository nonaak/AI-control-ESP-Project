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
  BODY_PAGE_ESP_STATUS = 4,
  BODY_PAGE_SENSOR_SETTINGS = 5,
  BODY_PAGE_ML_TRAINING = 6,
  BODY_PAGE_SYSTEM_SETTINGS = 7,  // Instellingen: scherm, SD, tijd
  BODY_PAGE_TIME_SETTINGS = 8,    // RTC tijd instellen
  BODY_PAGE_FUNSCRIPT_SETTINGS = 9, // Funscript mode settings
  BODY_PAGE_FORMAT_CONFIRM = 10,  // SD format bevestiging
  BODY_PAGE_PLAYBACK = 11          // Playback/Training scherm
};

// Menu functions
void bodyMenuInit();
void bodyMenuTick();
void bodyMenuDraw();
void bodyMenuForceRedraw();  // Force menu redraw
void bodyMenuHandleTouch(int16_t x, int16_t y, bool pressed);
void bodyMenuHandleButton(bool cPressed, bool zPressed);
void bodyMenuSetVariablePointers(uint16_t* bpm, float* tempValue, float* gsrValue, 
                                 bool* aiOverruleActive, float* currentTrustOverride, float* currentSleeveOverride,
                                 bool* isRecording, bool* espNowInitialized, float* trustSpeed, float* sleeveSpeed,
                                 uint32_t* lastCommTime, uint32_t* samplesRecorded);

// Playback functions
void startPlayback(const char* filename);
void updatePlayback();
void stopPlayback();
void saveStressMarkers();
void drawPlaybackScreen();
void drawStressLevelPopup();

// 🔥 ML Integration - sensor data doorsturen
void bodyMenuUpdateSensors(float bpm, float temp, float gsr);

// 🔥 Playback variabelen (extern)
extern float playbackProgress;
extern float playbackDuration;

// Menu state
extern BodyMenuMode bodyMenuMode;
extern BodyMenuPage bodyMenuPage;
extern int bodyMenuIdx;
extern bool bodyMenuEdit;
/*
  NVS SETTINGS - Centrale opslag voor alle instellingen
  
  ═══════════════════════════════════════════════════════════════════════════
  Slaat ALLES op in ESP32 interne flash (NVS = Non-Volatile Storage)
  Overleeft SD format, stroomuitval, etc.
  ═══════════════════════════════════════════════════════════════════════════
  
  NAMESPACES:
  - ai_settings    : AI parameters (autonomy, HR thresholds, etc.)
  - touch_cfg      : Touch settings
  - sensor_cal     : Sensor kalibratie (vervangt externe EEPROM!)
  - display_cfg    : Scherm instellingen
  - ml_model_a     : ML Model A (Langzaam profiel)
  - ml_model_b     : ML Model B (Snel profiel)  
  - ml_model_c     : ML Model C (Experiment)
  - ml_active      : Actief model + statistieken
  - stats          : Sessie statistieken
  
  GEBRUIK:
  ~15KB beschikbaar, ~3KB gebruikt
*/

#ifndef NVS_SETTINGS_H
#define NVS_SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         MODEL PROFIELEN
// ═══════════════════════════════════════════════════════════════════════════

#define ML_MODEL_SLOTS 3

// Model slot IDs
enum MLModelSlot {
  ML_SLOT_A = 0,    // Langzaam / Lange sessies
  ML_SLOT_B = 1,    // Snel / Korte sessies
  ML_SLOT_C = 2     // Experiment / Backup
};

// Model slot namen (voor UI)
extern const char* ML_SLOT_NAMES[ML_MODEL_SLOTS];

// Model info structuur
struct MLModelInfo {
  bool exists;              // Model aanwezig?
  float accuracy;           // Training accuracy (0.0 - 1.0)
  uint16_t sessions;        // Aantal training sessies
  uint32_t lastTrained;     // Timestamp laatste training
  uint16_t sampleCount;     // Aantal training samples
};

// ═══════════════════════════════════════════════════════════════════════════
//                         AI SETTINGS STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct AISettingsNVS {
  float autonomyLevel;      // 0-100%
  float hrLow;              // Ondergrens hartslag
  float hrHigh;             // Bovengrens hartslag
  float tempMax;            // Max temperatuur
  float gsrMax;             // Max GSR
  float responseRate;       // Reactiesnelheid
  bool aiEnabled;           // AI aan/uit
};

// ═══════════════════════════════════════════════════════════════════════════
//                         SENSOR KALIBRATIE STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct SensorCalibrationNVS {
  float beatThreshold;      // Hart beat detectie drempel
  float tempOffset;         // Temperatuur offset
  float tempSmoothing;      // Temperatuur smoothing factor
  float gsrBaseline;        // GSR baseline waarde
  float gsrSensitivity;     // GSR gevoeligheid
  float gsrSmoothing;       // GSR smoothing factor
};

// ═══════════════════════════════════════════════════════════════════════════
//                         DISPLAY SETTINGS STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct DisplaySettingsNVS {
  uint8_t rotation;         // Scherm rotatie (1 of 3)
  uint8_t brightness;       // Helderheid (0-255)
};

// ═══════════════════════════════════════════════════════════════════════════
//                         TOUCH SETTINGS STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct TouchSettingsNVS {
  bool menuEnabled;         // Touch menu aan/uit
  bool paramsEnabled;       // Touch params aan/uit
  bool emergencyEnabled;    // Touch noodstop aan/uit
};

// ═══════════════════════════════════════════════════════════════════════════
//                         SESSIE STATS STRUCTUUR
// ═══════════════════════════════════════════════════════════════════════════

struct SessionStatsNVS {
  uint32_t totalSessions;   // Totaal aantal sessies
  uint32_t totalEdges;      // Totaal aantal edges
  uint32_t totalOrgasms;    // Totaal aantal orgasmes
  uint32_t longestSession;  // Langste sessie in seconden
  uint32_t totalTime;       // Totale tijd in seconden
};

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - INITIALISATIE
// ═══════════════════════════════════════════════════════════════════════════

// Initialiseer NVS systeem, laad alle settings
void nvsSettings_begin();

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - AI SETTINGS
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadAI(AISettingsNVS* settings);
void nvsSettings_saveAI(const AISettingsNVS* settings);

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - SENSOR KALIBRATIE
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadSensorCal(SensorCalibrationNVS* cal);
void nvsSettings_saveSensorCal(const SensorCalibrationNVS* cal);

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - DISPLAY
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadDisplay(DisplaySettingsNVS* settings);
void nvsSettings_saveDisplay(const DisplaySettingsNVS* settings);

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - TOUCH
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadTouch(TouchSettingsNVS* settings);
void nvsSettings_saveTouch(const TouchSettingsNVS* settings);

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - ML MODELLEN
// ═══════════════════════════════════════════════════════════════════════════

// Actief model
uint8_t nvsSettings_getActiveModel();
void nvsSettings_setActiveModel(uint8_t slot);

// Model info
bool nvsSettings_getModelInfo(uint8_t slot, MLModelInfo* info);

// Model opslaan/laden (data = neural network weights)
bool nvsSettings_saveModel(uint8_t slot, const uint8_t* data, uint16_t size, float accuracy);
bool nvsSettings_loadModel(uint8_t slot, uint8_t* data, uint16_t* size);
bool nvsSettings_deleteModel(uint8_t slot);

// Update model stats na sessie
void nvsSettings_incrementModelSessions(uint8_t slot);
void nvsSettings_updateModelAccuracy(uint8_t slot, float newAccuracy, uint16_t newSamples);

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - SESSIE STATS
// ═══════════════════════════════════════════════════════════════════════════

void nvsSettings_loadStats(SessionStatsNVS* stats);
void nvsSettings_saveStats(const SessionStatsNVS* stats);
void nvsSettings_incrementSession(uint32_t durationSeconds, uint8_t edges, bool orgasm);

// ═══════════════════════════════════════════════════════════════════════════
//                         FUNCTIES - BACKUP / RESTORE
// ═══════════════════════════════════════════════════════════════════════════

// Alles opslaan (voor belangrijke momenten)
void nvsSettings_saveAll();

// Factory reset - wis ALLES
void nvsSettings_factoryReset();

// Debug - print alle settings
void nvsSettings_printAll();

#endif // NVS_SETTINGS_H

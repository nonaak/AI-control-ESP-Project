#include "body_menu.h"
#include "ml_training_view.h"  // ML Training UI
#include <SD.h>  // SD card voor bestandslijst
#include <SD_MMC.h>  // SD_MMC voor /recordings/ folder
#include <RTClib.h>  // RTC DS3231 voor tijd instellingen
#include <Preferences.h>  // ESP32 EEPROM (NVS)
#include <math.h>  // Voor fabs()
#include "body_display.h"
#include "body_config.h"
#include "body_fonts.h"
#include "body_gfx4.h"  // Voor G4_* constanten en body_gfx4_pushSample
#include "ads1115_sensors.h"
#include "sensor_settings.h"

// 🔥 NIEUW: Extern reference naar rendering pause flag
extern volatile bool g4_pauseRendering;

// Forward declarations
void drawSensorMode();
void drawMenuMode();
void drawSensorStatus();
void drawMainMenuItems();
void drawAISettingsItems();
void drawSensorCalItems();
void drawRecordingItems();
void drawESPStatusItems();
void drawSensorSettingsItems();
void drawSystemSettingsItems();
void drawTimeSettingsItems();
void drawFunscriptSettingsItems();
void drawFormatConfirmItems();
void startCalibration(uint8_t type);
void updateCalibration();
void drawCalibrationScreen();
void drawPlaybackProgressBar();
void drawStressLevelPopup();

// Menu state variables
BodyMenuMode bodyMenuMode = BODY_MODE_SENSORS;
BodyMenuPage bodyMenuPage = BODY_PAGE_MAIN;
int bodyMenuIdx = 0;
bool bodyMenuEdit = false;

// Parent page tracking voor menu navigatie (TERUG knop)
static BodyMenuPage parentPage = BODY_PAGE_MAIN;

// Forward declarations for main file variables
// These will be accessed via global pointers
uint16_t* g_BPM = nullptr;
float* g_tempValue = nullptr;
float* g_gsrValue = nullptr;
bool* g_aiOverruleActive = nullptr;
float* g_currentTrustOverride = nullptr;
float* g_currentSleeveOverride = nullptr;
bool* g_isRecording = nullptr;
bool* g_espNowInitialized = nullptr;
float* g_trustSpeed = nullptr;
float* g_sleeveSpeed = nullptr;
uint32_t* g_lastCommTime = nullptr;
uint32_t* g_samplesRecorded = nullptr;

// Simple getter functions to safely access values
float getBPM() { return g_BPM ? (float)*g_BPM : 0.0f; }
float getTempValue() { return g_tempValue ? *g_tempValue : 36.5f; }
float getGsrValue() { return g_gsrValue ? *g_gsrValue : 0.0f; }
bool getAiOverruleActive() { return g_aiOverruleActive ? *g_aiOverruleActive : false; }
float getCurrentTrustOverride() { return g_currentTrustOverride ? *g_currentTrustOverride : 1.0f; }
float getCurrentSleeveOverride() { return g_currentSleeveOverride ? *g_currentSleeveOverride : 1.0f; }
bool getIsRecording() { return g_isRecording ? *g_isRecording : false; }
bool getEspNowInitialized() { return g_espNowInitialized ? *g_espNowInitialized : false; }
float getTrustSpeed() { return g_trustSpeed ? *g_trustSpeed : 0.0f; }
float getSleeveSpeed() { return g_sleeveSpeed ? *g_sleeveSpeed : 0.0f; }
uint32_t getLastCommTime() { return g_lastCommTime ? *g_lastCommTime : 0; }
uint32_t getSamplesRecorded() { return g_samplesRecorded ? *g_samplesRecorded : 0; }
uint32_t lastCommTime = 0;
float trustSpeed = 0.0f;

// Hartslag geschiedenis voor graph
static float heartRateHistory[50];
static int historyIndex = 0;
static uint32_t lastHistoryUpdate = 0;

// Dirty flag voor menu redraw
static bool menuDirty = true;
static BodyMenuMode lastMenuMode = BODY_MODE_SENSORS;
static BodyMenuPage lastMenuPage = BODY_PAGE_MAIN;

// Kalibratie state
static bool isCalibrating = false;
static uint8_t calibrationType = 0; // 0=alle, 1=GSR, 2=Temp, 3=Hart
static uint32_t calibrationStartTime = 0;
static const uint32_t CALIBRATION_DURATION = 10000; // 10 seconden
static float gsrSum = 0, tempSum = 0, hrSum = 0;
static int sampleCount = 0;
static bool calibrationScreenDrawn = false; // Track of statische delen getekend zijn

// RTC tijd instellingen state
static int editYear = 2024;
static int editMonth = 1;
static int editDay = 1;
static int editHour = 0;
static int editMinute = 0;

// AI Settings state (dynamisch, wordt geladen uit EEPROM)
struct AISettingsData {
  float autonomyLevel = 90.0f;     // AI Autonomie (0-100%)
  float hrLow = 60.0f;             // Hartslag Laag
  float hrHigh = 140.0f;           // Hartslag Hoog
  float tempMax = 37.5f;           // Temp Maximum
  float gsrMax = 500.0f;           // GSR Maximum
  float responseRate = 0.5f;       // Response Rate (0-1.0)
  bool aiEnabled = false;          // AI AAN/UIT
};
static AISettingsData aiSettings;
static Preferences aiPrefs;  // ESP32 Preferences voor AI settings

// Sensor Settings state (wordt uit sensorConfig gehaald)
struct SensorSettingsEdit {
  float beatThreshold = 50000.0f;
  float tempOffset = 0.0f;
  float tempSmoothing = 0.2f;
  float gsrBaseline = 512.0f;
  float gsrSensitivity = 1.0f;
  float gsrSmoothing = 0.1f;
};
static SensorSettingsEdit sensorEdit;

// Playback state variabelen
static bool isPlaybackActive = false;
static bool isPlaybackPaused = false;
static float playbackSpeed = 100.0f;  // 10-200%
static float playbackProgress = 0.0f;  // 0.0-1.0
static int playbackCurrentLine = 0;
static int playbackTotalLines = 0;
static unsigned long playbackLastUpdate = 0;
static File playbackFile;
static char selectedPlaybackFile[64] = "";
static bool playbackScreenDrawn = false;  // Track of overlay al getekend is

// Stress marker data
struct StressMarker {
  unsigned long timestamp;
  int stressLevel;  // 0-6
};
static StressMarker stressMarkers[100];  // Max 100 markers per sessie
static int stressMarkerCount = 0;
static bool stressPopupActive = false;
static int selectedStressLevel = 2;  // Default: normaal

// ===== AI SETTINGS EEPROM FUNCTIES =====
void loadAISettings() {
  aiPrefs.begin("ai_settings", false);  // false = read/write
  
  aiSettings.autonomyLevel = aiPrefs.getFloat("autonomy", 90.0f);
  aiSettings.hrLow = aiPrefs.getFloat("hr_low", 60.0f);
  aiSettings.hrHigh = aiPrefs.getFloat("hr_high", 140.0f);
  aiSettings.tempMax = aiPrefs.getFloat("temp_max", 37.5f);
  aiSettings.gsrMax = aiPrefs.getFloat("gsr_max", 500.0f);
  aiSettings.responseRate = aiPrefs.getFloat("response", 0.5f);
  aiSettings.aiEnabled = aiPrefs.getBool("ai_enabled", false);
  
  aiPrefs.end();
  
  Serial.println("[AI SETTINGS] Geladen uit EEPROM:");
  Serial.printf("  Autonomy: %.0f%%\n", aiSettings.autonomyLevel);
  Serial.printf("  HR Low: %.0f BPM\n", aiSettings.hrLow);
  Serial.printf("  HR High: %.0f BPM\n", aiSettings.hrHigh);
  Serial.printf("  Temp Max: %.1f°C\n", aiSettings.tempMax);
  Serial.printf("  GSR Max: %.0f\n", aiSettings.gsrMax);
  Serial.printf("  Response: %.2f\n", aiSettings.responseRate);
  Serial.printf("  AI Enabled: %s\n", aiSettings.aiEnabled ? "JA" : "NEE");
}

void saveAISettings() {
  aiPrefs.begin("ai_settings", false);
  
  aiPrefs.putFloat("autonomy", aiSettings.autonomyLevel);
  aiPrefs.putFloat("hr_low", aiSettings.hrLow);
  aiPrefs.putFloat("hr_high", aiSettings.hrHigh);
  aiPrefs.putFloat("temp_max", aiSettings.tempMax);
  aiPrefs.putFloat("gsr_max", aiSettings.gsrMax);
  aiPrefs.putFloat("response", aiSettings.responseRate);
  aiPrefs.putBool("ai_enabled", aiSettings.aiEnabled);
  
  aiPrefs.end();
  
  Serial.println("[AI SETTINGS] Opgeslagen in EEPROM!");
}

void applyAISettings() {
  // Pas AI settings toe op BODY_CFG
  BODY_CFG.aiEnabled = aiSettings.aiEnabled;
  BODY_CFG.hrLowThreshold = aiSettings.hrLow;
  BODY_CFG.hrHighThreshold = aiSettings.hrHigh;
  BODY_CFG.tempHighThreshold = aiSettings.tempMax;
  BODY_CFG.gsrHighThreshold = aiSettings.gsrMax;
  BODY_CFG.recoveryRate = aiSettings.responseRate;
  BODY_CFG.mlAutonomyLevel = aiSettings.autonomyLevel / 100.0f;  // 0-100% -> 0.0-1.0
  
  Serial.println("[AI SETTINGS] Toegepast op BODY_CFG");
}

// ===== SENSOR SETTINGS EDIT FUNCTIES =====
void loadSensorSettingsToEdit() {
  sensorEdit.beatThreshold = sensorConfig.hrThreshold;
  sensorEdit.tempOffset = sensorConfig.tempOffset;
  sensorEdit.tempSmoothing = sensorConfig.tempSmoothing;
  sensorEdit.gsrBaseline = sensorConfig.ads_gsrBaseline;
  sensorEdit.gsrSensitivity = sensorConfig.gsrSensitivity;
  sensorEdit.gsrSmoothing = sensorConfig.gsrSmoothing;
  
  Serial.println("[SENSOR SETTINGS] Geladen uit sensorConfig");
}

void saveSensorSettingsFromEdit() {
  sensorConfig.hrThreshold = sensorEdit.beatThreshold;
  sensorConfig.tempOffset = sensorEdit.tempOffset;
  sensorConfig.tempSmoothing = sensorEdit.tempSmoothing;
  sensorConfig.ads_gsrBaseline = sensorEdit.gsrBaseline;
  sensorConfig.gsrSensitivity = sensorEdit.gsrSensitivity;
  sensorConfig.gsrSmoothing = sensorEdit.gsrSmoothing;
  
  saveSensorConfig();  // Sla op in EEPROM
  applySensorConfig(); // Pas toe op ADS1115
  
  Serial.println("[SENSOR SETTINGS] Opgeslagen en toegepast!");
}

// Font helpers (zoals HoofdESP)
static void setMenuFontTitle(){
  if (!body_gfx) return;  // Safety check
#if USE_ADAFRUIT_FONTS
  body_gfx->setFont(&FONT_TITLE);
  body_gfx->setTextSize(1);
#else
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(2);
#endif
}

static void setMenuFontItem(){
  if (!body_gfx) return;  // Safety check
#if USE_ADAFRUIT_FONTS
  body_gfx->setFont(&FONT_ITEM);
  body_gfx->setTextSize(1);
#else
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(3);  // Van 2 naar 3 voor nog grotere tekst
#endif
}

void bodyMenuSetVariablePointers(uint16_t* bpm, float* tempValue, float* gsrValue, 
                                 bool* aiOverruleActive, float* currentTrustOverride, float* currentSleeveOverride,
                                 bool* isRecording, bool* espNowInitialized, float* trustSpeed, float* sleeveSpeed,
                                 uint32_t* lastCommTime, uint32_t* samplesRecorded) {
  g_BPM = bpm;
  g_tempValue = tempValue;
  g_gsrValue = gsrValue;
  g_aiOverruleActive = aiOverruleActive;
  g_currentTrustOverride = currentTrustOverride;
  g_currentSleeveOverride = currentSleeveOverride;
  g_isRecording = isRecording;
  g_espNowInitialized = espNowInitialized;
  g_trustSpeed = trustSpeed;
  g_sleeveSpeed = sleeveSpeed;
  g_lastCommTime = lastCommTime;
  g_samplesRecorded = samplesRecorded;
  
  Serial.println("[BODY] Variable pointers set");
}

void bodyMenuInit() {
  // // Initialize display - UITGESCHAKELD: body_gfx4 doet dit al in main
  // body_gfx->begin();
  // body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // // Initialize canvas - UITGESCHAKELD: body_gfx4 doet dit al
  // body_cv->begin();
  
  // Initialize heart rate history
  for (int i = 0; i < 50; i++) {
    heartRateHistory[i] = 0.0f;
  }
  
  // Laad AI Settings uit EEPROM
  loadAISettings();
  applyAISettings();
  
  Serial.println("[BODY] Menu system initialized");
}

void bodyMenuTick() {
  // Update kalibratie als actief
  if (isCalibrating) {
    updateCalibration();
    menuDirty = true; // Blijf hertekenen tijdens kalibratie
  }
  
  // Update playback - herteken alleen progress bar als significante verandering
  // NIET updaten tijdens stress popup
  static float lastDrawnProgress = -1.0f;
  if (isPlaybackActive && bodyMenuPage == BODY_PAGE_PLAYBACK && !stressPopupActive) {
    // Alleen hertekenen als progress met meer dan 1% veranderd is
    if (fabs(playbackProgress - lastDrawnProgress) > 0.01f) {
      drawPlaybackProgressBar();
      lastDrawnProgress = playbackProgress;
    }
  }
  
  // Update heart rate history
  if (millis() - lastHistoryUpdate > 200) { // 5Hz update
    heartRateHistory[historyIndex] = getBPM();
    historyIndex = (historyIndex + 1) % 50;
    lastHistoryUpdate = millis();
  }
  
  // Check of menu state gewijzigd is
  if (bodyMenuMode != lastMenuMode || bodyMenuPage != lastMenuPage) {
    menuDirty = true;
    lastMenuMode = bodyMenuMode;
    lastMenuPage = bodyMenuPage;
  }
  
  // Alleen hertekenen als dirty
  if (menuDirty) {
    bodyMenuDraw();
    menuDirty = false;
  }
}

void bodyMenuForceRedraw() {
  menuDirty = true;
  lastMenuMode = bodyMenuMode;  // Sync
  lastMenuPage = bodyMenuPage;  // Sync
}

void bodyMenuDraw() {
  // Safety check: prevent crash if body_gfx is null
  if (!body_gfx) {
    Serial.println("[MENU] ERROR: body_gfx is NULL!");
    return;
  }
  
  // Als kalibratie actief is, toon kalibratie scherm
  if (isCalibrating) {
    drawCalibrationScreen();
    return;
  }

  // PLAYBACK mode: teken alleen overlay (body_gfx4 blijft actief)
  if (bodyMenuPage == BODY_PAGE_PLAYBACK) {
    // Teken popup OVER alles ALLEEN als actief
    if (stressPopupActive) {
      g4_pauseRendering = true;  // 🔥 NIEUW: Stop alle grafiek rendering
      drawStressLevelPopup();  // Popup OVERTEKENT alles
    } else {
      g4_pauseRendering = false;  // 🔥 NIEUW: Hervat grafiek rendering
      drawPlaybackScreen();  // Normale playback overlay
    }
    return;
  }
  
  // PLAYBACK mode: teken alleen overlay (body_gfx4 blijft actief)
  //if (bodyMenuPage == BODY_PAGE_PLAYBACK) {
    // Teken popup OVER alles ALLEEN als actief
    //if (stressPopupActive) {
      //drawStressLevelPopup();  // Popup OVERTEKENT alles
    //} else {
      //drawPlaybackScreen();  // Normale playback overlay
    //}
    //return;
  //}
  
  // ALLEEN menu mode tekenen - sensor mode wordt door body_gfx4 gedaan
  if (bodyMenuMode == BODY_MODE_MENU) {
    drawMenuMode();
  }
  // BODY_MODE_SENSORS wordt genegeerd - gebruik body_gfx4 in plaats daarvan
}

// void drawSensorMode() {
//   if (!body_gfx) return;  // Safety check
//   
//   Serial.println("[DRAW] Starting drawSensorMode");
//   
//   // Clear background
//   body_gfx->fillScreen(BODY_CFG.COL_BG);
//   Serial.println("[DRAW] fillScreen done");
//   
//   // Draw title bar
//   drawBodySpeedBarTop("Body Monitor");
//   Serial.println("[DRAW] title bar done");
//   
//   // Draw left frame
//   drawLeftFrame();
//   Serial.println("[DRAW] left frame done");
//   
//   // Draw sensor header
//   drawBodySensorHeader(getBPM(), getTempValue());
//   Serial.println("[DRAW] sensor header done");
//   
//   // Draw sensor graphs/indicators in canvas
//   Serial.println("[DRAW] About to call drawHeartRateGraph...");
//   float currentBPM = getBPM();
//   Serial.printf("[DRAW] BPM=%.1f, history=%p\n", currentBPM, (void*)heartRateHistory);
//   
//   drawHeartRateGraph(currentBPM, heartRateHistory, 50);
//   Serial.println("[DRAW] heart rate graph done");
//   
//   drawTemperatureBar(getTempValue(), BODY_CFG.tempMin, BODY_CFG.tempMax);
//   Serial.println("[DRAW] temperature bar done");
//   
//   drawGSRIndicator(getGsrValue(), BODY_CFG.gsrMax);
//   Serial.println("[DRAW] GSR indicator done");
//   
//   drawAIOverruleStatus(getAiOverruleActive(), getCurrentTrustOverride(), getCurrentSleeveOverride());
//   Serial.println("[DRAW] AI status done");
//   
//   // Draw right panel status
//   drawSensorStatus();
//   Serial.println("[DRAW] sensor status done");
//   
//   // Help text
//   body_gfx->setFont(nullptr);
//   body_gfx->setTextSize(1);
//   body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, R_WIN_Y + R_WIN_H - 15);
//   body_gfx->print("Touch: Menu  Long: Record");
//   Serial.println("[DRAW] drawSensorMode COMPLETE");
// }

// void drawSensorStatus() {
//   if (!body_gfx) return;  // Safety check
//   
//   // Right panel sensor status
//   int y = R_WIN_Y + 20;
//   setMenuFontTitle();
//   body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->print("Body Status");
//   
//   y += 30;
//   setMenuFontItem();
//   
//   // Heart rate
//   float bpm = getBPM();
//   uint16_t hrColor = (bpm > 60 && bpm < 140) ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_ORANGE;
//   body_gfx->setTextColor(hrColor, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->printf("Heart: %.0f BPM", bpm);
//   y += 20;
//   
//   // Temperature
//   float temp = getTempValue();
//   uint16_t tempColor = (temp > 36.0f && temp < 38.0f) ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_ORANGE;
//   body_gfx->setTextColor(tempColor, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->printf("Temp: %.1f C", temp);
//   y += 20;
//   
//   // GSR
//   float gsr = getGsrValue();
//   uint16_t gsrColor = (gsr < 500) ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_RED;
//   body_gfx->setTextColor(gsrColor, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->printf("GSR: %.0f", gsr);
//   y += 30;
//   
//   // AI Status
//   body_gfx->setTextColor(BODY_CFG.COL_AI, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->print("AI Overrule:");
//   y += 15;
//   bool aiActive = getAiOverruleActive();
//   body_gfx->setTextColor(aiActive ? BODY_CFG.DOT_RED : BODY_CFG.DOT_GREEN, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 15, y);
//   body_gfx->print(aiActive ? "ACTIVE" : "Standby");
//   
//   if (aiActive) {
//     y += 15;
//     body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
//     body_gfx->setCursor(R_WIN_X + 15, y);
//     body_gfx->printf("Trust: %.0f%%", getCurrentTrustOverride() * 100);
//     y += 12;
//     body_gfx->setCursor(R_WIN_X + 15, y);
//     body_gfx->printf("Snelheid: %.0f%%", getCurrentSleeveOverride() * 100);
//   }
//   
//   y += 30;
//   
//   // Recording status
//   bool recording = getIsRecording();
//   uint16_t recColor = recording ? BODY_CFG.DOT_RED : BODY_CFG.DOT_GREY;
//   body_gfx->setTextColor(recColor, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->printf("Recording: %s", recording ? "ON" : "OFF");
//   y += 15;
//   
//   // ESP-NOW status
//   bool espInit = getEspNowInitialized();
//   uint16_t espColor = espInit ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_RED;
//   body_gfx->setTextColor(espColor, BODY_CFG.COL_BG);
//   body_gfx->setCursor(R_WIN_X + 10, y);
//   body_gfx->printf("ESP-NOW: %s", espInit ? "OK" : "ERROR");
// }

void drawMenuMode() {
  if (!body_gfx) return;  // Safety check
  
  // Breed menu paneel als overlay (20px marge rondom)
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;  // 20px marge links + rechts
  const int MENU_H = 320 - 40;  // 20px marge boven + onder
  
  // Teken menu paneel met frames
  body_gfx->fillRect(MENU_X, MENU_Y, MENU_W, MENU_H, BODY_CFG.COL_BG);
  body_gfx->drawRoundRect(MENU_X, MENU_Y, MENU_W, MENU_H, 12, BODY_CFG.COL_FRAME2);
  body_gfx->drawRoundRect(MENU_X+2, MENU_Y+2, MENU_W-4, MENU_H-4, 10, BODY_CFG.COL_FRAME);
  
  // Title gecentreerd
  setMenuFontTitle();
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  
  // Title text
  const char* title = "Menu";
  switch(bodyMenuPage) {
    case BODY_PAGE_MAIN:
      title = "Menu";
      break;
    case BODY_PAGE_AI_SETTINGS:
      title = "AI Settings";
      break;
    case BODY_PAGE_SENSOR_CAL:
      title = "Sensor Kal";
      break;
    case BODY_PAGE_RECORDING:
      title = "Recording";
      break;
    // case BODY_PAGE_ESP_STATUS:  // UITGESCHAKELD - ESP-NOW status niet op deze ESP
    //   title = "ESP Status";
    //   break;
    case BODY_PAGE_SENSOR_SETTINGS:
      title = "Sensor Settings";
      break;
    case BODY_PAGE_ML_TRAINING:
      title = "AI Training";
      break;
    case BODY_PAGE_SYSTEM_SETTINGS:
      title = "Instellingen";
      break;
    case BODY_PAGE_TIME_SETTINGS:
      title = "Tijd Instellen";
      break;
    case BODY_PAGE_FUNSCRIPT_SETTINGS:
      title = "Funscript";
      break;
    case BODY_PAGE_FORMAT_CONFIRM:
      title = "SD Formatteren";
      break;
    case BODY_PAGE_PLAYBACK:
      title = "Playback";
      break;
  }
  
  // Centreer title (meer ruimte)
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  body_gfx->setCursor(MENU_X + (MENU_W - tw) / 2, MENU_Y + 34);
  body_gfx->print(title);
  
  // Teken menu content
  switch(bodyMenuPage) {
    case BODY_PAGE_MAIN:
      drawMainMenuItems();
      break;
    case BODY_PAGE_AI_SETTINGS:
      drawAISettingsItems();
      break;
    case BODY_PAGE_SENSOR_CAL:
      drawSensorCalItems();
      break;
    case BODY_PAGE_RECORDING:
      drawRecordingItems();
      break;
    // case BODY_PAGE_ESP_STATUS:  // UITGESCHAKELD
    //   drawESPStatusItems();
    //   break;
    case BODY_PAGE_SENSOR_SETTINGS:
      drawSensorSettingsItems();
      break;
    case BODY_PAGE_ML_TRAINING:
      drawMLTrainingView();
      break;
    case BODY_PAGE_SYSTEM_SETTINGS:
      drawSystemSettingsItems();
      break;
    case BODY_PAGE_TIME_SETTINGS:
      drawTimeSettingsItems();
      break;
    case BODY_PAGE_FUNSCRIPT_SETTINGS:
      drawFunscriptSettingsItems();
      break;
    case BODY_PAGE_FORMAT_CONFIRM:
      drawFormatConfirmItems();
      break;
    // BODY_PAGE_PLAYBACK wordt apart afgehandeld in bodyMenuDraw()
  }
}

void drawMainMenuItems() {
  // 2 kolommen (3 links + 3 rechts) + TERUG onderaan
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;  // 440 breed
  
  const int BTN_W = 190;     // Knop breedte (breder)
  const int BTN_H = 50;      // Knop hoogte  
  const int BTN_MARGIN_X = 20;  // Horizontale ruimte tussen kolommen
  const int BTN_MARGIN_Y = 8;   // Verticale ruimte
  const int START_X = MENU_X + 20;  // Start X
  const int START_Y = MENU_Y + 54;  // Start Y (meer ruimte voor titel)
  
  // Menu items met kleinere font voor knoppen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  const char* items[] = {
    "AI Training",
    "Opname",
    "AI Settings",
    "Kalibratie",
    "Sensors",
    "Instellingen"
  };
  
  // Kleuren
  uint16_t colors[] = {
    0x07FF,  // Cyaan (AI Training)
    0x07E0,  // Groen (Recording)
    0xF81F,  // Magenta (AI)
    0xF800,  // Rood (Kalibratie)
    0xFFE0,  // Geel (Sensor)
    0x841F   // Paars (Settings)
  };
  
  // Teken 6 knoppen (2 kolommen, 3 rijen)
  for (int i = 0; i < 6; i++) {
    int col = i / 3;  // 0 (links) of 1 (rechts)
    int row = i % 3;  // 0, 1, 2
    
    int x = START_X + col * (BTN_W + BTN_MARGIN_X);
    int y = START_Y + row * (BTN_H + BTN_MARGIN_Y);
    
    // Teken gekleurde knop
    body_gfx->fillRoundRect(x, y, BTN_W, BTN_H, 8, colors[i]);
    body_gfx->drawRoundRect(x, y, BTN_W, BTN_H, 8, 0xFFFF);  // Witte rand
    body_gfx->drawRoundRect(x+1, y+1, BTN_W-2, BTN_H-2, 7, 0xFFFF);  // Dubbele rand
    
    // Centreer tekst in knop
    body_gfx->setTextColor(0x0000, colors[i]);  // Zwarte tekst
    int16_t x1, y1; uint16_t tw, th;
    body_gfx->getTextBounds(items[i], 0, 0, &x1, &y1, &tw, &th);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(x + (BTN_W - tw) / 2 - x1, y + (BTN_H + th) / 2 - 2);
    #else
      body_gfx->setCursor(x + (BTN_W - tw) / 2, y + (BTN_H - th) / 2 + th - 2);
    #endif
    body_gfx->print(items[i]);
  }
  
  // TERUG knop onderaan (breed, gecentreerd)
  int btnY = MENU_Y + 227;  // 3px hoger (van 230 naar 227)
  int btnW = 300;
  int btnX = MENU_X + (MENU_W - btnW) / 2;
  body_gfx->fillRoundRect(btnX, btnY, btnW, 45, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btnX, btnY, btnW, 45, 8, 0xFFFF);
  body_gfx->drawRoundRect(btnX+1, btnY+1, btnW-2, 43, 7, 0xFFFF);
  
  // TERUG tekst met kleinere font
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  body_gfx->setTextColor(0xFFFF, 0x001F);  // Witte tekst op blauw
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds("TERUG", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btnX + (btnW - tw) / 2 - x1, btnY + (45 + th) / 2 - 2);
  #else
    body_gfx->setCursor(btnX + (btnW - tw) / 2, btnY + (45 - th) / 2 + th - 2);
  #endif
  body_gfx->print("TERUG");
}

void drawAISettingsItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // AI Settings met GFX fonts
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  // Subtitle met AI Eigenwil percentage
  // Labels en waarden (dynamisch uit aiSettings)
  const char* labels[] = {"AI Eigenwil:", "HR Laag:", "HR Hoog:", "Temp Max:", "GSR Max:", "Response:"};
  float values[] = {
    aiSettings.autonomyLevel,
    aiSettings.hrLow,
    aiSettings.hrHigh,
    aiSettings.tempMax,
    aiSettings.gsrMax,
    aiSettings.responseRate
  };
  
  float mlEigenwaarde = aiSettings.autonomyLevel;  // AI Autonomie percentage
  bool aiEnabled = aiSettings.aiEnabled;  // AI AAN/UIT status
  
  body_gfx->setCursor(MENU_X + 20, MENU_Y + 50);
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  body_gfx->print("AI: ");
  
  // AI status in kleur
  if (aiEnabled) {
    body_gfx->setTextColor(0x07E0, BODY_CFG.COL_BG);  // Groen
    body_gfx->print("AAN");
  } else {
    body_gfx->setTextColor(0xF800, BODY_CFG.COL_BG);  // Rood
    body_gfx->print("UIT");
  }
  
  // Autonoom label
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  body_gfx->print("     Autonoom: ");
  
  // Autonoom percentage in groen
  body_gfx->setTextColor(0x07E0, BODY_CFG.COL_BG);  // Groen
  body_gfx->printf("%.0f%%", mlEigenwaarde);
  
  int y = MENU_Y + 75;
  const int LINE_H = 25;
  const int VAL_X = MENU_X + 210;  // Waarde positie
  const int BTN_W = 35;  // +/- knop breedte
  const int BTN_H = 20;  // +/- knop hoogte
  const int MINUS_X = MENU_X + 320;  // - knop X
  const int PLUS_X = MENU_X + 360;   // + knop X
  
  for (int i = 0; i < 6; i++) {
    // Label links (lichtere grijs)
    body_gfx->setTextColor(0xC618, BODY_CFG.COL_BG);  // Lichtgrijs (was DOT_GREY = te donker)
    body_gfx->setCursor(MENU_X + 20, y);
    body_gfx->print(labels[i]);
    
    // Waarde veld (donkere box)
    int valBoxY = y - 5;  // Was y - 8, nu nog 3px naar beneden
    int valBoxH = 20;
    body_gfx->fillRoundRect(VAL_X, valBoxY, 100, valBoxH, 3, 0x2104);  // Donkergrijs
    body_gfx->drawRoundRect(VAL_X, valBoxY, 100, valBoxH, 3, 0x8410);  // Lichtgrijze rand
    body_gfx->setTextColor(0xFFFF, 0x2104);  // Witte tekst
    
    // Centreer waarde in box (horizontaal + verticaal)
    char valStr[10];
    sprintf(valStr, "%.2f", values[i]);
    int16_t xv1, yv1; uint16_t twv, thv;
    body_gfx->getTextBounds(valStr, 0, 0, &xv1, &yv1, &twv, &thv);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(VAL_X + (100 - twv) / 2 - xv1, valBoxY + valBoxH / 2 + thv / 2);
    #else
      body_gfx->setCursor(VAL_X + 10, y);
    #endif
    body_gfx->print(valStr);
    
    // - knop (blauw) gecentreerd
    int btnBoxY = y - 5;  // Was y - 8, nu nog 3px naar beneden
    body_gfx->fillRoundRect(MINUS_X, btnBoxY, BTN_W, BTN_H, 3, 0x001F);  // Blauw (was oranje)
    body_gfx->drawRoundRect(MINUS_X, btnBoxY, BTN_W, BTN_H, 3, 0xFFFF);
    body_gfx->setTextColor(0xFFFF, 0x001F);  // Witte tekst op blauw
    int16_t xm1, ym1; uint16_t twm, thm;
    body_gfx->getTextBounds("-", 0, 0, &xm1, &ym1, &twm, &thm);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(MINUS_X + (BTN_W - twm) / 2 - xm1, btnBoxY + BTN_H / 2 + thm / 2);
    #else
      body_gfx->setCursor(MINUS_X + 12, y);
    #endif
    body_gfx->print("-");
    
    // + knop (rood) gecentreerd
    body_gfx->fillRoundRect(PLUS_X, btnBoxY, BTN_W, BTN_H, 3, 0xF800);  // Rood (was groen)
    body_gfx->drawRoundRect(PLUS_X, btnBoxY, BTN_W, BTN_H, 3, 0xFFFF);
    body_gfx->setTextColor(0xFFFF, 0xF800);  // Witte tekst op rood
    int16_t xp1, yp1; uint16_t twp, thp;
    body_gfx->getTextBounds("+", 0, 0, &xp1, &yp1, &twp, &thp);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(PLUS_X + (BTN_W - twp) / 2 - xp1, btnBoxY + BTN_H / 2 + thp / 2);
    #else
      body_gfx->setCursor(PLUS_X + 12, y);
    #endif
    body_gfx->print("+");
    
    y += LINE_H;
  }
  
  // 3 knoppen onderaan met kleinere font - gecentreerd
  int btnY = MENU_Y + 225;
  int btnW = 120;
  int btnH = 40;
  int btnSpacing = 15;  // Ruimte tussen knoppen
  int totalBtnW = btnW * 3 + btnSpacing * 2;
  int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // AI AAN links
  int btn2X = btn1X + btnW + btnSpacing;  // Opslaan midden
  int btn3X = btn2X + btnW + btnSpacing;  // TERUG rechts
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  // AI AAN (paars)
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, 0x841F);
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x841F);
  int16_t xa1, ya1; uint16_t twa, tha;
  body_gfx->getTextBounds("AI AAN", 0, 0, &xa1, &ya1, &twa, &tha);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - twa) / 2 - xa1, btnY + (btnH + tha) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + 15, btnY + 10);
  #endif
  body_gfx->print("AI AAN");
  
  // Opslaan (groen)
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, BODY_CFG.DOT_GREEN);  // Groen
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, BODY_CFG.DOT_GREEN);
  int16_t xb1, yb1; uint16_t twb, thb;
  body_gfx->getTextBounds("Opslaan", 0, 0, &xb1, &yb1, &twb, &thb);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - twb) / 2 - xb1, btnY + (btnH + thb) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + 5, btnY + 10);
  #endif
  body_gfx->print("Opslaan");
  
  // TERUG (blauw)
  body_gfx->fillRoundRect(btn3X, btnY, btnW, btnH, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btn3X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);
  int16_t xc1, yc1; uint16_t twc, thc;
  body_gfx->getTextBounds("TERUG", 0, 0, &xc1, &yc1, &twc, &thc);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn3X + (btnW - twc) / 2 - xc1, btnY + (btnH + thc) / 2 - 2);
  #else
    body_gfx->setCursor(btn3X + 20, btnY + 10);
  #endif
  body_gfx->print("TERUG");
}

// ===== KALIBRATIE FUNCTIES =====
void startCalibration(uint8_t type) {
  isCalibrating = true;
  calibrationType = type;
  calibrationStartTime = millis();
  gsrSum = 0;
  tempSum = 0;
  hrSum = 0;
  sampleCount = 0;
  calibrationScreenDrawn = false; // Reset voor nieuwe kalibratie
  menuDirty = true;
  Serial.printf("[CAL] Start kalibratie type %d\n", type);
}

void updateCalibration() {
  if (!isCalibrating) return;
  
  uint32_t elapsed = millis() - calibrationStartTime;
  
  // Verzamel samples
  ADS1115_SensorData data = ads1115_getData();
  
  if (calibrationType == 0 || calibrationType == 1) { // GSR
    gsrSum += data.gsrRaw;
  }
  if (calibrationType == 0 || calibrationType == 2) { // Temp
    tempSum += data.temperature;
  }
  if (calibrationType == 0 || calibrationType == 3) { // Hart
    hrSum += data.pulseRaw;
  }
  sampleCount++;
  
  // Klaar met kalibratie?
  if (elapsed >= CALIBRATION_DURATION) {
    if (sampleCount > 0) {
      // Bereken gemiddeldes en sla op in EEPROM
      if (calibrationType == 0 || calibrationType == 1) {
        float gsrBaseline = gsrSum / sampleCount;
        sensorConfig.ads_gsrBaseline = gsrBaseline;
        ads1115_setGSRBaseline(gsrBaseline);
        Serial.printf("[CAL] GSR baseline: %.1f\n", gsrBaseline);
      }
      if (calibrationType == 0 || calibrationType == 2) {
        float tempAvg = tempSum / sampleCount;
        float tempOffset = 36.5f - tempAvg; // Normale lichaamstemperatuur
        sensorConfig.ads_ntcOffset = tempOffset;
        ads1115_setNTCOffset(tempOffset);
        Serial.printf("[CAL] Temp offset: %.2f\n", tempOffset);
      }
      if (calibrationType == 0 || calibrationType == 3) {
        int pulseBaseline = (int)(hrSum / sampleCount);
        sensorConfig.ads_pulseBaseline = pulseBaseline;
        ads1115_setPulseBaseline(pulseBaseline);
        Serial.printf("[CAL] Pulse baseline: %d\n", pulseBaseline);
      }
      
      // Sla kalibratie op in EEPROM
      saveSensorConfig();
      Serial.println("[CAL] Kalibratie opgeslagen in EEPROM");
    }
    
    isCalibrating = false;
    menuDirty = true;
    
    // Clear scherm na kalibratie
    body_gfx->fillScreen(BODY_CFG.COL_BG);
    
    Serial.println("[CAL] Kalibratie voltooid");
  }
}

void drawCalibrationScreen() {
  if (!body_gfx) return;
  
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // Teken statische delen alleen 1x
  if (!calibrationScreenDrawn) {
    body_gfx->fillScreen(BODY_CFG.COL_BG);
    
    uint16_t headerColor = 0x07E0; // Groen
    if (calibrationType == 1) headerColor = 0x07FF; // Cyaan (GSR)
    if (calibrationType == 2) headerColor = 0xFD20; // Oranje (Temp)
    if (calibrationType == 3) headerColor = 0xF800; // Rood (Hart)
    
    // Titel kader bovenaan (groter voor mooie fit)
    int titleBoxY = MENU_Y;
    int titleBoxH = 45;
    body_gfx->fillRoundRect(MENU_X, titleBoxY, MENU_W, titleBoxH, 8, 0x2104);
    body_gfx->drawRoundRect(MENU_X, titleBoxY, MENU_W, titleBoxH, 8, headerColor);
    body_gfx->drawRoundRect(MENU_X+1, titleBoxY+1, MENU_W-2, titleBoxH-2, 7, headerColor);
    
    setMenuFontTitle();
    body_gfx->setTextColor(headerColor, 0x2104);
    int16_t x1, y1; uint16_t tw, th;
    const char* title = "KALIBRATIE";
    body_gfx->getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
    body_gfx->setCursor(MENU_X + (MENU_W - tw) / 2 - x1, titleBoxY + (titleBoxH + th) / 2 - 3);
    body_gfx->print(title);
    
    // Sensor type kader eronder
    int sensorBoxY = titleBoxY + titleBoxH + 10;
    int sensorBoxH = 40;
    body_gfx->fillRoundRect(MENU_X, sensorBoxY, MENU_W, sensorBoxH, 8, headerColor);
    body_gfx->drawRoundRect(MENU_X, sensorBoxY, MENU_W, sensorBoxH, 8, 0xFFFF);
    body_gfx->drawRoundRect(MENU_X+1, sensorBoxY+1, MENU_W-2, sensorBoxH-2, 7, 0xFFFF);
    
    setMenuFontItem();
    body_gfx->setTextColor(0x0000, headerColor);
    const char* typeText[] = {"Alle Sensors", "GSR Sensor", "Temp Sensor", "Hart Sensor"};
    body_gfx->getTextBounds(typeText[calibrationType], 0, 0, &x1, &y1, &tw, &th);
    body_gfx->setCursor(MENU_X + (MENU_W - tw) / 2 - x1, sensorBoxY + (sensorBoxH + th) / 2 - 2);
    body_gfx->print(typeText[calibrationType]);
    
    calibrationScreenDrawn = true;
  }
  
  // Dynamische delen (progressbalk, sensor waardes)
  int16_t x1, y1; uint16_t tw, th;
  uint16_t headerColor = 0x07E0;
  if (calibrationType == 1) headerColor = 0x07FF;
  if (calibrationType == 2) headerColor = 0xFD20;
  if (calibrationType == 3) headerColor = 0xF800;
  
  // Progressbalk positie (lager op scherm)
  int barX = MENU_X + 20;
  int barY = MENU_Y + 115;
  int barW = MENU_W - 40;
  int barH = 30;
  
  // Teken progress bar frame alleen 1x (met mooi kader)
  if (!calibrationScreenDrawn) {
    body_gfx->fillRoundRect(barX, barY, barW, barH, 10, 0x1082);
    body_gfx->drawRoundRect(barX, barY, barW, barH, 10, 0xFFFF);
    body_gfx->drawRoundRect(barX+1, barY+1, barW-2, barH-2, 9, 0x8410);
    body_gfx->drawRoundRect(barX+2, barY+2, barW-4, barH-4, 8, 0x8410);
  }
  
  // Update alleen de vulling (elk frame)
  uint32_t elapsed = millis() - calibrationStartTime;
  float progress = (float)elapsed / CALIBRATION_DURATION;
  if (progress > 1.0f) progress = 1.0f;
  
  // Teken progressbalk vulling mooi (glad gradient)
  body_gfx->fillRoundRect(barX + 3, barY + 3, barW - 6, barH - 6, 7, 0x1082); // Wis binnenkant
  int fillW = (int)((barW - 6) * progress);
  if (fillW > 8) {
    // Vulling met mooie gradient (3 zones)
    for (int i = 0; i < barH - 6; i++) {
      uint16_t lineColor = headerColor;
      if (i < 3) lineColor = (headerColor | 0x1082);  // Highlight bovenaan
      else if (i > barH - 10) lineColor = (headerColor & 0xE71C) >> 1; // Shadow onderaan
      body_gfx->drawFastHLine(barX + 3, barY + 3 + i, fillW, lineColor);
    }
  }
  
  // Percentage ONDER de balk (groot)
  int percent = (int)(progress * 100);
  char percentText[8];
  sprintf(percentText, "%d%%", percent);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_TITLE);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(3);
  #endif
  body_gfx->getTextBounds(percentText, 0, 0, &x1, &y1, &tw, &th);
  
  // Wis oude percentage
  body_gfx->fillRect(MENU_X, barY + barH + 10, MENU_W, th + 15, BODY_CFG.COL_BG);
  
  body_gfx->setTextColor(headerColor, BODY_CFG.COL_BG);
  body_gfx->setCursor(MENU_X + (MENU_W - tw) / 2 - x1, barY + barH + 28);
  body_gfx->print(percentText);
  
  // Live sensor waardes in boxes (mooie kaders)
  ADS1115_SensorData data = ads1115_getData();
  int boxY = barY + barH + 75;  // Meer ruimte voor percentage en samples
  int boxW = (MENU_W - 70) / 3;
  int boxH = 70;
  int boxSpacing = 10;
  
  setMenuFontItem();
  int boxCount = 0;
  
  // Teken sensor boxes (frames en labels) alleen 1x
  if (!calibrationScreenDrawn) {
    if (calibrationType == 0 || calibrationType == 1) {
      int boxX = MENU_X + 30 + boxCount * (boxW + boxSpacing);
      body_gfx->fillRoundRect(boxX, boxY, boxW, boxH, 8, 0x1082);
      body_gfx->drawRoundRect(boxX, boxY, boxW, boxH, 8, 0x07FF);
      body_gfx->drawRoundRect(boxX+1, boxY+1, boxW-2, boxH-2, 7, 0x07FF);
      body_gfx->setTextColor(0x07FF, 0x1082);
      int16_t x1, y1; uint16_t tw, th;
      body_gfx->getTextBounds("GSR", 0, 0, &x1, &y1, &tw, &th);
      body_gfx->setCursor(boxX + (boxW - tw) / 2, boxY + 12);
      body_gfx->print("GSR");
      boxCount++;
    }
    if (calibrationType == 0 || calibrationType == 2) {
      int boxX = MENU_X + 30 + boxCount * (boxW + boxSpacing);
      body_gfx->fillRoundRect(boxX, boxY, boxW, boxH, 8, 0x1082);
      body_gfx->drawRoundRect(boxX, boxY, boxW, boxH, 8, 0xFD20);
      body_gfx->drawRoundRect(boxX+1, boxY+1, boxW-2, boxH-2, 7, 0xFD20);
      body_gfx->setTextColor(0xFD20, 0x1082);
      int16_t x1, y1; uint16_t tw, th;
      body_gfx->getTextBounds("TEMP", 0, 0, &x1, &y1, &tw, &th);
      body_gfx->setCursor(boxX + (boxW - tw) / 2, boxY + 12);
      body_gfx->print("TEMP");
      boxCount++;
    }
    if (calibrationType == 0 || calibrationType == 3) {
      int boxX = MENU_X + 30 + boxCount * (boxW + boxSpacing);
      body_gfx->fillRoundRect(boxX, boxY, boxW, boxH, 8, 0x1082);
      body_gfx->drawRoundRect(boxX, boxY, boxW, boxH, 8, 0xF800);
      body_gfx->drawRoundRect(boxX+1, boxY+1, boxW-2, boxH-2, 7, 0xF800);
      body_gfx->setTextColor(0xF800, 0x1082);
      int16_t x1, y1; uint16_t tw, th;
      body_gfx->getTextBounds("BPM", 0, 0, &x1, &y1, &tw, &th);
      body_gfx->setCursor(boxX + (boxW - tw) / 2, boxY + 12);
      body_gfx->print("BPM");
    }
  }
  
  // Update alleen waardes (elk frame)
  boxCount = 0;
  
  if (calibrationType == 0 || calibrationType == 1) {
    int boxX = MENU_X + 30 + boxCount * (boxW + boxSpacing);
    // Wis waarde gebied
    body_gfx->fillRoundRect(boxX + 6, boxY + 30, boxW - 12, 35, 5, 0x1082);
    body_gfx->setTextColor(0xFFFF, 0x1082);
    char val[16];
    sprintf(val, "%.0f", data.gsrSmooth);
    body_gfx->getTextBounds(val, 0, 0, &x1, &y1, &tw, &th);
    body_gfx->setCursor(boxX + (boxW - tw) / 2, boxY + 47);
    body_gfx->print(val);
    boxCount++;
  }
  if (calibrationType == 0 || calibrationType == 2) {
    int boxX = MENU_X + 30 + boxCount * (boxW + boxSpacing);
    body_gfx->fillRoundRect(boxX + 6, boxY + 30, boxW - 12, 35, 5, 0x1082);
    body_gfx->setTextColor(0xFFFF, 0x1082);
    char val[16];
    sprintf(val, "%.1f", data.temperature);
    body_gfx->getTextBounds(val, 0, 0, &x1, &y1, &tw, &th);
    body_gfx->setCursor(boxX + (boxW - tw) / 2, boxY + 47);
    body_gfx->print(val);
    boxCount++;
  }
  if (calibrationType == 0 || calibrationType == 3) {
    int boxX = MENU_X + 30 + boxCount * (boxW + boxSpacing);
    body_gfx->fillRoundRect(boxX + 6, boxY + 30, boxW - 12, 35, 5, 0x1082);
    body_gfx->setTextColor(0xFFFF, 0x1082);
    char val[16];
    sprintf(val, "%d", data.BPM);
    body_gfx->getTextBounds(val, 0, 0, &x1, &y1, &tw, &th);
    body_gfx->setCursor(boxX + (boxW - tw) / 2, boxY + 47);
    body_gfx->print(val);
  }
  
  // Samples counter in apart balkje onder percentage
  int sampleBarY = barY + barH + 55;
  int sampleBarW = 150;
  int sampleBarH = 20;
  int sampleBarX = MENU_X + (MENU_W - sampleBarW) / 2;
  
  // Teken samples balkje (1x)
  if (!calibrationScreenDrawn) {
    body_gfx->fillRoundRect(sampleBarX, sampleBarY, sampleBarW, sampleBarH, 6, 0x2104);
    body_gfx->drawRoundRect(sampleBarX, sampleBarY, sampleBarW, sampleBarH, 6, 0x8410);
  }
  
  // Update samples text
  body_gfx->fillRoundRect(sampleBarX + 2, sampleBarY + 2, sampleBarW - 4, sampleBarH - 4, 4, 0x2104);
  
  // Gebruik FONT_ITEM voor consistentie
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(1);
  #endif
  
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, 0x2104);
  char sampleText[32];
  sprintf(sampleText, "Samples: %d", sampleCount);
  body_gfx->getTextBounds(sampleText, 0, 0, &x1, &y1, &tw, &th);
  body_gfx->setCursor(sampleBarX + (sampleBarW - tw) / 2 - x1, sampleBarY + (sampleBarH + th) / 2 - 2);
  body_gfx->print(sampleText);
}

void drawSensorCalItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // Knop layout - zelfde breedte als TERUG knop
  const int BTN_W = MENU_W - 40;  // Zelfde breedte als TERUG knop (400px)
  const int BTN_H = 38;   // Hoogte
  const int BTN_SPACING = 5;  // Ruimte tussen knoppen
  const int START_X = MENU_X + 20;  // Zelfde start X als TERUG
  const int START_Y = MENU_Y + 50;  // Start hoger voor meer ruimte tot TERUG
  
  // Font instellen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  const char* items[] = {
    "Auto Kalibreer Alles",
    "GSR Sensor",
    "Temp Sensor",
    "Hart Sensor"
  };
  
  // Kleuren voor elke knop
  uint16_t colors[] = {
    0x07E0,  // Groen (Auto kalibreer alles)
    0x07FF,  // Cyaan (GSR)
    0xFD20,  // Oranje (Temp)
    0xF800   // Rood (Hart)
  };
  
  // Teken 4 knoppen
  for (int i = 0; i < 4; i++) {
    int y = START_Y + i * (BTN_H + BTN_SPACING);
    
    // Gekleurde knop met witte rand (zoals hoofdmenu)
    body_gfx->fillRoundRect(START_X, y, BTN_W, BTN_H, 8, colors[i]);
    body_gfx->drawRoundRect(START_X, y, BTN_W, BTN_H, 8, 0xFFFF);
    body_gfx->drawRoundRect(START_X+1, y+1, BTN_W-2, BTN_H-2, 7, 0xFFFF);  // Dubbele rand
    
    // Centreer tekst in knop (zwarte tekst)
    body_gfx->setTextColor(0x0000, colors[i]);
    int16_t x1, y1; uint16_t tw, th;
    body_gfx->getTextBounds(items[i], 0, 0, &x1, &y1, &tw, &th);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2 - x1, y + (BTN_H + th) / 2 - 2);
    #else
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2, y + (BTN_H - th) / 2 + th - 2);
    #endif
    body_gfx->print(items[i]);
  }
  
  // TERUG knop onderaan (blauw) - gecentreerd zoals andere menu's
  int btnY = MENU_Y + 230;
  int btnW = MENU_W - 40;
  int btnX = MENU_X + 20;
  body_gfx->fillRoundRect(btnX, btnY, btnW, 40, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btnX, btnY, btnW, 40, 8, 0xFFFF);
  
  // Centreer tekst in knop (verticaal)
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  body_gfx->setTextColor(0xFFFF, 0x001F);
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds("TERUG", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btnX + (btnW - tw) / 2 - x1, btnY + (40 + th) / 2 - 2);
  #else
    body_gfx->setCursor(btnX + (btnW - tw) / 2, btnY + 10);
  #endif
  body_gfx->print("TERUG");
}

// Geselecteerd bestand voor Recording menu
static int selectedRecordingFile = -1;
static String csvFiles[10];  // CSV bestanden lijst (globaal voor touch handler)
static int csvCount = -1;     // Aantal gevonden bestanden (-1 = nog niet gescand)
static uint32_t lastRecordingScan = 0;  // Laatste scan tijd

void drawRecordingItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(1);
  #endif
  
  // Links: bestandslijst area
  const int LIST_X = MENU_X + 20;
  const int LIST_Y = MENU_Y + 60;
  const int LIST_W = 230;
  
  // Rechts: knoppen area
  const int BTN_X = MENU_X + 270;
  const int BTN_Y = MENU_Y + 60;
  const int BTN_W = 150;
  const int BTN_H = 45;
  const int BTN_SPACING = 8;
  
  // Scan SD card voor .csv bestanden in /recordings/ (gebruikt globale variabelen)
  // Scan alleen als nodig (eerste keer of refresh na 1 seconde)
  if (csvCount == -1 || millis() - lastRecordingScan > 1000) {
    csvCount = 0;
    
    // Gebruik SD_MMC ipv SD (zoals in main .ino)
    extern bool formatSDCard();  // Zelfde SD_MMC als format functie
    File root = SD_MMC.open("/recordings");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file && csvCount < 10) {
        if (!file.isDirectory()) {
          String filename = String(file.name());
          if (filename.endsWith(".csv")) {
            csvFiles[csvCount] = filename;
            csvCount++;
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
    lastRecordingScan = millis();
    
    Serial.printf("[RECORDING MENU] Found %d CSV files in /recordings/\n", csvCount);
  }
  
  // Toon .csv bestanden met selectie
  if (csvCount == 0) {
    body_gfx->setTextColor(0xC618, BODY_CFG.COL_BG);  // Grijs
    body_gfx->setCursor(LIST_X, LIST_Y);
    body_gfx->print("Geen opnames gevonden");
  } else {
    for (int i = 0; i < csvCount && i < 8; i++) {  // Max 8 regels
      int y = LIST_Y + i * 20;
      
      // Highlight geselecteerd bestand
      if (i == selectedRecordingFile) {
        body_gfx->fillRoundRect(LIST_X - 2, y - 2, LIST_W, 18, 3, 0x07E0);  // Groen highlight
        body_gfx->setTextColor(0x0000, 0x07E0);  // Zwart op groen
      } else {
        body_gfx->setTextColor(0xFFFF, BODY_CFG.COL_BG);  // Wit
      }
      
      body_gfx->setCursor(LIST_X, y);
      // Kort bestandsnaam als te lang
      String shortName = csvFiles[i];
      if (shortName.length() > 25) {
        shortName = shortName.substring(0, 22) + "...";
      }
      body_gfx->print(shortName);
    }
  }
  
  // 4 knoppen rechts (verticaal)
  const char* btnLabels[] = {"PLAY", "DELETE", "AI analyze", "TERUG"};
  uint16_t btnColors[] = {0x07E0, 0xFD20, 0xF81F, 0x001F};  // Groen, Oranje, Magenta, Blauw
  
  for (int i = 0; i < 4; i++) {
    int y = BTN_Y + i * (BTN_H + BTN_SPACING);
    
    // Knop met dubbele witte rand
    body_gfx->fillRoundRect(BTN_X, y, BTN_W, BTN_H, 8, btnColors[i]);
    body_gfx->drawRoundRect(BTN_X, y, BTN_W, BTN_H, 8, 0xFFFF);
    body_gfx->drawRoundRect(BTN_X+1, y+1, BTN_W-2, BTN_H-2, 7, 0xFFFF);
    
    // Tekst gecentreerd (zwart op PLAY/AI, wit op DELETE/TERUG)
    uint16_t txtColor = (i == 1 || i == 3) ? 0xFFFF : 0x0000;  // Wit voor DELETE en TERUG
    body_gfx->setTextColor(txtColor, btnColors[i]);
    
    #if USE_ADAFRUIT_FONTS
      body_gfx->setFont(&FONT_ITEM);
      body_gfx->setTextSize(1);
    #else
      body_gfx->setTextSize(2);
    #endif
    
    int16_t x1, y1; uint16_t tw, th;
    body_gfx->getTextBounds(btnLabels[i], 0, 0, &x1, &y1, &tw, &th);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(BTN_X + (BTN_W - tw) / 2 - x1, y + (BTN_H + th) / 2 - 2);
    #else
      body_gfx->setCursor(BTN_X + (BTN_W - tw) / 2, y + (BTN_H - th) / 2 + th - 2);
    #endif
    body_gfx->print(btnLabels[i]);
  }
}

// void drawESPStatusItems() {
//   const int MENU_X = 20;
//   const int MENU_Y = 20;
//   const int MENU_W = 480 - 40;
//   
//   setMenuFontItem();
//   int y = MENU_Y + 60;
//   
//   body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
//   body_gfx->setCursor(MENU_X + 30, y);
//   body_gfx->print("ESP-NOW:");
//   y += 35;
//   
//   // ESP status met kleur
//   bool espOK = getEspNowInitialized();
//   uint16_t statusColor = espOK ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_RED;
//   body_gfx->setTextColor(statusColor, BODY_CFG.COL_BG);
//   body_gfx->setCursor(MENU_X + 30, y);
//   body_gfx->printf("%s", espOK ? "OK" : "FOUT");
//   y += 30;
//   
//   // Timing info
//   body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
//   uint32_t timeSince = millis() - getLastCommTime();
//   body_gfx->setCursor(MENU_X + 30, y);
//   body_gfx->printf("Last RX:");
//   y += 25;
//   body_gfx->setCursor(MENU_X + 40, y);
//   body_gfx->printf("%u sec", timeSince / 1000);
//   y += 30;
//   
//   // Data values
//   body_gfx->setCursor(MENU_X + 30, y);
//   body_gfx->printf("Trust: %.1f", getTrustSpeed());
//   y += 25;
//   body_gfx->setCursor(MENU_X + 30, y);
//   body_gfx->printf("Sleeve: %.1f", getSleeveSpeed());
//   
//   // TERUG knop onderaan (blauw)
//   int btnY = MENU_Y + 230;
//   int btnW = MENU_W - 40;
//   int btnX = MENU_X + 20;
//   body_gfx->fillRoundRect(btnX, btnY, btnW, 40, 8, 0x001F);  // Blauw
//   body_gfx->drawRoundRect(btnX, btnY, btnW, 40, 8, 0xFFFF);
//   
//   body_gfx->setTextColor(0xFFFF, 0x001F);
//   int16_t x1, y1; uint16_t tw, th;
//   body_gfx->getTextBounds("TERUG", 0, 0, &x1, &y1, &tw, &th);
//   body_gfx->setCursor(btnX + (btnW - tw) / 2, btnY + 10);
//   body_gfx->print("TERUG");
// }

void drawSensorSettingsItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // Font instellen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  // Subtitle
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  body_gfx->setCursor(MENU_X + 20, MENU_Y + 50);
  body_gfx->print("Kalibratie & drempelwaarden");
  
  int y = MENU_Y + 75;
  const int LINE_H = 25;
  const int VAL_X = MENU_X + 210;  // Waarde positie
  const int BTN_W = 35;  // +/- knop breedte
  const int BTN_H = 20;  // +/- knop hoogte
  const int MINUS_X = MENU_X + 320;  // - knop X
  const int PLUS_X = MENU_X + 360;   // + knop X
  
  // Labels en waarden (dynamisch uit sensorEdit)
  const char* labels[] = {"Hartslag Drempel:", "Temp Correctie:", "Temp Demping:", "GSR Basis:", "GSR Gevoelig:", "GSR Demping:"};
  float values[] = {
    sensorEdit.beatThreshold,
    sensorEdit.tempOffset,
    sensorEdit.tempSmoothing,
    sensorEdit.gsrBaseline,
    sensorEdit.gsrSensitivity,
    sensorEdit.gsrSmoothing
  };
  
  for (int i = 0; i < 6; i++) {
    // Label links (lichtere grijs)
    body_gfx->setTextColor(0xC618, BODY_CFG.COL_BG);  // Lichtgrijs
    body_gfx->setCursor(MENU_X + 20, y);
    body_gfx->print(labels[i]);
    
    // Waarde veld (donkere box)
    int valBoxY = y - 5;  // 3px naar beneden (was y - 8)
    int valBoxH = 20;
    body_gfx->fillRoundRect(VAL_X, valBoxY, 100, valBoxH, 3, 0x2104);  // Donkergrijs
    body_gfx->drawRoundRect(VAL_X, valBoxY, 100, valBoxH, 3, 0x8410);  // Lichtgrijze rand
    body_gfx->setTextColor(0xFFFF, 0x2104);  // Witte tekst
    
    // Centreer waarde in box (horizontaal + verticaal)
    char valStr[12];
    sprintf(valStr, "%.2f", values[i]);
    int16_t xv1, yv1; uint16_t twv, thv;
    body_gfx->getTextBounds(valStr, 0, 0, &xv1, &yv1, &twv, &thv);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(VAL_X + (100 - twv) / 2 - xv1, valBoxY + valBoxH / 2 + thv / 2);
    #else
      body_gfx->setCursor(VAL_X + 10, y);
    #endif
    body_gfx->print(valStr);
    
    // - knop (blauw) gecentreerd
    int btnBoxY = y - 5;  // 3px naar beneden (was y - 8)
    body_gfx->fillRoundRect(MINUS_X, btnBoxY, BTN_W, BTN_H, 3, 0x001F);  // Blauw
    body_gfx->drawRoundRect(MINUS_X, btnBoxY, BTN_W, BTN_H, 3, 0xFFFF);
    body_gfx->setTextColor(0xFFFF, 0x001F);  // Witte tekst op blauw
    int16_t xm1, ym1; uint16_t twm, thm;
    body_gfx->getTextBounds("-", 0, 0, &xm1, &ym1, &twm, &thm);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(MINUS_X + (BTN_W - twm) / 2 - xm1, btnBoxY + BTN_H / 2 + thm / 2);
    #else
      body_gfx->setCursor(MINUS_X + 12, y);
    #endif
    body_gfx->print("-");
    
    // + knop (rood) gecentreerd
    body_gfx->fillRoundRect(PLUS_X, btnBoxY, BTN_W, BTN_H, 3, 0xF800);  // Rood
    body_gfx->drawRoundRect(PLUS_X, btnBoxY, BTN_W, BTN_H, 3, 0xFFFF);
    body_gfx->setTextColor(0xFFFF, 0xF800);  // Witte tekst op rood
    int16_t xp1, yp1; uint16_t twp, thp;
    body_gfx->getTextBounds("+", 0, 0, &xp1, &yp1, &twp, &thp);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(PLUS_X + (BTN_W - twp) / 2 - xp1, btnBoxY + BTN_H / 2 + thp / 2);
    #else
      body_gfx->setCursor(PLUS_X + 12, y);
    #endif
    body_gfx->print("+");
    
    y += LINE_H;
  }
  
  // 2 knoppen onderaan - gecentreerd
  int btnY = MENU_Y + 225;
  int btnW = 200;
  int btnH = 40;
  int btnSpacing = 10;  // Ruimte tussen knoppen
  int totalBtnW = btnW * 2 + btnSpacing;
  int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // Opslaan links
  int btn2X = btn1X + btnW + btnSpacing;  // TERUG rechts
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  // Opslaan (groen, links)
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, BODY_CFG.DOT_GREEN);  // Groen
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, BODY_CFG.DOT_GREEN);
  int16_t xa1, ya1; uint16_t twa, tha;
  body_gfx->getTextBounds("Opslaan", 0, 0, &xa1, &ya1, &twa, &tha);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - twa) / 2 - xa1, btnY + (btnH + tha) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + 70, btnY + 10);
  #endif
  body_gfx->print("Opslaan");
  
  // TERUG (blauw, rechts)
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);
  int16_t xb1, yb1; uint16_t twb, thb;
  body_gfx->getTextBounds("TERUG", 0, 0, &xb1, &yb1, &twb, &thb);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - twb) / 2 - xb1, btnY + (btnH + thb) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + 65, btnY + 10);
  #endif
  body_gfx->print("TERUG");
}

void bodyMenuHandleTouch(int16_t x, int16_t y, bool pressed) {
  if (!pressed) return;
  
  menuDirty = true;
  
  if (bodyMenuMode == BODY_MODE_SENSORS) {
    bodyMenuMode = BODY_MODE_MENU;
    bodyMenuPage = BODY_PAGE_MAIN;
    bodyMenuIdx = 0;
  } else {
    // Nieuwe brede menu layout (20px marge)
    const int MENU_X = 20;
    const int MENU_Y = 20;
    const int MENU_W = 480 - 40;
    
    // Voor HOOFDMENU: 2 kolommen + TERUG
    if (bodyMenuPage == BODY_PAGE_MAIN) {
      const int BTN_W = 190;
      const int BTN_H = 50;
      const int BTN_MARGIN_X = 20;
      const int BTN_MARGIN_Y = 8;
      const int START_X = MENU_X + 20;
      const int START_Y = MENU_Y + 45;  // Match met draw functie
      
      // Check 6 knoppen (2 kolommen, 3 rijen)
      for (int i = 0; i < 6; i++) {
        int col = i / 3;  // 0 (links) of 1 (rechts)
        int row = i % 3;  // 0, 1, 2
        int btnX = START_X + col * (BTN_W + BTN_MARGIN_X);
        int btnY = START_Y + row * (BTN_H + BTN_MARGIN_Y);
        
        if (x >= btnX && x <= btnX + BTN_W && y >= btnY && y <= btnY + BTN_H) {
          bodyMenuIdx = i;
          
          // Nieuwe volgorde: 0=AI Training, 1=Opname, 2=AI Settings, 3=Kalibratie, 4=Sensors, 5=Instellingen
          if (i == 0) {
            parentPage = BODY_PAGE_MAIN;  // Onthoud waar we vandaan komen
            bodyMenuPage = BODY_PAGE_ML_TRAINING;
            bodyMenuIdx = 0;
            Serial.println("[MENU] Entering AI Training");
          } else if (i == 1) {
            parentPage = BODY_PAGE_MAIN;
            bodyMenuPage = BODY_PAGE_RECORDING;
            bodyMenuIdx = 0;
          } else if (i == 2) {
            parentPage = BODY_PAGE_MAIN;
            bodyMenuPage = BODY_PAGE_AI_SETTINGS;
            bodyMenuIdx = 0;
            // Laad AI settings (voor het geval ze gewijzigd zijn)
            loadAISettings();
          } else if (i == 3) {
            parentPage = BODY_PAGE_MAIN;
            bodyMenuPage = BODY_PAGE_SENSOR_CAL;
            bodyMenuIdx = 0;
          } else if (i == 4) {
            parentPage = BODY_PAGE_MAIN;
            bodyMenuPage = BODY_PAGE_SENSOR_SETTINGS;
            bodyMenuIdx = 0;
            // Laad sensor settings voor edit
            loadSensorSettingsToEdit();
          } else if (i == 5) {
            parentPage = BODY_PAGE_MAIN;
            bodyMenuPage = BODY_PAGE_SYSTEM_SETTINGS;
            bodyMenuIdx = 0;
            Serial.println("[MENU] Entering System Settings");
          }
          return;
        }
      }
      
      // Check TERUG knop onderaan
      int btnY = MENU_Y + 227;  // Match met draw functie
      int btnW = 300;
      int btnX = MENU_X + (MENU_W - btnW) / 2;
      if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + 45) {
        bodyMenuMode = BODY_MODE_SENSORS;
        return;
      }
    } else {
      // Voor SUBMENU's: Check TERUG knop onderaan (BEHALVE voor pagina's met eigen knoppen)
      if (bodyMenuPage != BODY_PAGE_TIME_SETTINGS && 
          bodyMenuPage != BODY_PAGE_FORMAT_CONFIRM && 
          bodyMenuPage != BODY_PAGE_AI_SETTINGS && 
          bodyMenuPage != BODY_PAGE_SENSOR_SETTINGS &&
          bodyMenuPage != BODY_PAGE_PLAYBACK) {  // 🔥 NIEUW!  
        int btnY = MENU_Y + 230;
        int btnX = MENU_X + 20;
        int btnW = MENU_W - 40;
        
        if (x >= btnX && x <= btnX + btnW && y >= btnY && y <= btnY + 40) {
          // Ga terug naar parent page (1 stap terug in hiërarchie)
          bodyMenuPage = parentPage;
          bodyMenuIdx = 0;
          Serial.printf("[MENU] Going back to parent page %d\n", parentPage);
          return;
        }
      }
      
      // Sensor calibratie menu item clicks
      if (bodyMenuPage == BODY_PAGE_SENSOR_CAL) {
        const int BTN_W = MENU_W - 40;
        const int BTN_H = 38;
        const int BTN_SPACING = 5;
        const int START_X = MENU_X + 20;
        const int START_Y = MENU_Y + 50;
        
        // Check of een van de 4 knoppen is geraakt
        for (int i = 0; i < 4; i++) {
          int btnY = START_Y + i * (BTN_H + BTN_SPACING);
          if (x >= START_X && x <= START_X + BTN_W && y >= btnY && y <= btnY + BTN_H) {
            // Start kalibratie: 0=Alle, 1=GSR, 2=Temp, 3=Hart
            startCalibration(i);
            return;
          }
        }
      }
      
      // Recording menu knoppen (rechts verticaal)
      if (bodyMenuPage == BODY_PAGE_RECORDING) {
        const int LIST_X = MENU_X + 20;
        const int LIST_Y = MENU_Y + 60;
        const int LIST_W = 230;
        
        const int BTN_X = MENU_X + 270;
        const int BTN_Y = MENU_Y + 60;
        const int BTN_W = 150;
        const int BTN_H = 45;
        const int BTN_SPACING = 8;
        
        // Check bestandsselectie (links)
        if (x >= LIST_X && x <= LIST_X + LIST_W && y >= LIST_Y) {
          int fileIndex = (y - LIST_Y) / 20;
          if (fileIndex >= 0 && fileIndex < csvCount && fileIndex < 8) {  // Max 8 zichtbare bestanden
            selectedRecordingFile = fileIndex;
            Serial.printf("[RECORDING] Selected file index: %d (%s)\n", fileIndex, csvFiles[fileIndex].c_str());
            menuDirty = true;  // Force redraw om highlight te tonen
            return;
          }
        }
        
        // Check of een van de 4 knoppen is geraakt
        for (int i = 0; i < 4; i++) {
          int btnY = BTN_Y + i * (BTN_H + BTN_SPACING);
          if (x >= BTN_X && x <= BTN_X + BTN_W && y >= btnY && y <= btnY + BTN_H) {
            // Recording acties: 0=PLAY, 1=DELETE, 2=AI analyze, 3=TERUG
            switch(i) {
              case 0:
                // PLAY - start playback
                if (selectedRecordingFile >= 0 && selectedRecordingFile < csvCount) {
                  Serial.printf("[RECORDING] Starting playback: %s\n", csvFiles[selectedRecordingFile].c_str());
                  // Kopieer naar selectedPlaybackFile
                  strncpy(selectedPlaybackFile, csvFiles[selectedRecordingFile].c_str(), sizeof(selectedPlaybackFile) - 1);
                  selectedPlaybackFile[sizeof(selectedPlaybackFile) - 1] = '\0';
                  // Start playback
                  startPlayback(selectedPlaybackFile);
                } else {
                  Serial.println("[RECORDING] Selecteer eerst een bestand!");
                }
                break;
              case 1:
                // DELETE geselecteerd bestand
                if (selectedRecordingFile >= 0 && selectedRecordingFile < csvCount) {
                  String filepath = "/recordings/" + csvFiles[selectedRecordingFile];
                  Serial.printf("[RECORDING] Deleting: %s\n", filepath.c_str());
                  
                  if (SD_MMC.remove(filepath.c_str())) {
                    Serial.println("[RECORDING] File deleted successfully!");
                    
                    // Reset lijst voor refresh
                    csvCount = -1;  // Trigger rescan bij volgende draw
                    selectedRecordingFile = -1;
                    menuDirty = true;
                  } else {
                    Serial.println("[RECORDING] ERROR: Failed to delete file!");
                  }
                } else {
                  Serial.println("[RECORDING] Selecteer eerst een bestand om te verwijderen!");
                }
                break;
              case 2:
                if (selectedRecordingFile >= 0) {
                  Serial.println("[RECORDING] Starting AI analyze...");
                  // TODO: AI Analyze implementatie
                } else {
                  Serial.println("[RECORDING] Selecteer eerst een bestand!");
                }
                break;
              case 3:
                // TERUG knop
                bodyMenuPage = BODY_PAGE_MAIN;
                bodyMenuIdx = 0;
                selectedRecordingFile = -1;  // Reset selectie
                Serial.println("[RECORDING] Terug naar hoofdmenu");
                break;
            }
            return;
          }
        }
      }
      
      // ML Training menu item clicks
      if (bodyMenuPage == BODY_PAGE_ML_TRAINING) {
        const int BTN_W = MENU_W - 40;
        const int BTN_H = 38;
        const int BTN_SPACING = 5;
        const int START_X = MENU_X + 20;
        const int START_Y = MENU_Y + 60;  // Match met drawMLTrainingView
        
        // Check of een van de 4 knoppen is geraakt
        for (int i = 0; i < 4; i++) {
          int btnY = START_Y + i * (BTN_H + BTN_SPACING);
          if (x >= START_X && x <= START_X + BTN_W && y >= btnY && y <= btnY + BTN_H) {
            // ML Training acties: 0=Data Opnemen, 1=Model Trainen, 2=AI Annotatie, 3=Model Manager
            switch(i) {
              case 0:
                // Toggle recording aan/uit
                if (g_isRecording && *g_isRecording) {
                  // Stop recording
                  *g_isRecording = false;
                  Serial.println("[ML TRAINING] Recording STOP");
                } else if (g_isRecording) {
                  // Start recording
                  *g_isRecording = true;
                  Serial.println("[ML TRAINING] Recording START");
                }
                break;
              case 1:
                Serial.println("[ML TRAINING] Model Trainen - TODO");
                break;
              case 2:
                Serial.println("[ML TRAINING] AI Annotatie - TODO");
                break;
              case 3:
                Serial.println("[ML TRAINING] Model Manager - TODO");
                break;
            }
            return;
          }
        }
      }
      
      // Playback scherm touch handling
      if (bodyMenuPage == BODY_PAGE_PLAYBACK) {
        // Stress popup heeft prioriteit
        if (stressPopupActive) {
          // Popup coordinates
          int popupW = 300;
          int popupH = 260;
          int popupX = (480 - popupW) / 2;
          int popupY = (320 - popupH) / 2;
          
          // Check radio buttons (7 stress levels)
          int levelY = popupY + 50;
          int levelH = 20;
          int levelSpacing = 3;
          int circleX = popupX + 20;
          
          for (int i = 0; i < 7; i++) {
            int circleY = levelY + i * (levelH + levelSpacing) + levelH / 2;
            int circleR = 6;
            
            // Check of cirkel geraakt is (ruime hitbox)
            if (x >= circleX - circleR - 10 && x <= circleX + circleR + 200 &&
                y >= circleY - circleR - 5 && y <= circleY + circleR + 5) {
              selectedStressLevel = i;
              Serial.printf("[PLAYBACK] Selected stress level: %d\n", i);
              menuDirty = true;
              return;
            }
          }
          
          // Check ANNULEREN / OPSLAAN knoppen
          int btnY = popupY + popupH - 50;
          int btnW = 120;
          int btnH = 35;
          int btnSpacing = 10;
          int totalBtnW = btnW * 2 + btnSpacing;
          int btn1X = popupX + (popupW - totalBtnW) / 2;
          int btn2X = btn1X + btnW + btnSpacing;
          
          // ANNULEREN
          if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
            Serial.println("[PLAYBACK] Stress popup geannuleerd");
            stressPopupActive = false;
            g4_pauseRendering = false;  // 🔥 NIEUW: Hervat grafiek rendering
            bodyMenuPage = BODY_PAGE_PLAYBACK;  // 🔥 NIEUW: Blijf in playback!

            // 🔥 NIEUW: Wis popup area EERST
            //body_gfx->fillScreen(BODY_CFG.COL_BG);

            // 🔥 NIEUW: Wis alleen popup area (niet grafieken)
            int popupW = 300;
            int popupH = 260;
            int popupX = (480 - popupW) / 2;
            int popupY = (320 - popupH) / 2;
            body_gfx->fillRect(popupX - 10, popupY - 10, popupW + 20, popupH + 20, BODY_CFG.COL_BG);

            playbackScreenDrawn = false;  // 🔥 NIEUW: Force hertekenen
            menuDirty = true;
            return;
          }
          
          // OPSLAAN
          if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
            Serial.printf("[PLAYBACK] Stress level opgeslagen: %d\n", selectedStressLevel);
            
            // Voeg marker toe
            if (stressMarkerCount < 100) {
              stressMarkers[stressMarkerCount].timestamp = millis() - playbackLastUpdate;
              stressMarkers[stressMarkerCount].stressLevel = selectedStressLevel;
              stressMarkerCount++;
              Serial.printf("[PLAYBACK] Marker toegevoegd (%d/%d)\n", stressMarkerCount, 100);
            }
            
            stressPopupActive = false;
            g4_pauseRendering = false;  // 🔥 NIEUW: Hervat grafiek rendering
            bodyMenuPage = BODY_PAGE_PLAYBACK;  // 🔥 NIEUW: Blijf in playback!

            // 🔥 NIEUW: Wis popup area EERST
            //body_gfx->fillScreen(BODY_CFG.COL_BG);

            // 🔥 NIEUW: Wis alleen popup area (niet grafieken)
            int popupW = 300;
            int popupH = 260;
            int popupX = (480 - popupW) / 2;
            int popupY = (320 - popupH) / 2;
            body_gfx->fillRect(popupX - 10, popupY - 10, popupW + 20, popupH + 20, BODY_CFG.COL_BG);

            playbackScreenDrawn = false;  // 🔥 NIEUW: Force hertekenen
            menuDirty = true;
            return;
          }
          
          return;  // Popup actief, blokkeer rest
        }
        
        // Speed controls (bovenaan rechts)
        int speedY = 0;
        int progressBarH = 25;  // Correcte hoogte (zelfde als in drawPlaybackScreen)
        int speedBtnW = 25;
        int speedBtnH = progressBarH;
        int speedX = 480 - 100 + 5;
        int plusX = speedX + speedBtnW + 42;  // Na speed text
        
        // - knop
        if (x >= speedX && x <= speedX + speedBtnW && y >= speedY && y <= speedY + speedBtnH) {
          playbackSpeed = max(10.0f, playbackSpeed - 10.0f);
          Serial.printf("[PLAYBACK] Speed: %.0f%%\n", playbackSpeed);
          menuDirty = true;
          return;
        }
        
        // + knop
        if (x >= plusX && x <= plusX + speedBtnW && y >= speedY && y <= speedY + speedBtnH) {
          playbackSpeed = min(200.0f, playbackSpeed + 10.0f);
          Serial.printf("[PLAYBACK] Speed: %.0f%%\n", playbackSpeed);
          menuDirty = true;
          return;
        }
        
        // Knoppen onderaan
        int btnY = 320 - 40;
        int btnH = 35;
        int btnW = 112;
        int btn1X = 5;      // STOP
        int btn2X = 122;    // PLAY/PAUZE
        int btn4X = 356;    // AI-ACTIE
        
        // STOP knop
        if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
          Serial.println("[PLAYBACK] STOP");
          stopPlayback();
          menuDirty = true;
          return;
        }
        
        // PLAY/PAUZE knop
        if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
          isPlaybackPaused = !isPlaybackPaused;
          Serial.printf("[PLAYBACK] %s\n", isPlaybackPaused ? "PAUZE" : "PLAY");
          menuDirty = true;
          return;
        }
        
        // AI-ACTIE knop
        if (x >= btn4X && x <= btn4X + btnW && y >= btnY && y <= btnY + btnH) {
          Serial.println("[PLAYBACK] AI-ACTIE - open stress popup");
          stressPopupActive = true;
          selectedStressLevel = 2;  // Default: normaal
          menuDirty = true;
          return;
        }
      }
      
      // System Settings menu item clicks
      if (bodyMenuPage == BODY_PAGE_SYSTEM_SETTINGS) {
        const int BTN_W = MENU_W - 40;
        const int BTN_H = 38;
        const int BTN_SPACING = 5;
        const int START_X = MENU_X + 20;
        const int START_Y = MENU_Y + 50;
        
        // Check of een van de 4 knoppen is geraakt
        for (int i = 0; i < 4; i++) {
          int btnY = START_Y + i * (BTN_H + BTN_SPACING);
          if (x >= START_X && x <= START_X + BTN_W && y >= btnY && y <= btnY + BTN_H) {
            // System Settings acties: 0=Scherm Rotatie, 1=SD Laden, 2=Format SD, 3=Tijd Instellen
            switch(i) {
              case 0:
                // Toggle scherm rotatie (180°)
                extern void toggleScreenRotation();
                toggleScreenRotation();
                Serial.println("[SYSTEM] Screen rotation toggled");
                break;
              case 1:
                // Open Funscript Settings pagina
                parentPage = BODY_PAGE_SYSTEM_SETTINGS;  // Onthoud parent
                bodyMenuPage = BODY_PAGE_FUNSCRIPT_SETTINGS;
                bodyMenuIdx = 0;
                Serial.println("[SYSTEM] Entering Funscript Settings");
                break;
              case 2:
                // Open format bevestigingsscherm
                parentPage = BODY_PAGE_SYSTEM_SETTINGS;  // Onthoud parent
                bodyMenuPage = BODY_PAGE_FORMAT_CONFIRM;
                bodyMenuIdx = 0;
                Serial.println("[SYSTEM] Opening format confirmation screen");
                break;
              case 3:
                // Laad huidige RTC tijd
                extern RTC_DS3231 rtc;
                extern bool rtcAvailable;
                if (rtcAvailable) {
                  DateTime now = rtc.now();
                  editYear = now.year();
                  editMonth = now.month();
                  editDay = now.day();
                  editHour = now.hour();
                  editMinute = now.minute();
                  Serial.printf("[SYSTEM] Loaded RTC time: %04d-%02d-%02d %02d:%02d\n",
                                editYear, editMonth, editDay, editHour, editMinute);
                } else {
                  Serial.println("[SYSTEM] RTC not available - using default time");
                }
                parentPage = BODY_PAGE_SYSTEM_SETTINGS;  // Onthoud parent
                bodyMenuPage = BODY_PAGE_TIME_SETTINGS;
                bodyMenuIdx = 0;
                Serial.println("[SYSTEM] Entering Time Settings");
                break;
            }
            return;
          }
        }
      }
      
      // Time Settings knoppen
      if (bodyMenuPage == BODY_PAGE_TIME_SETTINGS) {
        // Check EERST de OPSLAAN en TERUG knoppen onderaan (voor ze hebben prioriteit)
        int btnY = MENU_Y + 230;
        int btnW = 190;
        int btnH = 40;
        int btnSpacing = 20;
        int totalBtnW = btnW * 2 + btnSpacing;
        int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // OPSLAAN
        int btn2X = btn1X + btnW + btnSpacing;  // TERUG
        
        // OPSLAAN knop
        if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
          // Gebruik saveRTCTime() functie uit main .ino file
          extern bool saveRTCTime(int year, int month, int day, int hour, int minute);
          
          if (saveRTCTime(editYear, editMonth, editDay, editHour, editMinute)) {
            Serial.println("[TIME] Time saved successfully!");
          } else {
            Serial.println("[TIME] Failed to save time!");
          }
          
          // Terug naar System Settings
          bodyMenuPage = BODY_PAGE_SYSTEM_SETTINGS;
          bodyMenuIdx = 0;
          return;
        }
        
        // TERUG knop
        if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
          // Terug naar System Settings zonder opslaan
          bodyMenuPage = BODY_PAGE_SYSTEM_SETTINGS;
          bodyMenuIdx = 0;
          Serial.println("[TIME] Cancelled - returning to System Settings");
          return;
        }
        
        // Check +/- knoppen voor elk veld (jaar, maand, dag, uur, minuut)
        const int VAL_X = MENU_X + 150;
        const int BTN_W = 50;
        const int BTN_H = 28;
        const int MINUS_X = MENU_X + 280;
        const int PLUS_X = MENU_X + 340;
        const int START_Y = MENU_Y + 60;
        const int LINE_H = 35;
        
        for (int i = 0; i < 5; i++) {
          int btnBoxY = START_Y + i * LINE_H - 7;
          
          // - knop
          if (x >= MINUS_X && x <= MINUS_X + BTN_W && y >= btnBoxY && y <= btnBoxY + BTN_H) {
            switch(i) {
              case 0: if (editYear > 2000) editYear--; break;  // Jaar
              case 1: if (editMonth > 1) editMonth--; else editMonth = 12; break;  // Maand (1-12)
              case 2: if (editDay > 1) editDay--; else editDay = 31; break;  // Dag (1-31)
              case 3: if (editHour > 0) editHour--; else editHour = 23; break;  // Uur (0-23)
              case 4: if (editMinute > 0) editMinute--; else editMinute = 59; break;  // Minuut (0-59)
            }
            Serial.printf("[TIME] Decreased field %d\n", i);
            return;
          }
          
          // + knop
          if (x >= PLUS_X && x <= PLUS_X + BTN_W && y >= btnBoxY && y <= btnBoxY + BTN_H) {
            switch(i) {
              case 0: if (editYear < 2100) editYear++; break;  // Jaar
              case 1: if (editMonth < 12) editMonth++; else editMonth = 1; break;  // Maand (1-12)
              case 2: if (editDay < 31) editDay++; else editDay = 1; break;  // Dag (1-31)
              case 3: if (editHour < 23) editHour++; else editHour = 0; break;  // Uur (0-23)
              case 4: if (editMinute < 59) editMinute++; else editMinute = 0; break;  // Minuut (0-59)
            }
            Serial.printf("[TIME] Increased field %d\n", i);
            return;
          }
        }
      }
      
      // AI Settings knoppen
      if (bodyMenuPage == BODY_PAGE_AI_SETTINGS) {
        const int MENU_X = 20;
        const int MENU_Y = 20;
        const int MENU_W = 480 - 40;
        
        // Check EERST de onderste knoppen (AI AAN, Opslaan, TERUG)
        int btnY = MENU_Y + 225;
        int btnW = 120;
        int btnH = 40;
        int btnSpacing = 15;
        int totalBtnW = btnW * 3 + btnSpacing * 2;
        int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // AI AAN
        int btn2X = btn1X + btnW + btnSpacing;  // Opslaan
        int btn3X = btn2X + btnW + btnSpacing;  // TERUG
        
        // AI AAN knop
        if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
          aiSettings.aiEnabled = !aiSettings.aiEnabled;  // Toggle
          Serial.printf("[AI SETTINGS] AI toggled: %s\n", aiSettings.aiEnabled ? "AAN" : "UIT");
          menuDirty = true;
          return;
        }
        
        // Opslaan knop
        if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
          saveAISettings();
          applyAISettings();
          Serial.println("[AI SETTINGS] Settings opgeslagen en toegepast!");
          return;
        }
        
        // TERUG knop
        if (x >= btn3X && x <= btn3X + btnW && y >= btnY && y <= btnY + btnH) {
          bodyMenuPage = parentPage;
          bodyMenuIdx = 0;
          Serial.println("[AI SETTINGS] Terug naar parent");
          return;
        }
        
        // Check +/- knoppen voor elk veld (6 velden)
        const int VAL_X = MENU_X + 210;
        const int BTN_W = 35;
        const int BTN_H = 20;
        const int MINUS_X = MENU_X + 320;
        const int PLUS_X = MENU_X + 360;
        const int START_Y = MENU_Y + 75;
        const int LINE_H = 25;
        
        for (int i = 0; i < 6; i++) {
          int btnBoxY = START_Y + i * LINE_H - 5;
          
          // - knop
          if (x >= MINUS_X && x <= MINUS_X + BTN_W && y >= btnBoxY && y <= btnBoxY + BTN_H) {
            switch(i) {
              case 0: // AI Eigenwil (0-100%)
                if (aiSettings.autonomyLevel > 0.0f) aiSettings.autonomyLevel -= 1.0f;
                break;
              case 1: // HR Laag (40-120)
                if (aiSettings.hrLow > 40.0f) aiSettings.hrLow -= 1.0f;
                break;
              case 2: // HR Hoog (100-200)
                if (aiSettings.hrHigh > 100.0f) aiSettings.hrHigh -= 1.0f;
                break;
              case 3: // Temp Max (36.0-42.0)
                if (aiSettings.tempMax > 36.0f) aiSettings.tempMax -= 0.1f;
                break;
              case 4: // GSR Max (100-2000)
                if (aiSettings.gsrMax > 100.0f) aiSettings.gsrMax -= 10.0f;
                break;
              case 5: // Response Rate (0.0-1.0)
                if (aiSettings.responseRate > 0.01f) aiSettings.responseRate -= 0.01f;
                break;
            }
            Serial.printf("[AI SETTINGS] Decreased field %d\n", i);
            menuDirty = true;
            return;
          }
          
          // + knop
          if (x >= PLUS_X && x <= PLUS_X + BTN_W && y >= btnBoxY && y <= btnBoxY + BTN_H) {
            switch(i) {
              case 0: // AI Eigenwil (0-100%)
                if (aiSettings.autonomyLevel < 100.0f) aiSettings.autonomyLevel += 1.0f;
                break;
              case 1: // HR Laag (40-120)
                if (aiSettings.hrLow < 120.0f) aiSettings.hrLow += 1.0f;
                break;
              case 2: // HR Hoog (100-200)
                if (aiSettings.hrHigh < 200.0f) aiSettings.hrHigh += 1.0f;
                break;
              case 3: // Temp Max (36.0-42.0)
                if (aiSettings.tempMax < 42.0f) aiSettings.tempMax += 0.1f;
                break;
              case 4: // GSR Max (100-2000)
                if (aiSettings.gsrMax < 2000.0f) aiSettings.gsrMax += 10.0f;
                break;
              case 5: // Response Rate (0.0-1.0)
                if (aiSettings.responseRate < 1.0f) aiSettings.responseRate += 0.01f;
                break;
            }
            Serial.printf("[AI SETTINGS] Increased field %d\n", i);
            menuDirty = true;
            return;
          }
        }
      }
      
      // Sensor Settings knoppen
      if (bodyMenuPage == BODY_PAGE_SENSOR_SETTINGS) {
        const int MENU_X = 20;
        const int MENU_Y = 20;
        const int MENU_W = 480 - 40;
        
        // Check EERST de onderste knoppen (Opslaan, TERUG)
        int btnY = MENU_Y + 225;
        int btnW = 200;
        int btnH = 40;
        int btnSpacing = 10;
        int totalBtnW = btnW * 2 + btnSpacing;
        int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // Opslaan
        int btn2X = btn1X + btnW + btnSpacing;  // TERUG
        
        // Opslaan knop
        if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
          saveSensorSettingsFromEdit();
          Serial.println("[SENSOR SETTINGS] Settings opgeslagen!");
          return;
        }
        
        // TERUG knop
        if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
          bodyMenuPage = parentPage;
          bodyMenuIdx = 0;
          Serial.println("[SENSOR SETTINGS] Terug naar parent");
          return;
        }
        
        // Check +/- knoppen voor elk veld (6 velden)
        const int VAL_X = MENU_X + 210;
        const int BTN_W = 35;
        const int BTN_H = 20;
        const int MINUS_X = MENU_X + 320;
        const int PLUS_X = MENU_X + 360;
        const int START_Y = MENU_Y + 75;
        const int LINE_H = 25;
        
        for (int i = 0; i < 6; i++) {
          int btnBoxY = START_Y + i * LINE_H - 5;
          
          // - knop
          if (x >= MINUS_X && x <= MINUS_X + BTN_W && y >= btnBoxY && y <= btnBoxY + BTN_H) {
            switch(i) {
              case 0: // Beat Threshold (10000-100000)
                if (sensorEdit.beatThreshold > 10000.0f) sensorEdit.beatThreshold -= 1000.0f;
                break;
              case 1: // Temp Offset (-5.0 - +5.0)
                if (sensorEdit.tempOffset > -5.0f) sensorEdit.tempOffset -= 0.1f;
                break;
              case 2: // Temp Smoothing (0.0-1.0)
                if (sensorEdit.tempSmoothing > 0.0f) sensorEdit.tempSmoothing -= 0.05f;
                break;
              case 3: // GSR Baseline (0-4000)
                if (sensorEdit.gsrBaseline > 0.0f) sensorEdit.gsrBaseline -= 50.0f;
                break;
              case 4: // GSR Sensitivity (0.1-5.0)
                if (sensorEdit.gsrSensitivity > 0.1f) sensorEdit.gsrSensitivity -= 0.1f;
                break;
              case 5: // GSR Smoothing (0.0-1.0)
                if (sensorEdit.gsrSmoothing > 0.0f) sensorEdit.gsrSmoothing -= 0.05f;
                break;
            }
            Serial.printf("[SENSOR SETTINGS] Decreased field %d\n", i);
            menuDirty = true;
            return;
          }
          
          // + knop
          if (x >= PLUS_X && x <= PLUS_X + BTN_W && y >= btnBoxY && y <= btnBoxY + BTN_H) {
            switch(i) {
              case 0: // Beat Threshold (10000-100000)
                if (sensorEdit.beatThreshold < 100000.0f) sensorEdit.beatThreshold += 1000.0f;
                break;
              case 1: // Temp Offset (-5.0 - +5.0)
                if (sensorEdit.tempOffset < 5.0f) sensorEdit.tempOffset += 0.1f;
                break;
              case 2: // Temp Smoothing (0.0-1.0)
                if (sensorEdit.tempSmoothing < 1.0f) sensorEdit.tempSmoothing += 0.05f;
                break;
              case 3: // GSR Baseline (0-4000)
                if (sensorEdit.gsrBaseline < 4000.0f) sensorEdit.gsrBaseline += 50.0f;
                break;
              case 4: // GSR Sensitivity (0.1-5.0)
                if (sensorEdit.gsrSensitivity < 5.0f) sensorEdit.gsrSensitivity += 0.1f;
                break;
              case 5: // GSR Smoothing (0.0-1.0)
                if (sensorEdit.gsrSmoothing < 1.0f) sensorEdit.gsrSmoothing += 0.05f;
                break;
            }
            Serial.printf("[SENSOR SETTINGS] Increased field %d\n", i);
            menuDirty = true;
            return;
          }
        }
      }
      
      // Format Confirm knoppen
      if (bodyMenuPage == BODY_PAGE_FORMAT_CONFIRM) {
        int btnY = MENU_Y + 230;
        int btnW = 190;
        int btnH = 40;
        int btnSpacing = 20;
        int totalBtnW = btnW * 2 + btnSpacing;
        int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // ANNULEREN
        int btn2X = btn1X + btnW + btnSpacing;  // FORMATTEER
        
        // ANNULEREN knop
        if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
          bodyMenuPage = BODY_PAGE_SYSTEM_SETTINGS;
          bodyMenuIdx = 0;
          Serial.println("[FORMAT] Cancelled - returning to System Settings");
          return;
        }
        
        // FORMATTEER knop
        if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
          Serial.println("[FORMAT] Starting SD card format...");
          
          // Format SD kaart
          extern bool formatSDCard();
          if (formatSDCard()) {
            Serial.println("[FORMAT] SD card formatted successfully!");
          } else {
            Serial.println("[FORMAT] SD card format FAILED!");
          }
          
          // Terug naar System Settings
          bodyMenuPage = BODY_PAGE_SYSTEM_SETTINGS;
          bodyMenuIdx = 0;
          return;
        }
      }
      
      // Funscript Settings knoppen
      if (bodyMenuPage == BODY_PAGE_FUNSCRIPT_SETTINGS) {
        extern bool funscriptEnabled;
        
        // Check AAN/UIT toggle knoppen
        int btnY = MENU_Y + 60;
        int btnW = 180;
        int btnH = 50;
        int btnSpacing = 40;
        int totalBtnW = btnW * 2 + btnSpacing;
        int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // AAN knop
        int btn2X = btn1X + btnW + btnSpacing;  // UIT knop
        
        // AAN knop
        if (x >= btn1X && x <= btn1X + btnW && y >= btnY && y <= btnY + btnH) {
          if (!funscriptEnabled) {
            funscriptEnabled = true;
            Serial.println("[FUNSCRIPT] Mode: AAN");
            menuDirty = true;
          }
          return;
        }
        
        // UIT knop
        if (x >= btn2X && x <= btn2X + btnW && y >= btnY && y <= btnY + btnH) {
          if (funscriptEnabled) {
            funscriptEnabled = false;
            Serial.println("[FUNSCRIPT] Mode: UIT");
            menuDirty = true;
          }
          return;
        }
        
        // TERUG knop onderaan
        int terugY = MENU_Y + 230;
        int terugW = MENU_W - 40;
        int terugX = MENU_X + 20;
        
        if (x >= terugX && x <= terugX + terugW && y >= terugY && y <= terugY + 40) {
          bodyMenuPage = BODY_PAGE_SYSTEM_SETTINGS;
          bodyMenuIdx = 0;
          Serial.println("[FUNSCRIPT] Terug naar System Settings");
          return;
        }
      }
    }
  }
}

void drawSystemSettingsItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // Knop layout - zelfde breedte als TERUG knop
  const int BTN_W = MENU_W - 40;  // 400px breed
  const int BTN_H = 38;
  const int BTN_SPACING = 5;
  const int START_X = MENU_X + 20;
  const int START_Y = MENU_Y + 50;
  
  // Font instellen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  const char* items[] = {
    "Scherm Rotatie",
    "Funscript",
    "Format SD Kaart",
    "Tijd Instellen"
  };
  
  // Kleuren zoals op foto
  uint16_t colors[] = {
    0xFFE0,  // Geel (Scherm Rotatie)
    0x07FF,  // Cyaan (Funscript)
    0xFD20,  // Oranje (Format SD)
    0x841F   // Paars (Tijd Instellen)
  };
  
  // Text kleur (zwart voor alle knoppen)
  uint16_t textColors[] = {
    0x0000,  // Zwart (Scherm Rotatie)
    0x0000,  // Zwart (Funscript)
    0x0000,  // Zwart (Format SD)
    0x0000   // Zwart (Tijd Instellen)
  };
  
  // Teken 4 knoppen
  for (int i = 0; i < 4; i++) {
    int y = START_Y + i * (BTN_H + BTN_SPACING);
    
    // Gekleurde knop met witte rand
    body_gfx->fillRoundRect(START_X, y, BTN_W, BTN_H, 8, colors[i]);
    body_gfx->drawRoundRect(START_X, y, BTN_W, BTN_H, 8, 0xFFFF);
    body_gfx->drawRoundRect(START_X+1, y+1, BTN_W-2, BTN_H-2, 7, 0xFFFF);  // Dubbele rand
    
    // Centreer tekst in knop (kleur per knop)
    body_gfx->setTextColor(textColors[i], colors[i]);
    int16_t x1, y1; uint16_t tw, th;
    body_gfx->getTextBounds(items[i], 0, 0, &x1, &y1, &tw, &th);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2 - x1, y + (BTN_H + th) / 2 - 2);
    #else
      body_gfx->setCursor(START_X + (BTN_W - tw) / 2, y + (BTN_H - th) / 2 + th - 2);
    #endif
    body_gfx->print(items[i]);
  }
  
  // TERUG knop onderaan (blauw)
  int btnY = MENU_Y + 230;
  int btnW = MENU_W - 40;
  int btnX = MENU_X + 20;
  body_gfx->fillRoundRect(btnX, btnY, btnW, 40, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btnX, btnY, btnW, 40, 8, 0xFFFF);
  
  // Centreer tekst
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  body_gfx->setTextColor(0xFFFF, 0x001F);
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds("TERUG", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btnX + (btnW - tw) / 2 - x1, btnY + (40 + th) / 2 - 2);
  #else
    body_gfx->setCursor(btnX + (btnW - tw) / 2, btnY + 10);
  #endif
  body_gfx->print("TERUG");
}

void drawTimeSettingsItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // Font instellen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  // Tijd waarden met +/- knoppen
  const char* labels[] = {"Jaar:", "Maand:", "Dag:", "Uur:", "Minuut:"};
  int* values[] = {&editYear, &editMonth, &editDay, &editHour, &editMinute};
  
  int y = MENU_Y + 60;
  const int LINE_H = 35;
  const int VAL_X = MENU_X + 150;  // Waarde positie
  const int BTN_W = 50;  // +/- knop breedte
  const int BTN_H = 28;  // +/- knop hoogte
  const int MINUS_X = MENU_X + 280;  // - knop X
  const int PLUS_X = MENU_X + 340;   // + knop X
  
  for (int i = 0; i < 5; i++) {
    // Label links
    body_gfx->setTextColor(0xC618, BODY_CFG.COL_BG);  // Lichtgrijs
    body_gfx->setCursor(MENU_X + 30, y);
    body_gfx->print(labels[i]);
    
    // Waarde veld (donkere box)
    int valBoxY = y - 7;
    int valBoxH = 28;
    body_gfx->fillRoundRect(VAL_X, valBoxY, 110, valBoxH, 5, 0x2104);  // Donkergrijs
    body_gfx->drawRoundRect(VAL_X, valBoxY, 110, valBoxH, 5, 0x8410);  // Lichtgrijze rand
    body_gfx->setTextColor(0xFFFF, 0x2104);  // Witte tekst
    
    // Centreer waarde in box
    char valStr[10];
    if (i == 0) {
      sprintf(valStr, "%04d", *values[i]);  // Jaar: 4 cijfers
    } else {
      sprintf(valStr, "%02d", *values[i]);  // Rest: 2 cijfers
    }
    int16_t xv1, yv1; uint16_t twv, thv;
    body_gfx->getTextBounds(valStr, 0, 0, &xv1, &yv1, &twv, &thv);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(VAL_X + (110 - twv) / 2 - xv1, valBoxY + valBoxH / 2 + thv / 2);
    #else
      body_gfx->setCursor(VAL_X + 25, y);
    #endif
    body_gfx->print(valStr);
    
    // - knop (blauw)
    int btnBoxY = y - 7;
    body_gfx->fillRoundRect(MINUS_X, btnBoxY, BTN_W, BTN_H, 5, 0x001F);  // Blauw
    body_gfx->drawRoundRect(MINUS_X, btnBoxY, BTN_W, BTN_H, 5, 0xFFFF);
    body_gfx->setTextColor(0xFFFF, 0x001F);
    int16_t xm1, ym1; uint16_t twm, thm;
    body_gfx->getTextBounds("-", 0, 0, &xm1, &ym1, &twm, &thm);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(MINUS_X + (BTN_W - twm) / 2 - xm1, btnBoxY + BTN_H / 2 + thm / 2);
    #else
      body_gfx->setCursor(MINUS_X + 18, y);
    #endif
    body_gfx->print("-");
    
    // + knop (rood)
    body_gfx->fillRoundRect(PLUS_X, btnBoxY, BTN_W, BTN_H, 5, 0xF800);  // Rood
    body_gfx->drawRoundRect(PLUS_X, btnBoxY, BTN_W, BTN_H, 5, 0xFFFF);
    body_gfx->setTextColor(0xFFFF, 0xF800);
    int16_t xp1, yp1; uint16_t twp, thp;
    body_gfx->getTextBounds("+", 0, 0, &xp1, &yp1, &twp, &thp);
    #if USE_ADAFRUIT_FONTS
      body_gfx->setCursor(PLUS_X + (BTN_W - twp) / 2 - xp1, btnBoxY + BTN_H / 2 + thp / 2);
    #else
      body_gfx->setCursor(PLUS_X + 18, y);
    #endif
    body_gfx->print("+");
    
    y += LINE_H;
  }
  
  // 2 knoppen onderaan: OPSLAAN (groen) en TERUG (blauw)
  int btnY = MENU_Y + 230;
  int btnW = 190;
  int btnH = 40;
  int btnSpacing = 20;
  int totalBtnW = btnW * 2 + btnSpacing;
  int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // OPSLAAN links
  int btn2X = btn1X + btnW + btnSpacing;  // TERUG rechts
  
  // OPSLAAN (groen)
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, 0x07E0);  // Groen
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->drawRoundRect(btn1X+1, btnY+1, btnW-2, btnH-2, 7, 0xFFFF);
  body_gfx->setTextColor(0x0000, 0x07E0);  // Zwarte tekst
  int16_t xa1, ya1; uint16_t twa, tha;
  body_gfx->getTextBounds("OPSLAAN", 0, 0, &xa1, &ya1, &twa, &tha);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - twa) / 2 - xa1, btnY + (btnH + tha) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + 35, btnY + 10);
  #endif
  body_gfx->print("OPSLAAN");
  
  // TERUG (blauw)
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->drawRoundRect(btn2X+1, btnY+1, btnW-2, btnH-2, 7, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);  // Witte tekst
  int16_t xb1, yb1; uint16_t twb, thb;
  body_gfx->getTextBounds("TERUG", 0, 0, &xb1, &yb1, &twb, &thb);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - twb) / 2 - xb1, btnY + (btnH + thb) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + 45, btnY + 10);
  #endif
  body_gfx->print("TERUG");
}

void drawFormatConfirmItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  // Font instellen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  // Waarschuwingstekst (rood)
  int textY = MENU_Y + 60;
  body_gfx->setTextColor(0xF800, BODY_CFG.COL_BG);  // Rood
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  // Grote waarschuwing tekst
  body_gfx->setCursor(MENU_X + 90, textY);
  body_gfx->print("WAARSCHUWING!");
  
  textY += 35;
  
  // Details in lichtgrijs (kleinere font)
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(1);
  #endif
  
  body_gfx->setTextColor(0xC618, BODY_CFG.COL_BG);  // Lichtgrijs
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("Dit wist ALLE data op SD kaart:");
  
  textY += 18;
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("  - Alle opname bestanden (.csv)");
  
  textY += 18;
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("  - Training data");
  
  textY += 18;
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("  - Logs en sessiegegevens");
  
  textY += 30;
  body_gfx->setTextColor(0x07E0, BODY_CFG.COL_BG);  // Groen
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("AI model blijft behouden (ESP Flash)");
  
  textY += 25;
  body_gfx->setTextColor(0xF800, BODY_CFG.COL_BG);  // Rood
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("Deze actie kan NIET ongedaan worden!");
  
  // Knoppen onderaan (ANNULEREN / FORMATTEER)
  int btnY = MENU_Y + 230;
  int btnW = 190;
  int btnH = 40;
  int btnSpacing = 20;
  int totalBtnW = btnW * 2 + btnSpacing;
  int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // ANNULEREN links
  int btn2X = btn1X + btnW + btnSpacing;  // FORMATTEER rechts
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  // ANNULEREN (blauw, links)
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->drawRoundRect(btn1X+1, btnY+1, btnW-2, btnH-2, 7, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);
  int16_t xa1, ya1; uint16_t twa, tha;
  body_gfx->getTextBounds("ANNULEREN", 0, 0, &xa1, &ya1, &twa, &tha);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - twa) / 2 - xa1, btnY + (btnH + tha) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + (btnW - twa) / 2, btnY + 10);
  #endif
  body_gfx->print("ANNULEREN");
  
  // FORMATTEER (rood, rechts)
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, 0xF800);  // Rood
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->drawRoundRect(btn2X+1, btnY+1, btnW-2, btnH-2, 7, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0xF800);
  int16_t xb1, yb1; uint16_t twb, thb;
  body_gfx->getTextBounds("FORMATTEER", 0, 0, &xb1, &yb1, &twb, &thb);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - twb) / 2 - xb1, btnY + (btnH + thb) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + (btnW - twb) / 2, btnY + 10);
  #endif
  body_gfx->print("FORMATTEER");
}

void drawFunscriptSettingsItems() {
  const int MENU_X = 20;
  const int MENU_Y = 20;
  const int MENU_W = 480 - 40;
  
  extern bool funscriptEnabled;
  
  // Font instellen
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  // Toggle knoppen (AAN / UIT)
  int btnY = MENU_Y + 60;
  int btnW = 180;
  int btnH = 50;
  int btnSpacing = 40;
  int totalBtnW = btnW * 2 + btnSpacing;
  int btn1X = MENU_X + (MENU_W - totalBtnW) / 2;  // AAN knop
  int btn2X = btn1X + btnW + btnSpacing;  // UIT knop
  
  // AAN knop (groen als actief, grijs als niet)
  uint16_t aanColor = funscriptEnabled ? 0x07E0 : 0x4208;
  uint16_t aanTextColor = funscriptEnabled ? 0x0000 : 0xC618;
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, aanColor);
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->drawRoundRect(btn1X+1, btnY+1, btnW-2, btnH-2, 7, 0xFFFF);
  body_gfx->setTextColor(aanTextColor, aanColor);
  int16_t xa1, ya1; uint16_t twa, tha;
  body_gfx->getTextBounds("AAN", 0, 0, &xa1, &ya1, &twa, &tha);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - twa) / 2 - xa1, btnY + (btnH + tha) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + (btnW - twa) / 2, btnY + (btnH - tha) / 2 + tha - 2);
  #endif
  body_gfx->print("AAN");
  
  // UIT knop (rood als actief, grijs als niet)
  uint16_t uitColor = !funscriptEnabled ? 0xF800 : 0x4208;
  uint16_t uitTextColor = !funscriptEnabled ? 0xFFFF : 0xC618;
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, uitColor);
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->drawRoundRect(btn2X+1, btnY+1, btnW-2, btnH-2, 7, 0xFFFF);
  body_gfx->setTextColor(uitTextColor, uitColor);
  int16_t xb1, yb1; uint16_t twb, thb;
  body_gfx->getTextBounds("UIT", 0, 0, &xb1, &yb1, &twb, &thb);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - twb) / 2 - xb1, btnY + (btnH + thb) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + (btnW - twb) / 2, btnY + (btnH - thb) / 2 + thb - 2);
  #endif
  body_gfx->print("UIT");
  
  // Instructie tekst (onder de knoppen)
  int textY = btnY + btnH + 30;
  body_gfx->setTextColor(0xC618, BODY_CFG.COL_BG);  // Lichtgrijs
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(1);
  #endif
  
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("VR "); 
  body_gfx->print((char)0x1A);  // Pijltje → (rechtspijl)
  body_gfx->print(" Heresphere Funscript");
  
  textY += 18;
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("Optie AAN ");
  body_gfx->print((char)0x1A);
  body_gfx->print(" Body ESP stuurt");
  
  textY += 18;
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("   override commando's naar Hoofd ESP");
  
  textY += 18;
  body_gfx->setCursor(MENU_X + 30, textY);
  body_gfx->print("Optie UIT ");
  body_gfx->print((char)0x1A);
  body_gfx->print(" Normale werking");
  
  // TERUG knop onderaan (blauw)
  int terugY = MENU_Y + 230;
  int terugW = MENU_W - 40;
  int terugX = MENU_X + 20;
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  body_gfx->fillRoundRect(terugX, terugY, terugW, 40, 8, 0x001F);  // Blauw
  body_gfx->drawRoundRect(terugX, terugY, terugW, 40, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);
  int16_t xt1, yt1; uint16_t twt, tht;
  body_gfx->getTextBounds("TERUG", 0, 0, &xt1, &yt1, &twt, &tht);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(terugX + (terugW - twt) / 2 - xt1, terugY + (40 + tht) / 2 - 2);
  #else
    body_gfx->setCursor(terugX + (terugW - twt) / 2, terugY + 10);
  #endif
  body_gfx->print("TERUG");
}

void drawPlaybackProgressBar() {
  // DYNAMISCH: Update alleen progress bar (geroepen vanuit bodyMenuTick)
  // Progress bar MOET boven frame uitsteken (frame is op y=2)
  int progressBarH = 25;  // Hoog genoeg om zichtbaar te zijn
  int progressBarX = 0;
  int progressBarY = 0;
  int progressBarW = 480 - 100;
  
  // Wis progress bar area
  body_gfx->fillRect(progressBarX, progressBarY, progressBarW, progressBarH, 0x2104);
  
  // Teken progress (blauw)
  int progressFillW = (int)(progressBarW * playbackProgress);
  if (progressFillW > 0) {
    body_gfx->fillRect(progressBarX, progressBarY, progressFillW, progressBarH, 0x001F);
  }
}

void drawPlaybackScreen() {
  // Teken playback overlay
  // DYNAMISCH: progress bar (via bodyMenuTick)
  // STATISCH: speed controls, indicator, knoppen (1x)
  
  if (!playbackScreenDrawn) {  // Teken statische delen 1x
    // Teken EERST body_gfx4 frame met grafieken (basis)
    //body_gfx4_clear();  // Teken frame + wis grafieken
    if (!isPlaybackPaused) {
      body_gfx4_clear(); // 🔥 Skip als gepauzeerd = grafieken blijven!
    }
    
    // Zet correcte font voor ALLE tekst in overlay
    #if USE_ADAFRUIT_FONTS
      body_gfx->setFont(&FONT_ITEM);
      body_gfx->setTextSize(1);
    #else
      body_gfx->setFont(nullptr);
      body_gfx->setTextSize(2);
    #endif
    
    // Progress bar + speed controls bovenaan
  int16_t x1, y1; uint16_t tw, th;
  int progressBarH = 25;  // Vaste hoogte (zelfde als dynamische progress bar)
  int progressBarW = 480 - 100;
  
  // Speed controls rechts (statisch - 1x)
  int speedX = progressBarW + 5;
  int speedY = 0;
  int speedBtnW = 25;
  int speedBtnH = progressBarH;
  
  // - knop (blauw)
  body_gfx->fillRoundRect(speedX, speedY, speedBtnW, speedBtnH, 3, 0x001F);
  body_gfx->drawRect(speedX, speedY, speedBtnW, speedBtnH, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);
  body_gfx->getTextBounds("-", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(speedX + (speedBtnW - tw) / 2 - x1, speedY + (speedBtnH + th) / 2);
  #else
    body_gfx->setCursor(speedX + (speedBtnW - tw) / 2, speedY + (speedBtnH - th) / 2 + th - 2);
  #endif
  body_gfx->print("-");
  
  // Speed percentage (wit op zwart)
  int speedTextX = speedX + speedBtnW + 2;
  int speedTextW = 40;
  body_gfx->fillRect(speedTextX, speedY, speedTextW, speedBtnH, 0x0000);
  body_gfx->setTextColor(0xFFFF, 0x0000);
  char speedStr[8];
  sprintf(speedStr, "%.0f%%", playbackSpeed);
  body_gfx->getTextBounds(speedStr, 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(speedTextX + (speedTextW - tw) / 2 - x1, speedY + (speedBtnH + th) / 2);
  #else
    body_gfx->setCursor(speedTextX + (speedTextW - tw) / 2, speedY + (speedBtnH - th) / 2 + th - 2);
  #endif
  body_gfx->print(speedStr);
  
  // + knop (rood)
  int plusX = speedTextX + speedTextW + 2;
  body_gfx->fillRoundRect(plusX, speedY, speedBtnW, speedBtnH, 3, 0xF800);
  body_gfx->drawRect(plusX, speedY, speedBtnW, speedBtnH, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0xF800);
  body_gfx->getTextBounds("+", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(plusX + (speedBtnW - tw) / 2 - x1, speedY + (speedBtnH + th) / 2);
  #else
    body_gfx->setCursor(plusX + (speedBtnW - tw) / 2, speedY + (speedBtnH - th) / 2 + th - 2);
  #endif
  body_gfx->print("+");
  
  // Verticale indicator (statisch - 1x)
  int indicatorX = 240;
  int indicatorY1 = progressBarH + 5;
  int indicatorY2 = 320 - 45;
  
  body_gfx->drawLine(indicatorX - 1, indicatorY1, indicatorX - 1, indicatorY2, 0xFFFF);
  body_gfx->drawLine(indicatorX + 1, indicatorY1, indicatorX + 1, indicatorY2, 0xFFFF);
  body_gfx->drawLine(indicatorX, indicatorY1, indicatorX, indicatorY2, 0xFD20);
  
  body_gfx->fillTriangle(indicatorX, indicatorY1, indicatorX - 4, indicatorY1 - 6, indicatorX + 4, indicatorY1 - 6, 0xFD20);
  body_gfx->drawTriangle(indicatorX, indicatorY1, indicatorX - 4, indicatorY1 - 6, indicatorX + 4, indicatorY1 - 6, 0xFFFF);
  
  // Knoppen onderaan
  int btnY = 320 - 40;
  int btnH = 35;
  int btnW = 112;
  int btn1X = 5;      // STOP
  int btn2X = 122;    // PLAY/PAUZE
  int btn4X = 356;    // AI-ACTIE
  
  // STOP knop (oranje)
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFD20);
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0x0000, 0xFD20);
  body_gfx->getTextBounds("STOP", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - tw) / 2 - x1, btnY + (btnH + th) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + (btnW - tw) / 2, btnY + (btnH - th) / 2 + th - 2);
  #endif
  body_gfx->print("STOP");
  
  // AI-ACTIE knop (magenta)
  body_gfx->fillRoundRect(btn4X, btnY, btnW, btnH, 8, 0xF81F);
  body_gfx->drawRoundRect(btn4X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0x0000, 0xF81F);
  body_gfx->getTextBounds("AI-ACTIE", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn4X + (btnW - tw) / 2 - x1, btnY + (btnH + th) / 2 - 2);
  #else
    body_gfx->setCursor(btn4X + (btnW - tw) / 2, btnY + (btnH - th) / 2 + th - 2);
  #endif
  body_gfx->print("AI-ACTIE");
    
    playbackScreenDrawn = true;  // Statische delen zijn getekend
  }
  
  // PLAY/PAUZE knop (DYNAMISCH - altijd hertekenen voor status update)
  int btnY = 320 - 40;
  int btnH = 35;
  int btnW = 112;
  int btn2X = 122;
  
  // Zet correcte font voor knop tekst
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  uint16_t playColor = (isPlaybackActive && !isPlaybackPaused) ? 0x07E0 : 0xF800;
  const char* playLabel = (isPlaybackActive && !isPlaybackPaused) ? "PAUZE" : "PLAY";
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, playColor);
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0x0000, playColor);
  
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds(playLabel, 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - tw) / 2 - x1, btnY + (btnH + th) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + (btnW - tw) / 2, btnY + (btnH - th) / 2 + th - 2);
  #endif
  body_gfx->print(playLabel);
}


void drawStressLevelPopup() {
  // TODO: GRAFIEKEN VERSCHIJNEN RANDOM OVER POPUP - MOET GEFIXT WORDEN
  // Probleem: body_gfx4_pushSample() tekent direct op scherm en overtekent popup
  // Mogelijke oplossingen:
  //   1. Stop ALLE grafiek rendering (ook body_gfx4) tijdens stressPopupActive
  //   2. Gebruik dubbele buffering voor grafieken
  //   3. Hertekeneen popup elke frame als stressPopupActive
  
  // Popup overlay (gecentreerd) - OVER playback scherm
  int popupW = 300;
  int popupH = 260;
  int popupX = (480 - popupW) / 2;
  int popupY = (320 - popupH) / 2;
  
  // Donkere semi-transparante overlay (simpel: zwart rechthoek)
  body_gfx->fillRect(0, 0, 480, 320, 0x0000);
  
  // Popup achtergrond (donkergrijs met witte rand) - staat BOVENOP overlay
  body_gfx->fillRoundRect(popupX, popupY, popupW, popupH, 12, 0x2104);
  body_gfx->drawRoundRect(popupX, popupY, popupW, popupH, 12, 0xFFFF);
  body_gfx->drawRoundRect(popupX+1, popupY+1, popupW-2, popupH-2, 11, 0xFFFF);
  
  // Title
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_TITLE);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setFont(nullptr);
    body_gfx->setTextSize(2);
  #endif
  
  body_gfx->setTextColor(0xF81F, 0x2104);  // Magenta
  int16_t x1, y1; uint16_t tw, th;
  body_gfx->getTextBounds("STRESS LEVEL", 0, 0, &x1, &y1, &tw, &th);
  body_gfx->setCursor(popupX + (popupW - tw) / 2 - x1, popupY + 25);
  body_gfx->print("STRESS LEVEL");
  
  // 7 stress levels (0-6) als radio buttons
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(1);
  #endif
  
  const char* levelLabels[] = {
    "0 - Ontspannen",
    "1 - Rustig",
    "2 - Normaal",
    "3 - Verhoogd",
    "4 - Gestrest",
    "5 - Zeer gestrest",
    "6 - Extreem"
  };
  
  int levelY = popupY + 50;
  int levelH = 20;
  int levelSpacing = 3;
  
  for (int i = 0; i < 7; i++) {
    int y = levelY + i * (levelH + levelSpacing);
    
    // Radio button circle
    int circleX = popupX + 20;
    int circleY = y + levelH / 2;
    int circleR = 6;
    
    // Outer circle (wit)
    body_gfx->drawCircle(circleX, circleY, circleR, 0xFFFF);
    
    // Inner circle (selected = gevuld)
    if (i == selectedStressLevel) {
      body_gfx->fillCircle(circleX, circleY, circleR - 2, 0x07E0);  // Groen
    }
    
    // Label
    body_gfx->setTextColor(0xFFFF, 0x2104);
    body_gfx->setCursor(circleX + circleR + 10, y + levelH / 2 + 4);
    body_gfx->print(levelLabels[i]);
  }
  
  // Knoppen onderaan (ANNULEREN / OPSLAAN)
  int btnY = popupY + popupH - 50;
  int btnW = 120;
  int btnH = 35;
  int btnSpacing = 10;
  int totalBtnW = btnW * 2 + btnSpacing;
  int btn1X = popupX + (popupW - totalBtnW) / 2;
  int btn2X = btn1X + btnW + btnSpacing;
  
  #if USE_ADAFRUIT_FONTS
    body_gfx->setFont(&FONT_ITEM);
    body_gfx->setTextSize(1);
  #else
    body_gfx->setTextSize(2);
  #endif
  
  // ANNULEREN (blauw)
  body_gfx->fillRoundRect(btn1X, btnY, btnW, btnH, 8, 0x001F);
  body_gfx->drawRoundRect(btn1X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0xFFFF, 0x001F);
  body_gfx->getTextBounds("ANNULEREN", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn1X + (btnW - tw) / 2 - x1, btnY + (btnH + th) / 2 - 2);
  #else
    body_gfx->setCursor(btn1X + (btnW - tw) / 2, btnY + 10);
  #endif
  body_gfx->print("ANNULEREN");
  
  // OPSLAAN (groen)
  body_gfx->fillRoundRect(btn2X, btnY, btnW, btnH, 8, 0x07E0);
  body_gfx->drawRoundRect(btn2X, btnY, btnW, btnH, 8, 0xFFFF);
  body_gfx->setTextColor(0x0000, 0x07E0);
  body_gfx->getTextBounds("OPSLAAN", 0, 0, &x1, &y1, &tw, &th);
  #if USE_ADAFRUIT_FONTS
    body_gfx->setCursor(btn2X + (btnW - tw) / 2 - x1, btnY + (btnH + th) / 2 - 2);
  #else
    body_gfx->setCursor(btn2X + (btnW - twb) / 2, btnY + 10);
  #endif
  body_gfx->print("OPSLAAN");
}

void startPlayback(const char* filename) {
  // Open CSV bestand en initialiseer playback
  if (playbackFile) {
    playbackFile.close();
  }
  
  char fullPath[128];
  snprintf(fullPath, sizeof(fullPath), "/recordings/%s", filename);
  
  playbackFile = SD_MMC.open(fullPath, FILE_READ);
  if (!playbackFile) {
    Serial.printf("[PLAYBACK] Fout: kan bestand niet openen: %s\n", fullPath);
    return;
  }
  
  Serial.printf("[PLAYBACK] Start: %s\n", filename);
  
  // Tel aantal regels (voor progress)
  playbackTotalLines = 0;
  while (playbackFile.available()) {
    String line = playbackFile.readStringUntil('\n');
    if (line.length() > 0) {
      playbackTotalLines++;
    }
  }
  
  // Skip header regel
  if (playbackTotalLines > 0) {
    playbackTotalLines--;
  }
  
  Serial.printf("[PLAYBACK] Totaal regels: %d\n", playbackTotalLines);
  
  // Reset naar begin en skip header
  playbackFile.seek(0);
  if (playbackFile.available()) {
    playbackFile.readStringUntil('\n');  // Skip header
  }
  
  // Initialiseer playback variabelen
  isPlaybackActive = true;
  isPlaybackPaused = false;
  playbackCurrentLine = 0;
  playbackProgress = 0.0f;
  playbackLastUpdate = millis();
  
  // Reset stress markers
  stressMarkerCount = 0;
  
  // Wis scherm (verwijder recording menu)
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // Switch naar playback pagina (BLIJF in MENU mode voor touch handling!)
  bodyMenuPage = BODY_PAGE_PLAYBACK;
  bodyMenuMode = BODY_MODE_MENU;  // BELANGRIJK: blijf in MENU mode zodat touch handling werkt!
  playbackScreenDrawn = false;  // Forceer hertekenen van statische delen
  
  Serial.println("[PLAYBACK] Gestart");
}

void updatePlayback() {
  if (!isPlaybackActive || !playbackFile) {
    return;
  }
  
  // Stop update als gepauzeerd OF als popup actief is
  if (isPlaybackPaused || stressPopupActive) {
    return;
  }
  
  // Check timing (1000ms baseline, aangepast met speed)
  // playbackSpeed: 100% = normaal, 200% = 2x sneller, 50% = 2x langzamer
  unsigned long now = millis();
  unsigned long interval = (unsigned long)(1000.0f * (100.0f / playbackSpeed));
  
  if (now - playbackLastUpdate < interval) {
    return;
  }
  
  playbackLastUpdate = now;
  
  // Lees volgende regel
  if (!playbackFile.available()) {
    // Einde bereikt
    Serial.println("[PLAYBACK] Einde bereikt");
    stopPlayback();
    return;
  }
  
  String line = playbackFile.readStringUntil('\n');
  if (line.length() == 0) {
    return;
  }
  
  playbackCurrentLine++;
  playbackProgress = (float)playbackCurrentLine / (float)playbackTotalLines;
  
  // Parse CSV regel: timestamp,trust,sleeve,vibe,zuig,hr,hrv,temp,gsr,...
  // Format: timestamp,trust,sleeve,vibe,zuig,hr,hrv,temp,gsr,accelX,accelY,accelZ,gyroX,gyroY,gyroZ
  int commaPos[16];
  int commaCount = 0;
  
  for (int i = 0; i < line.length() && commaCount < 16; i++) {
    if (line[i] == ',') {
      commaPos[commaCount++] = i;
    }
  }
  
  if (commaCount < 8) {  // Minimaal 8 komma's voor hr, temp, gsr
    Serial.println("[PLAYBACK] Fout: ongeldige CSV regel");
    return;
  }
  
  // Parse CSV kolommen:
  // 0=Tijd_s, 1=Timestamp, 2=BPM, 3=Temp_C, 4=GSR, 5=Trust, 6=Sleeve, 7=Suction, 8=Vibe, 9=Zuig, ...
  String hrStr = line.substring(commaPos[1] + 1, commaPos[2]);    // Kolom 2: BPM
  String tempStr = line.substring(commaPos[2] + 1, commaPos[3]);  // Kolom 3: Temp_C
  String gsrStr = line.substring(commaPos[3] + 1, commaPos[4]);   // Kolom 4: GSR
  String trustStr = line.substring(commaPos[4] + 1, commaPos[5]); // Kolom 5: Trust
  String sleeveStr = line.substring(commaPos[5] + 1, commaPos[6]);// Kolom 6: Sleeve
  // Skip Suction (kolom 7)
  String vibeStr = line.substring(commaPos[7] + 1, commaPos[8]);  // Kolom 8: Vibe
  String zuigStr = line.substring(commaPos[8] + 1, commaPos[9]);  // Kolom 9: Zuig
  
  float hr = hrStr.toFloat();
  float temp = tempStr.toFloat();
  float gsr = gsrStr.toFloat();
  float trust = trustStr.toFloat();   // Trust: 0.0-2.0 van HoofdESP
  float sleeve = sleeveStr.toFloat(); // Sleeve: 0.0-2.0 van HoofdESP
  int vibe = vibeStr.toInt();         // Vibe: 0 of 1
  int zuig = zuigStr.toInt();         // Zuig: 0 of 1
  
  // Push sensor data naar body_gfx4 grafieken (G4_* constanten komen uit body_gfx4.h)
  body_gfx4_pushSample(G4_HART, hr, false);           // Hart: BPM
  body_gfx4_pushSample(G4_HUID, gsr / 10.0f, false);  // GSR (geschaald)
  body_gfx4_pushSample(G4_TEMP, temp, false);         // Temp: °C
  body_gfx4_pushSample(G4_ADEMHALING, 0, false);      // Ademhaling (geen data in CSV)
  body_gfx4_pushSample(G4_HOOFDESP, trust, false);    // Trust van CSV
  body_gfx4_pushSample(G4_ZUIGEN, zuig, false);       // Zuig van CSV
  body_gfx4_pushSample(G4_TRIL, vibe, false);         // Vibe van CSV
  
  // // Stuur ESP-NOW commando's (zoals in main file) - OUDE CODE
  // // Trust: 0-100%
  // if (trust >= 0 && trust <= 100) {
  //   uint8_t trustData[2] = {0x01, (uint8_t)trust};
  //   // esp_now_send(...); // TODO: implementeer ESP-NOW send via callback
  //   Serial.printf("[PLAYBACK] Trust: %d%%\n", trust);
  // }
  // 
  // // Sleeve: 0-100%
  // if (sleeve >= 0 && sleeve <= 100) {
  //   uint8_t sleeveData[2] = {0x02, (uint8_t)sleeve};
  //   // esp_now_send(...); // TODO: implementeer ESP-NOW send via callback
  //   Serial.printf("[PLAYBACK] Sleeve: %d%%\n", sleeve);
  // }
  // 
  // // Vibe: 0-100%
  // if (vibe >= 0 && vibe <= 100) {
  //   uint8_t vibeData[2] = {0x03, (uint8_t)vibe};
  //   // esp_now_send(...); // TODO: implementeer ESP-NOW send via callback
  //   Serial.printf("[PLAYBACK] Vibe: %d%%\n", vibe);
  // }
  // 
  // // Zuig: 0-100%
  // if (zuig >= 0 && zuig <= 100) {
  //   uint8_t zuigData[2] = {0x04, (uint8_t)zuig};
  //   // esp_now_send(...); // TODO: implementeer ESP-NOW send via callback
  //   Serial.printf("[PLAYBACK] Zuig: %d%%\n", zuig);
  // }
  
  // // Stuur ESP-NOW PLAYBACK_DATA commando naar HoofdESP - OUDE POGING
  // extern bool sendESPNowMessage(float newTrust, float newSleeve, bool overruleActive, const char* command, uint8_t stressLevel, bool vibeOn, bool zuigenOn);
  // 
  // // Converteer CSV waardes naar HoofdESP formaat
  // // Trust/Sleeve in CSV: 0.0-2.0 (HoofdESP range)
  // // HoofdESP verwacht: 0.0-1.0 voor newTrust/newSleeve
  // float trustNorm = trust / 2.0f;   // 0.0-2.0 -> 0.0-1.0
  // float sleeveNorm = sleeve / 2.0f; // 0.0-2.0 -> 0.0-1.0
  // bool vibeActive = (vibe > 0);     // 0/1 -> bool
  // bool zuigActive = (zuig > 0);     // 0/1 -> bool
  // 
  // sendESPNowMessage(trustNorm, sleeveNorm, false, "PLAYBACK_DATA", 0, vibeActive, zuigActive);
  // 
  // Serial.printf("[PLAYBACK] ESP-NOW TX: T:%.3f S:%.3f V:%d Z:%d\n", trustNorm, sleeveNorm, vibeActive, zuigActive);
  
  // Stuur ESP-NOW PLAYBACK_STRESS commando (hergebruik bestaande handler)
  extern bool sendESPNowMessage(float newTrust, float newSleeve, bool overruleActive, const char* command, uint8_t stressLevel, bool vibeOn, bool zuigenOn);
  
  // Converteer trust (0.0-2.0) naar stress level (1-7)
  // Trust mapping:
  //   0.0-0.3  -> stress 1 (rustig)
  //   0.3-0.7  -> stress 2 (opbouw)
  //   0.7-1.1  -> stress 3-4 (medium)
  //   1.1-1.5  -> stress 5 (intensief)
  //   1.5-1.8  -> stress 6 (bijna climax)
  //   1.8-2.0  -> stress 7 (CLIMAX!)
  uint8_t stressLevel;
  if (trust >= 1.8f) {
    stressLevel = 7;  // CLIMAX zone (speed step 7)
  } else if (trust >= 1.5f) {
    stressLevel = 6;  // Bijna climax (speed step 6)
  } else if (trust >= 1.1f) {
    stressLevel = 5;  // Intensief (speed step 5)
  } else if (trust >= 0.9f) {
    stressLevel = 4;  // Medium-hoog (speed step 4)
  } else if (trust >= 0.7f) {
    stressLevel = 3;  // Medium (speed step 3)
  } else if (trust >= 0.3f) {
    stressLevel = 2;  // Opbouw (speed step 2)
  } else {
    stressLevel = 1;  // Rustig (speed step 0-1)
  }
  
  bool vibeActive = (vibe > 0);  // 0/1 -> bool
  bool zuigActive = (zuig > 0);  // 0/1 -> bool
  
  sendESPNowMessage(0, 0, false, "PLAYBACK_STRESS", stressLevel, vibeActive, zuigActive);
  
  Serial.printf("[PLAYBACK] ESP-NOW TX: Stress:%d (T:%.2f) V:%d Z:%d\n", stressLevel, trust, vibeActive, zuigActive);
}

void stopPlayback() {
  isPlaybackActive = false;
  isPlaybackPaused = false;
  
  if (playbackFile) {
    playbackFile.close();
  }
  
  // Sla stress markers op naar .preview bestand
  if (stressMarkerCount > 0) {
    saveStressMarkers();
  }
  
  // Terug naar RECORDING MENU en wis scherm volledig
  bodyMenuPage = BODY_PAGE_RECORDING;
  bodyMenuMode = BODY_MODE_MENU;
  menuDirty = true;  // Force redraw
  
  // Wis scherm volledig zodat playback grafieken verdwijnen
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  Serial.println("[PLAYBACK] Gestopt - terug naar recording menu");
}

void saveStressMarkers() {
  if (stressMarkerCount == 0) {
    return;
  }
  
  // Genereer .preview bestandsnaam (gebaseerd op geselecteerde CSV)
  char previewPath[128];
  // Vervang .csv extensie met .preview
  String basename = String(selectedPlaybackFile);
  int dotPos = basename.lastIndexOf('.');
  if (dotPos > 0) {
    basename = basename.substring(0, dotPos);
  }
  snprintf(previewPath, sizeof(previewPath), "/recordings/%s.preview", basename.c_str());
  
  File previewFile = SD_MMC.open(previewPath, FILE_WRITE);
  if (!previewFile) {
    Serial.println("[PLAYBACK] Fout: kan preview bestand niet aanmaken");
    return;
  }
  
  // Schrijf header
  previewFile.println("timestamp,stress_level");
  
  // Schrijf alle markers
  for (int i = 0; i < stressMarkerCount; i++) {
    previewFile.printf("%lu,%d\n", stressMarkers[i].timestamp, stressMarkers[i].stressLevel);
  }
  
  previewFile.close();
  Serial.printf("[PLAYBACK] %d stress markers opgeslagen: %s\n", stressMarkerCount, previewPath);
}

void bodyMenuHandleButton(bool cPressed, bool zPressed) {
  // Handle hardware buttons if available
  // For now, just basic navigation
  
  if (cPressed) {
    if (bodyMenuMode == BODY_MODE_MENU) {
      bodyMenuMode = BODY_MODE_SENSORS; // C button always goes back to sensors
    }
  }
  
  if (zPressed) {
    // Z button could toggle recording - would need to call main file functions
    Serial.println("[BODY] Z button pressed - recording toggle requested");
    // Note: This would need proper implementation with callbacks to main file
  }
}

#include "body_menu.h"
#include "body_display.h"
#include "body_config.h"
#include "body_fonts.h"

// Forward declarations
void drawSensorMode();
void drawMenuMode();
void drawSensorStatus();
void drawMainMenuItems();
void drawAISettingsItems();
void drawSensorCalItems();
void drawRecordingItems();
void drawESPStatusItems();

// Menu state variables
BodyMenuMode bodyMenuMode = BODY_MODE_SENSORS;
BodyMenuPage bodyMenuPage = BODY_PAGE_MAIN;
int bodyMenuIdx = 0;
bool bodyMenuEdit = false;

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

// Font helpers (zoals HoofdESP)
static void setMenuFontTitle(){
#if USE_ADAFRUIT_FONTS
  body_gfx->setFont(&FONT_TITLE);
  body_gfx->setTextSize(1);
#else
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(2);
#endif
}

static void setMenuFontItem(){
#if USE_ADAFRUIT_FONTS
  body_gfx->setFont(&FONT_ITEM);
  body_gfx->setTextSize(1);
#else
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
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
  // Initialize display
  body_gfx->begin();
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // Initialize canvas
  body_cv->begin();
  
  // Initialize heart rate history
  for (int i = 0; i < 50; i++) {
    heartRateHistory[i] = 0.0f;
  }
  
  Serial.println("[BODY] Menu system initialized");
}

void bodyMenuTick() {
  // Update heart rate history
  if (millis() - lastHistoryUpdate > 200) { // 5Hz update
    heartRateHistory[historyIndex] = getBPM();
    historyIndex = (historyIndex + 1) % 50;
    lastHistoryUpdate = millis();
  }
  
  bodyMenuDraw();
}

void bodyMenuDraw() {
  if (bodyMenuMode == BODY_MODE_SENSORS) {
    drawSensorMode();
  } else {
    drawMenuMode();
  }
}

void drawSensorMode() {
  // Clear background
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // Draw title bar
  drawBodySpeedBarTop("Body Monitor");
  
  // Draw left frame
  drawLeftFrame();
  
  // Draw sensor header
  drawBodySensorHeader(getBPM(), getTempValue());
  
  // Draw sensor graphs/indicators in canvas
  drawHeartRateGraph(getBPM(), heartRateHistory, 50);
  drawTemperatureBar(getTempValue(), BODY_CFG.tempMin, BODY_CFG.tempMax);
  drawGSRIndicator(getGsrValue(), BODY_CFG.gsrMax);
  drawAIOverruleStatus(getAiOverruleActive(), getCurrentTrustOverride(), getCurrentSleeveOverride());
  
  // Draw right panel status
  drawSensorStatus();
  
  // Help text
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, R_WIN_Y + R_WIN_H - 15);
  body_gfx->print("Touch: Menu  Long: Record");
}

void drawSensorStatus() {
  // Right panel sensor status
  int y = R_WIN_Y + 20;
  setMenuFontTitle();
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->print("Body Status");
  
  y += 30;
  setMenuFontItem();
  
  // Heart rate
  float bpm = getBPM();
  uint16_t hrColor = (bpm > 60 && bpm < 140) ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_ORANGE;
  body_gfx->setTextColor(hrColor, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->printf("Heart: %.0f BPM", bpm);
  y += 20;
  
  // Temperature
  float temp = getTempValue();
  uint16_t tempColor = (temp > 36.0f && temp < 38.0f) ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_ORANGE;
  body_gfx->setTextColor(tempColor, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->printf("Temp: %.1f C", temp);
  y += 20;
  
  // GSR
  float gsr = getGsrValue();
  uint16_t gsrColor = (gsr < 500) ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_RED;
  body_gfx->setTextColor(gsrColor, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->printf("GSR: %.0f", gsr);
  y += 30;
  
  // AI Status
  body_gfx->setTextColor(BODY_CFG.COL_AI, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->print("AI Overrule:");
  y += 15;
  bool aiActive = getAiOverruleActive();
  body_gfx->setTextColor(aiActive ? BODY_CFG.DOT_RED : BODY_CFG.DOT_GREEN, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 15, y);
  body_gfx->print(aiActive ? "ACTIVE" : "Standby");
  
  if (aiActive) {
    y += 15;
    body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
    body_gfx->setCursor(R_WIN_X + 15, y);
    body_gfx->printf("Trust: %.0f%%", getCurrentTrustOverride() * 100);
    y += 12;
    body_gfx->setCursor(R_WIN_X + 15, y);
    body_gfx->printf("Snelheid: %.0f%%", getCurrentSleeveOverride() * 100);
  }
  
  y += 30;
  
  // Recording status
  bool recording = getIsRecording();
  uint16_t recColor = recording ? BODY_CFG.DOT_RED : BODY_CFG.DOT_GREY;
  body_gfx->setTextColor(recColor, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->printf("Recording: %s", recording ? "ON" : "OFF");
  y += 15;
  
  // ESP-NOW status
  bool espInit = getEspNowInitialized();
  uint16_t espColor = espInit ? BODY_CFG.DOT_GREEN : BODY_CFG.DOT_RED;
  body_gfx->setTextColor(espColor, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, y);
  body_gfx->printf("ESP-NOW: %s", espInit ? "OK" : "ERROR");
}

void drawMenuMode() {
  // Fill right panel
  body_gfx->fillRect(R_WIN_X, R_WIN_Y, R_WIN_W, R_WIN_H, BODY_CFG.COL_BG);
  body_gfx->drawRoundRect(R_WIN_X+0, R_WIN_Y+0, R_WIN_W, R_WIN_H, 12, BODY_CFG.COL_FRAME2);
  body_gfx->drawRoundRect(R_WIN_X+2, R_WIN_Y+2, R_WIN_W-4, R_WIN_H-4, 10, BODY_CFG.COL_FRAME);
  
  // Title
  setMenuFontTitle();
  body_gfx->setTextColor(BODY_CFG.COL_BRAND, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, R_WIN_Y + 30);
  
  switch(bodyMenuPage) {
    case BODY_PAGE_MAIN:
      body_gfx->print("Body Menu");
      drawMainMenuItems();
      break;
    case BODY_PAGE_AI_SETTINGS:
      body_gfx->print("AI Settings");
      drawAISettingsItems();
      break;
    case BODY_PAGE_SENSOR_CAL:
      body_gfx->print("Sensor Cal");
      drawSensorCalItems();
      break;
    case BODY_PAGE_RECORDING:
      body_gfx->print("Recording");
      drawRecordingItems();
      break;
    case BODY_PAGE_ESP_STATUS:
      body_gfx->print("ESP Status");
      drawESPStatusItems();
      break;
  }
  
  // Help text
  body_gfx->setFont(nullptr);
  body_gfx->setTextSize(1);
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 10, R_WIN_Y + R_WIN_H - 15);
  body_gfx->print("Touch: select  Long: back");
}

void drawMainMenuItems() {
  setMenuFontItem();
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  const char* items[] = {
    "< Terug naar sensors",
    "AI Overrule settings",  
    "Sensor calibratie",
    "Recording opties",
    "ESP-NOW status"
  };
  
  for (int i = 0; i < 5; i++) {
    bool selected = (i == bodyMenuIdx);
    uint16_t color = selected ? BODY_CFG.DOT_GREEN : BODY_CFG.COL_MENU_PINK;
    body_gfx->setTextColor(color, BODY_CFG.COL_BG);
    body_gfx->setCursor(R_WIN_X + 20, y);
    body_gfx->print(items[i]);
    y += LH;
  }
}

void drawAISettingsItems() {
  setMenuFontItem();
  int y = R_WIN_Y + 60;
  const int LH = 20;
  
  body_gfx->setTextColor(BODY_CFG.COL_MENU_PINK, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("< Terug");
  y += LH + 10;
  
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("AI instellingen:");
  y += LH;
  
  // Use the simple Body Config values instead of complex AIOverruleConfig
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Enabled: %s", BODY_CFG.aiEnabled ? "JA" : "NEE");
  y += 15;
  
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("HR Low: %.0f", BODY_CFG.hrLowThreshold);
  y += 15;
  
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("HR High: %.0f", BODY_CFG.hrHighThreshold);
  y += 15;
  
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Temp High: %.1f", BODY_CFG.tempHighThreshold);
  y += 15;
  
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("GSR High: %.0f", BODY_CFG.gsrHighThreshold);
}

void drawSensorCalItems() {
  setMenuFontItem();
  int y = R_WIN_Y + 60;
  
  body_gfx->setTextColor(BODY_CFG.COL_MENU_PINK, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("< Terug");
  y += 30;
  
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("Sensor kalibratie");
  y += 20;
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("nog niet geïmplementeerd");
}

void drawRecordingItems() {
  setMenuFontItem();
  int y = R_WIN_Y + 60;
  
  body_gfx->setTextColor(BODY_CFG.COL_MENU_PINK, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("< Terug");
  y += 30;
  
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Status: %s", getIsRecording() ? "RECORDING" : "Gestopt");
  y += 20;
  
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Samples: %u", getSamplesRecorded());
}

void drawESPStatusItems() {
  setMenuFontItem();
  int y = R_WIN_Y + 60;
  
  body_gfx->setTextColor(BODY_CFG.COL_MENU_PINK, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->print("< Terug");
  y += 30;
  
  body_gfx->setTextColor(BODY_CFG.DOT_GREY, BODY_CFG.COL_BG);
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("ESP-NOW: %s", getEspNowInitialized() ? "OK" : "FOUT");
  y += 15;
  
  uint32_t timeSinceLastComm = millis() - getLastCommTime();
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Last RX: %us ago", timeSinceLastComm / 1000);
  y += 15;
  
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Trust: %.1f", getTrustSpeed());
  y += 15;
  body_gfx->setCursor(R_WIN_X + 20, y);
  body_gfx->printf("Sleeve: %.1f", getSleeveSpeed());
}

void bodyMenuHandleTouch(int16_t x, int16_t y, bool pressed) {
  if (!pressed) return;
  
  if (bodyMenuMode == BODY_MODE_SENSORS) {
    // Switch to menu mode on touch
    bodyMenuMode = BODY_MODE_MENU;
    bodyMenuPage = BODY_PAGE_MAIN;
    bodyMenuIdx = 0;
  } else {
    // Handle menu navigation
    if (x >= R_WIN_X + 20 && x <= R_WIN_X + R_WIN_W - 20) {
      int itemY = R_WIN_Y + 60;
      int selectedItem = (y - itemY) / 20;
      
      if (selectedItem >= 0 && selectedItem < 5) {
        bodyMenuIdx = selectedItem;
        
        // Handle selection
        switch(bodyMenuPage) {
          case BODY_PAGE_MAIN:
            if (selectedItem == 0) {
              bodyMenuMode = BODY_MODE_SENSORS; // Back to sensors
            } else {
              bodyMenuPage = (BodyMenuPage)selectedItem; // Go to sub-page
              bodyMenuIdx = 0;
            }
            break;
          default:
            if (selectedItem == 0) {
              bodyMenuPage = BODY_PAGE_MAIN; // Back to main menu
              bodyMenuIdx = 0;
            }
            break;
        }
      }
    }
  }
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
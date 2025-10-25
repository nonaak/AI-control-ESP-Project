/*  
  Body ESP SC01 Plus - COMPLETE VERSION
  Hardware: Smart Panlee SC01 Plus ESP32-S3
  
  Sensor Changes:
  - ADS1115 ADC @ 0x48 op Wire1 (GPIO 10/11)
    - A0: GSR sensor
    - A1: Flex sensor (ademhaling)
    - A2: Pulse sensor (hartslag)
    - A3: NTC temperatuur sensor
  - DS3231 RTC @ 0x68 op Wire1
  - EEPROM @ 0x57 op Wire1
  
  Display: ST7796 480x320 RGB parallel (backlight GPIO 45)
  Touch: FT6336U @ 0x38 op Wire (GPIO 6/5)
  SD Card: CS pin 41
*/

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include <esp_now.h>
#include <WiFi.h>

// ===== ADS1115 Sensor System =====
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

#define SENSOR_SDA 10          // GPIO 10 = SDA voor sensoren (Wire1)
#define SENSOR_SCL 11          // GPIO 11 = SCL voor sensoren (Wire1)

// ADS1115 sensor calibratie
static int gsr_threshold = 0;
static float flex_baseline = 0.0f;
static int pulse_baseline = 0;
static int pulse_max = 0;
static int pulse_min = 32767;
static bool beat_detected = false;
static unsigned long lastBeat = 0;

// NTC Temperature constanten (10kΩ @ 25°C, B=3950K)
#define R_TOP 33000.0          // 33kΩ serie weerstand
#define NTC_NOMINAL 10000.0
#define TEMP_NOMINAL 25.0
#define B_COEFFICIENT 3950.0

// Math constanten
#ifndef PI
#define PI 3.14159265359
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530718
#endif

// Display rotatie
#define DISPLAY_ROTATION 1
static uint8_t currentRotation = DISPLAY_ROTATION;

// Include alle headers
#include "body_config.h"
#include "body_display.h"
#include "body_gfx4.h"
#include "input_touch.h"
#include "body_menu.h"
#include "menu_view.h"
#include "ai_overrule.h"
#include "sensor_settings.h"
#include "system_settings_view.h"
#include "playlist_view.h"
#include "overrule_view.h"
#include "ai_analyze_view.h"
#include "ai_event_config_view.h"
#include "confirm_view.h"
#include "colors_view.h"
#include "ml_training_view.h"
#include "ai_training_view.h"
#include "ml_stress_analyzer.h"

// Display reference
extern Arduino_GFX *body_gfx;

// ===== SD Card =====
#define SD_CS_PIN 41
static bool sdCardAvailable = false;
static int nextFileNumber = 1;

// ===== Recording =====
static File recordingFile;
static uint32_t recordingStartTime = 0;
static uint32_t samplesRecorded = 0;

// ===== Event Logging =====
static File eventLogFile;
static bool eventLoggingActive = false;

// ===== Sensor waarden =====
static uint16_t BPM = 0;
static uint32_t lastBeatMs = 0;

// Filters
static float hp_y_prev = 0, x_prev = 0, bp_y_prev = 0;
static float envAbs = 1000, acDiv = 600.0f;

const float FS_HZ = 50.0f;
const float DT = 1.0f/FS_HZ;
const float HP_FC = 0.5f, HP_TAU = 1.0f/(2*PI*HP_FC), HP_A = HP_TAU/(HP_TAU+DT);
const float LP_FC = 5.0f, LP_TAU = 1.0f/(2*PI*LP_FC), LP_A = DT/(LP_TAU+DT);

// Sensor variabelen
static float tempValue = 0.0f;
static float gsrValue = 0.0f;
static float gsrSmooth = 0.0f;
static float ademhalingVal = 0.0f;

// UI states
bool isRecording = false;
static bool isPlaying = false, uiMenu = false;

// App modes
enum AppMode : uint8_t { 
  MODE_MAIN=0, MODE_MENU, MODE_PLAYLIST, MODE_CONFIRM, 
  MODE_SENSOR_SETTINGS, MODE_SYSTEM_SETTINGS, MODE_OVERRULE, 
  MODE_AI_ANALYZE, MODE_AI_EVENT_CONFIG, MODE_COLORS, 
  MODE_ML_TRAINING, MODE_AI_TRAINING 
};
static AppMode mode = MODE_MAIN;

// Playback
static File playbackFile;
static bool isPlaybackActive = false;
static uint32_t playbackStartTime = 0;
static uint32_t nextSampleTime = 0;

// Confirmation
enum ConfirmType { CONFIRM_DELETE_FILE, CONFIRM_FORMAT_SD };
static ConfirmType confirmType;
static String confirmData;

// ===== ESP-NOW Communicatie =====
static uint8_t hoofdESP_MAC[] = {0xE4, 0x65, 0xB8, 0x7A, 0x85, 0xE4};
static uint8_t bodyESP_MAC[] = {0x08, 0xD1, 0xF9, 0xDC, 0xC3, 0xA4};

static String commBuffer = "";
static uint32_t lastCommTime = 0;
static float trustSpeed = 0.0f, sleeveSpeed = 0.0f, suctionLevel = 0.0f;
static bool vibeOn = false;
static bool zuigActive = false;
static float vacuumMbar = 0.0f;
static bool pauseActive = false;
static bool lubeTrigger = false;
static float cyclusTijd = 10.0f;
static uint32_t lastLubeTriggerTime = 0;
static float sleevePercentage = 0.0f;
static bool espNowInitialized = false;

// ESP-NOW message structuren
typedef struct __attribute__((packed)) {
  float trust;
  float sleeve;
  float suction;
  float pause;
  bool vibeOn;
  bool zuigActive;
  float vacuumMbar;
  bool pauseActive;
  bool lubeTrigger;
  float cyclusTijd;
  float sleevePercentage;
  uint8_t currentSpeedStep;
  char command[32];
} esp_now_receive_message_t;

typedef struct __attribute__((packed)) {
  float newTrust;
  float newSleeve;
  bool overruleActive;
  uint8_t stressLevel;
  bool vibeOn;
  bool zuigenOn;
  char command[32];
} esp_now_send_message_t;

// AI Overrule systeem
AIOverruleConfig aiConfig;
bool aiOverruleActive = false;
uint32_t lastAIUpdate = 0;
float currentTrustOverride = 1.0f;
float currentSleeveOverride = 1.0f;

// AI Control
static bool redBackgroundActive = false;
static bool aiTestModeActive = false;
static bool aiControlActive = false;
static bool userPausedAI = false;
static float aiTargetTrust = 1.5f;
static bool aiTargetVibe = true;
static bool aiTargetZuig = false;
static uint32_t lastAITestUpdate = 0;

// AI Stress Management
static bool aiStressModeActive = false;
static int currentStressLevel = 3;
static int aiStartSpeedStep = 3;
static int currentSpeedStep = 3;
static uint8_t hoofdESPSpeedStep = 3;
static uint32_t lastStressCheck = 0;
static uint32_t lastSpeedAdjust = 0;
static bool aiVacuumControl = true;
static bool aiVibeControl = false;
static uint32_t stressSimTimer = 0;

// ===== FORWARD DECLARATIONS =====
static void initSensors();
static void readADS1115Sensors();
static void calibrateADS1115();
static bool initESPNow();
static void sendESPNowMessage(const char* cmd);
static void updateStatusLabel();
static void enterMain();
static void enterMenu();
static void enterPlaylist();
static void enterSensorSettings();
static void enterSystemSettings();
static void enterOverrule();
static void enterAIAnalyze();
static void enterAIEventConfig();
static void enterColors();
static void enterMLTraining();
static void enterAITraining(String filename);
static void startRecording();
static void stopRecording();
static void startPlayback(String filename);
static void stopPlayback();
static void deleteFile(String filename);
static void formatSD();
static void showDeleteConfirm(String filename);
static void showFormatConfirm();
static void toggleScreenRotation();
static void saveSensorConfig();
static void resetSensorConfig();
static void localToggleAIOverrule();
static void startAIStressManagement();
static void stopAIStressManagement();
static void startAITest();
static void stopAITest();

// ===== ESP-NOW Callbacks =====
static void onESPNowReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len != sizeof(esp_now_receive_message_t)) {
    Serial.printf("[ESP-NOW] Size mismatch: %d != %d\n", len, sizeof(esp_now_receive_message_t));
    return;
  }
  
  esp_now_receive_message_t msg;
  memcpy(&msg, data, sizeof(msg));
  
  trustSpeed = msg.trust;
  sleeveSpeed = msg.sleeve;
  suctionLevel = msg.suction;
  vibeOn = msg.vibeOn;
  zuigActive = msg.zuigActive;
  vacuumMbar = msg.vacuumMbar;
  pauseActive = msg.pauseActive;
  lubeTrigger = msg.lubeTrigger;
  cyclusTijd = msg.cyclusTijd;
  sleevePercentage = msg.sleevePercentage;
  hoofdESPSpeedStep = msg.currentSpeedStep;
  
  lastCommTime = millis();
  
  Serial.printf("[ESP-NOW] RX: trust=%.2f sleeve=%.2f vibe=%d zuig=%d step=%d\n", 
                trustSpeed, sleeveSpeed, vibeOn, zuigActive, hoofdESPSpeedStep);
}

static void onESPNowSent(const uint8_t *mac, esp_now_send_status_t status) {
  Serial.printf("[ESP-NOW] TX %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ===== ADS1115 SENSOR FUNCTIONS =====
static void calibrateADS1115() {
  Serial.println("[ADS1115] Starting calibration...");
  
  // GSR baseline (10 samples)
  long gsr_sum = 0;
  for(int i = 0; i < 10; i++) {
    gsr_sum += ads.readADC_SingleEnded(0);
    delay(50);
  }
  gsr_threshold = gsr_sum / 10;
  Serial.printf("[ADS1115] GSR baseline: %d\n", gsr_threshold);
  
  // Flex baseline
  float flex_sum = 0;
  for(int i = 0; i < 10; i++) {
    int16_t raw = ads.readADC_SingleEnded(1);
    flex_sum += ads.computeVolts(raw);
    delay(50);
  }
  flex_baseline = flex_sum / 10.0f;
  Serial.printf("[ADS1115] Flex baseline: %.3fV\n", flex_baseline);
  
  // Pulse baseline
  long pulse_sum = 0;
  for(int i = 0; i < 10; i++) {
    pulse_sum += ads.readADC_SingleEnded(2);
    delay(50);
  }
  pulse_baseline = pulse_sum / 10;
  pulse_max = pulse_baseline + 100;
  pulse_min = pulse_baseline - 100;
  Serial.printf("[ADS1115] Pulse baseline: %d\n", pulse_baseline);
  
  Serial.println("[ADS1115] Calibration complete!");
}

static void readADS1115Sensors() {
  // A0: GSR
  int16_t gsr_raw = ads.readADC_SingleEnded(0);
  gsrValue = (float)(gsr_raw - gsr_threshold);
  if(gsrValue < 0) gsrValue = 0;
  gsrSmooth = gsrSmooth * 0.9f + gsrValue * 0.1f;
  
  // A1: Flex (ademhaling)
  int16_t flex_raw = ads.readADC_SingleEnded(1);
  float flex_voltage = ads.computeVolts(flex_raw);
  float flex_delta = flex_voltage - flex_baseline;
  ademhalingVal = 50.0f + (flex_delta * 100.0f);
  ademhalingVal = constrain(ademhalingVal, 0, 100);
  
  // A2: Pulse met beat detection
  int16_t pulse_raw = ads.readADC_SingleEnded(2);
  
  // High-pass filter
  float hp_y = HP_A * (hp_y_prev + pulse_raw - x_prev);
  hp_y_prev = hp_y;
  x_prev = pulse_raw;
  
  // Envelope detection
  envAbs = envAbs * 0.99f + fabs(hp_y) * 0.01f;
  float threshold = envAbs * 0.6f;
  
  // Beat detection
  static bool wasAbove = false;
  bool isAbove = (hp_y > threshold);
  
  if(isAbove && !wasAbove) {
    unsigned long now = millis();
    unsigned long interval = now - lastBeat;
    if(interval > 300 && interval < 2000) {
      BPM = 60000 / interval;
      lastBeat = now;
      beat_detected = true;
    }
  }
  wasAbove = isAbove;
  
  // A3: NTC Temperature
  int16_t ntc_raw = ads.readADC_SingleEnded(3);
  float ntc_voltage = ads.computeVolts(ntc_raw);
  
  // Bereken weerstand van NTC
  float r_ntc = R_TOP * ntc_voltage / (3.3f - ntc_voltage);
  
  // Steinhart-Hart vergelijking
  float steinhart = log(r_ntc / NTC_NOMINAL);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0f / (TEMP_NOMINAL + 273.15f);
  steinhart = 1.0f / steinhart;
  tempValue = steinhart - 273.15f;
  
  // Sanity check
  if(tempValue < 10.0f || tempValue > 50.0f) {
    tempValue = 25.0f;
  }
}

// ===== INIT FUNCTIONS =====
static void initSensors() {
  Serial.println("[SENSORS] Initializing...");
  
  // Init sensor I2C bus (Wire1)
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  Wire1.setClock(400000);
  delay(100);
  
  // I2C Scanner op Wire1
  Serial.println("[I2C] Scanning Wire1 bus...");
  for(byte addr = 1; addr < 127; addr++) {
    Wire1.beginTransmission(addr);
    if(Wire1.endTransmission() == 0) {
      Serial.printf("[I2C] Device found at 0x%02X\n", addr);
    }
  }
  
  // Init ADS1115
  if(!ads.begin(0x48, &Wire1)) {
    Serial.println("[ADS1115] NOT FOUND!");
  } else {
    Serial.println("[ADS1115] OK!");
    ads.setGain(GAIN_ONE);  // ±4.096V range
    ads.setDataRate(RATE_ADS1115_128SPS);
    delay(100);
    calibrateADS1115();
  }
  
  Serial.println("[SENSORS] Init complete!");
}

static bool initESPNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  if(esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init FAILED!");
    return false;
  }
  
  esp_now_register_recv_cb(onESPNowReceive);
  esp_now_register_send_cb(onESPNowSent);
  
  // Add HoofdESP peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, hoofdESP_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if(esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Add peer FAILED!");
    return false;
  }
  
  Serial.println("[ESP-NOW] Init OK!");
  return true;
}

static void sendESPNowMessage(const char* cmd) {
  if(!espNowInitialized) return;
  
  esp_now_send_message_t msg = {};
  msg.newTrust = currentTrustOverride;
  msg.newSleeve = currentSleeveOverride;
  msg.overruleActive = aiOverruleActive;
  msg.stressLevel = currentStressLevel;
  msg.vibeOn = vibeOn;
  msg.zuigenOn = zuigActive;
  strncpy(msg.command, cmd, 31);
  
  esp_err_t result = esp_now_send(hoofdESP_MAC, (uint8_t*)&msg, sizeof(msg));
  
  if(result == ESP_OK) {
    Serial.printf("[ESP-NOW] TX: %s\n", cmd);
  } else {
    Serial.printf("[ESP-NOW] TX FAILED: %d\n", result);
  }
}

// ===== SD CARD FUNCTIONS =====
static void initSDCard() {
  Serial.println("[SD] Initializing...");
  
  if(!SD.begin(SD_CS_PIN)) {
    Serial.println("[SD] Mount FAILED!");
    sdCardAvailable = false;
    return;
  }
  
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("[SD] Card size: %lluMB\n", cardSize);
  
  sdCardAvailable = true;
  
  // Find next file number
  File root = SD.open("/");
  while(true) {
    File entry = root.openNextFile();
    if(!entry) break;
    
    String name = entry.name();
    if(name.startsWith("REC") && name.endsWith(".txt")) {
      int num = name.substring(3, name.length()-4).toInt();
      if(num >= nextFileNumber) {
        nextFileNumber = num + 1;
      }
    }
    entry.close();
  }
  root.close();
  
  Serial.printf("[SD] Next file: REC%03d.txt\n", nextFileNumber);
}

// ===== MODE CHANGE FUNCTIONS =====
static void enterMain() {
  mode = MODE_MAIN;
  uiMenu = false;
  body_gfx4_init(body_gfx);
  bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
  body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
  updateStatusLabel();
}

static void enterMenu() {
  mode = MODE_MENU;
  uiMenu = true;
  menu_begin(body_gfx);
}

static void enterPlaylist() {
  mode = MODE_PLAYLIST;
  playlist_begin(body_gfx);
}

static void enterSensorSettings() {
  mode = MODE_SENSOR_SETTINGS;
  sensorSettings_begin(body_gfx);
}

static void enterSystemSettings() {
  mode = MODE_SYSTEM_SETTINGS;
  systemSettings_begin(body_gfx);
}

static void enterOverrule() {
  mode = MODE_OVERRULE;
  overrule_begin(body_gfx);
}

static void enterAIAnalyze() {
  mode = MODE_AI_ANALYZE;
  aiAnalyze_begin(body_gfx);
}

static void enterAIEventConfig() {
  mode = MODE_AI_EVENT_CONFIG;
  aiEventConfig_begin(body_gfx);
}

static void enterColors() {
  mode = MODE_COLORS;
  colors_begin(body_gfx);
}

static void enterMLTraining() {
  mode = MODE_ML_TRAINING;
  mlTraining_begin(body_gfx);
}

static void enterAITraining(String filename) {
  mode = MODE_AI_TRAINING;
  aiTraining_begin(body_gfx, filename);
}

// ===== RECORDING/PLAYBACK =====
static void startRecording() {
  if(!sdCardAvailable) {
    Serial.println("[REC] SD card not available!");
    return;
  }
  
  char filename[32];
  sprintf(filename, "/REC%03d.txt", nextFileNumber++);
  
  recordingFile = SD.open(filename, FILE_WRITE);
  if(!recordingFile) {
    Serial.println("[REC] Failed to open file!");
    return;
  }
  
  isRecording = true;
  recordingStartTime = millis();
  samplesRecorded = 0;
  
  // Write header
  recordingFile.println("# Body ESP Recording");
  recordingFile.println("# Time,BPM,Temp,GSR,Breathing,Trust,Sleeve,Vibe,Zuig");
  
  Serial.printf("[REC] Started: %s\n", filename);
}

static void stopRecording() {
  if(!isRecording) return;
  
  recordingFile.close();
  isRecording = false;
  
  Serial.printf("[REC] Stopped. Samples: %d\n", samplesRecorded);
}

static void startPlayback(String filename) {
  stopRecording();
  
  String path = "/" + filename;
  playbackFile = SD.open(path);
  
  if(!playbackFile) {
    Serial.println("[PLAY] Failed to open file!");
    return;
  }
  
  isPlaybackActive = true;
  playbackStartTime = millis();
  nextSampleTime = 0;
  
  // Skip header lines
  while(playbackFile.available()) {
    String line = playbackFile.readStringUntil('\n');
    if(!line.startsWith("#")) {
      playbackFile.seek(playbackFile.position() - line.length() - 1);
      break;
    }
  }
  
  Serial.printf("[PLAY] Started: %s\n", filename.c_str());
}

static void stopPlayback() {
  if(!isPlaybackActive) return;
  
  playbackFile.close();
  isPlaybackActive = false;
  isPlaying = false;
  
  Serial.println("[PLAY] Stopped");
}

static void deleteFile(String filename) {
  String path = "/" + filename;
  if(SD.remove(path)) {
    Serial.printf("[SD] Deleted: %s\n", filename.c_str());
  } else {
    Serial.printf("[SD] Delete failed: %s\n", filename.c_str());
  }
}

static void formatSD() {
  Serial.println("[SD] Formatting...");
  // Note: Arduino SD library doesn't support format
  // User should format SD card on computer
  body_gfx4_drawStatus("Format SD on computer!");
  delay(2000);
}

// ===== CONFIRMATION DIALOGS =====
static void showDeleteConfirm(String filename) {
  mode = MODE_CONFIRM;
  confirmType = CONFIRM_DELETE_FILE;
  confirmData = filename;
  confirm_begin(body_gfx, "Bestand verwijderen?", filename);
}

static void showFormatConfirm() {
  mode = MODE_CONFIRM;
  confirmType = CONFIRM_FORMAT_SD;
  confirmData = "";
  confirm_begin(body_gfx, "SD card formatteren?", "Alle data wordt gewist!");
}

// ===== UTILITY FUNCTIONS =====
static void toggleScreenRotation() {
  currentRotation = (currentRotation + 1) % 4;
  body_gfx->setRotation(currentRotation);
  Serial.printf("[DISPLAY] Rotation: %d\n", currentRotation);
}

static void updateStatusLabel() {
  char status[64];
  sprintf(status, "BPM:%d T:%.1f°C GSR:%.0f", BPM, tempValue, gsrSmooth);
  body_gfx4_drawStatus(status);
}

static void saveSensorConfig() {
  // Save to EEPROM if needed
  Serial.println("[CONFIG] Sensor settings saved");
}

static void resetSensorConfig() {
  // Reset to defaults
  Serial.println("[CONFIG] Sensor settings reset");
}

static void localToggleAIOverrule() {
  aiConfig.enabled = !aiConfig.enabled;
  aiOverruleActive = aiConfig.enabled;
  
  if(aiOverruleActive) {
    Serial.println("[AI] Overrule ENABLED");
  } else {
    Serial.println("[AI] Overrule DISABLED");
  }
}

// ===== AI CONTROL FUNCTIONS =====
static void startAIStressManagement() {
  aiStressModeActive = true;
  currentStressLevel = 3;
  lastStressCheck = millis();
  lastSpeedAdjust = millis();
  
  Serial.println("[AI-STRESS] Started");
}

static void stopAIStressManagement() {
  aiStressModeActive = false;
  
  Serial.println("[AI-STRESS] Stopped");
}

static void startAITest() {
  aiTestModeActive = true;
  aiControlActive = true;
  userPausedAI = false;
  lastAITestUpdate = millis();
  
  Serial.println("[AI-TEST] Started");
}

static void stopAITest() {
  aiTestModeActive = false;
  aiControlActive = false;
  
  Serial.println("[AI-TEST] Stopped");
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("Body ESP SC01 Plus V3");
  Serial.println("ADS1115 Sensor Migration");
  Serial.println("=================================\n");
  
  // Init display
  Serial.println("[SETUP] Initializing display...");
  body_display_init();
  delay(100);
  
  // Init sensors
  initSensors();
  
  // Init SD card
  initSDCard();
  
  // Init ESP-NOW
  espNowInitialized = initESPNow();
  
  // Init touch
  Serial.println("[SETUP] Initializing touch...");
  inputTouchInit();
  
  // Init UI
  Serial.println("[SETUP] Initializing UI...");
  body_gfx4_init(body_gfx);
  body_gfx4_drawButtons(false, false, false, false);
  updateStatusLabel();
  
  Serial.println("[SETUP] Ready!\n");
}

// ===== LOOP =====
void loop() {
  static uint32_t lastSensorRead = 0;
  static uint32_t lastUIUpdate = 0;
  static uint32_t lastESPNowSend = 0;
  
  uint32_t now = millis();
  
  // Read sensors @ 20Hz
  if(now - lastSensorRead >= 50) {
    lastSensorRead = now;
    readADS1115Sensors();
    
    // Write to recording file
    if(isRecording && recordingFile) {
      uint32_t timestamp = now - recordingStartTime;
      recordingFile.printf("%lu,%d,%.1f,%.0f,%.0f,%.2f,%.2f,%d,%d\n",
                          timestamp, BPM, tempValue, gsrSmooth, ademhalingVal,
                          trustSpeed, sleeveSpeed, vibeOn, zuigActive);
      samplesRecorded++;
    }
  }
  
  // Update UI @ 5Hz
  if(now - lastUIUpdate >= 200) {
    lastUIUpdate = now;
    
    if(mode == MODE_MAIN) {
      // Update main screen
      body_gfx4_drawSensorCircle(BPM, tempValue, gsrSmooth, ademhalingVal);
      updateStatusLabel();
    }
  }
  
  // Send ESP-NOW heartbeat @ 1Hz
  if(espNowInitialized && now - lastESPNowSend >= 1000) {
    lastESPNowSend = now;
    sendESPNowMessage("HEARTBEAT");
  }
  
  // Handle playback
  if(isPlaybackActive) {
    // Playback logic here
    // Read line from file, parse, set variables
  }
  
  // Handle different modes
  if(mode == MODE_MAIN) {
    TouchEvent ev = inputTouchPoll();
    
    if(ev.kind == TE_TAP_REC) {
      if(isRecording) {
        stopRecording();
      } else {
        startRecording();
      }
      bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
      body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
    }
    
    if(ev.kind == TE_TAP_PLAY) {
      if(isPlaying) {
        stopPlayback();
      } else {
        enterPlaylist();
      }
    }
    
    if(ev.kind == TE_TAP_MENU) {
      enterMenu();
    }
    
    if(ev.kind == TE_TAP_OVERRULE) {
      bool anyAIActive = aiStressModeActive || aiTestModeActive || aiConfig.enabled;
      
      if(anyAIActive) {
        if(aiStressModeActive) stopAIStressManagement();
        if(aiTestModeActive) stopAITest();
        if(aiConfig.enabled) {
          aiConfig.enabled = false;
          aiOverruleActive = false;
        }
      } else {
        startAIStressManagement();
      }
      
      bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
      body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
    }
  }
  
  else if(mode == MODE_MENU) {
    MenuEvent mev = menu_poll();
    if(mev == ME_BACK) enterMain();
    else if(mev == ME_SENSOR_SETTINGS) enterSensorSettings();
    else if(mev == ME_OVERRULE) enterOverrule();
    else if(mev == ME_SYSTEM_SETTINGS) enterSystemSettings();
    else if(mev == ME_PLAYLIST) enterPlaylist();
    else if(mev == ME_ML_TRAINING) enterMLTraining();
  }
  
  else if(mode == MODE_PLAYLIST) {
    PlaylistEvent pev = playlist_poll();
    if(pev == PE_BACK) enterMain();
    else if(pev == PE_PLAY_FILE) {
      String filename = playlist_getSelectedFile();
      if(filename.length() > 0) {
        startPlayback(filename);
        enterMain();
      }
    }
    else if(pev == PE_DELETE_CONFIRM) {
      String filename = playlist_getSelectedFile();
      if(filename.length() > 0) showDeleteConfirm(filename);
    }
    else if(pev == PE_AI_ANALYZE) {
      String filename = playlist_getSelectedFile();
      if(filename.length() > 0) {
        aiAnalyze_setSelectedFile(filename);
        enterAIAnalyze();
      }
    }
  }
  
  else if(mode == MODE_SENSOR_SETTINGS) {
    SensorEvent sev = sensorSettings_poll();
    if(sev == SE_BACK) enterMenu();
    else if(sev == SE_SAVE) {
      saveSensorConfig();
      body_gfx4_drawStatus("Instellingen opgeslagen!");
      delay(1000);
      enterMenu();
    }
    else if(sev == SE_RESET) {
      resetSensorConfig();
      body_gfx4_drawStatus("Instellingen gereset!");
      delay(1000);
    }
  }
  
  else if(mode == MODE_SYSTEM_SETTINGS) {
    SystemSettingsEvent sse = systemSettings_poll();
    if(sse == SSE_BACK) enterMenu();
    else if(sse == SSE_ROTATE_SCREEN) {
      toggleScreenRotation();
      systemSettings_begin(body_gfx);
    }
    else if(sse == SSE_COLORS) enterColors();
    else if(sse == SSE_FORMAT_SD) showFormatConfirm();
  }
  
  else if(mode == MODE_OVERRULE) {
    OverruleEvent oev = overrule_poll();
    if(oev == OE_BACK) enterMenu();
    else if(oev == OE_TOGGLE_AI) {
      localToggleAIOverrule();
      overrule_begin(body_gfx);
    }
    else if(oev == OE_SAVE) {
      body_gfx4_drawStatus("AI instellingen opgeslagen!");
      delay(1000);
      enterMenu();
    }
    else if(oev == OE_RESET) {
      aiConfig = AIOverruleConfig();
      overrule_begin(body_gfx);
    }
    else if(oev == OE_ANALYZE) enterAIAnalyze();
    else if(oev == OE_CONFIG) enterAIEventConfig();
  }
  
  else if(mode == MODE_AI_ANALYZE) {
    AnalyzeEvent aev = aiAnalyze_poll();
    if(aev == AE_BACK) enterOverrule();
    else if(aev == AE_TRAIN) {
      String filename = aiAnalyze_getSelectedFile();
      if(filename.length() > 0) enterAITraining(filename);
    }
  }
  
  else if(mode == MODE_AI_EVENT_CONFIG) {
    EventConfigEvent ece = aiEventConfig_poll();
    if(ece == ECE_BACK) enterOverrule();
    else if(ece == ECE_SAVE) {
      aiEventConfig_saveToEEPROM();
      body_gfx4_drawStatus("Event configuratie opgeslagen!");
      delay(1500);
      enterOverrule();
    }
    else if(ece == ECE_RESET) {
      aiEventConfig_resetToDefaults();
      aiEventConfig_begin(body_gfx);
    }
  }
  
  else if(mode == MODE_COLORS) {
    ColorEvent ce = colors_poll();
    if(ce == CE_BACK) enterSystemSettings();
    else if(ce == CE_COLOR_CHANGE) {
      body_gfx4_drawStatus("Kleur gewijzigd!");
      delay(1000);
    }
  }
  
  else if(mode == MODE_AI_TRAINING) {
    TrainingResult tr = aiTraining_poll();
    if(tr == TR_BACK || tr == TR_COMPLETE) {
      enterOverrule();
    }
  }
  
  else if(mode == MODE_ML_TRAINING) {
    MLTrainingEvent mte = mlTraining_poll();
    if(mte == MTE_BACK) enterMenu();
    else if(mte == MTE_IMPORT_DATA) mlTraining_setState(ML_STATE_IMPORT);
    else if(mte == MTE_TRAIN_MODEL) mlTraining_setState(ML_STATE_TRAINING);
    else if(mte == MTE_MODEL_MANAGER) mlTraining_setState(ML_STATE_MODEL_MANAGER);
  }
  
  else if(mode == MODE_CONFIRM) {
    ConfirmEvent cev = confirm_poll();
    
    if(cev == CONF_YES) {
      if(confirmType == CONFIRM_DELETE_FILE) {
        deleteFile(confirmData);
        enterPlaylist();
      } else if(confirmType == CONFIRM_FORMAT_SD) {
        formatSD();
        enterMenu();
      }
    } else if(cev == CONF_NO) {
      if(confirmType == CONFIRM_DELETE_FILE) {
        enterPlaylist();
      } else if(confirmType == CONFIRM_FORMAT_SD) {
        enterMenu();
      }
    }
  }
  
  delay(10);
}

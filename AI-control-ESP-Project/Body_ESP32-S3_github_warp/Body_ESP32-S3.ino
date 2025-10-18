/*  
  Body ESP32-S3 T-HMI – DEFINITIEVE VERSIE
  
  Hardware: LilyGO T-HMI ESP32-S3 2.8" IPS-TFT Display
  Sensors: ADS1115 (4-channel 16-bit ADC via I2C)
    - A0: Grove GSR
    - A1: Flex Sensor (90mm, breathing)
    - A2: Pulse Sensor (PPG, heartrate)
    - A3: 10kΩ NTC (skin temperature)
  
  Features:
    - Real-time biometric monitoring
    - ML stress analysis with autonomy system
    - Advanced stress management
    - MultiFunPlayer funscript integration
    - ESP-NOW communication with HoofdESP
    - Session recording/playback
    - Non-blocking architecture (NO delay() calls in loop!)
*/

// ===== T-HMI Hardware Configuration =====
#define PWR_EN_PIN  (10)
#define PWR_ON_PIN  (14) 
#define BAT_ADC_PIN (5)
#define BUTTON1_PIN (0)
#define BUTTON2_PIN (21)

// LCD pins (parallel 8-bit interface)
#define LCD_DATA0_PIN (48)
#define LCD_DATA1_PIN (47)
#define LCD_DATA2_PIN (39)
#define LCD_DATA3_PIN (40)
#define LCD_DATA4_PIN (41)
#define LCD_DATA5_PIN (42)
#define LCD_DATA6_PIN (45)
#define LCD_DATA7_PIN (46)
#define PCLK_PIN      (8)
#define CS_PIN        (6)
#define DC_PIN        (7)
#define RST_PIN       (-1)
#define BK_LIGHT_PIN  (38)

// Touch pins
#define TOUCHSCREEN_SCLK_PIN (1)
#define TOUCHSCREEN_MISO_PIN (4)
#define TOUCHSCREEN_MOSI_PIN (3)
#define TOUCHSCREEN_CS_PIN   (2)
#define TOUCHSCREEN_IRQ_PIN  (9)

// SD card pins (SD_MMC 1-bit mode)
#define SD_MMC_CLK  (12)
#define SD_MMC_CMD  (11)  
#define SD_MMC_DAT0 (13)

// I2C pins (Grove connector)
#define I2C_SDA_PIN (43)
#define I2C_SCL_PIN (44)

#define DISPLAY_ROTATION 3

// ===== Includes =====
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <SPI.h>
#include <SD_MMC.h>
#include "TFT_eSPI.h"
#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_ADS1X15.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Screen dimensions
const int SCR_W = 320;
const int SCR_H = 240;

// Math constants
#ifndef PI
#define PI 3.14159265359
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530718
#endif

// Project includes
#include "body_config.h"
#include "body_display.h"
#include "body_gfx4.h"
#include "input_touch.h"
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
#include "advanced_stress_manager.h"
#include "multifunplayer_client.h"
#include "touch_calibration.h"

// T-HMI Display
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI *body_gfx_tft = &tft;
static uint8_t currentRotation = DISPLAY_ROTATION;

// ===== ADS1115 ADC =====
Adafruit_ADS1115 ads;
static bool adsInitialized = false;

#define ADS_CH_GSR    0
#define ADS_CH_FLEX   1
#define ADS_CH_PULSE  2
#define ADS_CH_NTC    3

// NTC constants
#define NTC_R_FIXED   22000.0f
#define NTC_R_25C     10000.0f
#define NTC_B_VALUE   3950.0f
#define NTC_T0_KELVIN 298.15f

// Flex constants
#define FLEX_R_FIXED  22000.0f
#define FLEX_R_FLAT   25000.0f
#define FLEX_R_BENT   125000.0f

// Sensor data
static float gsrValue = 0.0f;
static float gsrSmooth = 0.0f;
static float flexValue = 0.0f;
static uint16_t BPM = 0;
static float tempValue = 36.5f;
static uint32_t lastBeatMs = 0;

// PPG processing
#define PPG_BUFFER_SIZE 50
static float ppgBuffer[PPG_BUFFER_SIZE];
static int ppgBufferIndex = 0;
static float ppgBaseline = 1.65f;
static float ppgThreshold = 0.05f;
static bool ppgPeakDetected = false;
static uint32_t ppgLastPeakTime = 0;

// ===== Non-blocking Timers =====
static uint32_t lastSensorRead = 0;
static uint32_t lastDebugPrint = 0;
static uint32_t lastHeartbeat = 0;
static uint32_t lastMLUpdate = 0;
static uint32_t lastStressUpdate = 0;

#define SENSOR_READ_INTERVAL    100   // 10Hz sensor reading
#define DEBUG_PRINT_INTERVAL    2000  // 2s debug output
#define HEARTBEAT_INTERVAL      2000  // 2s ESP-NOW heartbeat
#define ML_UPDATE_INTERVAL      1000  // 1Hz ML analysis
#define STRESS_UPDATE_INTERVAL  1000  // 1Hz stress management

// ===== SD Card =====
static bool sdCardAvailable = false;
static int nextFileNumber = 1;
static File recordingFile;
static uint32_t recordingStartTime = 0;
static uint32_t samplesRecorded = 0;
bool isRecording = false;

// ===== UI States =====
static bool isPlaying = false;
static bool uiMenu = false;

enum AppMode : uint8_t { 
  MODE_MAIN=0, MODE_MENU, MODE_PLAYLIST, MODE_CONFIRM, 
  MODE_SENSOR_SETTINGS, MODE_SYSTEM_SETTINGS, MODE_OVERRULE, 
  MODE_AI_ANALYZE, MODE_AI_EVENT_CONFIG, MODE_COLORS, MODE_AI_TRAINING,
  MODE_CALIBRATE
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

// ===== ESP-NOW =====
static uint8_t hoofdESP_MAC[] = {0xE4, 0x65, 0xB8, 0x7A, 0x85, 0xE4};
static uint8_t bodyESP_MAC[] = {0xEC, 0xDA, 0x3B, 0x98, 0xC5, 0x24};

static uint32_t lastCommTime = 0;
static float trustSpeed = 0.0f;
static float sleeveSpeed = 0.0f;
static float suctionLevel = 0.0f;
static float pauseTime = 0.0f;
static bool vibeOn = false;
static bool zuigActive = false;
static float vacuumMbar = 0.0f;
static bool pauseActive = false;
static bool lubeTrigger = false;
static float cyclusTijd = 10.0f;
static uint32_t lastLubeTriggerTime = 0;
static float sleevePercentage = 0.0f;
static bool espNowInitialized = false;
static uint8_t hoofdESPSpeedStep = 3;

// ESP-NOW message structures
typedef struct __attribute__((packed)) {
  float trust, sleeve, suction, pause;
  bool vibeOn, zuigActive, pauseActive, lubeTrigger;
  float vacuumMbar, cyclusTijd, sleevePercentage;
  uint8_t currentSpeedStep;
  char command[32];
} esp_now_receive_message_t;

typedef struct __attribute__((packed)) {
  float newTrust, newSleeve;
  bool overruleActive;
  uint8_t stressLevel;
  bool vibeOn, zuigenOn;
  char command[32];
} esp_now_send_message_t;

// ===== AI/ML System =====
AIOverruleConfig aiConfig;
bool aiOverruleActive = false;
float currentTrustOverride = 1.0f;
float currentSleeveOverride = 1.0f;

// AI Control
static bool redBackgroundActive = false;
static bool aiTestModeActive = false;
static bool aiControlActive = false;
static bool userPausedAI = false;
static bool aiStressModeActive = false;
static int currentStressLevel = 3;
static int aiStartSpeedStep = 3;
static int currentSpeedStep = 3;

// ===== ADS1115 Functions =====

void initADS1115() {
  Serial.println("[ADS1115] Initializing...");
  
  if (!ads.begin(0x48)) {
    Serial.println("[ADS1115] ERROR: Not found!");
    adsInitialized = false;
    return;
  }
  
  ads.setGain(GAIN_ONE);
  ads.setDataRate(RATE_ADS1115_128SPS);
  
  adsInitialized = true;
  Serial.println("[ADS1115] Initialized! (±4.096V, 128 SPS)");
}

float readNTCTemperature(float voltage) {
  if (voltage >= 3.29f) return 50.0f;
  if (voltage <= 0.01f) return 0.0f;
  
  float r_ntc = NTC_R_FIXED * voltage / (3.3f - voltage);
  float temp_kelvin = 1.0f / ((1.0f / NTC_T0_KELVIN) + (1.0f / NTC_B_VALUE) * log(r_ntc / NTC_R_25C));
  return temp_kelvin - 273.15f + sensorConfig.tempOffset;
}

float readFlexBreathing(float voltage) {
  if (voltage >= 3.29f || voltage <= 0.01f) return 50.0f;
  
  float r_flex = FLEX_R_FIXED * voltage / (3.3f - voltage);
  float breathValue = 100.0f - ((r_flex - FLEX_R_FLAT) / (FLEX_R_BENT - FLEX_R_FLAT) * 100.0f);
  return constrain(breathValue, 0.0f, 100.0f);
}

void processPPGSignal(float voltage) {
  ppgBuffer[ppgBufferIndex] = voltage;
  ppgBufferIndex = (ppgBufferIndex + 1) % PPG_BUFFER_SIZE;
  
  float sum = 0;
  for (int i = 0; i < PPG_BUFFER_SIZE; i++) sum += ppgBuffer[i];
  ppgBaseline = sum / PPG_BUFFER_SIZE;
  
  float acSignal = voltage - ppgBaseline;
  float maxAC = 0;
  for (int i = 0; i < PPG_BUFFER_SIZE; i++) {
    float ac = abs(ppgBuffer[i] - ppgBaseline);
    if (ac > maxAC) maxAC = ac;
  }
  ppgThreshold = maxAC * 0.3f;
  
  static float lastAC = 0;
  static bool risingEdge = false;
  
  if (acSignal > lastAC) {
    risingEdge = true;
  } else if (risingEdge && acSignal < lastAC) {
    if (acSignal > ppgThreshold && !ppgPeakDetected) {
      uint32_t now = millis();
      uint32_t ibi = now - ppgLastPeakTime;
      
      if (ibi > 300 && ibi < 2000) {
        BPM = (uint16_t)(60000UL / ibi);
        ppgLastPeakTime = now;
        lastBeatMs = now;
        ppgPeakDetected = true;
      }
    }
    risingEdge = false;
  }
  
  if (ppgPeakDetected && (millis() - lastBeatMs > 100)) {
    ppgPeakDetected = false;
  }
  
  lastAC = acSignal;
}

void readADS1115Sensors() {
  if (!adsInitialized) return;
  
  int16_t adc0 = ads.readADC_SingleEnded(ADS_CH_GSR);
  int16_t adc1 = ads.readADC_SingleEnded(ADS_CH_FLEX);
  int16_t adc2 = ads.readADC_SingleEnded(ADS_CH_PULSE);
  int16_t adc3 = ads.readADC_SingleEnded(ADS_CH_NTC);
  
  float volt_gsr = ads.computeVolts(adc0);
  float volt_flex = ads.computeVolts(adc1);
  float volt_pulse = ads.computeVolts(adc2);
  float volt_ntc = ads.computeVolts(adc3);
  
  // GSR
  gsrValue = volt_gsr * 310.0f;
  float calibratedGSR = (gsrValue - sensorConfig.gsrBaseline) * sensorConfig.gsrSensitivity;
  float smoothFactor = sensorConfig.gsrSmoothing;
  gsrSmooth = gsrSmooth * (1.0f - smoothFactor) + calibratedGSR * smoothFactor;
  if (gsrSmooth < 0) gsrSmooth = 0;
  
  // Flex
  flexValue = readFlexBreathing(volt_flex);
  
  // PPG
  processPPGSignal(volt_pulse);
  
  // NTC
  tempValue = readNTCTemperature(volt_ntc);
  
  // Feed to ML
  mlAnalyzer.addSensorSample((float)BPM, tempValue, gsrSmooth);
  
  // Update stress manager
  BiometricData bio((float)BPM, tempValue, gsrSmooth);
  stressManager.update(bio);
}

// ===== ESP-NOW =====

static void onESPNowReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(esp_now_receive_message_t)) {
    esp_now_receive_message_t message;
    memcpy(&message, incomingData, sizeof(message));
    
    trustSpeed = message.trust;
    sleeveSpeed = message.sleeve;
    suctionLevel = message.suction;
    pauseTime = message.pause;
    vibeOn = message.vibeOn;
    zuigActive = message.zuigActive;
    vacuumMbar = message.vacuumMbar;
    pauseActive = message.pauseActive;
    
    if (message.lubeTrigger && !lubeTrigger) {
      lastLubeTriggerTime = millis();
    }
    lubeTrigger = message.lubeTrigger;
    cyclusTijd = message.cyclusTijd;
    sleevePercentage = message.sleevePercentage;
    hoofdESPSpeedStep = message.currentSpeedStep;
    
    lastCommTime = millis();
  }
}

static bool initESPNow() {
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(4);
  
  if (esp_now_init() != ESP_OK) return false;
  
  esp_now_register_recv_cb(onESPNowReceive);
  
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, hoofdESP_MAC, 6);
  peerInfo.channel = 4;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) return false;
  
  Serial.println("[ESP-NOW] Initialized");
  return true;
}

static bool sendESPNowMessage(float newTrust, float newSleeve, bool overruleActive, 
                             const char* command, uint8_t stressLevel = 0, 
                             bool vibeOn = false, bool zuigenOn = false) {
  if (!espNowInitialized) return false;
  
  esp_now_send_message_t message;
  memset(&message, 0, sizeof(message));
  
  message.newTrust = newTrust;
  message.newSleeve = newSleeve;
  message.overruleActive = overruleActive;
  message.stressLevel = stressLevel;
  message.vibeOn = vibeOn;
  message.zuigenOn = zuigenOn;
  strncpy(message.command, command, sizeof(message.command) - 1);
  
  esp_err_t result = esp_now_send(hoofdESP_MAC, (uint8_t *)&message, sizeof(message));
  return (result == ESP_OK);
}

// ===== SD Card =====

static void initSDCard() {
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_DAT0);
  
  if (SD_MMC.begin("/sdcard", true)) {
    sdCardAvailable = true;
    while (SD_MMC.exists("/data" + String(nextFileNumber) + ".csv")) {
      nextFileNumber++;
    }
    Serial.printf("[SD_MMC] Ready - Next file: data%d.csv\n", nextFileNumber);
  } else {
    sdCardAvailable = false;
    Serial.println("[SD_MMC] Card not found!");
  }
}

void startRecording() {
  if (!sdCardAvailable) return;
  
  String filename = "/data" + String(nextFileNumber) + ".csv";
  recordingFile = SD_MMC.open(filename, FILE_WRITE);
  
  if (recordingFile) {
    recordingFile.println("Time,Heart,Temp,Skin,Breath,Trust,Sleeve,Suction,Pause,Vibe,ZuigActive,VacuumMbar");
    recordingStartTime = millis();
    samplesRecorded = 0;
    isRecording = true;
    Serial.printf("[SD] Recording: data%d.csv\n", nextFileNumber);
  }
}

void stopRecording() {
  if (recordingFile) {
    recordingFile.close();
    Serial.printf("[SD] Stopped. Samples: %u\n", samplesRecorded);
    nextFileNumber++;
  }
  isRecording = false;
}

static void recordSample() {
  if (!isRecording || !recordingFile) return;
  
  uint32_t timeMs = millis() - recordingStartTime;
  
  recordingFile.print(timeMs); recordingFile.print(",");
  recordingFile.print(BPM); recordingFile.print(",");
  recordingFile.print(tempValue, 2); recordingFile.print(",");
  recordingFile.print(gsrSmooth, 2); recordingFile.print(",");
  recordingFile.print(flexValue, 2); recordingFile.print(",");
  recordingFile.print(trustSpeed, 2); recordingFile.print(",");
  recordingFile.print(sleeveSpeed, 2); recordingFile.print(",");
  recordingFile.print(suctionLevel, 2); recordingFile.print(",");
  recordingFile.print(pauseTime, 2); recordingFile.print(",");
  recordingFile.print(vibeOn ? 1 : 0); recordingFile.print(",");
  recordingFile.print(zuigActive ? 1 : 0); recordingFile.print(",");
  recordingFile.println(vacuumMbar, 2);
  
  samplesRecorded++;
  
  if (samplesRecorded % 100 == 0) {
    recordingFile.flush();
  }
}

// ===== UI Functions =====

static void updateStatusLabel() {
  if (mode == MODE_MENU) body_gfx4_drawStatus("Menu");
  else if (mode == MODE_PLAYLIST) body_gfx4_drawStatus("Opname selecteren");
  else if (isRecording) body_gfx4_drawStatus("OPNEMEN ACTIEF");
  else if (isPlaying) body_gfx4_drawStatus("AFSPELEN ACTIEF");
  else body_gfx4_drawStatus("Gereed");
}

static void enterMain() {
  mode = MODE_MAIN;
  uiMenu = false;
  inputTouchResetCooldown();  // Fix dubbele touch
  
  if (BODY_CFG.autoRecordSessions && isRecording) {
    stopRecording();
  }
  
  body_gfx4_begin();
  body_gfx4_setLabel(G4_HART, "Hart");
  body_gfx4_setLabel(G4_HUID, "Huid");
  body_gfx4_setLabel(G4_TEMP, "Temp");
  body_gfx4_setLabel(G4_ADEMHALING, "Adem");
  //body_gfx4_setLabel(G4_OXY, "Oxy");
  body_gfx4_setLabel(G4_HOOFDESP, "SnelH");
  body_gfx4_setLabel(G4_ZUIGEN, "Zuigen");
  body_gfx4_setLabel(G4_TRIL, "Vibe");
  
  bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
  body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
  updateStatusLabel();
}

static void enterMenu() {
  mode = MODE_MENU;
  uiMenu = true;
  inputTouchResetCooldown();  // Fix dubbele touch
  menu_begin(body_gfx);
}

static void enterPlaylist() {
  mode = MODE_PLAYLIST;
  inputTouchResetCooldown();  // Fix dubbele touch
  playlist_begin(body_gfx);
}

// ===== AI Functions =====

void startAIStressManagement() {
  aiStressModeActive = true;
  aiControlActive = true;
  aiOverruleActive = true;
  
  aiStartSpeedStep = hoofdESPSpeedStep;
  currentSpeedStep = hoofdESPSpeedStep;
  currentStressLevel = 3;
  
  stressManager.startSession();
  
  float currentTrust = (aiStartSpeedStep / 7.0f) * 2.0f;
  sendESPNowMessage(currentTrust, 1.0f, true, "AI_STRESS_START");
  
  Serial.printf("[AI STRESS] Started from speed %d\n", aiStartSpeedStep);
}

void stopAIStressManagement() {
  aiStressModeActive = false;
  aiControlActive = false;
  
  stressManager.endSession("User stopped");
  
  sendESPNowMessage(1.0f, 1.0f, false, "MANUAL_CONTROL");
  Serial.println("[AI STRESS] Stopped");
}

void handleSerialInput() {
  if (!Serial.available()) return;
  
  String input = Serial.readStringUntil('\n');
  input.trim();
  
  if (input.startsWith("stress ")) {
    int level = input.substring(7).toInt();
    if (level >= 1 && level <= 7 && aiStressModeActive) {
      currentStressLevel = level;
      Serial.printf("[MANUAL] Stress level: %d\n", level);
    }
  } else if (input == "ml") {
    if (mlAnalyzer.isReady()) {
      StressAnalysis result = mlAnalyzer.analyzeStress();
      Serial.printf("[ML] Stress: %d (%.0f%%)\n", result.stressLevel, result.confidence * 100);
    }
  } else if (input == "help") {
    Serial.println("\n[COMMANDS]");
    Serial.println("  stress X - Set stress (1-7)");
    Serial.println("  ml       - Run ML analysis");
    Serial.println("  help     - Show help");
  }
}

// ===== Setup =====

void setup() {
  // Power (CRITICAL!)
  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, HIGH);
  
  Serial.begin(115200);
  while (!Serial && millis() < 2000);
  
  Serial.println("\n\n[T-HMI] Body ESP32-S3 DEFINITIEVE VERSIE");
  Serial.println("[T-HMI] Hardware: LilyGO T-HMI ESP32-S3 2.8\"");
  
  // Display
  Serial.println("[T-HMI] Init display...");
  tft.begin();
  tft.setRotation(DISPLAY_ROTATION);
  tft.setSwapBytes(true);
  tft.fillScreen(BODY_CFG.COL_BG);
  
  pinMode(BK_LIGHT_PIN, OUTPUT);
  digitalWrite(BK_LIGHT_PIN, HIGH);
  
  Serial.println("[T-HMI] Display OK - 320x240");
  
  // Graphics
  body_gfx_tft = &tft;
  body_gfx4_begin();
  body_gfx4_setCooldown(3000, 2000);
  
  // I2C
  Serial.println("[I2C] Init GPIO43/44...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  
  // ADS1115
  initADS1115();
  
  // Touch
  Serial.println("[TOUCH] Init resistive touch...");
  inputTouchBegin(body_gfx);
  
  // SD
  Serial.println("[SD] Init SD_MMC...");
  initSDCard();
  
  // ESP-NOW
  Serial.println("[ESP-NOW] Init...");
  espNowInitialized = initESPNow();
  
  // ML
  Serial.println("[ML] Init stress analyzer...");
  if (ml_begin()) {
    Serial.println("[ML] Ready");
  }
  
  // Advanced Stress Manager
  Serial.println("[STRESS] Init advanced manager...");
  stressManager.begin();
  stressManager.enableML(BODY_CFG.mlStressEnabled);
  stressManager.setMLAutonomyLevel(BODY_CFG.mlAutonomyLevel);
  
  // MultiFunPlayer (if enabled)
  if (BODY_CFG.mfpAutoConnect) {
    Serial.println("[MFP] Init MultiFunPlayer...");
    setupMultiFunPlayer();
  }
  
  // Load config
  loadSensorConfig();
  
  // Reset AI
  aiTestModeActive = false;
  aiStressModeActive = false;
  aiControlActive = false;
  
  // Enter main
  enterMain();
  
  Serial.println("\n[T-HMI] ✓ READY!\n");
}

// === EINDE DEEL 1 - PLAK DEEL 2 HIERONDER ===
// === DEEL 2 - PLAK DIT NA DEEL 1 ===

// ===== Main Loop (NON-BLOCKING!) =====

void loop() {
  uint32_t now = millis();
  
  // Serial commands
  handleSerialInput();
  
  // MultiFunPlayer WebSocket
  if (BODY_CFG.mfpMLIntegration) {
    mfpClient.loop();
  }
  
  if (mode == MODE_MAIN) {
    // Red background (AI paused)
    if (redBackgroundActive) {
      body_gfx->fillScreen(0xF800);
      body_gfx->setTextColor(0xFFFF, 0xF800);
      body_gfx->setTextSize(2);
      
      String pauseMsg = "AI GEPAUZEERD";
      int w = pauseMsg.length() * 12;
      body_gfx->setCursor((320 - w) / 2, 80);
      body_gfx->print(pauseMsg);
      
      body_gfx->setTextSize(1);
      String touchMsg = "Raak scherm aan";
      w = touchMsg.length() * 6;
      body_gfx->setCursor((320 - w) / 2, 120);
      body_gfx->print(touchMsg);
      
      int16_t tx, ty;
      if (inputTouchRead(tx, ty)) {
        redBackgroundActive = false;
        
        if (userPausedAI) {
          userPausedAI = false;
          aiControlActive = true;
          
          if (aiStressModeActive) {
            sendESPNowMessage(0.1f, 1.0f, true, "AI_STRESS_RESUME");
          }
        }
        
        enterMain();
      }
      
      return;  // Skip rest of loop
    }
    
    // NON-BLOCKING sensor reading
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
      readADS1115Sensors();
      lastSensorRead = now;
    }
    
    // ESP-NOW timeout check (non-blocking)
    if (now - lastCommTime > sensorConfig.commTimeout * 1000) {
      trustSpeed = sleeveSpeed = suctionLevel = pauseTime = 0.0f;
      zuigActive = false;
      vacuumMbar = 0.0f;
      pauseActive = false;
    }
    
    // ML stress analysis (non-blocking, 1Hz)
    if (now - lastMLUpdate >= ML_UPDATE_INTERVAL) {
      if (mlAnalyzer.isReady()) {
        StressAnalysis analysis = mlAnalyzer.analyzeStress();
        // Use analysis if needed
      }
      lastMLUpdate = now;
    }
    
    // Advanced Stress Management (non-blocking, 1Hz)
    if (aiStressModeActive && now - lastStressUpdate >= STRESS_UPDATE_INTERVAL) {
      StressDecision decision = stressManager.getStressDecision();
      
      // Execute stress decision
      if (decision.recommendedAction != ACTION_WAIT) {
        stressManager.executeAction(decision);
        
        // Send to HoofdESP
        float newTrust = (decision.recommendedSpeed / 7.0f) * 2.0f;
        sendESPNowMessage(newTrust, 1.0f, true, "AI_STRESS_UPDATE", 
                         decision.currentLevel, decision.vibeRecommended, decision.suctionRecommended);
      }
      
      lastStressUpdate = now;
    }
    
    // Dummy oxygen
    static float dummyPhase = 0;
    dummyPhase += 0.1f;
    if (dummyPhase > TWO_PI) dummyPhase -= TWO_PI;
    float oxyVal = 95 + 2 * sin(dummyPhase/7);
    
    // Draw samples
    body_gfx4_pushSample(G4_HART, BPM, (now - lastBeatMs < 100));
    body_gfx4_pushSample(G4_HUID, gsrSmooth);
    body_gfx4_pushSample(G4_TEMP, tempValue);
    body_gfx4_pushSample(G4_ADEMHALING, flexValue);
    //body_gfx4_pushSample(G4_OXY, oxyVal);
    
    // SNELH animation
    float snelheidVal;
    if (pauseActive) {
      snelheidVal = 0.0f;
    } else {
      static uint32_t animStartTime = 0;
      if (lubeTrigger && trustSpeed < 1.0f) {
        animStartTime = now;
      }
      
      uint32_t timeSinceReset = now - animStartTime;
      float speed = max(0.1f, cyclusTijd);
      float animFreq = speed * BODY_CFG.SNELH_SPEED_FACTOR;
      float phase = (timeSinceReset / 1000.0f) * animFreq;
      phase = fmod(phase, 1.0f) * 2.0f * PI;
      
      float s = 0.5f * (sinf(phase - PI/2.0f) + 1.0f);
      snelheidVal = s * 100.0f;
    }
    body_gfx4_pushSample(G4_HOOFDESP, snelheidVal);
    
    // ZUIGEN
    float zuigenVal;
    if (zuigActive && vacuumMbar > 0) {
      float maxMbar = BODY_CFG.vacuumGraphMaxMbar;
      float scaledVacuum = min(vacuumMbar / maxMbar, 1.0f);
      zuigenVal = 20.0f + (scaledVacuum * 80.0f);
    } else {
      zuigenVal = 5.0f;
    }
    body_gfx4_pushSample(G4_ZUIGEN, zuigenVal);
    
    // VIBE
    static float vibePhase = 0;
    vibePhase += 0.1f;
    bool vibeActive = vibeOn;
    float trilVal = vibeActive ? (100 + (3 * sin(vibePhase))) : (3 * sin(vibePhase));
    body_gfx4_pushSample(G4_TRIL, trilVal);
    
    // Draw buttons
    bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
    body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
    
    // Debug output (non-blocking, 2Hz)
    if (now - lastDebugPrint >= DEBUG_PRINT_INTERVAL) {
      Serial.printf("[BODY] BPM:%d T:%.1f GSR:%.0f Flex:%.0f AI:%s\n", 
                    BPM, tempValue, gsrSmooth, flexValue, 
                    aiStressModeActive ? "STRESS" : (aiOverruleActive ? "ON" : "OFF"));
      lastDebugPrint = now;
    }
    
    // Heartbeat to HoofdESP (non-blocking, 2Hz)
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      bool effectiveAI = aiOverruleActive || aiStressModeActive || aiTestModeActive;
      sendESPNowMessage(currentTrustOverride, currentSleeveOverride, effectiveAI, "HEARTBEAT");
      lastHeartbeat = now;
    }
    
    // Record sample (if recording)
    if (isRecording) {
      recordSample();
    }
    
    // Playback processing (if active)
    if (isPlaybackActive) {
      // Process playback sample (non-blocking)
      // Implementation depends on your playback format
    }
    
    // Touch input (non-blocking with cooldown)
    TouchEvent ev;
    if (inputTouchPoll(ev)) {
      if (body_gfx4_canTouch(true)) {
        body_gfx4_registerTouch(true);
        
        if (ev.kind == TE_TAP_REC) {
          isRecording = !isRecording;
          if (isRecording) startRecording();
          else stopRecording();
        }
        
        if (ev.kind == TE_TAP_PLAY) {
          if (isPlaying) {
            isPlaying = false;
          } else {
            enterPlaylist();
            return;
          }
        }
        
        if (ev.kind == TE_TAP_MENU) {
          uiMenu = true;
          mode = MODE_MENU;
          inputTouchResetCooldown();  // Fix dubbele touch
          menu_begin(body_gfx);
          return;
        }
        
        if (ev.kind == TE_TAP_OVERRULE) {
          bool anyAIActive = aiStressModeActive || aiTestModeActive || aiConfig.enabled;
          
          if (anyAIActive) {
            if (aiStressModeActive) stopAIStressManagement();
            if (aiTestModeActive) aiTestModeActive = false;
            if (aiConfig.enabled) {
              aiConfig.enabled = false;
              aiOverruleActive = false;
            }
            Serial.println("[AI] All stopped");
          } else {
            startAIStressManagement();
            Serial.println("[AI] Stress management started");
          }
          
          bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
          body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
          return;
        }
        
        updateStatusLabel();
      }
    }
    
    return;  // End of MODE_MAIN
  }
  
  // ===== Menu Mode =====
  if (mode == MODE_MENU) {
    MenuEvent mev = menu_poll();
    if (mev == ME_NONE) return;
    if (mev == ME_BACK) { enterMain(); return; }
    if (mev == ME_SENSOR_SETTINGS) { 
      mode = MODE_SENSOR_SETTINGS;
      inputTouchResetCooldown();  // Fix dubbele touch
      sensorSettings_begin(body_gfx);
      return;
    }
    if (mev == ME_OVERRULE) {
      mode = MODE_OVERRULE;
      inputTouchResetCooldown();  // Fix dubbele touch
      overrule_begin(body_gfx);
      return;
    }
    if (mev == ME_SYSTEM_SETTINGS) {
      mode = MODE_SYSTEM_SETTINGS;
      inputTouchResetCooldown();  // Fix dubbele touch
      systemSettings_begin(body_gfx);
      return;
    }
    if (mev == ME_PLAYLIST) {
      enterPlaylist();
      return;
    }
    if (mev == ME_ML_TRAINING) {
      mode = MODE_AI_TRAINING;
      inputTouchResetCooldown();  // Fix dubbele touch
      mlTraining_begin(body_gfx);
      return;
    }
  }
  
  // ===== Playlist Mode =====
  if (mode == MODE_PLAYLIST) {
    PlaylistEvent pev = playlist_poll();
    if (pev == PE_NONE) return;
    if (pev == PE_BACK) { enterMain(); return; }
    if (pev == PE_PLAY_FILE) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        Serial.printf("[PLAYLIST] Play: %s\n", filename.c_str());
        // Start playback implementation here
      }
      return;
    }
    if (pev == PE_DELETE_CONFIRM) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        mode = MODE_CONFIRM;
        inputTouchResetCooldown();  // Fix dubbele touch
        confirmType = CONFIRM_DELETE_FILE;
        confirmData = filename;
        confirm_begin(body_gfx, "Bestand verwijderen?", filename.c_str());
      }
      return;
    }
    if (pev == PE_AI_ANALYZE) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        aiAnalyze_setSelectedFile(filename);
        mode = MODE_AI_ANALYZE;
        inputTouchResetCooldown();  // Fix dubbele touch
        aiAnalyze_begin(body_gfx);
      }
      return;
    }
  }
  
  // ===== Sensor Settings Mode =====
  if (mode == MODE_SENSOR_SETTINGS) {
    SensorEvent sev = sensorSettings_poll();
    if (sev == SE_NONE) return;
    if (sev == SE_BACK) { enterMenu(); return; }
    if (sev == SE_SAVE) {
      saveSensorConfig();
      body_gfx4_drawStatus("Opgeslagen!");
      // Non-blocking wait for status display
      static uint32_t statusShowTime = 0;
      statusShowTime = millis();
      while (millis() - statusShowTime < 1000) {
        // Let user see the status
      }
      enterMenu();
      return;
    }
    if (sev == SE_RESET) {
      resetSensorConfig();
      body_gfx4_drawStatus("Gereset!");
      return;
    }
  }
  
  // ===== System Settings Mode =====
  if (mode == MODE_SYSTEM_SETTINGS) {
    SystemSettingsEvent sse = systemSettings_poll();
    if (sse == SSE_NONE) return;
    if (sse == SSE_BACK) { enterMenu(); return; }
    if (sse == SSE_ROTATE_SCREEN) {
      // Toggle rotation
      bool wasRotated = (currentRotation != DISPLAY_ROTATION);
      if (currentRotation == DISPLAY_ROTATION) {
        currentRotation = (DISPLAY_ROTATION + 2) % 4;
      } else {
        currentRotation = DISPLAY_ROTATION;
      }
      body_gfx->setRotation(currentRotation);
      body_gfx->fillScreen(BODY_CFG.COL_BG);
      inputTouchBegin(body_gfx);
      bool isNowRotated = (currentRotation != DISPLAY_ROTATION);
      inputTouchSetRotated(isNowRotated);
      systemSettings_begin(body_gfx);
      return;
    }
    if (sse == SSE_COLORS) {
      mode = MODE_COLORS;
      inputTouchResetCooldown();  // Fix dubbele touch
      colors_begin(body_gfx);
      return;
    }
    if (sse == SSE_FORMAT_SD) {
      mode = MODE_CONFIRM;
      inputTouchResetCooldown();  // Fix dubbele touch
      confirmType = CONFIRM_FORMAT_SD;
      confirmData = "";
      confirm_begin(body_gfx, "SD formatteren?", "Alle data gewist!");
      return;
    }
    if (sse == SSE_CALIBRATE) {
      mode = MODE_CALIBRATE;
      inputTouchResetCooldown();  // Fix dubbele touch
      touchCalibration_begin(body_gfx);
      return;
    }
  }
  
  // ===== Overrule Mode =====
  if (mode == MODE_OVERRULE) {
    OverruleEvent oev = overrule_poll();
    if (oev == OE_NONE) return;
    if (oev == OE_BACK) { enterMenu(); return; }
    if (oev == OE_TOGGLE_AI) {
      aiConfig.enabled = !aiConfig.enabled;
      if (!aiConfig.enabled) {
        currentTrustOverride = 1.0f;
        currentSleeveOverride = 1.0f;
        aiOverruleActive = false;
      }
      overrule_begin(body_gfx);
      return;
    }
    if (oev == OE_SAVE) {
      body_gfx4_drawStatus("AI instellingen opgeslagen!");
      enterMenu();
      return;
    }
    if (oev == OE_RESET) {
      aiConfig = AIOverruleConfig();
      overrule_begin(body_gfx);
      return;
    }
    if (oev == OE_ANALYZE) {
      mode = MODE_AI_ANALYZE;
      inputTouchResetCooldown();  // Fix dubbele touch
      aiAnalyze_begin(body_gfx);
      return;
    }
    if (oev == OE_CONFIG) {
      mode = MODE_AI_EVENT_CONFIG;
      inputTouchResetCooldown();  // Fix dubbele touch
      aiEventConfig_begin(body_gfx);
      return;
    }
  }
  
  // ===== AI Analyze Mode =====
  if (mode == MODE_AI_ANALYZE) {
    AnalyzeEvent aev = aiAnalyze_poll();
    if (aev == AE_NONE) return;
    if (aev == AE_BACK) { enterMenu(); return; }
    if (aev == AE_TRAIN) {
      String filename = aiAnalyze_getSelectedFile();
      if (filename.length() > 0) {
        mode = MODE_AI_TRAINING;
        inputTouchResetCooldown();  // Fix dubbele touch
        aiTraining_begin(body_gfx, filename);
      }
      return;
    }
  }
  
  // ===== AI Event Config Mode =====
  if (mode == MODE_AI_EVENT_CONFIG) {
    EventConfigEvent ece = aiEventConfig_poll();
    if (ece == ECE_NONE) return;
    if (ece == ECE_BACK) { enterMenu(); return; }
    if (ece == ECE_SAVE) {
      aiEventConfig_saveToEEPROM();
      body_gfx4_drawStatus("Event config opgeslagen!");
      enterMenu();
      return;
    }
    if (ece == ECE_RESET) {
      aiEventConfig_resetToDefaults();
      aiEventConfig_begin(body_gfx);
      return;
    }
  }
  
  // ===== Colors Mode =====
  if (mode == MODE_COLORS) {
    ColorEvent ce = colors_poll();
    if (ce == CE_NONE) return;
    if (ce == CE_BACK) { 
      mode = MODE_SYSTEM_SETTINGS;
      inputTouchResetCooldown();  // Fix dubbele touch
      systemSettings_begin(body_gfx);
      return;
    }
    if (ce == CE_COLOR_CHANGE) {
      body_gfx4_drawStatus("Kleur gewijzigd!");
      return;
    }
  }
  
  // ===== Calibration Mode =====
  if (mode == MODE_CALIBRATE) {
    if (touchCalibration_poll()) {
      // Kalibratie voltooid - terug naar system settings
      delay(3000);  // Laat resultaat 3 seconden zien
      mode = MODE_SYSTEM_SETTINGS;
      inputTouchResetCooldown();
      systemSettings_begin(body_gfx);
      return;
    }
    // Nog bezig met kalibratie
    return;
  }
  
  // ===== AI Training Mode =====

  // ===== test 2 AI Training Mode =====
if (mode == MODE_AI_TRAINING) {
// Test alleen aiTraining_poll
  TrainingResult tr = aiTraining_poll();
  
  if (tr == TR_BACK) {
    Serial.println("[AI TRAINING] Back pressed");
    enterMenu();
    return;
  }
  
  if (tr == TR_COMPLETE) {
    Serial.println("[AI TRAINING] Complete");
    enterMenu();
    return;
  }
  
  return;  // Skip rest
}

  // ===== test 1 ok AI Training Mode =====

  // ===== AI Training Mode =====
  //if (mode == MODE_AI_TRAINING) {
    // Test alleen aiTraining_poll
    //TrainingResult tr = aiTraining_poll();
  
    //if (tr == TR_BACK) {
      //Serial.println("[AI TRAINING] Back pressed");
      //enterMenu();
      //return;
    //}
  
    //if (tr == TR_COMPLETE) {
      //Serial.println("[AI TRAINING] Complete");
      //enterMenu();
      //return;
    //}
  
    //return;  // Skip rest
  //}

  //if (mode == MODE_AI_TRAINING) {
    // Try AI Training (12 feedback buttons for CSV labeling)
    //TrainingResult tr = aiTraining_poll();
    //if (tr != TR_NONE) {
      //if (tr == TR_BACK) {
        //enterMenu();
        //return;
      //}
      //if (tr == TR_COMPLETE) {
        //enterMenu();
        //return;
      //}
      //return;
    //}
    
    // Try ML Training system
    //MLTrainingEvent mte = mlTraining_poll();
    //if (mte == MTE_NONE) return;
    //if (mte == MTE_BACK) { enterMenu(); return; }
    
    //if (mte == MTE_IMPORT_DATA) {
      //mlTraining_setState(ML_STATE_IMPORT);
      //return;
    //}
    //if (mte == MTE_TRAIN_MODEL) {
      //mlTraining_setState(ML_STATE_TRAINING);
      //return;
    //}
    //if (mte == MTE_MODEL_MANAGER) {
      //mlTraining_setState(ML_STATE_MODEL_MANAGER);
      //return;
    //}
  //}
  
  // ===== Confirm Mode =====
  if (mode == MODE_CONFIRM) {
    ConfirmEvent cev = confirm_poll();
    if (cev == CONF_NONE) return;
    
    if (cev == CONF_YES) {
      if (confirmType == CONFIRM_DELETE_FILE) {
        // Delete file
        if (SD_MMC.remove("/" + confirmData)) {
          Serial.printf("[SD] Deleted: %s\n", confirmData.c_str());
        }
        mode = MODE_PLAYLIST;
        inputTouchResetCooldown();  // Fix dubbele touch
        playlist_begin(body_gfx);
      } else if (confirmType == CONFIRM_FORMAT_SD) {
        // Format SD (delete all data files)
        File root = SD_MMC.open("/");
        if (root) {
          int deletedCount = 0;
          File file = root.openNextFile();
          while (file) {
            String name = file.name();
            file.close();
            if (name.startsWith("data") && name.endsWith(".csv")) {
              if (SD_MMC.remove("/" + name)) deletedCount++;
            }
            file = root.openNextFile();
          }
          root.close();
          Serial.printf("[SD] Deleted %d files\n", deletedCount);
        }
        mode = MODE_MENU;
        inputTouchResetCooldown();  // Fix dubbele touch
        menu_begin(body_gfx);
      }
    } else if (cev == CONF_NO) {
      if (confirmType == CONFIRM_DELETE_FILE) {
        mode = MODE_PLAYLIST;
        inputTouchResetCooldown();  // Fix dubbele touch
        playlist_begin(body_gfx);
      } else if (confirmType == CONFIRM_FORMAT_SD) {
        mode = MODE_MENU;
        inputTouchResetCooldown();  // Fix dubbele touch
        menu_begin(body_gfx);
      }
    }
    return;
  }
}
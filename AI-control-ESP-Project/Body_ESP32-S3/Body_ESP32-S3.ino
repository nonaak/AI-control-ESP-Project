/*  
  Body ESP32-S3 T-HMI – Complete T-HMI version with ADS1115
  Hardware: LilyGO T-HMI ESP32-S3 2.8" IPS-TFT Display
  Sensors: ADS1115 (4-channel 16-bit ADC via I2C)
    - A0: Grove GSR
    - A1: Flex Sensor (90mm, breathing)
    - A2: Pulse Sensor (PPG, heartrate)
    - A3: 10kΩ NTC (skin temperature)
*/

// ===== T-HMI Hardware Configuration =====
// Power management
#define PWR_EN_PIN  (10)
#define PWR_ON_PIN  (14) 
#define BAT_ADC_PIN (5)
#define BUTTON1_PIN (0)
#define BUTTON2_PIN (21)  // Reed switch

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

// Touch pins (resistive)
#define TOUCHSCREEN_SCLK_PIN (1)
#define TOUCHSCREEN_MISO_PIN (4)
#define TOUCHSCREEN_MOSI_PIN (3)
#define TOUCHSCREEN_CS_PIN   (2)
#define TOUCHSCREEN_IRQ_PIN  (9)

// SD card pins (SD_MMC interface - 1-bit mode)
#define SD_MMC_CLK  (12)  // SD Clock
#define SD_MMC_CMD  (11)  // SD Command  
#define SD_MMC_DAT0 (13)  // SD Data 0

// I2C pins (Grove connector)
#define I2C_SDA_PIN (43)
#define I2C_SCL_PIN (44)

// Display rotation (0=0°, 1=90°, 2=180°, 3=270°)
#define DISPLAY_ROTATION 3
static uint8_t currentRotation = DISPLAY_ROTATION;

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
#include "rgb_off.h"
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

// T-HMI Display object
TFT_eSPI tft = TFT_eSPI();
TFT_eSPI *body_gfx_tft = &tft;

// ===== ADS1115 ADC Configuration =====
Adafruit_ADS1115 ads;
static bool adsInitialized = false;

// ADS1115 channel mapping
#define ADS_CH_GSR        0  // A0: Grove GSR sensor
#define ADS_CH_FLEX       1  // A1: Flex sensor (breathing)
#define ADS_CH_PULSE      2  // A2: Pulse sensor (PPG heartrate)
#define ADS_CH_NTC        3  // A3: 10kΩ NTC (skin temperature)

// NTC thermistor constants (typical 10kΩ NTC)
#define NTC_R_FIXED   22000.0f  // 22kΩ pull-up resistor
#define NTC_R_25C     10000.0f  // 10kΩ @ 25°C
#define NTC_B_VALUE   3950.0f   // B-value (typical for 10k NTC)
#define NTC_T0_KELVIN 298.15f   // 25°C in Kelvin

// Flex sensor constants
#define FLEX_R_FIXED  22000.0f  // 22kΩ fixed resistor
#define FLEX_R_FLAT   25000.0f  // ~25kΩ when flat
#define FLEX_R_BENT   125000.0f // ~125kΩ when fully bent

// Sensor data
static float gsrValue = 0.0f;
static float gsrSmooth = 0.0f;
static float flexValue = 0.0f;
static float breathingRate = 0.0f;
static uint16_t BPM = 0;
static float tempValue = 36.5f;
static uint32_t lastBeatMs = 0;

// PPG (Pulse) signal processing
#define PPG_BUFFER_SIZE 50
static float ppgBuffer[PPG_BUFFER_SIZE];
static int ppgBufferIndex = 0;
static float ppgBaseline = 1.65f;  // DC offset ~1.65V
static float ppgThreshold = 0.05f;  // Adaptive threshold
static bool ppgPeakDetected = false;
static uint32_t ppgLastPeakTime = 0;

// ===== SD Card =====
static bool sdCardAvailable = false;
static int nextFileNumber = 1;
static File recordingFile;
static uint32_t recordingStartTime = 0;
static uint32_t samplesRecorded = 0;
static File eventLogFile;
static bool eventLoggingActive = false;

// ===== UI States =====
bool isRecording = false;
static bool isPlaying = false;
static bool uiMenu = false;

// App modes
enum AppMode : uint8_t { 
  MODE_MAIN=0, MODE_MENU, MODE_PLAYLIST, MODE_CONFIRM, 
  MODE_SENSOR_SETTINGS, MODE_SYSTEM_SETTINGS, MODE_OVERRULE, 
  MODE_AI_ANALYZE, MODE_AI_EVENT_CONFIG, MODE_COLORS, MODE_AI_TRAINING 
};
static AppMode mode = MODE_MAIN;

// Playback
static File playbackFile;
static bool isPlaybackActive = false;
static uint32_t playbackStartTime = 0;
static uint32_t nextSampleTime = 0;

// Confirmation dialog
enum ConfirmType { CONFIRM_DELETE_FILE, CONFIRM_FORMAT_SD };
static ConfirmType confirmType;
static String confirmData;

// ===== ESP-NOW Communication =====
static uint8_t hoofdESP_MAC[] = {0xE4, 0x65, 0xB8, 0x7A, 0x85, 0xE4};
static uint8_t bodyESP_MAC[] = {0xEC, 0xDA, 0x3B, 0x98, 0xC5, 0x24};

static String commBuffer = "";
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

// ESP-NOW receive message structure
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

// ESP-NOW send message structure
typedef struct __attribute__((packed)) {
  float newTrust;
  float newSleeve;
  bool overruleActive;
  uint8_t stressLevel;
  bool vibeOn;
  bool zuigenOn;
  char command[32];
} esp_now_send_message_t;

// AI Overrule system
AIOverruleConfig aiConfig;
bool aiOverruleActive = false;
uint32_t lastAIUpdate = 0;
float currentTrustOverride = 1.0f;
float currentSleeveOverride = 1.0f;

// AI Test Control
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

// ===== ADS1115 Sensor Functions =====

void initADS1115() {
  Serial.println("[ADS1115] Initializing 16-bit ADC...");
  
  if (!ads.begin(0x48)) {
    Serial.println("[ADS1115] ERROR: Not found at I2C address 0x48!");
    Serial.println("[ADS1115] Check wiring:");
    Serial.println("  - VDD → 3V3");
    Serial.println("  - GND → GND");
    Serial.println("  - SDA → GPIO43 (Grove)");
    Serial.println("  - SCL → GPIO44 (Grove)");
    adsInitialized = false;
    return;
  }
  
  // Configure ADS1115
  ads.setGain(GAIN_ONE);  // ±4.096V range (0.125 mV/bit)
  ads.setDataRate(RATE_ADS1115_128SPS);  // 128 samples/second
  
  adsInitialized = true;
  Serial.println("[ADS1115] Initialized successfully!");
  Serial.println("[ADS1115] Configuration:");
  Serial.println("  - Gain: ±4.096V (GAIN_ONE)");
  Serial.println("  - Sample rate: 128 SPS");
  Serial.println("  - A0: Grove GSR");
  Serial.println("  - A1: Flex Sensor (breathing)");
  Serial.println("  - A2: Pulse Sensor (PPG)");
  Serial.println("  - A3: 10kΩ NTC (skin temp)");
}

float readNTCTemperature(float voltage) {
  // Voltage divider: 3.3V → R_FIXED (22kΩ) → [MEASURE] → NTC → GND
  // V_measure = 3.3V * (R_NTC / (R_FIXED + R_NTC))
  // R_NTC = R_FIXED * V_measure / (3.3 - V_measure)
  
  if (voltage >= 3.29f) return 50.0f;  // Sensor disconnected
  if (voltage <= 0.01f) return 0.0f;   // Short circuit
  
  float r_ntc = NTC_R_FIXED * voltage / (3.3f - voltage);
  
  // Steinhart-Hart equation (simplified B-parameter)
  // 1/T = 1/T0 + (1/B) * ln(R/R0)
  float temp_kelvin = 1.0f / (
    (1.0f / NTC_T0_KELVIN) + 
    (1.0f / NTC_B_VALUE) * log(r_ntc / NTC_R_25C)
  );
  
  float temp_celsius = temp_kelvin - 273.15f;
  
  // Apply calibration offset
  return temp_celsius + sensorConfig.tempOffset;
}

float readFlexBreathing(float voltage) {
  // Voltage divider: 3.3V → R_FIXED (22kΩ) → [MEASURE] → Flex → GND
  // Higher voltage = flat (low resistance)
  // Lower voltage = bent (high resistance)
  
  if (voltage >= 3.29f || voltage <= 0.01f) return 50.0f;  // Error
  
  float r_flex = FLEX_R_FIXED * voltage / (3.3f - voltage);
  
  // Map resistance to breathing value (0-100)
  // Flat (~25kΩ) → ~100 (full inhale)
  // Bent (~125kΩ) → ~0 (full exhale)
  float breathValue = 100.0f - ((r_flex - FLEX_R_FLAT) / (FLEX_R_BENT - FLEX_R_FLAT) * 100.0f);
  breathValue = constrain(breathValue, 0.0f, 100.0f);
  
  return breathValue;
}

void processPPGSignal(float voltage) {
  // Add to circular buffer
  ppgBuffer[ppgBufferIndex] = voltage;
  ppgBufferIndex = (ppgBufferIndex + 1) % PPG_BUFFER_SIZE;
  
  // Calculate baseline (DC component)
  float sum = 0;
  for (int i = 0; i < PPG_BUFFER_SIZE; i++) {
    sum += ppgBuffer[i];
  }
  ppgBaseline = sum / PPG_BUFFER_SIZE;
  
  // AC component (PPG signal minus DC offset)
  float acSignal = voltage - ppgBaseline;
  
  // Adaptive threshold (30% of recent peak)
  float maxAC = 0;
  for (int i = 0; i < PPG_BUFFER_SIZE; i++) {
    float ac = abs(ppgBuffer[i] - ppgBaseline);
    if (ac > maxAC) maxAC = ac;
  }
  ppgThreshold = maxAC * 0.3f;
  
  // Peak detection
  static float lastAC = 0;
  static bool risingEdge = false;
  
  if (acSignal > lastAC) {
    risingEdge = true;
  } else if (risingEdge && acSignal < lastAC) {
    // Peak detected!
    if (acSignal > ppgThreshold && !ppgPeakDetected) {
      uint32_t now = millis();
      uint32_t ibi = now - ppgLastPeakTime;  // Inter-beat interval
      
      if (ibi > 300 && ibi < 2000) {  // Valid heartbeat (30-200 BPM)
        BPM = (uint16_t)(60000UL / ibi);
        ppgLastPeakTime = now;
        lastBeatMs = now;
        ppgPeakDetected = true;
      }
    }
    risingEdge = false;
  }
  
  // Reset peak flag after some time
  if (ppgPeakDetected && (millis() - lastBeatMs > 100)) {
    ppgPeakDetected = false;
  }
  
  lastAC = acSignal;
}

void readADS1115Sensors() {
  if (!adsInitialized) return;
  
  // Read all 4 channels (takes ~31ms @ 128 SPS)
  int16_t adc0 = ads.readADC_SingleEnded(ADS_CH_GSR);
  int16_t adc1 = ads.readADC_SingleEnded(ADS_CH_FLEX);
  int16_t adc2 = ads.readADC_SingleEnded(ADS_CH_PULSE);
  int16_t adc3 = ads.readADC_SingleEnded(ADS_CH_NTC);
  
  // Convert to voltage
  float volt_gsr = ads.computeVolts(adc0);
  float volt_flex = ads.computeVolts(adc1);
  float volt_pulse = ads.computeVolts(adc2);
  float volt_ntc = ads.computeVolts(adc3);
  
  // Process GSR
  gsrValue = volt_gsr * 310.0f;  // Scale to 0-1023 range for compatibility
  float calibratedGSR = (gsrValue - sensorConfig.gsrBaseline) * sensorConfig.gsrSensitivity;
  float smoothFactor = sensorConfig.gsrSmoothing;
  gsrSmooth = gsrSmooth * (1.0f - smoothFactor) + calibratedGSR * smoothFactor;
  if (gsrSmooth < 0) gsrSmooth = 0;
  
  // Process Flex (breathing)
  flexValue = readFlexBreathing(volt_flex);
  
  // Process PPG (pulse/heartrate)
  processPPGSignal(volt_pulse);
  
  // Process NTC (temperature)
  tempValue = readNTCTemperature(volt_ntc);
  
  // Debug output (every 2 seconds)
  static uint32_t lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    Serial.println("[ADS1115] Raw voltages:");
    Serial.printf("  GSR: %.3fV  Flex: %.3fV  Pulse: %.3fV  NTC: %.3fV\n",
                  volt_gsr, volt_flex, volt_pulse, volt_ntc);
    Serial.println("[SENSORS] Processed values:");
    Serial.printf("  GSR: %.0f  Breath: %.0f  BPM: %d  Temp: %.1f°C\n",
                  gsrSmooth, flexValue, BPM, tempValue);
    lastDebug = millis();
  }
  
  // Feed to ML analyzer
  mlAnalyzer.addSensorSample((float)BPM, tempValue, gsrSmooth);
}

// ===== ESP-NOW Functions =====

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
  
  Serial.println("[ESP-NOW] Initialized successfully");
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

// ===== SD Card Functions =====

static void initSDCard() {
  SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_DAT0);
  
  if (SD_MMC.begin("/sdcard", true)) {  // true = 1-bit mode
    sdCardAvailable = true;
    while (SD_MMC.exists("/data" + String(nextFileNumber) + ".csv")) {
      nextFileNumber++;
    }
    Serial.printf("[SD_MMC] Ready - %.2f GB - Next file: data%d.csv\n", 
                  SD_MMC.cardSize() / 1024.0 / 1024.0 / 1024.0, nextFileNumber);
  } else {
    sdCardAvailable = false;
    Serial.println("[SD_MMC] ERROR: Card not found!");
  }
}

void startRecording() {
  if (!sdCardAvailable) {
    Serial.println("[SD] No card available!");
    return;
  }
  
  String filename = "/data" + String(nextFileNumber) + ".csv";
  recordingFile = SD_MMC.open(filename, FILE_WRITE);
  
  if (recordingFile) {
    recordingFile.println("Time,Heart,Temp,Skin,Breath,Trust,Sleeve,Suction,Pause,Vibe,ZuigActive,VacuumMbar");
    recordingStartTime = millis();
    samplesRecorded = 0;
    isRecording = true;
    Serial.printf("[SD] Recording started: data%d.csv\n", nextFileNumber);
  } else {
    Serial.println("[SD] ERROR: Cannot open file!");
  }
}

void stopRecording() {
  if (recordingFile) {
    recordingFile.close();
    Serial.printf("[SD] Recording stopped. Samples: %u\n", samplesRecorded);
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
  
  if (BODY_CFG.autoRecordSessions && isRecording) {
    stopRecording();
  }
  
  body_gfx4_begin();
  body_gfx4_setLabel(G4_HART, "Hart");
  body_gfx4_setLabel(G4_HUID, "Huid");
  body_gfx4_setLabel(G4_TEMP, "Temp");
  body_gfx4_setLabel(G4_ADEMHALING, "Adem");
  body_gfx4_setLabel(G4_OXY, "Oxy");
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
  menu_begin(body_gfx);
}

static void enterPlaylist() {
  mode = MODE_PLAYLIST;
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
  
  if (BODY_CFG.autoRecordSessions && !isRecording) {
    startRecording();
  }
  
  float currentTrust = (aiStartSpeedStep / 7.0f) * 2.0f;
  sendESPNowMessage(currentTrust, 1.0f, true, "AI_STRESS_START");
  
  Serial.printf("[AI STRESS] Started from speed %d\n", aiStartSpeedStep);
}

void stopAIStressManagement() {
  aiStressModeActive = false;
  aiControlActive = false;
  
  if (BODY_CFG.autoRecordSessions && isRecording) {
    stopRecording();
  }
  
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
      Serial.printf("[MANUAL] Stress level set to: %d\n", level);
    }
  } else if (input == "ml") {
    if (mlAnalyzer.isReady()) {
      StressAnalysis result = mlAnalyzer.analyzeStress();
      Serial.printf("[ML] Stress: %d (%.0f%% confidence)\n", 
                    result.stressLevel, result.confidence * 100);
    }
  } else if (input == "help") {
    Serial.println("\n[COMMANDS]");
    Serial.println("  stress X  - Set stress level (1-7)");
    Serial.println("  ml        - Run ML analysis");
    Serial.println("  help      - Show this help");
  }
}

// =====
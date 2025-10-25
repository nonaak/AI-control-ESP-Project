/*  
  Body ESP SC01 Plus V3 – ADS1115 Sensor Migration
  COMPLETE VERSION - Display + UI intact
  
  Sensor Changes:
  - Vervangen: MAX30105 → Pulse sensor op ADS1115 A2
  - Vervangen: MCP9808 → NTC temperatuur op ADS1115 A3  
  - Vervangen: GPIO 34 GSR → GSR sensor op ADS1115 A0
  - NIEUW: Flex sensor (ademhaling) op ADS1115 A1
  - VERWIJDERD: RGB LED code (CYD specific)
  - I2C: Wire1 op GPIO 10 (SDA) + 11 (SCL) voor sensoren
*/

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include <esp_now.h>
#include <WiFi.h>

// ===== ADS1115 Sensor System (NIEUW!) =====
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

#define SENSOR_SDA 10          // GPIO 10 = SDA voor sensoren
#define SENSOR_SCL 11          // GPIO 11 = SCL voor sensoren

// ADS1115 sensor calibratie variabelen
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

// ===== Screen dimensions voor SC01 Plus =====
//const int SCR_W = 480;
//const int SCR_H = 320;

// Math constanten
#ifndef PI
#define PI 3.14159265359
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530718
#endif

// >>> Rotatie van weergave (sync met input_touch.h)
#define DISPLAY_ROTATION 1
static uint8_t currentRotation = DISPLAY_ROTATION;  // Huidige rotatie (0=0°, 1=90°, 2=180°, 3=270°)

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

// Display wordt nu gedefinieerd in body_display.cpp
// Extern reference naar de body display
extern Arduino_GFX *body_gfx;

// ===== SD Card =====
#define SD_CS_PIN 41  // SC01 Plus SD card CS pin
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

// Dummy
static float dummyPhase = 0;

// Sensor variabelen
static float tempValue = 0.0f;
static float gsrValue = 0.0f;
static float gsrSmooth = 0.0f;

// UI states
bool isRecording=false;
static bool isPlaying=false, uiMenu=false;

// App modes
enum AppMode : uint8_t { MODE_MAIN=0, MODE_MENU, MODE_PLAYLIST, MODE_CONFIRM, MODE_SENSOR_SETTINGS, MODE_SYSTEM_SETTINGS, MODE_OVERRULE, MODE_AI_ANALYZE, MODE_AI_EVENT_CONFIG, MODE_COLORS, MODE_AI_TRAINING };
static AppMode mode = MODE_MAIN;

// Playback variabelen
static File playbackFile;
static bool isPlaybackActive = false;
static uint32_t playbackStartTime = 0;
static uint32_t nextSampleTime = 0;

// Confirmation dialog variabelen
enum ConfirmType { CONFIRM_DELETE_FILE, CONFIRM_FORMAT_SD };
static ConfirmType confirmType;
static String confirmData;  // Extra data (filename, etc.)

// ===== ESP-NOW Communicatie =====
// MAC adressen van het netwerk
static uint8_t hoofdESP_MAC[] = {0xE4, 0x65, 0xB8, 0x7A, 0x85, 0xE4};  // HoofdESP
static uint8_t bodyESP_MAC[] = {0x08, 0xD1, 0xF9, 0xDC, 0xC3, 0xA4};   // Body ESP (deze unit)

// ESP-NOW data buffer
static String commBuffer = "";
static uint32_t lastCommTime = 0;
static float trustSpeed = 0.0f, sleeveSpeed = 0.0f, suctionLevel = 0.0f, pauseTime = 0.0f;
static bool vibeOn = false;  // Handmatige VIBE toggle van HoofdESP
static bool zuigActive = false;  // Status zuigen modus actief
static float vacuumMbar = 0.0f;  // Vacuum waarde in mbar (0-10 bereik)
static bool pauseActive = false;  // C knop pauze status
static bool lubeTrigger = false;  // Lube cyclus start trigger
static float cyclusTijd = 10.0f;  // Cyclus duur in seconden
static uint32_t lastLubeTriggerTime = 0;  // Tijd van laatste lube trigger
static float sleevePercentage = 0.0f;  // Echte sleeve positie percentage (0-100)
static bool espNowInitialized = false;

// ESP-NOW ontvangst bericht structuur (van HoofdESP) - LUBE SYNC SYSTEEM
typedef struct __attribute__((packed)) {
  float trust;            // Trust speed (0.0-2.0)
  float sleeve;           // Sleeve speed (0.0-2.0)  
  float suction;          // Suction level (0.0-100.0)
  float pause;            // Pause tijd (0.0-10.0) - NIET MEER GEBRUIKT
  bool vibeOn;            // Handmatige VIBE toggle van HoofdESP Z-knop
  bool zuigActive;        // Status zuigen modus actief (true/false)
  float vacuumMbar;       // Vacuum waarde in mbar (0-10 mbar bereik)
  bool pauseActive;       // C knop pauze status (true=gepauzeerd)
  bool lubeTrigger;       // Lube cyclus start trigger (sync punt)
  float cyclusTijd;       // Verwachte cyclus duur in seconden
  float sleevePercentage; // Sleeve positie percentage (0.0-100.0)
  uint8_t currentSpeedStep; // Huidige versnelling (0-7)
  char command[32];       // "STATUS_UPDATE"
} esp_now_receive_message_t;

// ESP-NOW verzend bericht structuur (naar HoofdESP) - SIMPLE VERSION
typedef struct __attribute__((packed)) {
  float newTrust;         // AI berekende trust override (0.0-1.0)
  float newSleeve;        // AI berekende sleeve override (0.0-1.0)
  bool overruleActive;    // AI overrule status
  uint8_t stressLevel;    // Stress level 1-7 voor playback/AI
  bool vibeOn;           // Vibe status voor playback
  bool zuigenOn;         // Zuigen status voor playback
  char command[32];       // "AI_OVERRIDE", "EMERGENCY_STOP", "HEARTBEAT", "PLAYBACK_STRESS"
} esp_now_send_message_t;

// AI Overrule systeem variabelen (structuur definitie in ai_overrule.h)
AIOverruleConfig aiConfig;
bool aiOverruleActive = false;
uint32_t lastAIUpdate = 0;
float currentTrustOverride = 1.0f;  // 1.0 = geen override
float currentSleeveOverride = 1.0f;

// AI Test Control System
static bool redBackgroundActive = false;
static bool aiTestModeActive = false;     // AI test modus aan/uit
static bool aiControlActive = false;      // AI heeft controle
static bool userPausedAI = false;        // User heeft AI gepauzeerd
static float aiTargetTrust = 1.5f;        // AI doelsnelheid (versnelling 6 ≈ 1.5)
static bool aiTargetVibe = true;          // AI doel vibe status
static bool aiTargetZuig = false;        // AI doel zuig status
static uint32_t lastAITestUpdate = 0;     // Aparte timer voor AI test

// AI Stress Management System (Test 2)
static bool aiStressModeActive = false;   // AI stress management actief
static int currentStressLevel = 3;        // Huidige stress level (1-7)
static int aiStartSpeedStep = 3;          // Speed step waar AI mee startte
static int currentSpeedStep = 3;          // Huidige speed step van AI (1-7)
static uint8_t hoofdESPSpeedStep = 3;     // Laatst ontvangen speed step van HoofdESP
static uint32_t lastStressCheck = 0;      // Timer voor stress evaluatie
static uint32_t lastSpeedAdjust = 0;      // Timer voor snelheid aanpassingen
static bool aiVacuumControl = true;       // AI controle over auto vacuum
static bool aiVibeControl = false;        // AI controle over vibe
static uint32_t stressSimTimer = 0;       // Timer voor stress simulatie

// ESP-NOW ontvangst callback (nieuwere ESP32 Arduino core versie)
static void onESPNowReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  Serial.printf("[ESP-NOW] RX packet: len=%d, expected=%d\n", len, sizeof(esp_now_receive_message_t));
  if (len != sizeof(esp_now_receive_message_t)) {
    Serial.printf("[ESP-NOW] SIZE MISMATCH! Got %d bytes, expected %d\n", len, sizeof(esp_now_receive_message_t));
  }
  
  if (len == sizeof(esp_now_receive_message_t)) {
    esp_now_receive_message_t message;
    memcpy(&message, incomingData, sizeof(message));
    
    // Update machine parameters
    trustSpeed = message.trust;
    sleeveSpeed = message.sleeve;
    suctionLevel = message.suction;
    pauseTime = message.pause;
    vibeOn = message.vibeOn;
    zuigActive = message.zuigActive;
    vacuumMbar = message.vacuumMbar;
    pauseActive = message.pauseActive;
    
    // Lube sync systeem
    if (message.lubeTrigger && !lubeTrigger) {
      // Nieuwe lube trigger gedetecteerd!
      lastLubeTriggerTime = millis();
      Serial.println("[LUBE SYNC] Nieuwe cyclus start gedetecteerd!");
    }
    lubeTrigger = message.lubeTrigger;
    cyclusTijd = message.cyclusTijd;
    sleevePercentage = message.sleevePercentage;  // Update echte sleeve positie
    hoofdESPSpeedStep = message.currentSpeedStep; // Update huidige versnelling
    
    lastCommTime = millis();
    
    // Debug output
    Serial.printf("[ESP-NOW] RX SUCCESS: T:%.1f S:%.1f Su:%.1f P:%.1f V:%d Z:%d Vac:%.1f Cmd:%s\n", 
                  trustSpeed, sleeveSpeed, suctionLevel, pauseTime, vibeOn, zuigActive, vacuumMbar, message.command);
  } else {
    Serial.printf("[ESP-NOW] RX SIZE MISMATCH: Got %d bytes, expected %d\n", len, sizeof(esp_now_receive_message_t));
  }
}

// ESP-NOW initialisatie
static bool initESPNow() {
  Serial.println("[ESP-NOW] Initializing for SIMPLE protocol...");
  Serial.printf("[ESP-NOW] Send message size: %d bytes (simple)\n", sizeof(esp_now_send_message_t));
  Serial.printf("[ESP-NOW] Receive message size: %d bytes (simple)\n", sizeof(esp_now_receive_message_t));
  
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(4);  // Body ESP op kanaal 4 (same as HoofdESP)
  Serial.printf("[ESP-NOW] WiFi channel set to 4\n");
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init failed");
    return false;
  }
  Serial.println("[ESP-NOW] Core initialized");
  
  // Registreer ontvangst callback
  esp_now_register_recv_cb(onESPNowReceive);
  Serial.println("[ESP-NOW] RX callback registered");
  
  // Voeg HoofdESP toe als peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, hoofdESP_MAC, 6);
  peerInfo.channel = 4;  // HoofdESP op kanaal 4
  peerInfo.encrypt = false;
  
  Serial.printf("[ESP-NOW] Adding HoofdESP peer: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                hoofdESP_MAC[0], hoofdESP_MAC[1], hoofdESP_MAC[2], 
                hoofdESP_MAC[3], hoofdESP_MAC[4], hoofdESP_MAC[5]);
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Failed to add HoofdESP peer");
    return false;
  }
  
  Serial.println("[ESP-NOW] Initialized successfully - ready for communication");
  return true;
}

// ESP-NOW verzend functie - SIMPLE VERSION
static bool sendESPNowMessage(float newTrust, float newSleeve, bool overruleActive, const char* command, uint8_t stressLevel = 0, bool vibeOn = false, bool zuigenOn = false) {
  if (!espNowInitialized) return false;
  
  esp_now_send_message_t message;
  memset(&message, 0, sizeof(message)); // Clear all fields
  
  // Fill simple fields
  message.newTrust = newTrust;
  message.newSleeve = newSleeve;
  message.overruleActive = overruleActive;
  message.stressLevel = stressLevel;
  message.vibeOn = vibeOn;
  message.zuigenOn = zuigenOn;
  strncpy(message.command, command, sizeof(message.command) - 1);
  message.command[sizeof(message.command) - 1] = '\0';
  
  esp_err_t result = esp_now_send(hoofdESP_MAC, (uint8_t *)&message, sizeof(message));
  
  if (result == ESP_OK) {
    Serial.printf("[ESP-NOW] TX SUCCESS: T:%.1f S:%.1f O:%d Stress:%d V:%d Z:%d Cmd:%s (size=%d)\n", 
                  newTrust, newSleeve, overruleActive, stressLevel, vibeOn, zuigenOn, command, sizeof(message));
    return true;
  } else {
    Serial.printf("[ESP-NOW] TX FAILED: Error %d, Cmd:%s\n", result, command);
    return false;
  }
}

// Toggle AI Overrule functie
static void localToggleAIOverrule() {
  aiConfig.enabled = !aiConfig.enabled;
  
  if (!aiConfig.enabled) {
    // Reset overrides als AI uit staat
    currentTrustOverride = 1.0f;
    currentSleeveOverride = 1.0f;
    aiOverruleActive = false;
    Serial.println("[BODY] AI Overrule UITGESCHAKELD");
  } else {
    Serial.println("[BODY] AI Overrule INGESCHAKELD");
  }
}

// AI Test Control Functions
void startAITest() {
  aiTestModeActive = true;
  aiControlActive = false;
  userPausedAI = false;
  redBackgroundActive = false;
  
  // AUTOMATIC CSV RECORDING: Start recording for AI Test session
  if (BODY_CFG.autoRecordSessions && !isRecording) {
    startRecording();
    Serial.println("[AI TEST] Auto-recording started for AI Test session");
  }
  
  Serial.println("[AI TEST] Test modus gestart - AI gaat controle nemen");
}

void stopAITest() {
  aiTestModeActive = false;
  aiControlActive = false;
  userPausedAI = false; 
  redBackgroundActive = false;
  
  // AUTOMATIC CSV RECORDING: Stop recording when AI Test session ends
  if (BODY_CFG.autoRecordSessions && isRecording) {
    stopRecording();
    Serial.println("[AI TEST] Auto-recording stopped - AI Test session ended");
  }
  
  Serial.println("[AI TEST] Test modus gestopt");
  // Reset to manual control
  sendESPNowMessage(1.0f, 1.0f, false, "MANUAL_CONTROL");
}

// AI Stress Management Functions (Test 2)
void startAIStressManagement() {
  aiStressModeActive = true;
  aiControlActive = true;
  aiOverruleActive = true;  // Voor consistente debug output
  userPausedAI = false;
  redBackgroundActive = false;
  
  // Start met echte huidige versnelling van HoofdESP
  aiStartSpeedStep = hoofdESPSpeedStep;
  currentSpeedStep = hoofdESPSpeedStep;  // AI begint met huidige speed
  currentStressLevel = 3;  // Start stress level
  stressSimTimer = millis();
  
  // AUTOMATIC CSV RECORDING: Start recording for AI Stress Management session  
  if (BODY_CFG.autoRecordSessions && !isRecording) {
    startRecording();
    Serial.println("[AI STRESS] Auto-recording started for AI Stress session");
  }
  
  // AI neemt controle over met huidige snelheid
  float currentTrust = (aiStartSpeedStep / 7.0f) * 2.0f;  // Convert speed step naar trust
  sendESPNowMessage(currentTrust, 1.0f, true, "AI_STRESS_START");
  
  Serial.printf("[AI STRESS] AI neemt controle over vanaf speed step %d (trust %.1f), stress level %d\n",
                aiStartSpeedStep, currentTrust, currentStressLevel);
}

void stopAIStressManagement() {
  aiStressModeActive = false;
  aiControlActive = false;
  aiVacuumControl = false;
  aiVibeControl = false;
  
  // AUTOMATIC CSV RECORDING: Stop recording when AI Stress session ends
  if (BODY_CFG.autoRecordSessions && isRecording) {
    stopRecording();
    Serial.println("[AI STRESS] Auto-recording stopped - AI Stress session ended");
  }
  
  Serial.println("[AI STRESS] AI controle gestopt - terug naar manueel");
  sendESPNowMessage(1.0f, 1.0f, false, "MANUAL_CONTROL");
}

// AI Overrule logica
static void updateAIOverrule(float heartRate, float temperature, float gsrLevel) {
  if (!aiConfig.enabled) {
    // Reset overrides als AI uit staat
    currentTrustOverride = 1.0f;
    currentSleeveOverride = 1.0f;
    aiOverruleActive = false;
    return;
  }
  
  uint32_t now = millis();
  if (now - lastAIUpdate < 1000) return;  // Update elke seconde
  lastAIUpdate = now;
  
  bool riskDetected = false;
  float riskLevel = 0.0f;
  
  // Analyseer risico factoren
  if (heartRate < aiConfig.hrLowThreshold || heartRate > aiConfig.hrHighThreshold) {
    riskLevel += 0.4f;  // Hartslag risico
    riskDetected = true;
  }
  
  if (temperature > aiConfig.tempHighThreshold) {
    riskLevel += 0.3f;  // Temperatuur risico
    riskDetected = true;
  }
  
  if (gsrLevel > aiConfig.gsrHighThreshold) {
    riskLevel += 0.3f;  // Stress risico
    riskDetected = true;
  }
  
  // Graduele aanpassing gebaseerd op risico niveau
  if (riskDetected && riskLevel > 0.2f) {
    // Risico gedetecteerd - verlaag speeds
    float targetTrust = 1.0f - (riskLevel * (1.0f - aiConfig.trustReduction));
    
    currentTrustOverride = min(currentTrustOverride, targetTrust);
    currentSleeveOverride = 1.0f;  // Sleeve altijd op 100%
    
    aiOverruleActive = true;
  } else {
    // Geen risico - geleidelijk herstellen
    currentTrustOverride = min(1.0f, currentTrustOverride + aiConfig.recoveryRate);
    currentSleeveOverride = 1.0f;  // Sleeve altijd op 100%
    
    if (currentTrustOverride >= 0.99f) {  // Alleen trust checken
      aiOverruleActive = false;
    }
  }
  
  // Stuur update naar HoofdESP als er significante verandering is
  static float lastSentTrust = 1.0f, lastSentSleeve = 1.0f;
  if (abs(currentTrustOverride - lastSentTrust) > 0.05f || 
      abs(currentSleeveOverride - lastSentSleeve) > 0.05f) {
    
    sendESPNowMessage(currentTrustOverride, currentSleeveOverride, aiOverruleActive, "AI_OVERRIDE");
    lastSentTrust = currentTrustOverride;
    lastSentSleeve = currentSleeveOverride;
  }
}

// Functie om scherm 180° te draaien
static void toggleScreenRotation() {
  // Wissel tussen normale rotatie en 180° gedraaid
  bool wasRotated = (currentRotation != DISPLAY_ROTATION);
  
  if (currentRotation == DISPLAY_ROTATION) {
    currentRotation = (DISPLAY_ROTATION + 2) % 4;  // +180°
  } else {
    currentRotation = DISPLAY_ROTATION;  // Terug naar normaal
  }
  
  body_gfx->setRotation(currentRotation);
  body_gfx->fillScreen(BODY_CFG.COL_BG);  // Scherm leegmaken na rotatie
  
  // Touch system opnieuw initialiseren met nieuwe orientatie
  inputTouchBegin(body_gfx);
  
  // Touch rotatie instellen
  bool isNowRotated = (currentRotation != DISPLAY_ROTATION);
  inputTouchSetRotated(isNowRotated);
}

// SD card functies
static void initSDCard() {
  if (SD.begin(SD_CS_PIN)) {
    sdCardAvailable = true;
    // Zoek het volgende beschikbare bestand nummer
    while (SD.exists("/data" + String(nextFileNumber) + ".csv")) {
      nextFileNumber++;
    }
    Serial.println("[BODY] SD card gereed");
  } else {
    sdCardAvailable = false;
    Serial.println("[BODY] SD card fout!");
  }
  delay(500);
}

void startRecording() {
  if (!sdCardAvailable) {
    Serial.println("[BODY] Geen SD card!");
    return;
  }
  
  String filename = "/data" + String(nextFileNumber) + ".csv";
  recordingFile = SD.open(filename, FILE_WRITE);
  
  if (recordingFile) {
    // Uitgebreide CSV header met alle sensor data inclusief vacuum
    recordingFile.println("Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause,Adem,Tril,ZuigActive,VacuumMbar");
    recordingStartTime = millis();
    samplesRecorded = 0;
    Serial.printf("[BODY] Opname gestart: %d\n", nextFileNumber);
  } else {
    isRecording = false;
    Serial.println("[BODY] Kan niet opnemen!");
  }
}

void stopRecording() {
  if (recordingFile) {
    recordingFile.close();
    Serial.printf("[BODY] Opname gestopt. Samples: %u\n", samplesRecorded);
    nextFileNumber++;  // Klaar voor volgende opname
  }
}

static void recordSample(float heartVal, float tempVal, float huidVal, float oxyVal, bool beat) {
  // Roep uitgebreide functie aan zonder ESP32 data (backwards compatibility)
  recordSampleExtended(heartVal, tempVal, huidVal, oxyVal, beat, 0, 0, 0, 0, 0, 0);
}

static void recordSampleExtended(float heartVal, float tempVal, float huidVal, float oxyVal, bool beat,
                                float trust, float sleeve, float suction, float pause, float adem, float tril) {
  if (!isRecording || !recordingFile) return;
  
  uint32_t timeMs = millis() - recordingStartTime;
  
  // Uitgebreide CSV regel: Time,Heart,Temp,Skin,Oxygen,Beat,Trust,Sleeve,Suction,Pause,Adem,Tril,ZuigActive,VacuumMbar
  recordingFile.print(timeMs);
  recordingFile.print(",");
  recordingFile.print(heartVal, 2);
  recordingFile.print(",");
  recordingFile.print(tempVal, 2);
  recordingFile.print(",");
  recordingFile.print(huidVal, 2);
  recordingFile.print(",");
  recordingFile.print(oxyVal, 2);
  recordingFile.print(",");
  recordingFile.print(beat ? 1 : 0);
  recordingFile.print(",");
  recordingFile.print(trust, 2);
  recordingFile.print(",");
  recordingFile.print(sleeve, 2);
  recordingFile.print(",");
  recordingFile.print(suction, 2);
  recordingFile.print(",");
  recordingFile.print(pause, 2);
  recordingFile.print(",");
  recordingFile.print(adem, 2);
  recordingFile.print(",");
  recordingFile.print(tril, 2);
  recordingFile.print(",");
  recordingFile.print(zuigActive ? 1 : 0);
  recordingFile.print(",");
  recordingFile.println(vacuumMbar, 2);
  
  samplesRecorded++;
  
  // Elke 100 samples even flushen
  if (samplesRecorded % 100 == 0) {
    recordingFile.flush();
  }
}

// ===== Event Logging Functies =====
static void initEventLogging() {
  if (!sdCardAvailable) return;
  
  String filename = "/events" + String(nextFileNumber) + ".csv";
  eventLogFile = SD.open(filename, FILE_WRITE);
  
  if (eventLogFile) {
    eventLogFile.println("Timestamp,EventType,Parameter,Value,Details");
    eventLoggingActive = true;
    Serial.printf("[BODY] Event logging gestart: %s\n", filename.c_str());
  }
}

static void logEvent(const char* eventType, const char* details, const char* parameter = "", float value = 0.0f) {
  if (!eventLoggingActive || !eventLogFile) return;
  
  uint32_t timestamp = millis();
  eventLogFile.printf("%u,%s,%s,%.2f,%s\n", timestamp, eventType, parameter, value, details);
  eventLogFile.flush();
  
  Serial.printf("[EVENT] %s: %s=%.1f - %s\n", eventType, parameter, value, details);
}

static void stopEventLogging() {
  if (eventLogFile) {
    eventLogFile.close();
    eventLoggingActive = false;
    Serial.println("[BODY] Event logging gestopt");
  }
}

static void updateStatusLabel(){
  if (mode == MODE_MENU)       body_gfx4_drawStatus("Menu");
  else if (mode == MODE_PLAYLIST) body_gfx4_drawStatus("Opname selecteren");
  else if (mode == MODE_CONFIRM) body_gfx4_drawStatus("Bevestiging");
  else if (mode == MODE_SENSOR_SETTINGS) body_gfx4_drawStatus("Sensor instellingen");
  else if (mode == MODE_SYSTEM_SETTINGS) body_gfx4_drawStatus("Systeem instellingen");
  else if (mode == MODE_OVERRULE) body_gfx4_drawStatus("AI Overrule instellingen");
  else if (mode == MODE_AI_ANALYZE) body_gfx4_drawStatus("AI Data Analyse");
  else if (mode == MODE_AI_EVENT_CONFIG) body_gfx4_drawStatus("AI Event Configuratie");
  else if (mode == MODE_COLORS) body_gfx4_drawStatus("Kleurinstellingen");
  else if (mode == MODE_AI_TRAINING) body_gfx4_drawStatus("AI Training");
  else if (uiMenu)             body_gfx4_drawStatus("Menu open");
  else if (isRecording)        body_gfx4_drawStatus("OPNEMEN ACTIEF");
  else if (isPlaying)          body_gfx4_drawStatus("AFSPELEN ACTIEF");
  else                         body_gfx4_drawStatus("Gereed");
}

static void enterMain(){
  mode = MODE_MAIN; uiMenu = false;
  
  // AUTOMATIC CSV RECORDING: Stop auto-recording when returning to main (session ends)
  if (BODY_CFG.autoRecordSessions && isRecording) {
    // Only stop if this was an auto-started recording, not manually started
    // We'll always stop auto-recording when leaving a session mode
    stopRecording();
    Serial.println("[MAIN] Auto-recording stopped - returned to main screen");
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

static void enterMenu(){
  mode = MODE_MENU;
  uiMenu = true;
  menu_begin(body_gfx);
  Serial.println("[BODY] Entered menu mode");
}

static void enterPlaylist(){
  mode = MODE_PLAYLIST;
  playlist_begin(body_gfx);
  Serial.println("[BODY] Entered playlist mode");
}

static void enterSensorSettings(){
  mode = MODE_SENSOR_SETTINGS;
  sensorSettings_begin(body_gfx);
  Serial.println("[BODY] Entered sensor settings mode");
}

static void enterSystemSettings(){
  mode = MODE_SYSTEM_SETTINGS;
  systemSettings_begin(body_gfx);
  Serial.println("[BODY] Entered system settings mode");
}

static void enterOverrule(){
  mode = MODE_OVERRULE;
  overrule_begin(body_gfx);
  Serial.println("[BODY] Entered AI overrule mode");
}

static void enterAIAnalyze(){
  Serial.println("[BODY] enterAIAnalyze() starting...");
  
  Serial.println("[BODY] Setting mode to MODE_AI_ANALYZE...");
  mode = MODE_AI_ANALYZE;
  
  Serial.println("[BODY] Calling aiAnalyze_begin()...");
  aiAnalyze_begin(body_gfx);
  
  Serial.println("[BODY] AI analyze mode entered successfully");
}

static void enterAIEventConfig(){
  mode = MODE_AI_EVENT_CONFIG;
  aiEventConfig_begin(body_gfx);
  Serial.println("[BODY] Entered AI event config mode");
}

static void enterColors(){
  mode = MODE_COLORS;
  colors_begin(body_gfx);
  Serial.println("[BODY] Entered colors mode");
}

static void enterMLTraining(){
  mode = MODE_AI_TRAINING;
  mlTraining_begin(body_gfx);
  Serial.println("[BODY] Entered ML Training mode");
}

static void enterAITraining(const String& filename){
  mode = MODE_AI_TRAINING;
  aiTraining_begin(body_gfx, filename);
  Serial.printf("[BODY] Entered AI Training mode with file: %s\n", filename.c_str());
}

// Playback functies
static void startPlayback(String filename) {
  if (!sdCardAvailable) return;
  
  playbackFile = SD.open("/" + filename);
  if (playbackFile) {
    // Skip header regel voor CSV en ALY bestanden
    playbackFile.readStringUntil('\n');
    
    isPlaybackActive = true;
    playbackStartTime = millis();
    nextSampleTime = 0;
    
    // Terug naar main screen maar in playback mode
    isPlaying = true;
    isRecording = false;
    enterMain();
    
    String fileType = filename.endsWith(".aly") ? "ALY" : "CSV";
    Serial.printf("[PLAYBACK] Starting %s playback: %s\n", fileType.c_str(), filename.c_str());
  }
}

static void stopPlayback() {
  if (playbackFile) {
    playbackFile.close();
  }
  isPlaybackActive = false;
  isPlaying = false;
  
  // Notify HoofdESP that playback has stopped
  sendESPNowMessage(1.0f, 1.0f, false, "PLAYBACK_STOP", 0);
  
  Serial.println("[BODY] Playback gestopt");
}

// Parse CSV regel om stress level te extraheren
static uint8_t parseCSVStressLevel(String csvLine) {
  int commaCount = 0;
  int lastComma = -1;
  
  // Tel komma's en vind laatste
  for (int i = 0; i < csvLine.length(); i++) {
    if (csvLine.charAt(i) == ',') {
      commaCount++;
      lastComma = i;
    }
  }
  
  // Stress level is laatste kolom (na 13 komma's = kolom 14)
  if (commaCount >= 13) {
    String stressStr = csvLine.substring(lastComma + 1);
    stressStr.trim();
    int stress = stressStr.toInt();
    return constrain(stress, 1, 7);  // Zorg voor bereik 1-7
  }
  
  return 3;  // Default stress level
}

// Parse ALY regel om stress level te extraheren
static uint8_t parseALYStressLevel(String alyLine) {
  // ALY format: "timestamp,stress_level,confidence,reasoning"
  int firstComma = alyLine.indexOf(',');
  if (firstComma == -1) return 3;
  
  int secondComma = alyLine.indexOf(',', firstComma + 1);
  if (secondComma == -1) return 3;
  
  String stressStr = alyLine.substring(firstComma + 1, secondComma);
  stressStr.trim();
  int stress = stressStr.toInt();
  return constrain(stress, 1, 7);  // Zorg voor bereik 1-7
}

// Lees volgende playback sample
static void processPlaybackSample() {
  if (!isPlaybackActive || !playbackFile || !playbackFile.available()) {
    return;
  }
  
  uint32_t currentTime = millis() - playbackStartTime;
  
  // Lees volgende regel
  String line = playbackFile.readStringUntil('\n');
  if (line.length() == 0) {
    // Einde bestand bereikt
    stopPlayback();
    return;
  }
  
  line.trim();
  
  // Parse timestamp (eerste kolom)
  int firstComma = line.indexOf(',');
  if (firstComma == -1) return;
  
  uint32_t sampleTime = line.substring(0, firstComma).toInt();
  
  // Check timing - speel sample af als tijd gekomen is
  if (currentTime >= sampleTime) {
    uint8_t stressLevel = 3;  // Default
    
    // Voor CSV: bereken stress level uit trust speed + lees vibe/zuigen data
    // Voor ALY: gebruik directe stress level
    bool playbackVibeOn = false;
    bool playbackZuigenOn = false;
    
    String filename = playbackFile.name();
    if (filename.endsWith(".csv")) {
      // Parse CSV regel om alle velden te krijgen
      int commaIndex[11];
      int commaCount = 0;
      for (int i = 0; i < line.length() && commaCount < 11; i++) {
        if (line.charAt(i) == ',') {
          commaIndex[commaCount++] = i;
        }
      }
      
      if (commaCount >= 11) {
        // Parse trust speed (kolom 7) en converteer naar ZUIVERE stress level (1-7)
        float trustSpeed = line.substring(commaIndex[5] + 1, commaIndex[6]).toFloat();
        
        // Zuivere mapping: Trust 0.0-2.0 -> Stress 1-7
        if (trustSpeed <= 0.0f) {
          stressLevel = 1;
        } else if (trustSpeed >= 2.0f) {
          stressLevel = 7;
        } else {
          stressLevel = (int)(1 + (trustSpeed / 2.0f) * 6.0f);
        }
        
        // Parse vibration level (kolom 12)
        int vibrationEnd = line.indexOf(',', commaIndex[10] + 1);
        if (vibrationEnd == -1) vibrationEnd = line.length();
        String vibrationStr = line.substring(commaIndex[10] + 1, vibrationEnd);
        vibrationStr.trim();
        float vibrationLevel = vibrationStr.toFloat();
        playbackVibeOn = (vibrationLevel > 50.0f);
        
        // Parse suction level (kolom 9)
        String suctionStr = line.substring(commaIndex[7] + 1, commaIndex[8]);
        suctionStr.trim();
        float suctionLevel = suctionStr.toFloat();
        playbackZuigenOn = (suctionLevel > 0.5f);
      } else {
        stressLevel = 3;
      }
    } else if (filename.endsWith(".aly")) {
      stressLevel = parseALYStressLevel(line);
    }
    
    // Stuur stress level + vibe/zuigen data naar HoofdESP
    sendESPNowMessage(1.0f, 1.0f, false, "PLAYBACK_STRESS", stressLevel, playbackVibeOn, playbackZuigenOn);
    
    Serial.printf("[PLAYBACK] Time=%u, Stress=%d, Vibe=%s, Zuigen=%s\n", 
                  sampleTime, stressLevel, playbackVibeOn ? "ON" : "OFF", 
                  playbackZuigenOn ? "ON" : "OFF");
    
    nextSampleTime = sampleTime;
  } else {
    // Te vroeg - ga terug in bestand voor volgende keer
    playbackFile.seek(playbackFile.position() - line.length() - 1);
  }
}

static void deleteFile(String filename) {
  if (SD.remove("/" + filename)) {
    Serial.printf("[BODY] Verwijderd: %s\n", filename.c_str());
  } else {
    Serial.println("[BODY] Verwijderen mislukt");
  }
}

static void showDeleteConfirm(String filename) {
  mode = MODE_CONFIRM;
  confirmType = CONFIRM_DELETE_FILE;
  confirmData = filename;
  confirm_begin(body_gfx, "Bestand verwijderen?", filename.c_str());
  Serial.printf("[BODY] Confirm delete: %s\n", filename.c_str());
}

static void showFormatConfirm() {
  mode = MODE_CONFIRM;
  confirmType = CONFIRM_FORMAT_SD;
  confirmData = "";
  confirm_begin(body_gfx, "SD kaart formatteren?", "Alle data wordt gewist!");
  Serial.println("[BODY] Format confirm shown");
}

static void formatSD() {
  // Let op: SD.format() is niet beschikbaar in alle Arduino SD libraries
  // Als alternatief: verwijder alle data*.csv bestanden
  File root = SD.open("/");
  if (!root) {
    Serial.println("[BODY] SD card fout!");
    return;
  }
  
  int deletedCount = 0;
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    file.close();
    
    if (name.startsWith("data") && name.endsWith(".csv")) {
      if (SD.remove("/" + name)) {
        deletedCount++;
      }
    }
    file = root.openNextFile();
  }
  root.close();
  
  Serial.printf("[BODY] Verwijderd: %d bestanden\n", deletedCount);
}

// ===== ADS1115 SENSOR FUNCTIES (NIEUW!) =====

static void calibrateADS1115Sensors() {
  Serial.println("\n[CALIBRATIE] Start sensor calibratie...");
  Serial.println("[CALIBRATIE] Raak sensoren NIET aan...");
  delay(2000);
  
  // GSR calibratie (A0)
  long gsr_sum = 0;
  for(int i = 0; i < 500; i++) {
    gsr_sum += ads.readADC_SingleEnded(0);
    delay(5);
  }
  gsr_threshold = gsr_sum / 500;
  Serial.printf("[CALIBRATIE] GSR threshold = %d\n", gsr_threshold);
  
  // Flex baseline (A1)
  long flex_sum = 0;
  for(int i = 0; i < 100; i++) {
    flex_sum += ads.readADC_SingleEnded(1);
    delay(10);
  }
  flex_baseline = ads.computeVolts(flex_sum / 100);
  Serial.printf("[CALIBRATIE] Flex baseline = %.3fV\n", flex_baseline);
  
  // Pulse baseline (A2)
  long pulse_sum = 0;
  for(int i = 0; i < 100; i++) {
    pulse_sum += ads.readADC_SingleEnded(2);
    delay(10);
  }
  pulse_baseline = pulse_sum / 100;
  Serial.printf("[CALIBRATIE] Pulse baseline = %d\n", pulse_baseline);
  
  Serial.println("[CALIBRATIE] Klaar!\n");
}

static void readADS1115Sensors() {
  // ===== A0: GSR Sensor =====
  int gsr_raw = ads.readADC_SingleEnded(0);
  gsrValue = (float)gsr_raw;
  
  // Toepassen van baseline en sensitivity
  float calibratedValue = (gsrValue - sensorConfig.gsrBaseline) * sensorConfig.gsrSensitivity;
  
  // Smoothing met configureerbare factor
  float smoothFactor = sensorConfig.gsrSmoothing;  // 0.0 tot 1.0
  gsrSmooth = gsrSmooth * (1.0f - smoothFactor) + calibratedValue * smoothFactor;
  
  // Zorg dat waarde positief blijft
  if (gsrSmooth < 0) gsrSmooth = 0;
  
  // ===== A1: Flex Sensor (Ademhaling) =====
  int flex_raw = ads.readADC_SingleEnded(1);
  float flex_voltage = ads.computeVolts(flex_raw);
  float ademhalingVal = 100 - ((flex_voltage - 0.5) / 2.0 * 100);
  // Ademhaling wordt hieronder in loop gebruikt
  
  // ===== A2: Pulse Sensor =====
  int pulse_raw = ads.readADC_SingleEnded(2);
  
  // Update min/max voor threshold
  if(pulse_raw > pulse_max) pulse_max = pulse_raw;
  if(pulse_raw < pulse_min) pulse_min = pulse_raw;
  
  int pulse_threshold_val = pulse_min + ((pulse_max - pulse_min) / 2);
  
  // Beat detection
  bool beat = false;
  if(pulse_raw > pulse_threshold_val && !beat_detected) {
    unsigned long now = millis();
    if(lastBeat > 0) {
      unsigned long ibi = now - lastBeat;
      if(ibi > 300 && ibi < 2000) {
        BPM = 60000 / ibi;
        beat = true;
        lastBeatMs = now;
      }
    }
    lastBeat = now;
    beat_detected = true;
  }
  if(pulse_raw < pulse_threshold_val) {
    beat_detected = false;
  }
  
  // Reset min/max elke 2 seconden
  static unsigned long lastReset = 0;
  if(millis() - lastReset > 2000) {
    pulse_max = pulse_baseline;
    pulse_min = pulse_baseline;
    lastReset = millis();
  }
  
  // Voor filtering zoals in origineel - simuleer IR waarde uit pulse
  float x = (float)pulse_raw;
  float hp_y = HP_A * (hp_y_prev + x - x_prev);
  hp_y_prev = hp_y; x_prev = x;
  float bp_y = bp_y_prev + LP_A * (hp_y - bp_y_prev);
  bp_y_prev = bp_y;
  
  float currAbs = fabsf(bp_y);
  envAbs = fmaxf(envAbs * 0.98f, currAbs);
  float targetDiv = fmaxf(150.0f, envAbs / 0.90f);
  acDiv = acDiv*0.9f + targetDiv*0.1f;
  
  // ===== A3: NTC Temperatuur =====
  int ntc_raw = ads.readADC_SingleEnded(3);
  float ntc_voltage = ads.computeVolts(ntc_raw);
  
  if(ntc_voltage < 0.1) {
    tempValue = 0.0f;  // Sensor open
  } else if(ntc_voltage > 3.2) {
    tempValue = 0.0f;  // Sensor short
  } else {
    float ntc_resistance = R_TOP * ntc_voltage / (3.3 - ntc_voltage);
    float steinhart = ntc_resistance / NTC_NOMINAL;
    steinhart = log(steinhart);
    steinhart /= B_COEFFICIENT;
    steinhart += 1.0 / (TEMP_NOMINAL + 273.15);
    steinhart = 1.0 / steinhart;
    tempValue = steinhart - 273.15 + sensorConfig.tempOffset;
  }
}

// ===== SENSOR INITIALISATIE =====
static void initSensors() {
  Serial.println("[BODY] Initializing sensors...");
  
  // Start sensor I2C bus (Wire1 op pins 10/11)
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  Wire1.setClock(400000);
  
  // Initialize ADS1115
  if (!ads.begin(0x48, &Wire1)) {
    Serial.println("[BODY] ADS1115 niet gevonden!");
    Serial.println("[BODY] Check I2C bedrading op GPIO 10/11");
  } else {
    Serial.println("[BODY] ADS1115 OK!");
    ads.setGain(GAIN_ONE);              // ±4.096V
    ads.setDataRate(RATE_ADS1115_128SPS); // 128 samples/sec
    
    // Calibreer sensors
    calibrateADS1115Sensors();
  }
  
  // ESP-NOW communicatie initialiseren
  espNowInitialized = initESPNow();
  if (espNowInitialized) {
    Serial.println("[BODY] ESP-NOW gereed!");
  } else {
    Serial.println("[BODY] ESP-NOW fout!");
  }
}

static void readESP32Comm() {
  // ESP-NOW berichten worden automatisch verwerkt via callback
  // Deze functie checkt alleen nog timeout
  
  // Timeout check - zet waarden op 0 als geen communicatie
  if (millis() - lastCommTime > sensorConfig.commTimeout * 1000) {  // timeout in seconden
    trustSpeed = sleeveSpeed = suctionLevel = pauseTime = 0.0f;
    zuigActive = false;
    vacuumMbar = 0.0f;
    pauseActive = false;
    lubeTrigger = false;
    cyclusTijd = 10.0f;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("[BODY] Starting Body ESP SC01 Plus...");
  Serial.println("[BODY] ADS1115 Sensor System Active");
  
  // Initialize body display system
  body_gfx->begin();
  body_gfx->setRotation(DISPLAY_ROTATION);
  body_gfx->fillScreen(BODY_CFG.COL_BG);

  // === BACKLIGHT AANZETTEN ===
    pinMode(45, OUTPUT);
    digitalWrite(45, HIGH);
    Serial.println("[BODY] Backlight ON!");
  
  // Initialize body graphics system with HoofdESP styling
  body_gfx4_begin();
  
  // Set touch cooldowns: 3 seconds for main screen, 2 seconds for menu
  body_gfx4_setCooldown(3000, 2000);
  
  // Initialize touch system
  inputTouchBegin(body_gfx);
  
  // Load sensor settings
  loadSensorConfig();
  Serial.println("[BODY] Sensor configuratie geladen");
  
  // Initialize sensors (ADS1115 op Wire1, pins 10/11)
  initSensors();
  
  // Initialize SD card
  initSDCard();
  
  // Initialize event logging
  initEventLogging();
  
  // Initialize ML Stress Analyzer
  Serial.println("[BODY] Initializing ML Stress Analyzer...");
  if (ml_begin()) {
    Serial.println("[BODY] ML Stress Analyzer initialized successfully");
  } else {
    Serial.println("[BODY] ML Stress Analyzer initialization failed, continuing without ML");
  }
  
  // Reset alle AI status bij opstart
  aiTestModeActive = false;
  aiStressModeActive = false;
  aiControlActive = false;
  userPausedAI = false;
  redBackgroundActive = false;
  Serial.println("[BODY] AI systemen gereset - wachten op manuele activatie");
  
  // Enter main mode
  enterMain();
  
  Serial.println("[BODY] Initialization complete");
  Serial.println("[BODY] Sensors: GSR(A0), Flex(A1), Pulse(A2), NTC(A3)");
}

// AI Stress Management Control Function (Test 2)
void handleAIStressManagement() {
  if (!aiStressModeActive) return;
  
  uint32_t now = millis();
  
  // AI beslissingslogica (elke 5 seconden evaluatie)
  if (now - lastStressCheck > 5000) {
    static int prevStressLevel = 3;
    
    // Check stress level veranderingen
    if (currentStressLevel != prevStressLevel) {
      Serial.printf("[AI STRESS] Stress level veranderd: %d -> %d\n", prevStressLevel, currentStressLevel);
      
      // AI reacties op stress levels
      if (currentStressLevel == 7) {
        // NOODGEVAL: Maximale interventie
        currentSpeedStep = 7;  // Hoogste versnelling
        aiVibeControl = true;  // Vibe AAN
        aiVacuumControl = true; // Auto vacuum AAN
        
        // Stuur commando's naar HoofdESP voor vibe en vacuum
        float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
        sendESPNowMessage(newTrust, 1.0f, true, "AI_VIBE_ON");
        delay(50); // Korte delay tussen commando's
        sendESPNowMessage(newTrust, 1.0f, true, "AI_VACUUM_ON");
        
        Serial.println("[AI STRESS] NOODGEVAL! Niveau 7 - Max speed + Vibe + Vacuum");
        
      } else if (currentStressLevel == 6) {
        // Hoge stress: drastisch verlagen
        if (currentSpeedStep > 1) {
          currentSpeedStep = 1;  // Naar speed step 1
          aiVacuumControl = false; // Auto vacuum UIT
          aiVibeControl = false;   // Vibe UIT
          
          // Stuur commando's naar HoofdESP
          float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
          sendESPNowMessage(newTrust, 1.0f, true, "AI_VACUUM_OFF");
          delay(50);
          sendESPNowMessage(newTrust, 1.0f, true, "AI_VIBE_OFF");
          
          Serial.println("[AI STRESS] Hoge stress (6) - Verlagen naar speed 1, vacuum UIT");
        } else if (currentSpeedStep == 1) {
          currentSpeedStep = 0;  // Naar langzaamste
          Serial.println("[AI STRESS] Stress blijft hoog - Naar langzaamste (0)");
        }
        
      } else if (currentStressLevel == 5) {
        // Stress daalt: vacuum weer aan
        if (!aiVacuumControl) {
          aiVacuumControl = true;
          
          // Stuur commando naar HoofdESP
          float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
          sendESPNowMessage(newTrust, 1.0f, true, "AI_VACUUM_ON");
          
          Serial.println("[AI STRESS] Stress daalt (5) - Auto vacuum weer AAN");
        }
        
      } else if (currentStressLevel <= 3 && prevStressLevel >= 5) {
        // Stress significant gedaald: speed weer verhogen
        if (currentSpeedStep < aiStartSpeedStep) {
          currentSpeedStep++;
          Serial.printf("[AI STRESS] Stress gedaald (%d) - Speed verhogen naar %d\n", currentStressLevel, currentSpeedStep);
        }
      }
      
      // Stuur nieuwe instellingen naar HoofdESP
      float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
      sendESPNowMessage(newTrust, 1.0f, true, "AI_STRESS_ADJUST");
      
      prevStressLevel = currentStressLevel;
    }
    
    // AI kan ook graduele aanpassingen maken (na 1 minuut stabiliteit)
    else if (now - lastSpeedAdjust > 60000) {  // 1 minuut wachten
      if (currentStressLevel == 2 && currentSpeedStep < 7) {
        // Lage stress: voorzichtig verhogen
        currentSpeedStep++;
        float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
        sendESPNowMessage(newTrust, 1.0f, true, "AI_STRESS_ADJUST");
        Serial.printf("[AI STRESS] Stabiele lage stress - Speed verhogen naar %d\n", currentSpeedStep);
        lastSpeedAdjust = now;
      }
    }
    
    lastStressCheck = now;
  }
  
  // Debug output (elke 10 seconden)
  static uint32_t lastDebug = 0;
  if (now - lastDebug > 10000) {
    Serial.printf("[AI STRESS] Level: %d, Speed: %d, Vacuum: %s, Vibe: %s\n",
                  currentStressLevel, currentSpeedStep, aiVacuumControl ? "AAN" : "UIT", aiVibeControl ? "AAN" : "UIT");
    lastDebug = now;
  }
}

// Seriële Input Handler voor stress level testing
void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // Check voor stress commando: "stress X" waar X = 1-7
    if (input.startsWith("stress ")) {
      int stressLevel = input.substring(7).toInt();
      
      if (stressLevel >= 1 && stressLevel <= 7) {
        // Check of AI Stress Management actief is
        if (aiStressModeActive) {
          // Forceer stress level override
          currentStressLevel = stressLevel;
          Serial.printf("[SERIAL DEBUG] Stress level handmatig ingesteld op: %d\n", stressLevel);
          
        } else {
          Serial.printf("[SERIAL DEBUG] ERROR: AI Stress Management is niet actief! Start eerst AI.\n");
        }
        
      } else {
        Serial.printf("[SERIAL DEBUG] ERROR: Ongeldig stress level. Gebruik 1-7.\n");
      }
      
    } else if (input == "ml") {
      // ML Stress Analysis commando
      if (mlAnalyzer.isReady()) {
        StressAnalysis result = mlAnalyzer.analyzeStress();
        Serial.printf("[ML ANALYSE] Stress Level: %d (%.1f%% confidence)\n", 
                      result.stressLevel, result.confidence * 100);
        Serial.printf("[ML ANALYSE] Reasoning: %s\n", result.reasoning.c_str());
        
        // Automatisch toepassen van ML resultaat
        if (aiStressModeActive && result.confidence > 0.7f) {
          currentStressLevel = result.stressLevel;
          Serial.printf("[ML AUTO] ML stress level %d automatisch toegepast\n", result.stressLevel);
        }
      } else {
        Serial.println("[ML ANALYSE] Onvoldoende sensor data - wacht even...");
      }
      
    } else if (input == "help") {
      Serial.println("[SERIAL DEBUG] Beschikbare commando's:");
      Serial.println("  stress X    - Stel stress level in (X = 1-7)");
      Serial.println("  ml          - ML stress analyse uitvoeren");
      Serial.println("  help        - Toon deze help");
    }
  }
}

// Universele AI Pause Detectie (voor alle AI modes)
void handleAIPauseDetection() {
  // Skip als er geen AI actief is
  if (!aiStressModeActive && !aiTestModeActive) return;
  
  // Detect user pause (C-knop van HoofdESP)
  if (aiControlActive && pauseActive && !userPausedAI) {
    
    if (aiStressModeActive && currentStressLevel == 7) {
      // STRESS LEVEL 7: C-knop schakelt AI volledig uit + emergency safe mode
      Serial.println("[AI STRESS] C-knop EMERGENCY OVERRIDE bij stress level 7!");
      
      // Stuur emergency override commando naar HoofdESP
      sendESPNowMessage(0.0f, 0.0f, false, "AI_EMERGENCY_OVERRIDE");
      
      // Stop AI systeem
      stopAIStressManagement();
      return;
      
    } else if (aiStressModeActive) {
      // Stress levels 1-6: normale rode scherm gedrag
      userPausedAI = true;
      aiControlActive = false;
      redBackgroundActive = true;
      Serial.printf("[AI STRESS] User pause bij stress level %d - rode achtergrond ON\n", currentStressLevel);
      
    } else {
      // Test 1 modus: rode scherm gedrag
      userPausedAI = true;
      aiControlActive = false;
      redBackgroundActive = true;
      Serial.println("[AI TEST] User pause detected - GEREGISTREERD, rode achtergrond ON");
    }
  }
}

// AI Test Control Function
void handleAITestControl() {
  if (!aiTestModeActive) return;
  
  uint32_t now = millis();
  
  // Start AI control
  if (!aiControlActive && !userPausedAI) {
    aiControlActive = true;
    Serial.println("[AI TEST] AI neemt controle");
    
    // Send AI commands to HoofdESP
    sendESPNowMessage(aiTargetTrust, 1.0f, true, "AI_TEST_START");
  }
  
  // AI LEER MODUS: registreer user acties maar DOE NIKS
  if (aiControlActive && !userPausedAI && (now - lastAITestUpdate > 1000)) {
    // Check vibe changes
    static bool lastVibeState = false;
    if (vibeOn != lastVibeState) {
      Serial.printf("[AI LEER] User toggled VIBE to %s - GEREGISTREERD\n", vibeOn ? "ON" : "OFF");
      lastVibeState = vibeOn;
    }
    
    // Check zuig changes  
    static bool lastZuigState = false;
    if (zuigActive != lastZuigState) {
      Serial.printf("[AI LEER] User toggled ZUIGEN to %s - GEREGISTREERD\n", zuigActive ? "ACTIEF" : "UIT");
      lastZuigState = zuigActive;
    }
    
    lastAITestUpdate = now;
  }
}

void loop() {
  if (mode == MODE_MAIN){
    // Check voor rode achtergrond (AI gepauzeerd)
    if (redBackgroundActive) {
      // Teken rode achtergrond
      body_gfx->fillScreen(0xF800);  // Rood
      
      // Toon bericht
      body_gfx->setTextColor(0xFFFF, 0xF800);  // Wit op rood
      body_gfx->setTextSize(2);
      int16_t x, y; uint16_t w, h;
      String pauseMsg = "AI GEPAUZEERD";
      body_gfx->getTextBounds(pauseMsg, 0, 0, &x, &y, &w, &h);
      body_gfx->setCursor((SCR_W - w) / 2, SCR_H / 2 - 40);
      body_gfx->print(pauseMsg);
      
      body_gfx->setTextSize(1);
      String touchMsg = "Raak scherm aan om door te gaan";
      body_gfx->getTextBounds(touchMsg, 0, 0, &x, &y, &w, &h);
      body_gfx->setCursor((SCR_W - w) / 2, SCR_H / 2);
      body_gfx->print(touchMsg);
      
      // Check voor ANY touch input om door te gaan
      int16_t tx, ty;
      if (inputTouchRead(tx, ty)) {
        Serial.printf("[RODE SCHERM] Touch detected at (%d,%d)\n", tx, ty);
        redBackgroundActive = false;
        
        // Direct AI resume handling
        if (userPausedAI) {
          userPausedAI = false;
          aiControlActive = true;
          
          if (aiStressModeActive) {
            float resumeTrust = 0.1f;
            sendESPNowMessage(resumeTrust, 1.0f, true, "AI_STRESS_RESUME");
            Serial.printf("[AI STRESS] Touch resume bij stress level %d\n", currentStressLevel);
          } else if (aiTestModeActive) {
            aiTargetTrust = 0.1f;
            sendESPNowMessage(aiTargetTrust, 1.0f, true, "AI_RESUME_SLOW");
            Serial.println("[AI TEST] Touch resume");
          }
        }
        
        // Terug naar normale display
        enterMain();
      }
      
      delay(50);
      return;
    }
    
    // Lees alle sensoren via ADS1115 (NIEUW!)
    readADS1115Sensors();
    readESP32Comm();

    // Gebruik echte sensor data
    float heartVal = (float)BPM;
    float tempVal = tempValue;
    float huidVal = gsrSmooth;
    
    // Feed sensor data to ML analyzer (every 100ms = 10Hz)
    static uint32_t lastMLUpdate = 0;
    if (millis() - lastMLUpdate >= 100) {
      mlAnalyzer.addSensorSample(heartVal, tempVal, huidVal);
      lastMLUpdate = millis();
    }
    
    // Dummy oxygen (nog geen sensor voor)
    dummyPhase += 0.1f;
    if (dummyPhase > TWO_PI) dummyPhase -= TWO_PI;
    float oxyVal = 95 + 2 * sin(dummyPhase/7);
    
    // Ademhaling komt nu van Flex sensor (A1)
    int flex_raw = ads.readADC_SingleEnded(1);
    float flex_voltage = ads.computeVolts(flex_raw);
    float ademhalingVal = 100 - ((flex_voltage - 0.5) / 2.0 * 100);
    ademhalingVal = constrain(ademhalingVal, 0, 100);
    
    // Beat detection voor display
    bool beat = (millis() - lastBeatMs < 200);  // Beat indicator

    // Draw samples naar scherm met HoofdESP styling
    body_gfx4_pushSample(G4_HART, bp_y_prev, beat);
    body_gfx4_pushSample(G4_HUID, huidVal);
    body_gfx4_pushSample(G4_TEMP, tempVal);
    body_gfx4_pushSample(G4_ADEMHALING, ademhalingVal);
    body_gfx4_pushSample(G4_OXY, oxyVal);
    
    // SNELH: Trust speed animatie
    float snelheidVal;
    static uint32_t animStartTime = 0;
    
    if (lubeTrigger && trustSpeed < 1.0f) {
      animStartTime = millis();
    }
    
    if (pauseActive) {
      snelheidVal = 0.0f;
    } else {
      uint32_t timeSinceReset = millis() - animStartTime;
      float speed = max(0.1f, cyclusTijd);
      float animFreq = speed * BODY_CFG.SNELH_SPEED_FACTOR;
      float phase = (timeSinceReset / 1000.0f) * animFreq;
      phase = fmod(phase, 1.0f) * 2.0f * PI;
      float s = 0.5f * (sinf(phase - PI/2.0f) + 1.0f);
      snelheidVal = s * 100.0f;
    }
    body_gfx4_pushSample(G4_HOOFDESP, snelheidVal);
    
    // ZUIGEN: Vacuum data
    float zuigenVal;
    if (zuigActive && vacuumMbar > 0) {
      float maxMbar = BODY_CFG.vacuumGraphMaxMbar;
      float scaledVacuum = min(vacuumMbar / maxMbar, 1.0f);
      zuigenVal = 20.0f + (scaledVacuum * 80.0f);
    } else {
      zuigenVal = 5.0f;
    }
    body_gfx4_pushSample(G4_ZUIGEN, zuigenVal);
    
    // VIBE: Handmatige VIBE toggle
    static float vibePhase = 0;
    vibePhase += 0.1f;
    float trilVal = vibeOn ? (100 + (3 * sin(vibePhase))) : (3 * sin(vibePhase));
    body_gfx4_pushSample(G4_TRIL, trilVal);
    
    // Teken knoppen elke loop
    bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
    body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
    
    // Debug output (elke 2 seconden)
    static uint32_t lastDebugTime = 0;
    if (millis() - lastDebugTime >= 2000) {
      Serial.printf("[SENSORS] HR:%d Temp:%.1f GSR:%.0f Flex:%.0f%%\n", 
                    BPM, tempVal, huidVal, ademhalingVal);
      Serial.printf("[HOOFD] Trust:%.1f Sleeve:%.1f Vacuum:%.1f\n",
                    trustSpeed, sleeveSpeed, vacuumMbar);
      lastDebugTime = millis();
    }
    
    // Handle serial input voor stress level testing
    handleSerialInput();
    
    // Update AI overrule systeem
    updateAIOverrule(heartVal, tempVal, huidVal);
    
    // Handle AI pause detection
    handleAIPauseDetection();
    
    // Handle AI test control
    handleAITestControl();
    
    // Handle AI stress management
    handleAIStressManagement();
    
    // Send heartbeat naar HoofdESP
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 2000) {
      bool effectiveAIActive = aiOverruleActive || aiStressModeActive || aiTestModeActive;
      sendESPNowMessage(currentTrustOverride, currentSleeveOverride, effectiveAIActive, "HEARTBEAT");
      lastHeartbeat = millis();
    }
    
    // Process playback if active
    if (isPlaybackActive) {
      processPlaybackSample();
    }
    
    // Record data if recording
    if (isRecording) {
      recordSampleExtended(heartVal, tempVal, huidVal, oxyVal, beat, 
                          trustSpeed, sleeveSpeed, suctionLevel, pauseTime, 
                          ademhalingVal, trilVal);
    }

    // Handle touch input met cooldown
    TouchEvent ev;
    if (inputTouchPoll(ev)) {
      if (body_gfx4_canTouch(true)) {  // Main screen cooldown
        body_gfx4_registerTouch(true);
        
        if (ev.kind == TE_TAP_REC) {
          if (isRecording) {
            isRecording = false;
            stopRecording();
          } else {
            isRecording = true;
            startRecording();
          }
        }
        if (ev.kind == TE_TAP_PLAY) {
          if (isPlaying) {
            stopPlayback();
            isPlaying = false;
          } else {
            enterPlaylist();
            return;
          }
        }
        if (ev.kind == TE_TAP_MENU) {
          uiMenu = true;
          mode = MODE_MENU;
          bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
          body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
          menu_begin(body_gfx);
          return;
        }
        if (ev.kind == TE_TAP_OVERRULE) {
          // AI knop: Universele AI toggle
          bool anyAIActive = aiStressModeActive || aiTestModeActive || aiConfig.enabled;
          
          if (anyAIActive) {
            // STOP alle AI systemen
            if (aiStressModeActive) stopAIStressManagement();
            if (aiTestModeActive) stopAITest();
            if (aiConfig.enabled) {
              aiConfig.enabled = false;
              aiOverruleActive = false;
            }
          } else {
            // START AI stress management
            startAIStressManagement();
          }
          
          bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
          body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
          return;
        }
        
        bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
        body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
        updateStatusLabel();
      }
    }

    delay(20);
    return;
  }

  // Menu mode  
  if (mode == MODE_MENU){
    MenuEvent mev = menu_poll();
    if (mev == ME_NONE) { delay(10); return; }
    if (mev == ME_BACK) { uiMenu = false; enterMain(); return; }
    if (mev == ME_SENSOR_SETTINGS){ enterSensorSettings(); return; }
    if (mev == ME_OVERRULE){ enterOverrule(); return; }
    if (mev == ME_SYSTEM_SETTINGS){ enterSystemSettings(); return; }
    if (mev == ME_PLAYLIST){ enterPlaylist(); return; }
    if (mev == ME_ML_TRAINING){ enterMLTraining(); return; }
  }

  if (mode == MODE_PLAYLIST){
    PlaylistEvent pev = playlist_poll();
    if (pev == PE_NONE) { delay(10); return; }
    if (pev == PE_BACK) { enterMain(); return; }
    if (pev == PE_PLAY_FILE) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) startPlayback(filename);
      return;
    }
    if (pev == PE_DELETE_CONFIRM) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) showDeleteConfirm(filename);
      return;
    }
    if (pev == PE_DELETE_FILE) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        deleteFile(filename);
        playlist_begin(body_gfx);
      }
      return;
    }
    if (pev == PE_AI_ANALYZE) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        aiAnalyze_setSelectedFile(filename);
        enterAIAnalyze();
      }
      return;
    }
  }
  
  if (mode == MODE_SYSTEM_SETTINGS) {
    SystemSettingsEvent sse = systemSettings_poll();
    if (sse == SSE_NONE) { delay(10); return; }
    if (sse == SSE_BACK) { enterMenu(); return; }
    if (sse == SSE_ROTATE_SCREEN) {
      toggleScreenRotation();
      systemSettings_begin(body_gfx);
      return;
    }
    if (sse == SSE_COLORS) { enterColors(); return; }
    if (sse == SSE_FORMAT_SD) { showFormatConfirm(); return; }
  }
  
  if (mode == MODE_SENSOR_SETTINGS) {
    SensorEvent sev = sensorSettings_poll();
    if (sev == SE_NONE) { delay(10); return; }
    if (sev == SE_BACK) { enterMenu(); return; }
    if (sev == SE_SAVE) {
      saveSensorConfig();
      body_gfx4_drawStatus("Instellingen opgeslagen!");
      delay(1000);
      enterMenu();
      return;
    }
    if (sev == SE_RESET) {
      resetSensorConfig();
      body_gfx4_drawStatus("Instellingen gereset!");
      delay(1000);
      return;
    }
  }
  
  if (mode == MODE_OVERRULE) {
    OverruleEvent oev = overrule_poll();
    if (oev == OE_NONE) { delay(10); return; }
    if (oev == OE_BACK) { enterMenu(); return; }
    if (oev == OE_TOGGLE_AI) {
      localToggleAIOverrule();
      overrule_begin(body_gfx);
      return;
    }
    if (oev == OE_SAVE) {
      body_gfx4_drawStatus("AI instellingen opgeslagen!");
      delay(1000);
      enterMenu();
      return;
    }
    if (oev == OE_RESET) {
      aiConfig = AIOverruleConfig();
      overrule_begin(body_gfx);
      return;
    }
    if (oev == OE_ANALYZE) { enterAIAnalyze(); return; }
    if (oev == OE_CONFIG) { enterAIEventConfig(); return; }
  }
  
  if (mode == MODE_AI_ANALYZE) {
    AnalyzeEvent aev = aiAnalyze_poll();
    if (aev == AE_NONE) { delay(10); return; }
    if (aev == AE_BACK) { enterOverrule(); return; }
    if (aev == AE_TRAIN) {
      String filename = aiAnalyze_getSelectedFile();
      if (filename.length() > 0) enterAITraining(filename);
      return;
    }
  }
  
  if (mode == MODE_AI_EVENT_CONFIG) {
    EventConfigEvent ece = aiEventConfig_poll();
    if (ece == ECE_NONE) { delay(10); return; }
    if (ece == ECE_BACK) { enterOverrule(); return; }
    if (ece == ECE_SAVE) {
      aiEventConfig_saveToEEPROM();
      body_gfx4_drawStatus("Event configuratie opgeslagen!");
      delay(1500);
      enterOverrule();
      return;
    }
    if (ece == ECE_RESET) {
      aiEventConfig_resetToDefaults();
      aiEventConfig_begin(body_gfx);
      return;
    }
  }
  
  if (mode == MODE_COLORS) {
    ColorEvent ce = colors_poll();
    if (ce == CE_NONE) { delay(10); return; }
    if (ce == CE_BACK) { enterSystemSettings(); return; }
    if (ce == CE_COLOR_CHANGE) {
      body_gfx4_drawStatus("Kleur gewijzigd!");
      delay(1000);
      return;
    }
  }
  
  if (mode == MODE_AI_TRAINING) {
    TrainingResult tr = aiTraining_poll();
    if (tr != TR_NONE) {
      if (tr == TR_BACK) { enterOverrule(); return; }
      if (tr == TR_COMPLETE) { enterOverrule(); return; }
      return;
    }
    
    MLTrainingEvent mte = mlTraining_poll();
    if (mte == MTE_NONE) { delay(10); return; }
    if (mte == MTE_BACK) { enterMenu(); return; }
    if (mte == MTE_IMPORT_DATA) { mlTraining_setState(ML_STATE_IMPORT); return; }
    if (mte == MTE_TRAIN_MODEL) { mlTraining_setState(ML_STATE_TRAINING); return; }
    if (mte == MTE_MODEL_MANAGER) { mlTraining_setState(ML_STATE_MODEL_MANAGER); return; }
  }
  
  if (mode == MODE_CONFIRM) {
    ConfirmEvent cev = confirm_poll();
    if (cev == CONF_NONE) { delay(10); return; }
    
    if (cev == CONF_YES) {
      if (confirmType == CONFIRM_DELETE_FILE) {
        deleteFile(confirmData);
        mode = MODE_PLAYLIST;
        playlist_begin(body_gfx);
      } else if (confirmType == CONFIRM_FORMAT_SD) {
        formatSD();
        mode = MODE_MENU;
        menu_begin(body_gfx);
      }
    } else if (cev == CONF_NO) {
      if (confirmType == CONFIRM_DELETE_FILE) {
        mode = MODE_PLAYLIST;
        playlist_begin(body_gfx);
      } else if (confirmType == CONFIRM_FORMAT_SD) {
        mode = MODE_MENU;
        menu_begin(body_gfx);
      }
    }
    return;
  }
  
  delay(10);
}

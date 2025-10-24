/*  
  Body ESP CYD V2 – 4-band + Menu + Touch-kalibratie (touch fix)
  - DISPLAY_ROTATION = 2 (matcht jouw werkende code patroon)
*/

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <SPI.h>
#include <SD.h>
#include <Arduino_GFX_Library.h>
#include <esp_now.h>
#include <WiFi.h>
#include "MAX30105.h"
#include "heartRate.h"

// ===== Screen dimensions voor CYD (Cheap Yellow Display) =====
const int SCR_W = 320;
const int SCR_H = 240;

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
#include "rgb_off.h"
#include "mcp9808.h"
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
// TIJDELIJK UITGESCHAKELD - S3 upgrade
// #include "multifunplayer_client.h"

// Display wordt nu gedefinieerd in body_display.cpp
// Extern reference naar de body display
extern Arduino_GFX *body_gfx;

// ===== SD Card =====
#define SD_CS_PIN 5  // CYD SD card CS pin
static bool sdCardAvailable = false;
static int nextFileNumber = 1;

// ===== Recording =====
static File recordingFile;
static uint32_t recordingStartTime = 0;
static uint32_t samplesRecorded = 0;

// ===== Event Logging =====
static File eventLogFile;
static bool eventLoggingActive = false;

// ===== MAX30102 =====
#define PIN_SDA 21
#define PIN_SCL 22
#define MAX_I2C_ADDR 0x57
MAX30105 max30;

// Filters
static float hp_y_prev = 0, x_prev = 0, bp_y_prev = 0;
static float envAbs = 1000, acDiv = 600.0f;
static uint16_t BPM = 0;
static uint32_t lastBeatMs = 0;

const float FS_HZ = 50.0f;
const float DT = 1.0f/FS_HZ;
const float HP_FC = 0.5f, HP_TAU = 1.0f/(2*PI*HP_FC), HP_A = HP_TAU/(HP_TAU+DT);
const float LP_FC = 5.0f, LP_TAU = 1.0f/(2*PI*LP_FC), LP_A = DT/(LP_TAU+DT);

// Dummy
static float dummyPhase = 0;

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

// ===== GSR Sensor (GPIO34) =====
#define GSR_PIN 34
static float gsrValue = 0.0f;
static float gsrSmooth = 0.0f;

// ===== MCP9808 Temperature =====
static float tempValue = 0.0f;
static bool mcpInitialized = false;

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

// Stress Test - Automatische stress level simulatie (UITGESCHAKELD - gebruik seriële interface)
/*
void stressTest() {
  uint32_t elapsed = millis() - stressSimTimer;
  
  // Stress scenario simulatie (zoals beschreven)
  if (elapsed < 30000) {          // 0-30s: Stress 3 -> 2
    currentStressLevel = 2;
  } else if (elapsed < 90000) {   // 30s-90s: Stress 2 (AI verhoogt speed)
    currentStressLevel = 2;
  } else if (elapsed < 150000) {  // 90s-150s: Stress 2 -> 3 (AI verhoogt weer)
    currentStressLevel = 3;
  } else if (elapsed < 180000) {  // 150s-180s: Stress 3 (AI doet niks)
    currentStressLevel = 3;
  } else if (elapsed < 200000) {  // 180s-200s: Stress SPIKE naar 6!
    currentStressLevel = 6;
  } else if (elapsed < 230000) {  // 200s-230s: Stress blijft 6 (AI verlaagt naar 0)
    currentStressLevel = 6;
  } else if (elapsed < 260000) {  // 230s-260s: Stress daalt naar 5
    currentStressLevel = 5;
  } else if (elapsed < 290000) {  // 260s-290s: Stress daalt naar 3
    currentStressLevel = 3;
  } else if (elapsed < 310000) {  // 290s-310s: STRESS EMERGENCY niveau 7!
    currentStressLevel = 7;
  } else {                        // 310s+: Scenario reset
    stressSimTimer = millis();
    currentStressLevel = 3;
  }
}
*/

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
  
  // NOTE: zuigActive boolean is beschikbaar voor toekomstige AI logica indien gewenst
  // Momenteel wordt alleen de status ontvangen en gelogd, geen extra risico berekening
  
  // Graduele aanpassing gebaseerd op risico niveau
  if (riskDetected && riskLevel > 0.2f) {
    // Risico gedetecteerd - verlaag speeds
    float targetTrust = 1.0f - (riskLevel * (1.0f - aiConfig.trustReduction));
    // float targetSleeve = 1.0f - (riskLevel * (1.0f - aiConfig.sleeveReduction));  // UITGESCHAKELD
    
    currentTrustOverride = min(currentTrustOverride, targetTrust);
    // currentSleeveOverride = min(currentSleeveOverride, targetSleeve);  // UITGESCHAKELD
    currentSleeveOverride = 1.0f;  // Sleeve altijd op 100%
    
    aiOverruleActive = true;
  } else {
    // Geen risico - geleidelijk herstellen
    currentTrustOverride = min(1.0f, currentTrustOverride + aiConfig.recoveryRate);
    // currentSleeveOverride = min(1.0f, currentSleeveOverride + aiConfig.recoveryRate);  // UITGESCHAKELD
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
  
  Serial.printf("[EVENT] %s: %s=%1.f - %s\n", eventType, parameter, value, details);
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

// Touch kalibratie verwijderd - niet nodig voor CYD touchscreen

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
        // Geen vibe/zuigen invloed - pure basis stress voor ML training
        float trustSpeed = line.substring(commaIndex[5] + 1, commaIndex[6]).toFloat();
        
        // Zuivere mapping: Trust 0.0-2.0 -> Stress 1-7 (lineaire verdeling)
        if (trustSpeed <= 0.0f) {
          stressLevel = 1;  // Minimum stress
        } else if (trustSpeed >= 2.0f) {
          stressLevel = 7;  // Maximum stress
        } else {
          // Lineaire mapping: 0.0-2.0 -> 1-7
          stressLevel = (int)(1 + (trustSpeed / 2.0f) * 6.0f);
        }
        
        // Parse vibration level (kolom 12) - alleen eerste waarde voor volgende komma
        int vibrationEnd = line.indexOf(',', commaIndex[10] + 1);
        if (vibrationEnd == -1) vibrationEnd = line.length();  // Als laatste kolom
        String vibrationStr = line.substring(commaIndex[10] + 1, vibrationEnd);
        vibrationStr.trim();
        float vibrationLevel = vibrationStr.toFloat();
        // CORRECTED: Vibe sensor data: laag (-10 tot +10) = UIT, hoog (>50) = AAN
        playbackVibeOn = (vibrationLevel > 50.0f);
        
        // Parse suction level (kolom 9)
        String suctionStr = line.substring(commaIndex[7] + 1, commaIndex[8]);
        suctionStr.trim();
        float suctionLevel = suctionStr.toFloat();
        playbackZuigenOn = (suctionLevel > 0.5f);
        
        // DEBUG: Laat zien wat we hebben gelezen (threshold 50.0 voor vibe sensor)
        Serial.printf("[PLAYBACK DEBUG] vibrationStr='%s' (%.2f) -> %s, suctionStr='%s' (%.2f) -> %s\n", 
                      vibrationStr.c_str(), vibrationLevel, playbackVibeOn ? "ON" : "OFF",
                      suctionStr.c_str(), suctionLevel, playbackZuigenOn ? "ON" : "OFF");
      } else {
        stressLevel = 3;  // Default voor incomplete CSV
      }
    } else if (filename.endsWith(".aly")) {
      stressLevel = parseALYStressLevel(line);
      // ALY heeft geen vibe/zuigen data, gebruik defaults
    }
    
    // Stuur stress level + vibe/zuigen data naar HoofdESP
    sendESPNowMessage(1.0f, 1.0f, false, "PLAYBACK_STRESS", stressLevel, playbackVibeOn, playbackZuigenOn);
    
    Serial.printf("[PLAYBACK] Time=%u, Stress=%d, Vibe=%s, Zuigen=%s, File=%s\n", 
                  sampleTime, stressLevel, playbackVibeOn ? "ON" : "OFF", 
                  playbackZuigenOn ? "ON" : "OFF", filename.c_str());
    
    // Update voor volgende sample
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

// ===== Sensor Functies =====
static void initSensors() {
  // GSR sensor
  pinMode(GSR_PIN, INPUT);
  
  // MCP9808 initialiseren met juiste adres
  Wire.beginTransmission(0x1F);
  if (Wire.endTransmission() == 0) {
    mcpInitialized = true;
    Serial.println("[BODY] MCP9808 gevonden!");
  } else {
    mcpInitialized = false;
    Serial.println("[BODY] MCP9808 niet gevonden");
  }
  
  // ESP-NOW communicatie initialiseren
  espNowInitialized = initESPNow();
  if (espNowInitialized) {
    Serial.println("[BODY] ESP-NOW gereed!");
  } else {
    Serial.println("[BODY] ESP-NOW fout!");
  }
}

static void readGSR() {
  int rawValue = analogRead(GSR_PIN);
  gsrValue = (float)rawValue;
  
  // Toepassen van baseline en sensitivity
  float calibratedValue = (gsrValue - sensorConfig.gsrBaseline) * sensorConfig.gsrSensitivity;
  
  // Smoothing met configureerbare factor
  float smoothFactor = sensorConfig.gsrSmoothing;  // 0.0 tot 1.0
  gsrSmooth = gsrSmooth * (1.0f - smoothFactor) + calibratedValue * smoothFactor;
  
  // Zorg dat waarde positief blijft
  if (gsrSmooth < 0) gsrSmooth = 0;
}

static void readMCP9808() {
  if (!mcpInitialized) return;
  
  Wire.beginTransmission(0x1F);
  Wire.write(0x05);  // Temperature register
  if (Wire.endTransmission(false) != 0) return;
  
  if (Wire.requestFrom(0x1F, 2) == 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    
    int16_t temp = ((msb & 0x1F) << 8) | lsb;
    if (msb & 0x10) temp -= 8192;
    
    tempValue = temp * 0.0625f + sensorConfig.tempOffset;
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
  Serial.println("[BODY] Starting Body ESP with HoofdESP styling...");
  
  rgbOffInit();

  // Initialize body display system
  body_gfx->begin();
  body_gfx->setRotation(DISPLAY_ROTATION);
  body_gfx->fillScreen(BODY_CFG.COL_BG);
  
  // Initialize body graphics system with HoofdESP styling
  body_gfx4_begin();
  
  // Set touch cooldowns: 3 seconds for main screen, 2 seconds for menu
  body_gfx4_setCooldown(3000, 2000);
  
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  bool ok = max30.begin(Wire, I2C_SPEED_STANDARD, MAX_I2C_ADDR) ||
            max30.begin(Wire, I2C_SPEED_STANDARD);
  if (!ok) {
    body_gfx->setCursor(20, 20);
    body_gfx->setTextColor(0xF800, 0x0000); // Red on black
    body_gfx->print("MAX30102 niet gevonden!");
    while (1) delay(1000);
  }
  max30.setup();
  
  // Load sensor settings
  loadSensorConfig();
  Serial.println("[BODY] Sensor configuratie geladen");
  
  // Configure MAX30102
  max30.setPulseAmplitudeIR(sensorConfig.hrLedPower);
  max30.setPulseAmplitudeRed(sensorConfig.hrLedPower);
  max30.setPulseAmplitudeGreen(0);
  max30.setLEDMode(2);  // Red + IR mode
  max30.setSampleRate(100);  // 100 Hz
  max30.setPulseWidth(411);  // 18 bit
  max30.setADCRange(4096);   // 15-bit ADC
  max30.enableDIETEMPRDY();
  
  Serial.printf("[BODY] MAX30102 configured - LED Power: %d\n", sensorConfig.hrLedPower);
  
  // Initialize touch with new display
  inputTouchBegin(body_gfx);
  
  // Initialize SD card
  initSDCard();
  
  // Initialize event logging
  initEventLogging();
  
  // Initialize sensors
  initSensors();
  
  // Initialize ML Stress Analyzer
  Serial.println("[BODY] Initializing ML Stress Analyzer...");
  if (ml_begin()) {
    Serial.println("[BODY] ML Stress Analyzer initialized successfully");
  } else {
    Serial.println("[BODY] ML Stress Analyzer initialization failed, continuing without ML");
  }
  
  // TIJDELIJK UITGESCHAKELD - S3 upgrade
  // Initialize MultiFunPlayer client
  // Serial.println("[BODY] Initializing MultiFunPlayer integration...");
  // setupMultiFunPlayer();
  
  // Reset alle AI status bij opstart
  aiTestModeActive = false;
  aiStressModeActive = false;
  aiControlActive = false;
  userPausedAI = false;
  redBackgroundActive = false;
  Serial.println("[BODY] AI systemen gereset - wachten op manuele activatie");
  
  // Enter main mode
  enterMain();
  
  Serial.println("[BODY] Initialization complete - Body monitoring active");
}

// AI Stress Management Control Function (Test 2)
void handleAIStressManagement() {
  if (!aiStressModeActive) return;
  
  uint32_t now = millis();
  
  // Update Stress Test (UITGESCHAKELD - gebruik seriële interface)
  // stressTest();
  
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
        Serial.println("[AI STRESS] Vibe & Vacuum ON commando's gestuurd naar HoofdESP");
        
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
          Serial.println("[AI STRESS] Vacuum/Vibe OFF commando's gestuurd naar HoofdESP");
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
          Serial.println("[AI STRESS] Vacuum ON commando gestuurd naar HoofdESP");
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
    Serial.printf("[AI STRESS] Level: %d, Speed: geschat, Vacuum: %s, Vibe: %s\n",
                  currentStressLevel, aiVacuumControl ? "AAN" : "UIT", aiVibeControl ? "AAN" : "UIT");
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
          
          // Trigger immediate AI response
          static uint32_t lastStressForce = 0;
          uint32_t now = millis();
          if (now - lastStressForce > 1000) {  // Debounce 1 seconde
            Serial.printf("[SERIAL DEBUG] AI reageert op stress level %d...\n", stressLevel);
            
            // Simuleer exacte stress response logica (uit handleAIStressManagement)
            if (stressLevel == 7) {
              // NOODGEVAL: Maximale interventie
              currentSpeedStep = 7;  // Hoogste versnelling
              aiVibeControl = true;  // Vibe AAN
              aiVacuumControl = true; // Auto vacuum AAN
              
              // Stuur speciale commando's om vibe en vacuum aan te zetten op HoofdESP
              float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
              sendESPNowMessage(newTrust, 1.0f, true, "AI_VIBE_ON");
              delay(50); // Korte delay tussen commando's
              sendESPNowMessage(newTrust, 1.0f, true, "AI_VACUUM_ON");
              
              Serial.println("[SERIAL DEBUG] NOODGEVAL! Niveau 7 - Max speed + Vibe + Vacuum");
              Serial.println("[SERIAL DEBUG] Vibe & Vacuum commando's gestuurd naar HoofdESP");
              Serial.println("[SERIAL DEBUG] TIP: Bij stress 7 stopt C-knop de hele AI!");
              
            } else if (stressLevel == 6) {
              // Hoge stress: drastisch verlagen
              if (currentSpeedStep > 1) {
                currentSpeedStep = 1;  // Naar speed step 1
                aiVacuumControl = false; // Auto vacuum UIT
                aiVibeControl = false;   // Vibe UIT
                
                // Stuur commando's naar HoofdESP
                float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
                sendESPNowMessage(newTrust, 1.0f, true, "AI_VACUUM_OFF");
                delay(50); // Korte delay tussen commando's
                sendESPNowMessage(newTrust, 1.0f, true, "AI_VIBE_OFF");
                
                Serial.println("[SERIAL DEBUG] Hoge stress (6) - Verlagen naar speed 1, vacuum UIT");
                Serial.println("[SERIAL DEBUG] Vacuum/Vibe OFF commando's gestuurd naar HoofdESP");
              } else if (currentSpeedStep == 1) {
                currentSpeedStep = 0;  // Naar langzaamste
                Serial.println("[SERIAL DEBUG] Stress blijft hoog - Naar langzaamste (0)");
              }
              
            } else if (stressLevel == 5) {
              // Stress daalt: vacuum weer aan
              if (!aiVacuumControl) {
                aiVacuumControl = true;
                
                // Stuur commando naar HoofdESP
                float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
                sendESPNowMessage(newTrust, 1.0f, true, "AI_VACUUM_ON");
                
                Serial.println("[SERIAL DEBUG] Stress daalt (5) - Auto vacuum weer AAN");
                Serial.println("[SERIAL DEBUG] Vacuum ON commando gestuurd naar HoofdESP");
              }
              
            } else if (stressLevel <= 3) {
              // Lage stress: speed weer verhogen
              if (currentSpeedStep < aiStartSpeedStep) {
                currentSpeedStep++;
                Serial.printf("[SERIAL DEBUG] Stress gedaald (%d) - Speed verhogen naar %d\n", stressLevel, currentSpeedStep);
              }
            }
            
            // Stuur nieuwe instellingen naar HoofdESP (behalve bij level 4 - geen actie)
            if (stressLevel != 4) {
              float newTrust = (currentSpeedStep / 7.0f) * 2.0f;
              sendESPNowMessage(newTrust, 1.0f, true, "AI_STRESS_MANUAL");
            } else {
              Serial.printf("[SERIAL DEBUG] Medium stress (%d) - Geen actie\n", stressLevel);
            }
            
            lastStressForce = now;
          }
          
        } else {
          Serial.printf("[SERIAL DEBUG] ERROR: AI Stress Management is niet actief! Start eerst AI.\n");
        }
        
      } else {
        Serial.printf("[SERIAL DEBUG] ERROR: Ongeldig stress level '%s'. Gebruik 1-7.\n", input.substring(7).c_str());
      }
      
    } else if (input == "ml") {
      // ML Stress Analysis commando
      if (mlAnalyzer.isReady()) {
        StressAnalysis result = mlAnalyzer.analyzeStress();
        Serial.printf("[ML ANALYSE] Stress Level: %d (%.1f%% confidence)\n", 
                      result.stressLevel, result.confidence * 100);
        Serial.printf("[ML ANALYSE] Reasoning: %s\n", result.reasoning.c_str());
        Serial.printf("[ML ANALYSE] Processing time: %d ms\n", result.processingTime);
        
        // Automatisch toepassen van ML resultaat
        if (aiStressModeActive && result.confidence > 0.7f) {
          currentStressLevel = result.stressLevel;
          Serial.printf("[ML AUTO] ML stress level %d automatisch toegepast\n", result.stressLevel);
        }
      } else {
        Serial.println("[ML ANALYSE] Onvoldoende sensor data - wacht even...");
      }
      
    } else if (input == "mlinfo") {
      // ML System Info
      Serial.println("[ML INFO] " + mlAnalyzer.getModelInfo());
      Serial.printf("[ML INFO] Ready: %s\n", mlAnalyzer.isReady() ? "JA" : "NEE");
      Serial.printf("[ML INFO] Predictions: %d, Avg time: %.1fms\n", 
                    mlAnalyzer.getPredictionCount(), mlAnalyzer.getAverageProcessingTime());
      
    } else if (input == "mlfeatures") {
      // Show extracted features
      if (mlAnalyzer.isReady()) {
        mlAnalyzer.printFeatures();
      } else {
        Serial.println("[ML DEBUG] Onvoldoende data voor feature extractie");
      }
      
    } else if (input == "mlbuffer") {
      // Show sensor buffer
      mlAnalyzer.printBuffer();
      
    } else if (input == "help") {
      Serial.println("[SERIAL DEBUG] Beschikbare commando's:");
      Serial.println("  stress X    - Stel stress level in (X = 1-7)");
      Serial.println("  ml          - ML stress analyse uitvoeren");
      Serial.println("  mlinfo      - ML systeem informatie");
      Serial.println("  mlfeatures  - Toon extracted features");
      Serial.println("  mlbuffer    - Toon sensor data buffer");
      Serial.println("  help        - Toon deze help");
      Serial.println("");
      Serial.println("[SERIAL DEBUG] Stress levels:");
      Serial.println("  1-3: Lage stress -> Speed verhogen");
      Serial.println("  4:   Medium stress -> Geen actie");
      Serial.println("  5:   Stress daalt -> Vacuum weer aan");
      Serial.println("  6:   Hoge stress -> Speed naar 1, vacuum uit");
      Serial.println("  7:   NOODGEVAL -> Max speed + vibe + vacuum");
      Serial.println("       (Bij stress 7 stopt C-knop de hele AI!)");
      
    } else if (input.length() > 0) {
      Serial.printf("[SERIAL DEBUG] Onbekend commando: '%s'. Type 'help' voor hulp.\n", input.c_str());
    }
  }
}

// Universele AI Pause Detectie (voor alle AI modes)
void handleAIPauseDetection() {
  // Skip als er geen AI actief is
  if (!aiStressModeActive && !aiTestModeActive) return;
  
  // DEBUG: Pauze conditie check (ook voor continue monitoring)
  static bool lastPauseState = false;
  static uint32_t lastPauseDebug = 0;
  if (pauseActive != lastPauseState) {
    Serial.printf("[DEBUG PAUZE] pauseActive veranderd: %s -> %s\n", 
                  lastPauseState ? "JA" : "NEE", pauseActive ? "JA" : "NEE");
    Serial.printf("[DEBUG PAUZE] Conditie check: aiControlActive=%s, userPausedAI=%s\n",
                  aiControlActive ? "JA" : "NEE", userPausedAI ? "JA" : "NEE");
    lastPauseState = pauseActive;
    lastPauseDebug = millis();
  }
  
  // Extra debug elke 5 seconden als pauseActive blijft true
  if (pauseActive && (millis() - lastPauseDebug > 5000)) {
    Serial.printf("[DEBUG PAUZE CONTINUE] pauseActive=JA, aiControl=%s, userPaused=%s\n",
                  aiControlActive ? "JA" : "NEE", userPausedAI ? "JA" : "NEE");
    lastPauseDebug = millis();
  }
  
  // Detect user pause (C-knop van HoofdESP) - gedrag afhankelijk van AI modus
  static bool lastPauseCheck = false;
  bool currentPauseCheck = (aiControlActive && pauseActive && !userPausedAI);
  if (currentPauseCheck != lastPauseCheck) {
    Serial.printf("[DEBUG PAUSE LOGICA] Conditie veranderd naar %s (aiControl:%s, pause:%s, !userPaused:%s)\n",
                  currentPauseCheck ? "WAAR" : "ONWAAR", 
                  aiControlActive ? "JA" : "NEE", 
                  pauseActive ? "JA" : "NEE", 
                  !userPausedAI ? "JA" : "NEE");
    lastPauseCheck = currentPauseCheck;
  }
  
  if (aiControlActive && pauseActive && !userPausedAI) {
    Serial.println("[DEBUG PAUSE LOGICA] ALLE VOORWAARDEN VOLDAAN - Pause logica wordt uitgevoerd!");
    
    if (aiStressModeActive && currentStressLevel == 7) {
      // STRESS LEVEL 7: C-knop schakelt AI volledig uit + emergency safe mode
      Serial.println("[AI STRESS] C-knop EMERGENCY OVERRIDE bij stress level 7!");
      Serial.println("[AI STRESS] AI volledig uit → Safe mode: animatie pauze, min snelheid, vibe uit, zuigen uit");
      
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
      Serial.printf("[AI STRESS] User pause bij stress level %d - rode achtergrond ON (test 1 gedrag)\n", currentStressLevel);
      
    } else {
      // Test 1 modus: rode scherm gedrag
      userPausedAI = true;
      aiControlActive = false;
      redBackgroundActive = true;
      Serial.println("[AI LEER] User pause detected - GEREGISTREERD, rode achtergrond ON");
    }
  }
  
  // Resume logica is verplaatst naar directe touch handler voor betere responsiviteit
}

// AI Test Control Function
void handleAITestControl() {
  if (!aiTestModeActive) return;
  
  uint32_t now = millis();
  
  // Start AI control
  if (!aiControlActive && !userPausedAI) {
    aiControlActive = true;
    Serial.println("[AI TEST] AI neemt controle - Versnelling 6 + Vibe ON");
    
    // Send AI commands to HoofdESP
    sendESPNowMessage(aiTargetTrust, 1.0f, true, "AI_TEST_START");
  }
  
  // AI LEER MODUS: registreer user acties maar DOE NIKS
  if (aiControlActive && !userPausedAI && (now - lastAITestUpdate > 1000)) {
    // Check vibe changes
    static bool lastVibeState = false;
    if (vibeOn != lastVibeState) {
      Serial.printf("[AI LEER] User toggled VIBE to %s - GEREGISTREERD (geen actie)\n", vibeOn ? "ON" : "OFF");
      // AI leert: vibe toggle gedetecteerd, maar doet niks
      lastVibeState = vibeOn;
    }
    
    // Check zuig changes  
    static bool lastZuigState = false;
    if (zuigActive != lastZuigState) {
      Serial.printf("[AI LEER] User toggled ZUIGEN to %s - GEREGISTREERD (geen actie)\n", zuigActive ? "ACTIEF" : "UIT");
      // AI leert: zuig toggle gedetecteerd, maar doet niks
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
      body_gfx->setCursor((320 - w) / 2, 80);
      body_gfx->print(pauseMsg);
      
      body_gfx->setTextSize(1);
      String touchMsg = "Raak scherm aan om door te gaan";
      body_gfx->getTextBounds(touchMsg, 0, 0, &x, &y, &w, &h);
      body_gfx->setCursor((320 - w) / 2, 120);
      body_gfx->print(touchMsg);
      
      // Check voor ANY touch input om door te gaan (rode scherm = volledig scherm touch)
      int16_t tx, ty;
      if (inputTouchRead(tx, ty)) {
        Serial.printf("[RODE SCHERM] Touch detected at (%d,%d)\n", tx, ty);
        redBackgroundActive = false;
        
        // Direct AI resume handling - niet wachten op volgende loop
        if (userPausedAI) {
          userPausedAI = false;
          aiControlActive = true;
          
          if (aiStressModeActive) {
            // Stress management: herstart vanaf speed 0 met stress monitoring
            float resumeTrust = 0.1f;  // Allerlaagste versnelling
            sendESPNowMessage(resumeTrust, 1.0f, true, "AI_STRESS_RESUME");
            Serial.printf("[AI STRESS] Touch resume bij stress level %d - herstart langzaam\n", currentStressLevel);
          } else if (aiTestModeActive) {
            // Test 1 gedrag: herstart langzaam
            aiTargetTrust = 0.1f;
            sendESPNowMessage(aiTargetTrust, 1.0f, true, "AI_RESUME_SLOW");
            Serial.println("[AI LEER] Touch resume GEREGISTREERD - AI herstart allerlaagste (trustSpeed 0.1)");
          } else {
            // Fallback: normale resume
            sendESPNowMessage(1.0f, 1.0f, false, "HEARTBEAT");
            Serial.println("[BODY] Touch detected op rode scherm - resuming normal operation");
          }
        } else {
          // Geen AI resume nodig
          sendESPNowMessage(1.0f, 1.0f, false, "HEARTBEAT");
          Serial.println("[BODY] Touch detected op rode scherm - resuming normal operation");
        }
        
        // Terug naar normale display
        enterMain();
      }
      
      delay(50);  // Korte delay voor touch responsiveness
      return;
    }
    // Lees alle sensoren
    long ir = max30.getIR();
    readGSR();
    readMCP9808();
    readESP32Comm();

    // MAX30102 hartslag processing
    float x = (float)ir;
    float hp_y = HP_A * (hp_y_prev + x - x_prev);
    hp_y_prev = hp_y; x_prev = x;
    float bp_y = bp_y_prev + LP_A * (hp_y - bp_y_prev);
    bp_y_prev = bp_y;

    float currAbs = fabsf(bp_y);
    envAbs = fmaxf(envAbs * 0.98f, currAbs);
    float targetDiv = fmaxf(150.0f, envAbs / 0.90f);
    acDiv = acDiv*0.9f + targetDiv*0.1f;

    // Beat detection
    bool beat = (ir > sensorConfig.hrThreshold) && checkForBeat(ir);
    if (beat) {
      uint32_t now = millis();
      uint32_t dt = now - lastBeatMs;
      lastBeatMs = now;
      if (dt > 300 && dt < 2000) BPM = (uint16_t)(60000UL / dt);
    }

    // Use real sensor data with fallback
    float tempVal = mcpInitialized ? tempValue : (36.5f + 0.5f * sin(dummyPhase/3));
    float huidVal = gsrSmooth;
    
    // Feed sensor data to ML analyzer (every 100ms = 10Hz)
    static uint32_t lastMLUpdate = 0;
    if (millis() - lastMLUpdate >= 100) {
      mlAnalyzer.addSensorSample((float)BPM, tempVal, huidVal);
      lastMLUpdate = millis();
    }
    
    // Dummy oxygen
    dummyPhase += 0.1f;
    if (dummyPhase > TWO_PI) dummyPhase -= TWO_PI;
    float oxyVal = 95 + 2 * sin(dummyPhase/7);
    
    // Dummy ademhaling (flex-sensor simulatie)
    float ademhalingVal = 50 + 30 * sin(dummyPhase/4);  // Langzamere ademhaling

    // Draw samples naar scherm met HoofdESP styling - nieuwe volgorde
    body_gfx4_pushSample(G4_HART, bp_y, beat);
    body_gfx4_pushSample(G4_HUID, huidVal);
    body_gfx4_pushSample(G4_TEMP, tempVal);
    body_gfx4_pushSample(G4_ADEMHALING, ademhalingVal);
    body_gfx4_pushSample(G4_OXY, oxyVal);
    
    // SNELH: Simpele trust speed animatie met lube reset
    float snelheidVal;
    float currentAnimationPhase = 0.0f;
    
    // Animatie gebaseerd op tijd sinds lube trigger + trust speed
    static uint32_t animStartTime = 0;
    
    // Reset bij lube trigger - ALLEEN bij lage snelheid
    if (lubeTrigger) {
      float speed = max(0.1f, cyclusTijd);  // cyclusTijd = trustSpeed
      if (speed < 1.0f) {  // Alleen reset bij lage snelheid
        animStartTime = millis();
        Serial.println("[SNELH] Reset door lube trigger (lage snelheid)");
      } else {
        Serial.printf("[SNELH] Lube trigger genegeerd bij snelheid %.1f\n", speed);
      }
    }
    
    if (pauseActive) {
      // Pauze actief
      snelheidVal = 0.0f;
      currentAnimationPhase = 0.0f;
    } else {
      // Bereken animatie gebaseerd op trust speed
      uint32_t timeSinceReset = millis() - animStartTime;
      float speed = max(0.1f, cyclusTijd);  // cyclusTijd = trustSpeed
      
      // Animatie frequentie: hogere speed = snellere golven
      float animFreq = speed * BODY_CFG.SNELH_SPEED_FACTOR;  // Direct speed naar frequentie
      float phase = (timeSinceReset / 1000.0f) * animFreq;
      phase = fmod(phase, 1.0f) * 2.0f * PI;  // Wrap naar golf cyclus
      
      // Golf van 0-100% - start bij 0% (niet 50%)
      float s = 0.5f * (sinf(phase - PI/2.0f) + 1.0f);  // Start bij 0%
      snelheidVal = s * 100.0f;  // 0-100%
      currentAnimationPhase = phase;
    }
    body_gfx4_pushSample(G4_HOOFDESP, snelheidVal);
    
    // ZUIGEN: Echte vacuum data van HoofdESP in mbar (configureerbaar bereik)
    // Schalen naar grafiek waarde: 0 mbar = baseline, maxMbar = top grafiek
    float zuigenVal;
    if (zuigActive && vacuumMbar > 0) {
      // Actieve zuiging: schaal 0-maxMbar naar 20-100 grafiek waarde
      float maxMbar = BODY_CFG.vacuumGraphMaxMbar;  // Configureerbaar maximum (standaard 10.0f)
      float scaledVacuum = min(vacuumMbar / maxMbar, 1.0f);  // Normaliseren naar 0-1
      zuigenVal = 20.0f + (scaledVacuum * 80.0f);  // 0mbar=20, maxMbar=100
    } else {
      // Geen zuiging actief: lage baseline waarde
      zuigenVal = 5.0f;
    }
    body_gfx4_pushSample(G4_ZUIGEN, zuigenVal);
    
    // VIBE: Handmatige VIBE toggle van HoofdESP Z-knop (bit 3 debug LED)
    static float vibePhase = 0;
    vibePhase += 0.1f;
    bool vibeActive = vibeOn;  // Gebruik echte VIBE signaal van HoofdESP!
    float trilVal = vibeActive ? (100 + (3 * sin(vibePhase))) : (3 * sin(vibePhase));
    body_gfx4_pushSample(G4_TRIL, trilVal);
    // Teken knoppen elke loop (omdat ze door grafieken overschreven kunnen worden)
    bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
    body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
    
    // Debug output (elke 2 seconden)
    static uint32_t lastDebugTime = 0;
    static bool dimensionsShown = false;
    if (millis() - lastDebugTime >= 2000) {
      bool effectiveAI = aiOverruleActive || aiStressModeActive || aiTestModeActive;
      Serial.printf("[BODY] HR:%.0f Temp:%.1f GSR:%.0f AI:%s\n", 
                    (float)BPM, tempVal, huidVal, effectiveAI ? "ON" : "OFF");
      Serial.printf("[DEBUG AI] aiOverrule:%s aiStress:%s aiTest:%s aiControl:%s\n",
                    aiOverruleActive ? "Y" : "N", aiStressModeActive ? "Y" : "N", 
                    aiTestModeActive ? "Y" : "N", aiControlActive ? "Y" : "N");
      if (aiOverruleActive) {
        Serial.printf("[AI] Trust:%.2f (Sleeve:%.2f UITGESCHAKELD)\n", 
                      currentTrustOverride, currentSleeveOverride);
      }
      Serial.printf("[HOOFD] Trust:%.1f Sleeve:%.1f Suction:%.1f Pause:%.1f\n",
                    trustSpeed, sleeveSpeed, suctionLevel, pauseTime);
      Serial.printf("[SNELH] trustSpeed:%.1f pauseActive:%s animPhase:%.1f sleeve:%.1f%%\n",
                    trustSpeed, pauseActive ? "JA" : "NEE", currentAnimationPhase, sleevePercentage);
      Serial.printf("[VACUUM] Zuigen:%s Vacuum:%.1f mbar (grafiek:%.1f)\n",
                    zuigActive ? "ACTIEF" : "UIT", vacuumMbar, zuigenVal);
      
      // Show grafiek dimensions once
      if (!dimensionsShown) {
        Serial.printf("[GRAFIEK] Screen: %dx%d pixels\n", body_gfx->width(), body_gfx->height());
        dimensionsShown = true;
      }
      
      lastDebugTime = millis();
    }
    
    // Handle serial input voor stress level testing
    handleSerialInput();
    
    // Update AI overrule systeem
    updateAIOverrule((float)BPM, tempVal, gsrSmooth);
    
    // Handle AI pause detection (universeel voor alle AI modes)
    handleAIPauseDetection();
    
    // Handle AI test control
    handleAITestControl();
    
    // Handle AI stress management (Test 2)
    handleAIStressManagement();
    
    // DEBUG: AI test timer (UITGESCHAKELD)
    // static bool aiTestStarted = false;
    // if (!aiTestStarted && millis() > 10000) {
    //   startAITest();
    //   aiTestStarted = true;
    // }
    
    // Send heartbeat naar HoofdESP (elke 2 seconden voor debug)
    static uint32_t lastHeartbeat = 0;
    if (millis() - lastHeartbeat >= 2000) {
      Serial.println("[BODY] Sending heartbeat to HoofdESP...");
      // Zorg dat AI status correct is voor stress management
      bool effectiveAIActive = aiOverruleActive || aiStressModeActive || aiTestModeActive;
      sendESPNowMessage(currentTrustOverride, currentSleeveOverride, effectiveAIActive, "HEARTBEAT");
      lastHeartbeat = millis();
    }
    
    // Process playback if active
    if (isPlaybackActive) {
      processPlaybackSample();
    }
    
    // TIJDELIJK UITGESCHAKELD - S3 upgrade  
    // Process MultiFunPlayer WebSocket communication
    // mfpClient.loop();
    
    // Record data if recording
    if (isRecording) {
      recordSampleExtended((float)BPM, tempVal, huidVal, oxyVal, beat, 
                          trustSpeed, sleeveSpeed, suctionLevel, pauseTime, 
                          ademhalingVal, trilVal);
    }

    // Handle touch input met cooldown
    TouchEvent ev;
    if (inputTouchPoll(ev)) {
      if (body_gfx4_canTouch(true)) {  // Main screen cooldown
        body_gfx4_registerTouch(true);
        
        if (ev.kind == TE_TAP_REC) {
          // Toggle recording
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
            Serial.println("[BODY] Playback stopped");
          } else {
            // Open playlist voor file selectie
            enterPlaylist();
            return;
          }
        }
        if (ev.kind == TE_TAP_MENU) {
          uiMenu = true;
          mode = MODE_MENU;  // Switch to menu mode
          bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
          body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
          Serial.println("[BODY] Entering menu mode");
          
          // Initialize menu system
          menu_begin(body_gfx);
          Serial.println("[BODY] Menu initialized");
          return;
        }
        if (ev.kind == TE_TAP_OVERRULE) {
          // AI knop: Universele AI toggle - kan altijd alle AI systemen uit/aan
          bool anyAIActive = aiStressModeActive || aiTestModeActive || aiConfig.enabled;
          
          if (anyAIActive) {
            // STOP alle AI systemen
            if (aiStressModeActive) {
              stopAIStressManagement();
              Serial.println("[BODY] AI Stress Management gestopt via AI knop!");
            }
            if (aiTestModeActive) {
              stopAITest();
              Serial.println("[BODY] AI Test gestopt via AI knop!");
            }
            if (aiConfig.enabled) {
              aiConfig.enabled = false;
              aiOverruleActive = false;
              Serial.println("[BODY] AI Overrule uitgeschakeld via AI knop!");
            }
            Serial.println("[BODY] ==> ALLE AI SYSTEMEN UITGESCHAKELD");
            
          } else {
            // START AI stress management (als niks actief is)
            startAIStressManagement();
            Serial.println("[BODY] AI Stress Management gestart via AI knop!");
          }
          
          bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
          body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
          return;
        }
        // Update button display om AI stress status te tonen
        bool aiActive = aiConfig.enabled || aiStressModeActive || aiTestModeActive;
        body_gfx4_drawButtons(isRecording, isPlaying, uiMenu, aiActive);
        updateStatusLabel();
      }
    }

    delay(20);
    return;
  }

  // Menu mode - HoofdESP style menu  
  if (mode == MODE_MENU){
    MenuEvent mev = menu_poll();
    if (mev == ME_NONE) { delay(10); return; }
    if (mev == ME_BACK) { uiMenu = false; enterMain(); return; }
    if (mev == ME_SENSOR_SETTINGS){
      enterSensorSettings();
      return;
    }
    if (mev == ME_OVERRULE){
      enterOverrule();
      return;
    }
    if (mev == ME_SYSTEM_SETTINGS){
      enterSystemSettings();
      return;
    }
    if (mev == ME_PLAYLIST){
      enterPlaylist();
      return;
    }
    if (mev == ME_ML_TRAINING){
      enterMLTraining();
      return;
    }
  }

  if (mode == MODE_PLAYLIST){
    PlaylistEvent pev = playlist_poll();
    if (pev == PE_NONE) { delay(10); return; }
    if (pev == PE_BACK) { enterMain(); return; }
    if (pev == PE_PLAY_FILE) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        startPlayback(filename);
      }
      return;
    }
    if (pev == PE_DELETE_CONFIRM) {
      String filename = playlist_getSelectedFile();
      if (filename.length() > 0) {
        showDeleteConfirm(filename);
      }
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
      Serial.println("[BODY] PE_AI_ANALYZE event triggered");
      String filename = playlist_getSelectedFile();
      Serial.printf("[BODY] Got filename from playlist: '%s'\n", filename.c_str());
      
      if (filename.length() > 0) {
        Serial.println("[BODY] Filename is valid, setting in AI Analyze...");
        
        // Set selected file in AI Analyze en start
        aiAnalyze_setSelectedFile(filename);
        Serial.println("[BODY] File set successfully, entering AI Analyze...");
        enterAIAnalyze();
        Serial.println("[BODY] AI Analyze entered successfully");
      } else {
        Serial.println("[BODY] ERROR: No filename selected!");
        body_gfx4_drawStatus("Geen bestand geselecteerd!");
        delay(2000);
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
    if (sse == SSE_COLORS) {
      enterColors();
      return;
    }
    if (sse == SSE_FORMAT_SD) {
      showFormatConfirm();
      return;
    }
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
    if (oev == OE_ANALYZE) {
      enterAIAnalyze();
      return;
    }
    if (oev == OE_CONFIG) {
      enterAIEventConfig();
      return;
    }
  }
  
  if (mode == MODE_AI_ANALYZE) {
    AnalyzeEvent aev = aiAnalyze_poll();
    if (aev == AE_NONE) { delay(10); return; }
    if (aev == AE_BACK) { enterOverrule(); return; }
    if (aev == AE_TRAIN) {
      String filename = aiAnalyze_getSelectedFile();
      if (filename.length() > 0) {
        enterAITraining(filename);
      }
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
      // Kleur is gewijzigd, voorlopig alleen feedback
      body_gfx4_drawStatus("Kleur gewijzigd!");
      delay(1000);
      return;
    }
  }
  
  if (mode == MODE_AI_TRAINING) {
    // Check if we're using the old AI Training system (12 feedback buttons)
    // This is determined by checking if currentFile is set in ai_training_view.cpp
    
    // Try AI Training system first (12 feedback buttons for .csv labeling)
    TrainingResult tr = aiTraining_poll();
    if (tr != TR_NONE) {
      if (tr == TR_BACK) {
        enterOverrule(); // AI Training goes back to AI Analyze
        return;
      }
      if (tr == TR_COMPLETE) {
        // Training completed - could save .aly file or return to analyze
        enterOverrule();
        return;
      }
      return;
    }
    
    // If AI Training returns TR_NONE, try ML Training system
    MLTrainingEvent mte = mlTraining_poll();
    if (mte == MTE_NONE) { delay(10); return; }
    if (mte == MTE_BACK) { enterMenu(); return; }
    
    // Handle ML Training state transitions
    if (mte == MTE_IMPORT_DATA) {
      mlTraining_setState(ML_STATE_IMPORT);
      return;
    }
    if (mte == MTE_TRAIN_MODEL) {
      mlTraining_setState(ML_STATE_TRAINING);
      return;
    }
    if (mte == MTE_MODEL_MANAGER) {
      mlTraining_setState(ML_STATE_MODEL_MANAGER);
      return;
    }
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


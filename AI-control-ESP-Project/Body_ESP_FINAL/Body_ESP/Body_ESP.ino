// TEST 123

/*
  SC01 Plus Touch Test - FT6336U Calibratie
  
  Test touch controller met visual feedback
  - Rood scherm = niet geïnitialiseerd
  - Groen scherm = touch controller OK
  - Teken witte pixels waar je aanraakt
  - Serial output met touch coordinaten
*/

// ========== TOUCH CALIBRATIE INSTELLINGEN ==========
// METHODE 1: Gebruik rotatie (makkelijk)
// 0 = 0°   (portrait)
// 1 = 90°  (landscape, USB rechts)
// 2 = 180° (portrait, ondersteboven)
// 3 = 270° (landscape, USB links)
#define TOUCH_ROTATION  0   // 0, 1, 2, of 3

// METHODE 2: Handmatige controle (geavanceerd)
// Zet USE_MANUAL_MAPPING op 1 om rotatie te negeren en alles zelf in te stellen
#define USE_MANUAL_MAPPING  1   // 0 = gebruik TOUCH_ROTATION, 1 = gebruik onderstaande

// Als USE_MANUAL_MAPPING = 1, pas deze aan:
#define MANUAL_SWAP_XY   1   // 0 of 1 - wissel X en Y
#define MANUAL_FLIP_X    0   // 0 of 1 - spiegel X (horizontaal)
#define MANUAL_FLIP_Y    1   // 0 of 1 - spiegel Y (verticaal)
// ====================================================

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <FT6X36.h>
#include <Adafruit_ADS1X15.h>  // ADS1115 sensor library
#include <RTClib.h>             // RTC DS3231 library
#include <SD_MMC.h>             // SD card voor CSV recording (SD_MMC mode)
#include <esp_now.h>            // ESP-NOW communicatie
#include <WiFi.h>               // WiFi voor ESP-NOW
#include "esp_task_wdt.h"       // 🔥 NIEUW: Watchdog timer
#include "config.h"             // SC01 Plus configuratie (eenvoudig)
#include "body_config.h"        // Body ESP configuratie (geavanceerd)
#include "body_gfx4.h"          // Grafisch systeem
#include "body_fonts.h"         // Font configuratie
#include "ads1115_sensors.h"    // ADS1115 sensor processing
#include "body_menu.h"          // Menu systeem
#include "sensor_settings.h"    // Sensor kalibratie (EEPROM)
#include "multifunplayer_client.h"  // MultiFunPlayer WebSocket client

// 🔥 NIEUW: Extern reference naar rendering pause flag
extern volatile bool g4_pauseRendering;

// ===== Screen dimensions voor SC01 Plus =====
const int SCR_W = 480;
const int SCR_H = 320;

// ===== SC01 Plus Display Pins =====
// Display wordt nu gedefinieerd in body_display.cpp
#define GFX_BL 45  // Backlight pin
// 
// // 8-bit parallel bus
// Arduino_DataBus *bus = new Arduino_ESP32LCD8(
//     0,   // DC
//     GFX_NOT_DEFINED,  // CS
//     47,  // WR
//     GFX_NOT_DEFINED,  // RD
//     9,   // D0
//     46,  // D1
//     3,   // D2
//     8,   // D3
//     18,  // D4
//     17,  // D5
//     16,  // D6
//     15   // D7
// );
// 
// // ST7796 display driver (480x320)
// Arduino_GFX *gfx = new Arduino_ST7796(
//     bus, 
//     4,    // RST
//     1,    // rotation (landscape)
//     true  // IPS
// );

// Gebruik extern display van body_display.cpp
extern Arduino_GFX *body_gfx;
#define gfx body_gfx  // Alias voor test code

// ===== Touch Controller =====
#define TOUCH_SDA 6
#define TOUCH_SCL 5
#define TOUCH_INT -1  // Polling mode

FT6X36 *ts = nullptr;

// ===== ADS1115 Sensor (Wire1 - GPIO 10/11) =====
#define SENSOR_SDA 10
#define SENSOR_SCL 11
// Adafruit_ADS1115 ads;  // VERWIJDERD - gedefinieerd in ads1115_sensors.cpp
extern Adafruit_ADS1115 ads;  // Extern reference
bool adsAvailable = false;

// ===== RTC DS3231 (Wire1 - I2C address 0x68) =====
RTC_DS3231 rtc;
bool rtcAvailable = false;

// ===== I2C EEPROM (Wire1 - AT24C02 256 bytes) =====
// A0=A1=A2=GND → adres 0x50
bool eepromAvailable = false;
#define EEPROM_ADDR 0x50

// ===== ESP-NOW Communicatie =====
// MAC adressen van het netwerk
static uint8_t hoofdESP_MAC[] = {0xE4, 0x65, 0xB8, 0x7A, 0x85, 0xE4};  // HoofdESP
static uint8_t bodyESP_MAC[] = {0xE8, 0x06, 0x90, 0xDD, 0x7E, 0x18};   // Body ESP SC01 Plus (deze unit)

// ESP-NOW data variabelen
static uint32_t lastCommTime = 0;
static float trustSpeed = 0.0f, sleeveSpeed = 0.0f, suctionLevel = 0.0f;
static bool vibeOn = false;  // Handmatige VIBE toggle van HoofdESP
static bool zuigActive = false;  // Status zuigen modus actief
static float vacuumMbar = 0.0f;  // Vacuum waarde in mbar (0-10 bereik)
static bool pauseActive = false;  // C knop pauze status
static bool lubeTrigger = false;  // Lube cyclus start trigger
static float cyclusTijd = 10.0f;  // Cyclus duur in seconden
static uint32_t lastLubeTriggerTime = 0;  // Tijd van laatste lube trigger
static float sleevePercentage = 0.0f;  // Echte sleeve positie percentage (0-100)
static uint8_t hoofdESPSpeedStep = 3;  // Laatst ontvangen speed step van HoofdESP
static bool espNowInitialized = false;

// 🔥 NIEUW: ESP-NOW Retry Queue
//#define ESPNOW_QUEUE_SIZE 5
//#define MAX_RETRIES 3
//#define RETRY_DELAY_MS 100

//struct ESPNowQueueItem {
  //esp_now_send_message_t message;
  //uint32_t timestamp;
  //uint8_t retryCount;
  //bool inUse;
//};

//static ESPNowQueueItem espNowQueue[ESPNOW_QUEUE_SIZE];
//static int queueHead = 0;  // Volgende te verzenden
//static int queueTail = 0;  // Volgende vrije slot
//static int queueCount = 0; // Aantal berichten in queue

// ESP-NOW ontvangst bericht structuur (van HoofdESP)
typedef struct __attribute__((packed)) {
  float trust;            // Trust speed (0.0-2.0)
  float sleeve;           // Sleeve speed (0.0-2.0)  
  float suction;          // Suction level (0.0-100.0)
  float pause;            // Pause tijd (0.0-10.0)
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

// ESP-NOW verzend bericht structuur (naar HoofdESP)
typedef struct __attribute__((packed)) {
  float newTrust;         // AI berekende trust override (0.0-1.0)
  float newSleeve;        // AI berekende sleeve override (0.0-1.0)
  bool overruleActive;    // AI overrule status
  uint8_t stressLevel;    // Stress level 1-7 voor playback/AI
  bool vibeOn;            // Vibe status voor playback
  bool zuigenOn;          // Zuigen status voor playback
  char command[32];       // "AI_OVERRIDE", "EMERGENCY_STOP", "HEARTBEAT", "PLAYBACK_STRESS"
} esp_now_send_message_t;

// 🔥 NIEUW: ESP-NOW Retry Queue (NA typedef!)
#define ESPNOW_QUEUE_SIZE 5
#define MAX_RETRIES 3
#define RETRY_DELAY_MS 100

struct ESPNowQueueItem {
  esp_now_send_message_t message;
  uint32_t timestamp;
  uint8_t retryCount;
  bool inUse;
};

static ESPNowQueueItem espNowQueue[ESPNOW_QUEUE_SIZE];
static int queueHead = 0;
static int queueTail = 0;
static int queueCount = 0;


// ===== Callback variabelen =====
volatile bool touchDetected = false;
volatile uint16_t touchX = 0;
volatile uint16_t touchY = 0;

// ===== Kleur test knoppen - UITGESCHAKELD (vervangen door body_gfx4) =====
// struct ColorButton {
//   int x, y, w, h;
//   uint16_t targetColor;
//   uint16_t currentColor;
//   const char* label;
// };
// 
// ColorButton buttons[4] = {
//   {20, 30, 100, 70, 0xF800, 0xFFFF, "ROOD"},     // Links boven
//   {360, 50, 100, 70, 0x07E0, 0xFFFF, "GROEN"},   // Rechts boven
//   {50, 230, 100, 70, 0x001F, 0xFFFF, "BLAUW"},   // Links onder
//   {340, 220, 120, 80, 0xFFE0, 0xFFFF, "GEEL"}    // Rechts onder (groter)
// };
// 
// // Functie om knoppen te tekenen
// void drawButtons() {
//   for (int i = 0; i < 4; i++) {
//     // Knop rechthoek
//     gfx->fillRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, buttons[i].currentColor);
//     gfx->drawRoundRect(buttons[i].x, buttons[i].y, buttons[i].w, buttons[i].h, 8, 0x0000);  // Zwarte rand
//     
//     // Label tekst (in target kleur)
//     gfx->setTextSize(2);
//     gfx->setTextColor(buttons[i].targetColor, buttons[i].currentColor);
//     
//     // Center text - horizontaal en verticaal
//     int16_t x1, y1; uint16_t tw, th;
//     gfx->getTextBounds((char*)buttons[i].label, 0, 0, &x1, &y1, &tw, &th);
//     int textX = buttons[i].x + (buttons[i].w - tw) / 2;
//     int textY = buttons[i].y + (buttons[i].h) / 2 - 6;  // text omhoog is -, text omlaag is + 12px omhoog totaal
//     gfx->setCursor(textX, textY);
//     gfx->print(buttons[i].label);
//   }
// }
// 
// // Check of touch binnen knop valt
// bool checkButtonTouch(int x, int y) {
//   for (int i = 0; i < 4; i++) {
//     if (x >= buttons[i].x && x < buttons[i].x + buttons[i].w &&
//         y >= buttons[i].y && y < buttons[i].y + buttons[i].h) {
//       // Knop ingedrukt - verander naar target kleur
//       if (buttons[i].currentColor != buttons[i].targetColor) {
//         buttons[i].currentColor = buttons[i].targetColor;
//         drawButtons();  // Herteken alle knoppen
//         Serial.printf("[BUTTON] %s pressed - changed to target color!\n", buttons[i].label);
//       }
//       return true;
//     }
//   }
//   return false;
// }

// ===== Body GFX4 Touch Handler =====
// UI states voor knoppen
bool isRecording = false;
bool isPlaying = false;
bool menuActive = false;
bool aiOverruleActive = false;

// ===== CSV Recording =====
File csvFile;
String csvFilename = "";
static uint32_t lastCSVWrite = 0;
const uint32_t CSV_WRITE_INTERVAL = 1000;  // Elke 1 seconde een regel schrijven
uint32_t recordingStartTime = 0;
uint32_t csvSampleCount = 0;

// 🔥 NIEUW: SD card write buffer (60 samples = 1 minuut bij 1 Hz)
#define SD_BUFFER_SIZE 60
static String csvBuffer[SD_BUFFER_SIZE];
static int bufferIndex = 0;
static unsigned long lastBufferFlush = 0;

// 🔥 NIEUW: Flush buffer naar SD kaart
void flushCSVBuffer() {
  if (bufferIndex == 0 || !csvFile) {
    return;  // Niks te flushen
  }
  
  Serial.printf("[CSV BUFFER] Flushing %d samples to SD...\n", bufferIndex);
  
  // Schrijf alle buffered regels in 1 keer
  for (int i = 0; i < bufferIndex; i++) {
    csvFile.println(csvBuffer[i]);
  }
  
  csvFile.flush();  // Force write naar SD
  
  Serial.printf("[CSV BUFFER] Flushed! Total samples: %d\n", csvSampleCount);
  
  // Reset buffer
  bufferIndex = 0;
  lastBufferFlush = millis();
}

void startCSVRecording() {
  if (!rtcAvailable) {
    Serial.println("[CSV] ERROR: RTC niet beschikbaar, kan geen bestand aanmaken");
    isRecording = false;
    return;
  }
  
  // Maak bestandsnaam met timestamp
  DateTime now = rtc.now();
  char filename[64];
  //sprintf(filename, "/recordings/session_%04d%02d%02d_%02d%02d%02d.csv",
          //now.year(), now.month(), now.day(),
          //now.hour(), now.minute(), now.second());
  sprintf(filename, "/recordings/%02d-%02d - %02d-%02d-%02d.csv",
        now.hour(), now.minute(),        // Tijd: HH-MM
        now.day(), now.month(), now.year() % 100);  // Datum: DD-MM-YY        
  csvFilename = String(filename);
  
  // Maak recordings directory als die niet bestaat
  if (!SD_MMC.exists("/recordings")) {
    if (SD_MMC.mkdir("/recordings")) {
      Serial.println("[CSV] Created /recordings directory");
    } else {
      Serial.println("[CSV] ERROR: Could not create /recordings directory");
      isRecording = false;
      return;
    }
  }
  
  // Open bestand voor schrijven
  csvFile = SD_MMC.open(csvFilename.c_str(), FILE_WRITE);
  if (!csvFile) {
    Serial.printf("[CSV] ERROR: Could not create file: %s\n", csvFilename.c_str());
    isRecording = false;
    return;
  }
  
  // Schrijf CSV header
  csvFile.println("Tijd_s,Timestamp,BPM,Temp_C,GSR,Trust,Sleeve,Suction,Vibe,Zuig,Vacuum_mbar,Pause,SleevePos_%,SpeedStep,AI_Override");
  csvFile.flush();
  
  recordingStartTime = millis();
  csvSampleCount = 0;
  lastCSVWrite = millis();
  
  Serial.printf("[CSV] Recording STARTED: %s\n", csvFilename.c_str());
}

void stopCSVRecording() {
  // 🔥 NIEUW: Flush buffer voordat bestand sluiten
  flushCSVBuffer();

  if (csvFile) {
    csvFile.close();
    Serial.printf("[CSV] Recording STOPPED: %s (%u samples)\n", csvFilename.c_str(), csvSampleCount);
  }
  csvFilename = "";
  csvSampleCount = 0;

  // 🔥 NIEUW: Reset buffer
  bufferIndex = 0;
}

void updateCSVRecording() {
  if (!isRecording || !csvFile) return;
  
  uint32_t now = millis();
  if (now - lastCSVWrite < CSV_WRITE_INTERVAL) return;  // Nog geen tijd voor nieuwe sample
  
  lastCSVWrite = now;
  
  // Haal sensor data op
  ADS1115_SensorData sensorData = ads1115_getData();
  
  // Bereken tijd sinds start (in seconden)
  float elapsedTime = (now - recordingStartTime) / 1000.0f;
  
  // Maak timestamp string
  if (rtcAvailable) {
    DateTime rtcNow = rtc.now();
    char timestamp[32];
    sprintf(timestamp, "%04d-%02d-%02d_%02d:%02d:%02d",
            rtcNow.year(), rtcNow.month(), rtcNow.day(),
            rtcNow.hour(), rtcNow.minute(), rtcNow.second());
    
    // 🔥 NIEUW: Schrijf naar BUFFER in plaats van direct naar SD
    char csvLine[256];
    sprintf(csvLine, "%.1f,%s,%u,%.2f,%.1f,%.2f,%.2f,%.1f,%d,%d,%.1f,%d,%.0f,%u,%d",
            elapsedTime,           // Tijd sinds start
            timestamp,             // RTC timestamp
            sensorData.BPM,        // Hartslag
            sensorData.temperature, // Temperatuur
            sensorData.gsrSmooth,  // GSR
            trustSpeed,            // Trust snelheid
            sleeveSpeed,           // Sleeve snelheid
            suctionLevel,          // Suction level
            vibeOn ? 1 : 0,        // Vibe status
            zuigActive ? 1 : 0,    // Zuig status
            vacuumMbar,            // Vacuum mbar
            pauseActive ? 1 : 0,   // Pause status
            sleevePercentage,      // Sleeve positie %
            hoofdESPSpeedStep,     // Speed step
            aiOverruleActive ? 1 : 0);  // AI override
    
    csvBuffer[bufferIndex++] = String(csvLine);
    csvSampleCount++;
    
    // 🔥 NIEUW: Flush als buffer vol (60 samples = 1 minuut)
    if (bufferIndex >= SD_BUFFER_SIZE) {
      flushCSVBuffer();
    }
    
    // Status print (elke 10 samples)
    if (csvSampleCount % 10 == 0) {
      Serial.printf("[CSV] Sample %u: BPM=%u Temp=%.1f GSR=%.0f (buffered: %d/%d)\n",
                    csvSampleCount, sensorData.BPM, sensorData.temperature, 
                    sensorData.gsrSmooth, bufferIndex, SD_BUFFER_SIZE);
    }
  }
}

// Menu mode
enum AppMode { MODE_MAIN = 0, MODE_MENU = 1 };
static AppMode currentMode = MODE_MAIN;

// Extern menu state uit body_menu.cpp
extern BodyMenuMode bodyMenuMode;
extern BodyMenuPage bodyMenuPage;
extern int bodyMenuIdx;
extern bool bodyMenuEdit;

// Touch debounce
static uint32_t lastButtonTouch = 0;

// Sensor push timing (globaal zodat we het kunnen resetten)
static uint32_t lastSensorPush = 0;

// Scherm rotatie state (1 = normaal landscape, 3 = 180° gedraaid)
static uint8_t screenRotation = 1;

// Funscript mode (Aan/Uit) - extern beschikbaar voor menu
bool funscriptEnabled = false;

// ===== ESP-NOW Callback =====
static void onESPNowReceive(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(esp_now_receive_message_t)) {
    esp_now_receive_message_t message;
    memcpy(&message, incomingData, sizeof(message));
    
    // Update machine parameters
    trustSpeed = message.trust;
    sleeveSpeed = message.sleeve;
    suctionLevel = message.suction;
    vibeOn = message.vibeOn;
    zuigActive = message.zuigActive;
    vacuumMbar = message.vacuumMbar;
    pauseActive = message.pauseActive;
    
    // Lube sync systeem
    if (message.lubeTrigger && !lubeTrigger) {
      lastLubeTriggerTime = millis();
      Serial.println("[LUBE SYNC] Nieuwe cyclus start!");
    }
    lubeTrigger = message.lubeTrigger;
    cyclusTijd = message.cyclusTijd;
    sleevePercentage = message.sleevePercentage;
    hoofdESPSpeedStep = message.currentSpeedStep;
    
    lastCommTime = millis();

    // ═══════════════════════════════════════════════════════════
    // COMMAND HANDLING - Hooft ESP Commands
    // ═══════════════════════════════════════════════════════════
    
    // Check command type
    if (strcmp(message.command, "STATUS_UPDATE") == 0) {
      // Normale status update - niks extra doen
    }
    else if (strcmp(message.command, "ORGASM_TRIGGER") == 0) {
      Serial.println("[CMD] ✅ ORGASM_TRIGGER ontvangen!");
  
      // AI stopt met overrides (geen commands naar Hooft ESP)
      // MAAR: sensors blijven lezen, CSV blijft loggen, ML blijft leren
      if (aiOverruleActive) {
        aiOverruleActive = false;
        Serial.println("[AI] Overrides PAUSED - monitoring blijft actief");
      }
  
      // TODO: Log event voor ML training
    }
    else if (strcmp(message.command, "FUNSCRIPT_ON") == 0) {
      funscriptEnabled = true;
      Serial.println("[CMD] ✅ Funscript ENABLED door Hooft ESP");
    }
    else if (strcmp(message.command, "FUNSCRIPT_OFF") == 0) {
      funscriptEnabled = false;
      Serial.println("[CMD] ✅ Funscript DISABLED door Hooft ESP");
    }
    else if (strcmp(message.command, "ORGASM_COMPLETE") == 0) {
      Serial.println("[CMD] ✅ ORGASM_COMPLETE - User drukte C knop");
      // TODO: AI berekent optimale cooldown tijd
      // TODO: Stuurt COOLDOWN_OVERRIDE naar Hooft ESP
    }  
    else if (strcmp(message.command, "COOLDOWN_COMPLETE") == 0) {
       Serial.println("[CMD] ✅ COOLDOWN_COMPLETE - hervat AI overrides");
  
       // AI hervat normale overrides
      if (!aiOverruleActive) {
        aiOverruleActive = true;
        Serial.println("[AI] Overrides RESUMED - AI mag weer ingrijpen");
      }
    }
    else {
      Serial.printf("[CMD] ⚠️ Unknown command: %s\n", message.command);
    }
    
    Serial.printf("[ESP-NOW] RX: T:%.1f S:%.1f Su:%.1f V:%d Z:%d Vac:%.1f P:%d Lube:%d Cyc:%.1fs Pos:%.0f%% Step:%d Cmd:%s\n", 
                  trustSpeed, sleeveSpeed, suctionLevel, vibeOn, zuigActive, vacuumMbar,
                  pauseActive, lubeTrigger, cyclusTijd, sleevePercentage, hoofdESPSpeedStep, message.command);
  } else {
    Serial.printf("[ESP-NOW] RX SIZE MISMATCH: %d bytes\n", len);
  }
}

// ===== ESP-NOW Initialisatie =====
static bool initESPNow() {
  Serial.println("[ESP-NOW] Initializing...");
  Serial.printf("[ESP-NOW] Body MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                bodyESP_MAC[0], bodyESP_MAC[1], bodyESP_MAC[2],
                bodyESP_MAC[3], bodyESP_MAC[4], bodyESP_MAC[5]);
  
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(4);  // Kanaal 4 (sync met HoofdESP)
  
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init failed");
    return false;
  }
  
  // Registreer ontvangst callback
  esp_now_register_recv_cb(onESPNowReceive);
  
  // Voeg HoofdESP toe als peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, hoofdESP_MAC, 6);
  peerInfo.channel = 4;
  peerInfo.encrypt = false;
  
  Serial.printf("[ESP-NOW] Adding HoofdESP: %02X:%02X:%02X:%02X:%02X:%02X\n",
                hoofdESP_MAC[0], hoofdESP_MAC[1], hoofdESP_MAC[2],
                hoofdESP_MAC[3], hoofdESP_MAC[4], hoofdESP_MAC[5]);
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Failed to add peer");
    return false;
  }
  
  Serial.println("[ESP-NOW] Initialized OK!");
  return true;
}

// 🔥 NIEUW: Voeg bericht toe aan retry queue
bool addToESPNowQueue(const esp_now_send_message_t& message) {
  if (queueCount >= ESPNOW_QUEUE_SIZE) {
    Serial.println("[ESP-NOW QUEUE] FULL - dropping oldest message");
    // Overschrijf oudste (head)
    queueHead = (queueHead + 1) % ESPNOW_QUEUE_SIZE;
    queueCount--;
  }
  
  // Voeg toe aan tail
  espNowQueue[queueTail].message = message;
  espNowQueue[queueTail].timestamp = millis();
  espNowQueue[queueTail].retryCount = 0;
  espNowQueue[queueTail].inUse = true;
  
  queueTail = (queueTail + 1) % ESPNOW_QUEUE_SIZE;
  queueCount++;
  
  Serial.printf("[ESP-NOW QUEUE] Added message (queue: %d/%d)\n", queueCount, ESPNOW_QUEUE_SIZE);
  return true;
}

// 🔥 NIEUW: Verwerk retry queue
void processESPNowQueue() {
  if (queueCount == 0 || !espNowInitialized) return;
  
  ESPNowQueueItem* item = &espNowQueue[queueHead];
  
  // Check of het tijd is voor retry
  if (millis() - item->timestamp < RETRY_DELAY_MS) {
    return;  // Nog niet tijd voor retry
  }
  
  // Probeer te verzenden
  esp_err_t result = esp_now_send(hoofdESP_MAC, (uint8_t*)&item->message, sizeof(item->message));
  
  if (result == ESP_OK) {
    // Success! Verwijder uit queue
    Serial.printf("[ESP-NOW QUEUE] Retry SUCCESS (attempt %d)\n", item->retryCount + 1);
    item->inUse = false;
    queueHead = (queueHead + 1) % ESPNOW_QUEUE_SIZE;
    queueCount--;
  } else {
    // Failed - increment retry
    item->retryCount++;
    item->timestamp = millis();  // Reset timer voor volgende retry
    
    if (item->retryCount >= MAX_RETRIES) {
      // Max retries bereikt - drop message
      Serial.printf("[ESP-NOW QUEUE] Max retries reached - dropping message\n");
      item->inUse = false;
      queueHead = (queueHead + 1) % ESPNOW_QUEUE_SIZE;
      queueCount--;
    } else {
      Serial.printf("[ESP-NOW QUEUE] Retry %d/%d failed\n", item->retryCount, MAX_RETRIES);
    }
  }
}

// ===== ESP-NOW Verzend Functie =====
// Extern beschikbaar voor MultiFunPlayer client
bool sendESPNowMessage(float newTrust, float newSleeve, bool overruleActive, const char* command, uint8_t stressLevel = 0, bool vibeOn = false, bool zuigenOn = false) {
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
  
  if (result == ESP_OK) {
    Serial.printf("[ESP-NOW] TX: T:%.1f S:%.1f O:%d Stress:%d Cmd:%s\n",
                  newTrust, newSleeve, overruleActive, stressLevel, command);
    return true;
  } else {
    // 🔥 NIEUW: Bij failure → toevoegen aan retry queue
    Serial.printf("[ESP-NOW] TX FAILED: %d - adding to retry queue\n", result);
    addToESPNowQueue(message);
    return false;  // Direct send failed, maar wordt ge-retry'd
    //Serial.printf("[ESP-NOW] TX FAILED: %d\n", result);
    //return false;
  }
}

// ===== RTC Tijd Opslaan Functie (voor menu) =====
bool saveRTCTime(int year, int month, int day, int hour, int minute) {
  if (!rtcAvailable) {
    Serial.println("[RTC] ERROR: RTC not available!");
    return false;
  }
  
  Serial.printf("[RTC] Saving time: %04d-%02d-%02d %02d:%02d:00\n",
                year, month, day, hour, minute);
  
  rtc.adjust(DateTime(year, month, day, hour, minute, 0));
  
  // Verify dat tijd opgeslagen is
  delay(100);  // Kleine delay voor RTC write
  DateTime now = rtc.now();
  Serial.printf("[RTC] Verified time: %04d-%02d-%02d %02d:%02d:%02d\n",
                now.year(), now.month(), now.day(),
                now.hour(), now.minute(), now.second());
  
  return true;
}

// ===== SD Kaart Format Functie (voor menu) =====
bool formatSDCard() {
  Serial.println("\n========================================");
  Serial.println("[SD FORMAT] Starting SD card format...");
  Serial.println("[SD FORMAT] WARNING: This will erase ALL data!");
  Serial.println("========================================\n");
  
  // Check of SD kaart beschikbaar is
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("[SD FORMAT] ERROR: No SD card attached!");
    return false;
  }
  
  Serial.println("[SD FORMAT] SD card detected, starting delete...");
  
  // Verwijder alle bestanden en mappen recursief
  int deletedFiles = 0;
  int deletedDirs = 0;
  
  // Helper functie om recursief te verwijderen
  std::function<void(const char*)> deleteRecursive = [&](const char* dirname) {
    File root = SD_MMC.open(dirname);
    if (!root) {
      Serial.printf("[SD FORMAT] Failed to open: %s\n", dirname);
      return;
    }
    if (!root.isDirectory()) {
      root.close();
      return;
    }
    
    File file = root.openNextFile();
    while (file) {
      String path = String(dirname) + "/" + String(file.name());
      
      if (file.isDirectory()) {
        Serial.printf("[SD FORMAT] Dir: %s\n", path.c_str());
        deleteRecursive(path.c_str());  // Recursief
        if (SD_MMC.rmdir(path.c_str())) {
          Serial.printf("[SD FORMAT] Deleted dir: %s\n", path.c_str());
          deletedDirs++;
        }
      } else {
        Serial.printf("[SD FORMAT] File: %s\n", path.c_str());
        if (SD_MMC.remove(path.c_str())) {
          Serial.printf("[SD FORMAT] Deleted: %s\n", path.c_str());
          deletedFiles++;
        } else {
          Serial.printf("[SD FORMAT] FAILED to delete: %s\n", path.c_str());
        }
      }
      
      file = root.openNextFile();
    }
    root.close();
  };
  
  // Start met root directory
  deleteRecursive("/");
  
  Serial.println("\n========================================");
  Serial.printf("[SD FORMAT] Format complete!\n");
  Serial.printf("[SD FORMAT] Deleted %d files and %d directories\n", deletedFiles, deletedDirs);
  Serial.println("[SD FORMAT] AI model in ESP32 flash is SAFE");
  Serial.println("========================================\n");
  
  return true;
}

// ===== Scherm Rotatie Toggle Functie (voor menu) =====
void toggleScreenRotation() {
  // Toggle tussen rotatie 1 (normaal) en 3 (180° gedraaid)
  if (screenRotation == 1) {
    screenRotation = 3;
  } else {
    screenRotation = 1;
  }
  
  // Pas rotatie toe op display
  body_gfx->setRotation(screenRotation);
  
  // Clear scherm en herteken alles
  body_gfx->fillScreen(0x0000);
  
  // Force menu redraw
  extern void bodyMenuForceRedraw();
  bodyMenuForceRedraw();
  
  Serial.printf("[SCREEN] Rotation changed to: %d (%s)\n", 
                screenRotation, (screenRotation == 1) ? "Normal" : "180 degrees");
}

void touchCallback(TPoint point, TEvent e) {
  // Track touch state om duplicate TouchEnd events te voorkomen
  static bool wasTouched = false;
  
  // Bij TouchStart/Tap: markeer als touched
  if (e == TEvent::Tap || e == TEvent::TouchStart) {
    wasTouched = true;
    return;  // Nog niet verwerken
  }
  
  // Bij TouchEnd: alleen verwerken als er eerst een touch was
  if (e == TEvent::TouchEnd) {
    if (!wasTouched) {
      return;  // Negeer duplicate TouchEnd zonder voorafgaande touch
    }
    wasTouched = false;  // Reset voor volgende touch
  } else {
    return;  // Andere events negeren
  }
  
  // Raw coordinaten (touch is native 320x480 portrait)
  int16_t rawX = point.x;
  int16_t rawY = point.y;
  
  int16_t x, y;
  
  #if USE_MANUAL_MAPPING
    // ===== MANUAL MODE: Volledige controle =====
    x = rawX;
    y = rawY;
    
    // SWAP X en Y
    #if MANUAL_SWAP_XY
      int16_t temp = x;
      x = y;
      y = temp;
    #endif
    
    // Basis FLIP voor rotatie 1 (normaal)
    #if MANUAL_FLIP_X
      x = 480 - 1 - x;
    #endif
    
    #if MANUAL_FLIP_Y
      y = 320 - 1 - y;
    #endif
    
    // Extra flip voor rotatie 3 (180° gedraaid)
    if (screenRotation == 3) {
      x = 480 - 1 - x;
      y = 320 - 1 - y;
    }
    
    // Serial output UITGESCHAKELD (zie sensor debug in loop)
    // Serial.printf("RAW: x=%d, y=%d -> MANUAL(S%d,FX%d,FY%d): x=%d, y=%d (event=%d)\n", 
    //               rawX, rawY, MANUAL_SWAP_XY, MANUAL_FLIP_X, MANUAL_FLIP_Y, x, y, (int)e);
  
  #else
    // ===== AUTO MODE: Gebruik rotatie =====
    #if TOUCH_ROTATION == 0
      // 0° - Portrait (native)
      x = rawX;
      y = rawY;
      
    #elif TOUCH_ROTATION == 1
      // 90° - Landscape (USB rechts) - WORKING COMBINATIE
      // Equivalent: SWAP_XY=1, FLIP_X=0, FLIP_Y=1
      x = rawY;
      y = 320 - 1 - rawX;
      
    #elif TOUCH_ROTATION == 2
      // 180° - Portrait (ondersteboven)
      x = 320 - 1 - rawX;
      y = 480 - 1 - rawY;
      
    #elif TOUCH_ROTATION == 3
      // 270° - Landscape (USB links)
      x = 480 - 1 - rawY;
      y = rawX;
      
    #else
      #error "TOUCH_ROTATION must be 0, 1, 2, or 3"
    #endif
    
    // Serial output UITGESCHAKELD (zie sensor debug in loop)
    // Serial.printf("RAW: x=%d, y=%d -> ROT%d: x=%d, y=%d (event=%d)\n", 
    //               rawX, rawY, TOUCH_ROTATION, x, y, (int)e);
  #endif
  
  // Clamp binnen scherm grenzen (480x320 landscape)
  if (x < 0) x = 0;
  if (x >= 480) x = 479;
  if (y < 0) y = 0;
  if (y >= 320) y = 319;
  
  touchX = x;
  touchY = y;
  touchDetected = true;
  
  // Simpele debounce: negeer touches binnen 300ms
  if (millis() - lastButtonTouch < 300) {
    return;
  }
  
  // ===== CHECK MENU KNOP EERST (werkt in beide modes) =====
  // SKIP tijdens playback - playback heeft eigen touch handling
  extern BodyMenuPage bodyMenuPage;
  bool isInPlayback = (bodyMenuPage == BODY_PAGE_PLAYBACK);
  
  // MENU knop gebied: x tussen 239-351, y tussen 280-315
  if (!isInPlayback && y >= 280 && y < 315 && x >= 239 && x < 351) {
    lastButtonTouch = millis();
    
    // Toggle tussen MAIN en MENU mode
    if (currentMode == MODE_MAIN) {
      currentMode = MODE_MENU;
      menuActive = true;
      bodyMenuMode = BODY_MODE_MENU;  // Activeer menu mode
      bodyMenuPage = BODY_PAGE_MAIN;  // Start bij hoofdmenu
      bodyMenuIdx = 0;  // Reset selectie
      bodyMenuForceRedraw();  // Force onmiddellijke hertekening
      Serial.println("[MENU] Entering menu mode");
    } else {
      currentMode = MODE_MAIN;
      menuActive = false;
      bodyMenuMode = BODY_MODE_SENSORS;  // Niet meer gebruikt, maar blijft voor compatibiliteit
      
      // 🔥 NIEUW: Reset rendering pause (kan nog aan staan vanaf popup)
      g4_pauseRendering = false;

      // Volledige hertekening van body_gfx4 scherm
      body_gfx4_clear();  // Clear menu EN teken frame
      body_gfx4_drawButtons(isRecording, isPlaying, menuActive, aiOverruleActive);
      
      // Force onmiddellijke sensor update zodat grafieken zichtbaar worden
      lastSensorPush = 0;  // Reset timer (wordt gedeclareerd in loop)
      
      Serial.println("[MENU] Back to main screen (body_gfx4)");
    }
    return;  // Stop hier, knop is afgehandeld
  }
  
  // ===== MENU MODE TOUCH HANDLING =====
  if (currentMode == MODE_MENU) {
    lastButtonTouch = millis();
    bodyMenuHandleTouch(x, y, true);  // Stuur touch door naar menu systeem
    return;  // Menu verwerkt de touch, stop hier
  }
  
  // ===== MAIN MODE TOUCH HANDLING =====
  // Check of touch op body_gfx4 knoppen is (onderaan scherm)
  // Button layout (volgens body_gfx4.cpp):
  // BUTTON_Y = SCR_H - 40 = 280
  // BUTTON_W = (SCR_W - 30) / 4 = 112
  // BUTTON_SPACING = (SCR_W - 10) / 4 = 117
  // BUTTON_START_X = 5
  
  if (y >= 280 && y < 315) {  // Binnen knop hoogte
    lastButtonTouch = millis();
    
    if (x >= 5 && x < 117) {
      // REC knop - toggle recording
      isRecording = !isRecording;
      
      if (isRecording) {
        startCSVRecording();  // Start nieuwe recording
      } else {
        stopCSVRecording();   // Stop huidige recording
      }
      
      body_gfx4_drawButtons(isRecording, isPlaying, menuActive, aiOverruleActive);
      Serial.printf("[BUTTON] REC %s\n", isRecording ? "ON" : "OFF");
    }
    else if (x >= 122 && x < 234) {
      // PLAY knop - open Recording menu
      currentMode = MODE_MENU;
      menuActive = true;
      bodyMenuMode = BODY_MODE_MENU;
      bodyMenuPage = BODY_PAGE_RECORDING;  // Direct naar Recording menu
      bodyMenuIdx = 0;
      bodyMenuForceRedraw();
      Serial.println("[BUTTON] PLAY - Opening Recording menu");
    }
    else if (x >= 356 && x < 475) {
      // AI knop
      aiOverruleActive = !aiOverruleActive;
      body_gfx4_drawButtons(isRecording, isPlaying, menuActive, aiOverruleActive);
      Serial.printf("[BUTTON] AI %s\n", aiOverruleActive ? "ON" : "OFF");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 🔥 NIEUW: Watchdog timer (10 sec timeout)
  Serial.println("[WDT] Enabling watchdog timer...");
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 10000,      // 10 seconden
    .idle_core_mask = 0,      // Niet monitoren van idle cores
    .trigger_panic = true     // Reset bij timeout
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);       // Add current thread
  Serial.println("[WDT] Watchdog active - will reset if frozen for >10 sec!");

  Serial.println("\n\n=== SC01 Plus Touch Test ===");
  
  // ===== Display initialisatie =====
  gfx->begin();
  gfx->setRotation(1);  // Landscape
  
  // Backlight aan
  pinMode(GFX_BL, OUTPUT);
  digitalWrite(GFX_BL, HIGH);
  
  // // Rood scherm = bezig met initialisatie - UITGESCHAKELD
  // gfx->fillScreen(0xF800);
  // gfx->setTextColor(0xFFFF);
  // gfx->setTextSize(2);
  // gfx->setCursor(10, 10);
  // gfx->println("Touch Test - Initializing...");
  
  Serial.println("[DISPLAY] 480x320 initialized");
  
  // ===== Touch initialisatie =====
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);
  
  Serial.println("[TOUCH] Starting I2C...");
  
  // FT6X36 zonder interrupt (polling mode)
  ts = new FT6X36(&Wire, TOUCH_INT);
  
  if (ts->begin()) {
    Serial.println("[TOUCH] FT6336U initialized OK!");
    
    // Registreer callback
    ts->registerTouchHandler(touchCallback);
    
    // // Groen scherm = OK - UITGESCHAKELD: direct naar body_gfx4
    // gfx->fillScreen(0x07E0);
    // gfx->setCursor(10, 10);
    // gfx->setTextColor(0x0000);  // Zwarte tekst
    // gfx->println("Touch Controller OK!");
    // gfx->setCursor(10, 40);
    // gfx->println("Raak scherm aan...");
    // gfx->setCursor(10, 70);
    // gfx->println("Witte pixels = touch");
    // gfx->setCursor(10, 100);
    // #if USE_MANUAL_MAPPING
    //   gfx->printf("MANUAL: S%d FX%d FY%d", MANUAL_SWAP_XY, MANUAL_FLIP_X, MANUAL_FLIP_Y);
    // #else
    //   gfx->printf("Rotation: %d", TOUCH_ROTATION);
    // #endif
    
  } else {
    Serial.println("[TOUCH] ERROR: FT6336U not found!");
    // gfx->fillScreen(0xF800);  // Rood = fout
    // gfx->setCursor(10, 40);
    // gfx->println("ERROR: Touch not found!");
    // 
    // // Blijf rood
    // gfx->setCursor(10, 40);
    // gfx->println("ERROR: Touch not found!");
    // gfx->setCursor(10, 70);
    // gfx->println("Check I2C wiring:");
    // gfx->setCursor(10, 100);
    // gfx->println("SDA=GPIO6, SCL=GPIO5");
    
    while(1) delay(1000);  // Stop hier - check Serial Monitor
  }
  
  // ===== I2C Sensors Test (Wire1 - GPIO 10/11) =====
  Serial.println("\n[I2C] Starting Wire1 on GPIO 10/11...");
  Wire1.begin(SENSOR_SDA, SENSOR_SCL);
  Wire1.setClock(400000);
  
  // int yPos = 130;  // Display uitgeschakeld - alleen Serial logging
  
  // Test 1: ADS1115 (0x48) - gebruik ads1115_begin() functie
  Serial.println("[I2C] Testing ADS1115 (0x48)...");
  if (ads1115_begin()) {
    Serial.println("[I2C] ADS1115 OK via ads1115_begin()!");
    adsAvailable = true;
    
    // Laad kalibratie uit ESP32 EEPROM
    Serial.println("[CONFIG] Loading calibration from ESP32 EEPROM...");
    loadSensorConfig();  // Laadt en past toe op ADS1115
    
    // gfx->setCursor(10, yPos);
    // gfx->setTextColor(0x07E0);
    // gfx->println("ADS1115: OK");
  } else {
    Serial.println("[I2C] ADS1115 NOT found");
    // gfx->setCursor(10, yPos);
    // gfx->setTextColor(0xF800);
    // gfx->println("ADS1115: FAIL");
  }
  // yPos += 30;
  
  // Test 2: RTC DS3231 (0x68)
  Serial.println("[I2C] Testing RTC DS3231 (0x68)...");
  if (rtc.begin(&Wire1)) {
    Serial.println("[I2C] RTC DS3231 OK!");
    rtcAvailable = true;
    
    // Check of RTC tijd verloren heeft
    if (rtc.lostPower()) {
      Serial.println("[RTC] Power was lost - setting default time");
      // Zet standaard tijd: 1 januari 2024, 00:00:00
      rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
    }
    
    // Toon huidige tijd
    DateTime now = rtc.now();
    Serial.printf("[RTC] Current time: %02d/%02d/%04d %02d:%02d:%02d\n",
                  now.day(), now.month(), now.year(),
                  now.hour(), now.minute(), now.second());
  } else {
    Serial.println("[I2C] RTC DS3231 NOT found");
    rtcAvailable = false;
  }
  // yPos += 30;
  
  // Test 3: I2C EEPROM (0x50) - AT24C02 256 bytes
  Serial.println("[I2C] Testing EEPROM (0x50 - A0/A1/A2=GND)...");
  Wire1.beginTransmission(EEPROM_ADDR);
  if (Wire1.endTransmission() == 0) {
    Serial.println("[I2C] EEPROM OK!");
    eepromAvailable = true;
    // gfx->setCursor(10, yPos);
    // gfx->setTextColor(0x07E0);
    // gfx->println("EEPROM: OK");
  } else {
    Serial.println("[I2C] EEPROM NOT found");
    // gfx->setCursor(10, yPos);
    // gfx->setTextColor(0xF800);
    // gfx->println("EEPROM: FAIL");
  }
  
  // ===== EEPROM Test (write/read/clear) =====
  if (eepromAvailable) {
    Serial.println("\n[EEPROM] Testing write/read/clear...");
    
    // Test data
    uint8_t testData[] = {0xAA, 0x55, 0x12, 0x34};
    uint8_t addr = 0;
    
    // Write test
    Serial.print("[EEPROM] Writing: ");
    for (int i = 0; i < 4; i++) {
      Wire1.beginTransmission(EEPROM_ADDR);
      Wire1.write(addr + i);  // Address
      Wire1.write(testData[i]);  // Data
      Wire1.endTransmission();
      delay(5);  // EEPROM write delay
      Serial.printf("0x%02X ", testData[i]);
    }
    Serial.println();
    
    delay(10);
    
    // Read test
    Serial.print("[EEPROM] Reading: ");
    bool readOK = true;
    for (int i = 0; i < 4; i++) {
      Wire1.beginTransmission(EEPROM_ADDR);
      Wire1.write(addr + i);  // Set address
      Wire1.endTransmission();
      Wire1.requestFrom(EEPROM_ADDR, 1);
      if (Wire1.available()) {
        uint8_t readByte = Wire1.read();
        Serial.printf("0x%02X ", readByte);
        if (readByte != testData[i]) readOK = false;
      } else {
        Serial.print("ERR ");
        readOK = false;
      }
    }
    Serial.println(readOK ? "- MATCH!" : "- MISMATCH!");
    
    // Clear test
    Serial.print("[EEPROM] Clearing: ");
    for (int i = 0; i < 4; i++) {
      Wire1.beginTransmission(EEPROM_ADDR);
      Wire1.write(addr + i);
      Wire1.write(0xFF);  // Cleared byte
      Wire1.endTransmission();
      delay(5);
    }
    Serial.println("Done");
    
    Serial.println("[EEPROM] Test PASSED - EEPROM fully functional!");
  }
  
  // ===== SD Card Initialisatie (SD_MMC mode) =====
  Serial.println("\n[SD CARD] Initializing SD_MMC...");
  SD_MMC.setPins(39, 40, 38);  // CLK, CMD, D0
  if (SD_MMC.begin("/sdcard", true)) {  // 1-bit mode
    Serial.println("[SD CARD] Initialized OK!");
  } else {
    Serial.println("[SD CARD] Init failed - recording won't work");
  }
  
  // ===== ESP-NOW Initialisatie =====
  Serial.println("\n[ESP-NOW] Starting initialization...");
  espNowInitialized = initESPNow();
  
  if (espNowInitialized) {
    Serial.println("[ESP-NOW] Ready for communication!");
    // Test bericht versturen
    sendESPNowMessage(1.0f, 1.0f, false, "HEARTBEAT", 0, false, false);
  } else {
    Serial.println("[ESP-NOW] FAILED - continuing without ESP-NOW");
  }
  
  // ===== Stress Manager Initialisatie =====
  Serial.println("\n[AI] Initializing Stress Manager...");
  extern AdvancedStressManager stressManager;
  stressManager.begin();
  Serial.println("[AI] Stress Manager ready!");

  // ===== Body GFX4 Initialisatie =====
  Serial.println("\n[BODY_GFX4] Initializing graphics system...");
  body_gfx4_begin();
  body_gfx4_setLabel(G4_HART, "Hart");
  body_gfx4_setLabel(G4_HUID, "Huid");
  body_gfx4_setLabel(G4_TEMP, "Temp");
  body_gfx4_setLabel(G4_ADEMHALING, "Adem");
  body_gfx4_setLabel(G4_HOOFDESP, "Trust");
  body_gfx4_setLabel(G4_ZUIGEN, "Zuigen");
  body_gfx4_setLabel(G4_TRIL, "Vibe");
  Serial.println("[BODY_GFX4] Graphics system ready!");
  
  // ===== Menu Systeem Initialisatie =====
  Serial.println("\n[MENU] Initializing menu system...");
  bodyMenuInit();
  Serial.println("[MENU] Menu system ready!");
  
  // ===== MultiFunPlayer Initialisatie =====
  Serial.println("\n[MFP] Initializing MultiFunPlayer client...");
  setupMultiFunPlayer();  // Zie body_config.h voor configuratie
  Serial.println("[MFP] Client ready! Toggle Funscript in System Settings.");
  
  Serial.println("\n[BODY] System ready!");
  Serial.println("[BODY] Touch knoppen: REC, PLAY, MENU, AI");
  Serial.println("[BODY] ESP-NOW: heartbeat elke 5 seconden");
  Serial.println("[BODY] 7 sensor grafieken actief");
  Serial.println("[BODY] Funscript: Toggle in Menu > Instellingen > Funscript");
  Serial.println("========================================\n");
}

void loop() {
  esp_task_wdt_reset();  // 🔥 Reset watchdog elke loop iteratie

  // 🧪 TEST: Uncomment om watchdog te testen (ESP reset na 10 sec)
   //static bool tested = false;
   //if (!tested && millis() > 5000) {
     //Serial.println("[TEST] Freezing loop to trigger watchdog...");
     //while(1); // Freeze
     //tested = true;
   //}

  // Debug: bevestig dat loop draait
  static bool firstLoop = true;
  if (firstLoop) {
    Serial.println("\n[LOOP] Loop started!");
    firstLoop = false;
  }
  
  // POLLING MODE: handmatig touch data lezen
  if (ts) {
    ts->processTouch();  // Lees I2C
    ts->loop();          // Verwerk events
  }
  
  // Touch wordt nu afgehandeld in touchCallback (body_gfx4 knoppen)
  if (touchDetected) {
    touchDetected = false;
  }
  
  // Check of playback actief is
  bool isPlayback = (bodyMenuPage == BODY_PAGE_PLAYBACK);
  
  // ===== MENU MODE POLLING (als menu actief is OF playback actief) =====
  if (currentMode == MODE_MENU || isPlayback) {
    bodyMenuTick();  // Update menu display en playback overlay
    // Menu touch handling gebeurt al in touchCallback
  }
  // MAIN MODE: body_gfx4 grafieken worden automatisch ge-update via pushSample()
  
  // ===== ECHTE SENSOR DATA (zie config.h voor interval) =====
  // ALLEEN in MAIN mode (NIET tijdens playback - playback gebruikt CSV data)
  if (currentMode == MODE_MAIN && !isPlayback && millis() - lastSensorPush > SENSOR_INTERVAL_MS) {
    if (adsAvailable) {
      // Lees alle ADS1115 sensoren
      ads1115_readAll();
      ADS1115_SensorData sensorData = ads1115_getData();
      
      // Push echte sensor data naar grafieken
      body_gfx4_pushSample(G4_HART, sensorData.BPM);                // Hart: BPM
      body_gfx4_pushSample(G4_HUID, sensorData.gsrSmooth / GSR_SCHAAL_FACTOR);  // GSR: zie config.h
      body_gfx4_pushSample(G4_TEMP, sensorData.temperature);        // Temp: °C
      body_gfx4_pushSample(G4_ADEMHALING, sensorData.breathValue);  // Ademhaling: 0-100%

            // ═══════════════════════════════════════════════════════════
      // AI STRESS MANAGER UPDATE - Alleen als AI knop AAN
      // ═══════════════════════════════════════════════════════════
      
      if (aiOverruleActive) {
        // Update stress manager met biometric data
        extern AdvancedStressManager stressManager;
        
        BiometricData bio;
        bio.heartRate = sensorData.BPM;
        bio.temperature = sensorData.temperature;
        bio.gsrValue = sensorData.gsrSmooth;
        bio.timestamp = millis();
        
        stressManager.update(bio);
        
        // Haal AI beslissing op
        StressDecision decision = stressManager.getStressDecision();
        
        // Stuur AI override naar Hooft ESP (elke 5 seconden)
        static uint32_t lastAIUpdate = 0;
        if (millis() - lastAIUpdate > 1000) {
          float trustOverride = decision.recommendedSpeed / 7.0f;  // 0-1 range
          float sleeveOverride = trustOverride;  // Same for now
          
          sendESPNowMessage(
            trustOverride,
            sleeveOverride,
            true,  // overruleActive
            "AI_OVERRIDE",
            decision.currentLevel,
            decision.vibeRecommended,
            decision.suctionRecommended
          );
          
          Serial.printf("[AI] Override sent: Speed=%d, Level=%d, Vibe=%d, Suction=%d\n",
                        decision.recommendedSpeed, decision.currentLevel,
                        decision.vibeRecommended, decision.suctionRecommended);
          
          lastAIUpdate = millis();
        }
      }

    } else {
      // Fallback: dummy data als ADS1115 niet beschikbaar
      float t = millis() / 1000.0f;
      body_gfx4_pushSample(G4_HART, 60 + 20 * sin(t * 2));
      body_gfx4_pushSample(G4_HUID, 50 + 10 * sin(t * 0.5));
      body_gfx4_pushSample(G4_TEMP, 36 + 1 * sin(t * 0.3));
      body_gfx4_pushSample(G4_ADEMHALING, 15 + 5 * sin(t));
    }
    
    // ESP-NOW data met animatie
    // HoofdESP: zeer agressieve sinus animatie die sneller beweegt bij hogere trust
    static float thrustPhase = 0;
    if (!pauseActive && trustSpeed > 0.05f) {  // Alleen animatie als NIET gepauzeerd EN trust > 0
      // Trust animatie (zie config.h om snelheid/hoogte aan te passen)
      float thrustFreq = trustSpeed * TRUST_ANIM_SNELHEID;
      thrustPhase += thrustFreq * 0.1f;  // Increment per 100ms
      float thrustAnim = 50 + TRUST_ANIM_HOOGTE * sin(thrustPhase);
      body_gfx4_pushSample(G4_HOOFDESP, thrustAnim);
    } else {
      thrustPhase = 0;  // Reset fase
      body_gfx4_pushSample(G4_HOOFDESP, 50);  // Vlakke lijn in midden bij pauze
    }
    
    body_gfx4_pushSample(G4_ZUIGEN, zuigActive ? 100 : 0);        // Zuigen: 0 of 100
    
    // Vibe: zaagrand patroon (zie config.h voor snelheid)
    static float vibePhase = 0;
    if (vibeOn) {
      vibePhase += VIBE_ANIM_SNELHEID;  // Zie config.h
      if (vibePhase > 1.0f) vibePhase = 0;
      body_gfx4_pushSample(G4_TRIL, vibePhase * 100);  // Lineair 0-100
    } else {
      vibePhase = 0;
      body_gfx4_pushSample(G4_TRIL, 50);  // Middenlijn bij uit
    }
    
    lastSensorPush = millis();
  }
  
  // ===== SENSOR + TOUCH DEBUG (elke 2 seconden) - UITGESCHAKELD =====
  // static uint32_t lastRead = 0;
  // if (millis() - lastRead > 2000) {
  //   Serial.println("\n=== I2C STATUS ===");
  //   
  //   // Touch status
  //   uint8_t touches = ts ? ts->touched() : 0;
  //   Serial.printf("[TOUCH Wire]   Touches: %d\n", touches);
  //   
  //   // ADS1115 sensor readout
  //   if (adsAvailable) {
  //     int16_t adc0 = ads.readADC_SingleEnded(0);
  //     int16_t adc1 = ads.readADC_SingleEnded(1);
  //     int16_t adc2 = ads.readADC_SingleEnded(2);
  //     int16_t adc3 = ads.readADC_SingleEnded(3);
  //     
  //     float v0 = ads.computeVolts(adc0);
  //     float v1 = ads.computeVolts(adc1);
  //     float v2 = ads.computeVolts(adc2);
  //     float v3 = ads.computeVolts(adc3);
  //     
  //     Serial.printf("[ADS1115] A0=%d (%.3fV) A1=%d (%.3fV)\n", adc0, v0, adc1, v1);
  //     Serial.printf("[ADS1115] A2=%d (%.3fV) A3=%d (%.3fV)\n", adc2, v2, adc3, v3);
  //     
  //     if (adc0 == 0 && adc1 == 0 && adc2 == 0 && adc3 == 0) {
  //       Serial.println("[WARNING] Alle ADC = 0 - I2C fout!");
  //     }
  //   } else {
  //     Serial.println("[ADS1115] NOT AVAILABLE");
  //   }
  //   
  //   // RTC readout
  //   if (rtcAvailable) {
  //     DateTime now = rtc.now();
  //     float temp = rtc.getTemperature();
  //     Serial.printf("[RTC] Time: %04d-%02d-%02d %02d:%02d:%02d\n",
  //                   now.year(), now.month(), now.day(),
  //                   now.hour(), now.minute(), now.second());
  //     Serial.printf("[RTC] Temp: %.2f C\n", temp);
  //   } else {
  //     Serial.println("[RTC] NOT AVAILABLE");
  //   }
  //   
  //   // EEPROM - skip (already tested in setup)
  //   if (eepromAvailable) {
  //     Serial.println("[EEPROM] OK (tested at startup)");
  //   } else {
  //     Serial.println("[EEPROM] NOT AVAILABLE");
  //   }
  //   
  //   Serial.println("==================\n");
  //   lastRead = millis();
  // }
  
  // ===== CSV RECORDING UPDATE =====
  updateCSVRecording();  // Update CSV bestand als recording actief is
  
  // ===== PLAYBACK UPDATE =====
  updatePlayback();  // Update playback als actief
  
  // ===== MULTIFUNPLAYER LOOP (als funscript enabled) =====
  static bool lastFunscriptState = false;
  if (funscriptEnabled != lastFunscriptState) {
    // Status veranderd
    if (funscriptEnabled) {
      Serial.println("[MFP] Funscript mode ENABLED - connecting to MultiFunPlayer...");
      mfpClient.enable(true);
      mfpClient.connect();
    } else {
      Serial.println("[MFP] Funscript mode DISABLED - disconnecting...");
      mfpClient.disconnect();
      mfpClient.enable(false);
    }
    lastFunscriptState = funscriptEnabled;
  }
  
  // MultiFunPlayer WebSocket loop (alleen als enabled)
  if (funscriptEnabled) {
    mfpClient.loop();
  }
  
  // 🔥 NIEUW: Process ESP-NOW retry queue
  processESPNowQueue();

  // ===== ESP-NOW HEARTBEAT (elke 5 seconden) =====
  static uint32_t lastHeartbeat = 0;
  if (espNowInitialized && (millis() - lastHeartbeat > 5000)) {
    sendESPNowMessage(1.0f, 1.0f, false, "HEARTBEAT", 0, false, false);
    lastHeartbeat = millis();
    
    // Toon laatste ontvangen data
    if (millis() - lastCommTime < 10000) {  // Als recent data ontvangen
      Serial.printf("[ESP-NOW] Last RX: T:%.1f S:%.1f Su:%.1f V:%d Z:%d Vac:%.1f P:%d Pos:%.0f%% Step:%d (%.1fs ago)\n",
                    trustSpeed, sleeveSpeed, suctionLevel, vibeOn, zuigActive, vacuumMbar,
                    pauseActive, sleevePercentage, hoofdESPSpeedStep,
                    (millis() - lastCommTime) / 1000.0f);
    } else {
      Serial.println("[ESP-NOW] No recent data from HoofdESP");
    }
    
    // Toon MultiFunPlayer status
    if (funscriptEnabled) {
      Serial.printf("[MFP] Status: %s | Actions: %d | ML Overrides: %d (%.1f%%)\n",
                    mfpClient.isConnected() ? "Connected" : "Disconnected",
                    mfpClient.getTotalActions(),
                    mfpClient.getMLOverrides(),
                    mfpClient.getMLOverridePercentage());
    }
  }
  
  delay(10);  // Kleine delay voor stabiele polling
}

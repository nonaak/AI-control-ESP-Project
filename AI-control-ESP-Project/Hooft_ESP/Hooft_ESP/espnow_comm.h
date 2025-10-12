#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ===============================================================================
// ESP-NOW COMMUNICATIE DEFINITTIES - HOOFDESP (E4:65:B8:7A:85:E4)
// KANAAL: 4
// ===============================================================================

// MAC Addresses van alle ESP devices - ALL ON CHANNEL 4
extern const uint8_t bodyESP_MAC[6];    // Body ESP (EC:DA:3B:98:C5:24) - Channel 4
extern const uint8_t pumpUnit_MAC[6];   // Pomp Unit (60:01:94:59:18:86) - Channel 4  
extern const uint8_t m5atom_MAC[6];     // M5Atom (50:02:91:87:23:F8) - Channel 4

// ===============================================================================
// MESSAGE STRUCTURES
// ===============================================================================

// Ontvangen van Body ESP - AI Commands (SIMPLE VERSION)
typedef struct __attribute__((packed)) {
  float newTrust;         // AI berekende trust override (0.0-1.0)
  float newSleeve;        // AI berekende sleeve override (0.0-1.0)
  bool overruleActive;    // AI overrule status
  uint8_t stressLevel;    // Stress level 1-7 voor playback/AI
  bool vibeOn;           // Vibe status voor playback
  bool zuigenOn;         // Zuigen status voor playback
  char command[32];       // "AI_OVERRIDE", "EMERGENCY_STOP", "HEARTBEAT", "PLAYBACK_STRESS"
} bodyESP_message_t;

// Body ESP AI Commands (Body ESP beslist, HoofdESP voert uit):
// "AI_OVERRIDE"   - Trust/sleeve override (bestaand)
// "AI_COMMAND"    - AI commando (speed/vacuum/pause/LED)
// "HEARTBEAT"     - Verbinding check


// Verzenden naar Pomp Unit - MainMsgV3 (backwards compatible)
struct __attribute__((packed)) MainMsgV3 {
  uint8_t  version;           // Bericht versie (3)
  uint8_t  arrow_full;        // Pijl vol status (0/1)
  uint8_t  session_started;   // Sessie start trigger
  uint32_t punch_count;       // Huidige punch teller
  uint32_t punch_goal;        // Punch doel voor lube trigger
  int16_t  vacuum_set_x10;    // Vacuum setpoint (tienden cmHg)
  uint8_t  force_pump_state;  // 0=AUTO, 1=FORCE_OFF, 2=FORCE_ON
  uint8_t  cmd_lube_prime_now;// Pulse commando: lube prime
  uint8_t  cmd_lube_shot_now; // Pulse commando: lube shot
  uint16_t lube_duration_ms;  // Lube timing in milliseconden (0-20000ms)
  uint8_t  cmd_toggle_zuigen; // Pulse commando: toggle zuigen mode (0/1)
  int16_t  zuig_target_x10;   // Zuig target vacuum (tienden cmHg)
};

// Ontvangen van Pomp Unit - Vacuum System Telemetry
struct __attribute__((packed)) PumpStatusMsg {
  uint8_t  version;              // Message versie (1)
  float    current_vacuum_cmHg;  // 🎯 HX711 vacuum waarde (real-time!)
  uint8_t  vacuum_pump_on;       // Vacuum pomp status (0/1)
  uint8_t  lube_pump_on;         // Lube pomp status (0/1)
  uint8_t  servo_open;           // Servo valve positie (0/1)
  uint8_t  air_relay_open;       // Air relay status (0/1)
  float    lube_remaining_sec;   // Resterende lube tijd
  char     system_status[8];     // "OK"/"TARE"/"ERROR"
  uint8_t  force_pump_state;     // Huidige force state
  uint8_t  zuig_active;          // Zuigen mode actief op Pomp Unit (0/1)
  uint32_t uptime_sec;           // System uptime
};

// Verzenden naar Body ESP - Machine Status Update
typedef struct __attribute__((packed)) {
  float trust;            // Huidige trust speed (0.0-2.0)
  float sleeve;           // Huidige sleeve speed (0.0-2.0)  
  float suction;          // Suction level (0.0-100.0)
  float pause;            // Pause tijd tussen cycles (0.0-10.0)
  bool vibeOn;            // Handmatige VIBE toggle (Z-knop debug LED bit 3)
  bool zuigActive;        // Zuigen mode actief status
  float vacuumMbar;       // Vacuum waarde in mbar voor grafiek
  bool pauseActive;       // C knop pauze status (true=gepauzeerd)
  bool lubeTrigger;       // Lube cyclus start trigger (sync punt)
  float cyclusTijd;       // Verwachte cyclus duur in seconden
  float sleevePercentage; // Sleeve positie percentage (0.0-100.0)
  uint8_t currentSpeedStep; // Huidige versnelling (0-7)
  char command[32];       // "STATUS_UPDATE"
} machineStatus_message_t;

// Ontvangen van M5Atom - Motion Data (AtomMotion from M5Atom)
typedef struct __attribute__((packed)) {
  uint8_t ver;     // Message version (1)
  uint8_t kind;    // Message kind (1 = MK_ATOM_MOTION)
  int8_t dir;      // Motion direction: +1=UP, -1=DOWN, 0=STILL
  uint8_t speed;   // Motion speed (0-100)
  uint8_t flags;   // Extra flags
} atomMotion_message_t;

// Ontvangen van M5Atom - Remote Commands (future use)
typedef struct __attribute__((packed)) {
  char command[32];       // "START", "STOP", "PAUSE", "EMERGENCY"
  float trustSpeed;       // Gewenste trust speed
  float sleeveSpeed;      // Gewenste sleeve speed
  int duration;          // Session duur (minuten)
} m5atom_command_message_t;

// Verzenden naar M5Atom - Pump Colors (matches M5Atom PumpColors struct)
typedef struct __attribute__((packed)) {
  uint8_t ver;     // Message version (1)
  uint8_t kind;    // Message kind (2 = MK_PUMP_COLORS)
  uint8_t a_r, a_g, a_b;  // Pump A color
  uint8_t b_r, b_g, b_b;  // Pump B color
  uint8_t flags;   // Status flags
} pumpColors_message_t;

// Verzenden naar M5Atom - Status & Data Streaming
typedef struct __attribute__((packed)) {
  float trustSpeed;       // Huidige speeds
  float sleeveSpeed;      
  bool aiOverruleActive; // AI status van Body ESP
  float sessionTime;     // Lopende sessie tijd
  char status[32];       // "ACTIVE", "PAUSED", "EMERGENCY"
} monitoring_message_t;

// ===============================================================================
// GLOBAL VARIABLES (declarations)
// ===============================================================================

// Machine state - VACUUM SYSTEEM
extern float currentVacuumSetpoint;     // cmHg (user setting)
extern float currentVacuumReading;      // cmHg (from PumpStatusMsg)
extern bool vacuumPumpStatus;           // From telemetry
extern bool lubePumpStatus;             // From telemetry
extern bool aiOverruleActive;           // From Body ESP
extern bool emergencyStop;

// Session control
extern uint32_t punchCount;
extern uint32_t punchGoal;
extern bool arrowFull;
extern bool sessionActive;

// Motion data from M5Atom
extern int8_t currentMotionDir;     // +1=UP, -1=DOWN, 0=STILL
extern uint8_t currentMotionSpeed;  // 0-100

// "ZUIGEN" mode variables - TRACKED FROM PUMP UNIT
extern bool pompUnitZuigActive;         // Zuig active status from Pump Unit
extern float zuigTargetVacuum;          // Target vacuum for zuig mode (UI setting)

// Communication status
extern uint32_t bodyESP_lastContact;
extern uint32_t pumpUnit_lastContact;
extern uint32_t m5atom_lastContact;
extern bool bodyESP_connected;
extern bool pumpUnit_connected;
extern bool m5atom_connected;

// AI override values from Body ESP
extern float bodyESP_trustOverride;
extern float bodyESP_sleeveOverride;

// Debug LED control for M5StickC Plus (bit 3 in ESP-NOW flags)
extern bool debugLedState;
extern bool vibeState;  // Z-knop debug LED toggle state

// Timing constants
extern const uint32_t BODY_ESP_UPDATE_INTERVAL;   // 500ms (2Hz)
extern const uint32_t M5ATOM_UPDATE_INTERVAL;     // 250ms (4Hz)
extern const uint32_t BODY_ESP_TIMEOUT;           // 10 seconds
extern const uint32_t PUMP_UNIT_TIMEOUT;          // 5 seconds
extern const uint32_t M5ATOM_TIMEOUT;             // 15 seconds

// ===============================================================================
// FUNCTION DECLARATIONS
// ===============================================================================

void initESPNow();
void onESPNowReceive(const esp_now_recv_info *info, const uint8_t *data, int len);

// Message handlers
void handleBodyESPMessage(const bodyESP_message_t &msg);                    // Uitgebreide AI overrule
void handlePumpUnitMessage(const PumpStatusMsg &msg);
void handleM5AtomMotionMessage(const atomMotion_message_t &msg);
void handleM5AtomCommandMessage(const m5atom_command_message_t &msg);

// Send functions
void sendPumpControlMessage(const MainMsgV3 &msg);
void sendBodyESPStatusUpdate(const machineStatus_message_t &msg);
void sendM5AtomStatusUpdate(const monitoring_message_t &msg);
void sendM5AtomPumpColors(const pumpColors_message_t &msg);

// Control functions
void updateVacuumControl();
void sendPumpControlMessages();
void sendPumpHeartbeat();
void sendStatusUpdates();
void checkCommunicationTimeouts();
void performSafetyChecks();

// Safety functions
void handlePumpError();
void handleBodyESPTimeout();
void handleEmergencyStop();
void forwardVacuumDataToBodyESP(const PumpStatusMsg &status);

// Vacuum control helpers
float getUserTrustSpeed();
float getUserSleeveSpeed();

// Motion sync function
void syncMotionToAnimation();

// Lube control functions
void triggerStartLube(float seconds);
void triggerLubeShot(float seconds);

// Zuigen control functions
void sendToggleZuigenCommand();

// Real-time vacuum control
void sendImmediateArrowUpdate();

// Body ESP AI Overrule Handlers (UITGEBREID)
void handleBodyESPAISpeedControl(const bodyESP_message_t &msg);
void handleBodyESPAIVacuumControl(const bodyESP_message_t &msg);
void handleBodyESPAISessionControl(const bodyESP_message_t &msg);
void handleBodyESPAILedControl(const bodyESP_message_t &msg);

// Body ESP Biomedical AI Handlers (NEW)
void handleBodyESPBioFeedback(const bodyESP_message_t &msg);
void handleBodyESPSafetyOverride(const bodyESP_message_t &msg);
void handleBodyESPComfortAdjust(const bodyESP_message_t &msg);
void processAIDecisions(const bodyESP_message_t &msg);

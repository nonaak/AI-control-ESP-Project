#include "espnow_comm.h"
#include "ui.h"
#include "vacuum.h"
#include "config.h"

// External Vibe state from ui.cpp
extern bool vibeState;
#include <esp_wifi.h>

// ===============================================================================
// MAC ADDRESSES
// ===============================================================================
const uint8_t bodyESP_MAC[6] = {0xEC, 0xDA, 0x3B, 0x98, 0xC5, 0x24};  // Body ESP - T-HMI ESP32-S3
const uint8_t pumpUnit_MAC[6] = {0x62, 0x01, 0x94, 0x59, 0x18, 0x86}; // Pomp Unit - All on Ch 4 (AP mode MAC)
const uint8_t m5atom_MAC[6] = {0x30, 0x83, 0x98, 0xE2, 0xC7, 0x64};   // M5StickC Plus - All on Ch 4

// ===============================================================================
// HELPER FUNCTIONS
// ===============================================================================

// Converteer mbar naar cmHg (1 mbar = 0.75 cmHg)
float convertMbarToCmHg(float mbar) {
  return mbar * 0.75f;  // 1 mbar = 0.75 cmHg
}

// Legacy kPa converter (not used anymore)
float convertKPaToCmHg(float kPa) {
  return kPa * 7.5f;  // Gebruik 7.5 voor eenvoud (nauwkeurig genoeg)
}

// ===============================================================================
// GLOBAL VARIABLES DEFINITIONS
// ===============================================================================

// Machine state - VACUUM SYSTEEM
float currentVacuumSetpoint = -20.0f;    // cmHg (user setting)
float currentVacuumReading = 0.0f;       // cmHg (from PumpStatusMsg)
bool vacuumPumpStatus = false;           // From telemetry
bool lubePumpStatus = false;             // From telemetry
bool aiOverruleActive = false;           // From Body ESP
bool emergencyStop = false;

// Session control
uint32_t punchCount = 0;
uint32_t punchGoal = 25;
bool arrowFull = false;
bool sessionActive = false;

// Motion data from M5Atom
int8_t currentMotionDir = 0;     // Start in STILL state
uint8_t currentMotionSpeed = 0;  // Start at 0 speed

// "ZUIGEN" mode variables - TRACKED FROM PUMP UNIT
bool pompUnitZuigActive = false;        // Zuig active status from Pump Unit
float zuigTargetVacuum = -18.0f;        // Target vacuum for zuig mode (UI setting)

// Communication status
uint32_t bodyESP_lastContact = 0;
uint32_t pumpUnit_lastContact = 0;
uint32_t m5atom_lastContact = 0;
bool bodyESP_connected = false;
bool pumpUnit_connected = false;
bool m5atom_connected = false;

// AI override values from Body ESP
float bodyESP_trustOverride = 1.0f;      // Default: geen override
float bodyESP_sleeveOverride = 1.0f;     // Default: geen override

// Timing constants
const uint32_t BODY_ESP_UPDATE_INTERVAL = 500;   // 500ms (2Hz)
const uint32_t M5ATOM_UPDATE_INTERVAL = 250;     // 250ms (4Hz)
const uint32_t BODY_ESP_TIMEOUT = 10000;         // 10 seconds
const uint32_t PUMP_UNIT_TIMEOUT = 5000;         // 5 seconds
const uint32_t M5ATOM_TIMEOUT = 15000;           // 15 seconds
const uint32_t PUMP_HEARTBEAT_INTERVAL = 1000;   // 1 second heartbeat

// Internal timing variables
static uint32_t lastBodyESPUpdate = 0;
static uint32_t lastM5AtomUpdate = 0;
static uint32_t lastPumpControlUpdate = 0;
static uint32_t lastPumpHeartbeat = 0;
const uint32_t PUMP_CONTROL_UPDATE_INTERVAL = 2000; // Send pump control every 2 seconds

// ===============================================================================
// ESP-NOW SETUP
// ===============================================================================
void initESPNow() {
  // Initialize WiFi in station mode
  WiFi.mode(WIFI_STA);
  
  // Set WiFi channel to 4 (same as M5Atom and Pump Unit)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(4, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  Serial.printf("[ESP-NOW] WiFi channel forced to 4\n");
  
  // Initialize ESP-NOW after channel setup
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init failed!");
    return;
  }
  
  Serial.println("[ESP-NOW] Initialized successfully");
  Serial.printf("[ESP-NOW] WiFi Channel: %d\n", WiFi.channel());
  esp_now_register_recv_cb(onESPNowReceive);
  
  // Use channel 4 for all peers (consistent with M5Atom and Pump Unit)
  const int peerChannel = 4;  // Fixed channel 4
  Serial.printf("[ESP-NOW] Using channel 4 for all peers\n");
  
  // Add Body ESP peer
  esp_now_peer_info_t bodyPeer;
  memset(&bodyPeer, 0, sizeof(bodyPeer));
  memcpy(bodyPeer.peer_addr, bodyESP_MAC, 6);
  bodyPeer.channel = peerChannel;  // Channel 4
  bodyPeer.encrypt = false;
  if (esp_now_add_peer(&bodyPeer) == ESP_OK) {
    Serial.println("[ESP-NOW] Body ESP peer added (channel 4)");
  } else {
    Serial.println("[ESP-NOW] Failed to add Body ESP peer");
  }
  
  // Add Pomp Unit peer
  esp_now_peer_info_t pumpPeer;
  memset(&pumpPeer, 0, sizeof(pumpPeer));
  memcpy(pumpPeer.peer_addr, pumpUnit_MAC, 6);
  pumpPeer.channel = peerChannel;  // Channel 4
  pumpPeer.encrypt = false;
  if (esp_now_add_peer(&pumpPeer) == ESP_OK) {
    Serial.println("[ESP-NOW] Pump Unit peer added (channel 4)");
  } else {
    Serial.println("[ESP-NOW] Failed to add Pump Unit peer");
  }
  
  // Add M5Atom peer
  esp_now_peer_info_t atomPeer;
  memset(&atomPeer, 0, sizeof(atomPeer));
  memcpy(atomPeer.peer_addr, m5atom_MAC, 6);
  atomPeer.channel = peerChannel;  // Channel 4
  atomPeer.encrypt = false;
  if (esp_now_add_peer(&atomPeer) == ESP_OK) {
    Serial.println("[ESP-NOW] M5Atom peer added (channel 4)");
  } else {
    Serial.println("[ESP-NOW] M5Atom failed to add peer");
  }
}

// ===============================================================================
// RECEIVE CALLBACK
// ===============================================================================
void onESPNowReceive(const esp_now_recv_info *info, const uint8_t *data, int len) {
  // Reduced debug spam - only show important messages
  
  // Check sender MAC address
  if (memcmp(info->src_addr, bodyESP_MAC, 6) == 0) {
    // Message from Body ESP (uitgebreide AI overrule)
    if (len == sizeof(bodyESP_message_t)) {
      bodyESP_message_t msg;
      memcpy(&msg, data, sizeof(msg));
      handleBodyESPMessage(msg);
    } else {
      Serial.printf("[ESP-NOW] Body ESP message size mismatch: got %d, expected %d\n", len, sizeof(bodyESP_message_t));
    }
  }
  else if (memcmp(info->src_addr, pumpUnit_MAC, 6) == 0) {
    // Message from Pump Unit
    if (len == sizeof(PumpStatusMsg)) {
      PumpStatusMsg status;
      memcpy(&status, data, sizeof(status));
      
      if (status.version == 1) {
        handlePumpUnitMessage(status);
      } else {
        Serial.printf("[ESP-NOW] Pump Unit version mismatch: got %d, expected 1\n", status.version);
      }
    } else {
      Serial.printf("[ESP-NOW] Pump Unit message size mismatch: got %d, expected %d\n", len, sizeof(PumpStatusMsg));
    }
  }
  else if (memcmp(info->src_addr, m5atom_MAC, 6) == 0) {
    // Message from M5Atom
    if (len == sizeof(atomMotion_message_t)) {
      // Motion data message
      atomMotion_message_t msg;
      memcpy(&msg, data, sizeof(msg));
      Serial.printf("[ESP-NOW] M5Atom motion: ver=%d, kind=%d\n", msg.ver, msg.kind);
      if (msg.ver == 1 && msg.kind == 1) {  // MK_ATOM_MOTION
        handleM5AtomMotionMessage(msg);
      } else {
        Serial.printf("[ESP-NOW] M5Atom motion header mismatch: ver=%d (exp:1), kind=%d (exp:1)\n", msg.ver, msg.kind);
      }
    }
    else if (len == sizeof(m5atom_command_message_t)) {
      // Command message
      Serial.println("[ESP-NOW] M5Atom command message");
      m5atom_command_message_t msg;
      memcpy(&msg, data, sizeof(msg));
      handleM5AtomCommandMessage(msg);
    }
    else {
      Serial.printf("[ESP-NOW] M5Atom unknown message size: %d (motion:%d, cmd:%d)\n", 
                    len, sizeof(atomMotion_message_t), sizeof(m5atom_command_message_t));
    }
  }
  else {
    Serial.printf("[ESP-NOW] Message from UNKNOWN MAC: %02X:%02X:%02X:%02X:%02X:%02X (len=%d)\n",
                  info->src_addr[0], info->src_addr[1], info->src_addr[2],
                  info->src_addr[3], info->src_addr[4], info->src_addr[5], len);
    Serial.printf("[ESP-NOW] Expected Pump Unit MAC: 62:01:94:59:18:86\n");
    Serial.printf("[ESP-NOW] Expected Body ESP MAC:  08:D1:F9:DC:C3:A4\n");
    Serial.printf("[ESP-NOW] Expected M5Atom MAC:    30:83:98:E2:C7:64\n");
  }
}

// ===============================================================================
// MESSAGE HANDLERS
// ===============================================================================

void handleBodyESPMessage(const bodyESP_message_t &msg) {
  bodyESP_lastContact = millis();
  bodyESP_connected = true;
  
  Serial.printf("[RX Body ESP] Cmd:'%s' Trust:%.2f Sleeve:%.2f Active:%d\n",
                msg.command, msg.newTrust, msg.newSleeve, msg.overruleActive);
  
  // AI override functies
  if (strcmp(msg.command, "AI_OVERRIDE") == 0) {
    // AI override actief - pas trust/sleeve factors aan
    bodyESP_trustOverride = (msg.newTrust < 0.0f) ? 0.0f : (msg.newTrust > 1.0f) ? 1.0f : msg.newTrust;
    bodyESP_sleeveOverride = (msg.newSleeve < 0.0f) ? 0.0f : (msg.newSleeve > 1.0f) ? 1.0f : msg.newSleeve;
    aiOverruleActive = msg.overruleActive;
    
    Serial.printf("[AI Override] Trust factor: %.2f, Sleeve factor: %.2f\n", 
                  bodyESP_trustOverride, bodyESP_sleeveOverride);
  }
  else if (strcmp(msg.command, "HEARTBEAT") == 0) {
    // Heartbeat update - connection status al bijgewerkt
  }
  else if (strcmp(msg.command, "EMERGENCY_STOP") == 0) {
    Serial.println("[EMERGENCY] Body ESP emergency stop!");
    handleEmergencyStop();
  }
  else if (strcmp(msg.command, "AI_TEST_START") == 0) {
    Serial.printf("[AI TEST] AI neemt controle - target trust: %.1f\n", msg.newTrust);
    // Converteer trust speed naar speed step (0.0-2.0 trust -> 0-7 steps)
    extern uint8_t g_speedStep;
    extern bool paused;
    float clampedTrust = max(0.0f, min(2.0f, msg.newTrust));
    g_speedStep = (uint8_t)(clampedTrust / 2.0f * 7.0f);  // 0-7 bereik
    paused = false;   // Zorg dat animatie loopt
    Serial.printf("[AI TEST] Trust %.1f -> Speed step %d, unpaused\n", msg.newTrust, g_speedStep);
  }
  else if (strcmp(msg.command, "AI_RESUME_SLOW") == 0) {
    Serial.printf("[AI TEST] AI herstart - target trust: %.1f\n", msg.newTrust);
    // Converteer trust speed naar speed step
    extern uint8_t g_speedStep;
    extern bool paused;
    float clampedTrust = max(0.0f, min(2.0f, msg.newTrust));
    g_speedStep = (uint8_t)(clampedTrust / 2.0f * 7.0f);  // 0-7 bereik
    paused = false;   // Zorg dat animatie loopt
    Serial.printf("[AI TEST] Trust %.1f -> Speed step %d, unpaused\n", msg.newTrust, g_speedStep);
  }
  else if (strcmp(msg.command, "AI_STRESS_START") == 0) {
    Serial.printf("[AI STRESS] AI Stress Management start - trust: %.1f\n", msg.newTrust);
    extern uint8_t g_speedStep;
    extern bool paused;
    float clampedTrust = max(0.0f, min(2.0f, msg.newTrust));
    g_speedStep = (uint8_t)(clampedTrust / 2.0f * 7.0f);
    paused = false;
    Serial.printf("[AI STRESS] Start vanaf speed step %d\n", g_speedStep);
  }
  else if (strcmp(msg.command, "AI_STRESS_ADJUST") == 0) {
    Serial.printf("[AI STRESS] AI aanpassing - trust: %.1f\n", msg.newTrust);
    extern uint8_t g_speedStep;
    float clampedTrust = max(0.0f, min(2.0f, msg.newTrust));
    g_speedStep = (uint8_t)(clampedTrust / 2.0f * 7.0f);
    Serial.printf("[AI STRESS] Speed aangepast naar %d\n", g_speedStep);
  }
  else if (strcmp(msg.command, "AI_STRESS_RESUME") == 0) {
    Serial.printf("[AI STRESS] AI resume na pauze - trust: %.1f\n", msg.newTrust);
    extern uint8_t g_speedStep;
    extern bool paused;
    float clampedTrust = max(0.0f, min(2.0f, msg.newTrust));
    g_speedStep = (uint8_t)(clampedTrust / 2.0f * 7.0f);
    paused = false;
    Serial.printf("[AI STRESS] Resume naar speed %d, unpaused\n", g_speedStep);
  }
  else if (strcmp(msg.command, "AI_VIBE_ON") == 0) {
    Serial.println("[AI STRESS] AI activates VIBE - stress level 7 emergency!");
    vibeState = true;
    Serial.printf("[AI VIBE] Vibe automatisch ingeschakeld door AI (stress 7)\n");
  }
  else if (strcmp(msg.command, "AI_VIBE_OFF") == 0) {
    Serial.println("[AI STRESS] AI deactivates VIBE");
    vibeState = false;
    Serial.printf("[AI VIBE] Vibe automatisch uitgeschakeld door AI\n");
  }
  else if (strcmp(msg.command, "AI_VACUUM_ON") == 0) {
    Serial.println("[AI STRESS] AI activates VACUUM - stress level 7 emergency!");
    // Controleer of vacuum al actief is
    if (!pompUnitZuigActive) {
      Serial.println("[AI VACUUM] Vacuum was uit - activeren via toggle commando");
      sendToggleZuigenCommand();
    } else {
      Serial.println("[AI VACUUM] Vacuum was al actief - geen actie nodig");
    }
  }
  else if (strcmp(msg.command, "AI_VACUUM_OFF") == 0) {
    Serial.println("[AI STRESS] AI deactivates VACUUM");
    // Controleer of vacuum actief is
    if (pompUnitZuigActive) {
      Serial.println("[AI VACUUM] Vacuum was aan - deactiveren via toggle commando");
      sendToggleZuigenCommand();
    } else {
      Serial.println("[AI VACUUM] Vacuum was al uit - geen actie nodig");
    }
  }
  else if (strcmp(msg.command, "PLAYBACK_STRESS") == 0) {
    // Handle playback stress level data from Body ESP
    Serial.printf("[PLAYBACK] Stress level %d, Vibe: %s, Zuigen: %s received during playback\n", 
                  msg.stressLevel, msg.vibeOn ? "ON" : "OFF", msg.zuigenOn ? "ON" : "OFF");
    
    // Convert stress level (1-7) to speed step (0-6)
    extern uint8_t g_speedStep;
    extern bool paused;
    
    // Map stress level (1-7) to speed step (0-6)
    g_speedStep = msg.stressLevel - 1;  // 1->0, 2->1, ..., 7->6
    
    // Clamp to valid range (0-6, max speed step is 7)
    if (g_speedStep > 6) g_speedStep = 6;
    
    // Update vibe status voor playback
    vibeState = msg.vibeOn;
    
    // Update zuigen status voor playback
    static bool lastPlaybackZuigen = false;
    if (msg.zuigenOn != lastPlaybackZuigen) {
      if (msg.zuigenOn && !pompUnitZuigActive) {
        Serial.println("[PLAYBACK] Activating zuigen for playback");
        sendToggleZuigenCommand();
      } else if (!msg.zuigenOn && pompUnitZuigActive) {
        Serial.println("[PLAYBACK] Deactivating zuigen for playback");
        sendToggleZuigenCommand();
      }
      lastPlaybackZuigen = msg.zuigenOn;
    }
    
    // NOTE: Respecteer C-knop pause - forceer NIET paused = false
    // De gebruiker kan nog steeds pauzeren met C-knop tijdens playback
    
    Serial.printf("[PLAYBACK] Animation updated - Speed: %d, Vibe: %s, Zuigen: %s, Paused: %s\n", 
                  g_speedStep, vibeState ? "ON" : "OFF", pompUnitZuigActive ? "ON" : "OFF", paused ? "YES" : "NO");
  }
  else if (strcmp(msg.command, "PLAYBACK_STOP") == 0) {
    // Handle playback stop notification from Body ESP
    Serial.println("[PLAYBACK] Playback stopped - returning to manual control");
    
    // Optionally reset to a neutral state or keep current settings
    // For now, just log the event and let user continue with current settings
  }
  else if (strcmp(msg.command, "AI_EMERGENCY_OVERRIDE") == 0) {
    Serial.println("[AI EMERGENCY] C-knop emergency override - stress level 7!");
    Serial.println("[AI EMERGENCY] Activeer safe mode: pauze, min speed, vibe uit, zuigen uit");
    
    // 1. Zet animatie op pauze
    extern bool paused;
    paused = true;
    
    // 2. Snelheid naar minimum (0)
    extern uint8_t g_speedStep;
    g_speedStep = 0;
    
    // 3. Vibe UIT
    vibeState = false;
    
    // 4. Zuigen UIT (als het aan staat)
    if (pompUnitZuigActive) {
      Serial.println("[AI EMERGENCY] Zuigen deactiveren via toggle commando");
      sendToggleZuigenCommand();
    }
    
    // 5. AI overrides resetten naar neutraal
    bodyESP_trustOverride = 1.0f;
    bodyESP_sleeveOverride = 1.0f;
    aiOverruleActive = false;
    
    Serial.println("[AI EMERGENCY] Safe mode actief - gebruiker heeft volledige controle");
  }
  else {
    // Unknown command - log for debugging
    Serial.printf("[WARNING] Unknown command from Body ESP: '%s' (Trust:%.2f, Sleeve:%.2f, StressLevel:%d, Vibe:%d, Zuigen:%d)\n", 
                  msg.command, msg.newTrust, msg.newSleeve, msg.stressLevel, msg.vibeOn, msg.zuigenOn);
  }
}


void handlePumpUnitMessage(const PumpStatusMsg &msg) {
  pumpUnit_lastContact = millis();
  pumpUnit_connected = true;
  
  // 🎯 Update vacuum display met real-time HX711 data
  currentVacuumReading = msg.current_vacuum_cmHg;
  vacuumPumpStatus = msg.vacuum_pump_on;
  lubePumpStatus = msg.lube_pump_on;
  
  // Update zuigen status van Pomp Unit - voor auto vacuum uitsluiting
  static bool prevZuigActive = false;
  pompUnitZuigActive = (msg.zuig_active == 1);  // Update globale variabele
  
  if (pompUnitZuigActive != prevZuigActive) {
    Serial.printf("[RX Pump] Zuigen status changed: %d->%d\n", prevZuigActive, pompUnitZuigActive);
    prevZuigActive = pompUnitZuigActive;
  }
  
  // System health monitoring
  if (strcmp(msg.system_status, "ERROR") == 0) {
    handlePumpError();
  }
  
  // Debug output - include zuig status
  float vacuum_mbar = msg.current_vacuum_cmHg / 0.75f;
  Serial.printf("[RX Pump] Vacuum: %.0f mbar, VacPump:%d, ZuigActive:%d, Status:%s\n",
                abs(vacuum_mbar), msg.vacuum_pump_on, pompUnitZuigActive, msg.system_status);
  
  // Forward naar Body ESP voor AI/logging - TODO: implementeer later
  // forwardVacuumDataToBodyESP(msg);
}

void handleM5AtomMotionMessage(const atomMotion_message_t &msg) {
  m5atom_lastContact = millis();
  m5atom_connected = true;
  
  // Update motion data
  currentMotionDir = msg.dir;
  currentMotionSpeed = msg.speed;
  
  const char* dirStr = (msg.dir > 0) ? "UP" : (msg.dir < 0) ? "DOWN" : "STILL";
  Serial.printf("[RX M5Atom] ✅ Motion: %s(%d) Speed:%u%%\n", dirStr, msg.dir, msg.speed);
  
  // Reset connection lost warning
  static bool wasDisconnected = true;
  if (wasDisconnected) {
    Serial.println("[M5ATOM] 🟢 Connection restored!");
    wasDisconnected = false;
  }
}

void handleM5AtomCommandMessage(const m5atom_command_message_t &msg) {
  m5atom_lastContact = millis();
  m5atom_connected = true;
  
  Serial.printf("[RX M5Atom] Command: %s, Trust: %.1f, Sleeve: %.1f\n",
                msg.command, msg.trustSpeed, msg.sleeveSpeed);
  
  if (strcmp(msg.command, "START") == 0) {
    sessionActive = true;
    emergencyStop = false;
  }
  else if (strcmp(msg.command, "STOP") == 0) {
    sessionActive = false;
  }
  else if (strcmp(msg.command, "EMERGENCY") == 0) {
    handleEmergencyStop();
  }
  // TODO: Implementeer andere remote commando's
}

// ===============================================================================
// SEND FUNCTIONS
// ===============================================================================

void sendPumpControlMessage(const MainMsgV3 &msg) {
  // SIMPLE VERSION - geen delays voor snellere auto vacuum
  esp_err_t result = esp_now_send(pumpUnit_MAC, (uint8_t*)&msg, sizeof(msg));
  
  if (result == ESP_OK) {
    Serial.printf("[TX Pump OK] arrow_full=%d, force_state=%d, vacuum=%.1f", 
                  msg.arrow_full, msg.force_pump_state, msg.vacuum_set_x10/10.0f);
    if (msg.cmd_toggle_zuigen) {
      Serial.printf(", TOGGLE_ZUIGEN: target=%.1f", msg.zuig_target_x10/10.0f);
    }
    if (msg.cmd_lube_prime_now || msg.cmd_lube_shot_now) {
      Serial.printf(", LUBE: prime=%d shot=%d", msg.cmd_lube_prime_now, msg.cmd_lube_shot_now);
    }
    Serial.println("");
  } else {
    Serial.printf("[TX Pump ERROR] Send failed: %d\n", result);
  }
}

void sendBodyESPStatusUpdate(const machineStatus_message_t &msg) {
  esp_err_t result = esp_now_send(bodyESP_MAC, (uint8_t*)&msg, sizeof(msg));
  if (result != ESP_OK) {
    Serial.printf("[TX Body ESP ERROR] Send failed: %d\n", result);
  }
}

void sendM5AtomStatusUpdate(const monitoring_message_t &msg) {
  esp_err_t result = esp_now_send(m5atom_MAC, (uint8_t*)&msg, sizeof(msg));
  if (result == ESP_OK) {
    Serial.printf("[TX M5Atom Status OK] Trust:%.1f, Sleeve:%.1f, Status:%s\n", 
                  msg.trustSpeed, msg.sleeveSpeed, msg.status);
  } else {
    Serial.printf("[TX M5Atom Status ERROR] Send failed: %d\n", result);
  }
}

void sendM5AtomPumpColors(const pumpColors_message_t &msg) {
  esp_err_t result = esp_now_send(m5atom_MAC, (uint8_t*)&msg, sizeof(msg));
  if (result == ESP_OK) {
    Serial.printf("[TX M5Atom Colors OK] Sent pump colors: A(%d,%d,%d) B(%d,%d,%d), flags=0x%02X (VacLED:%d DebugLED:%d)\n", 
                  msg.a_r, msg.a_g, msg.a_b, msg.b_r, msg.b_g, msg.b_b, msg.flags, 
                  (msg.flags & 0x04) ? 1 : 0, (msg.flags & 0x08) ? 1 : 0);
  } else {
    Serial.printf("[TX M5Atom Colors ERROR] Send failed: %d (ESP_ERR_ESPNOW_NOT_INIT=%d)\n", 
                  result, ESP_ERR_ESPNOW_NOT_INIT);
  }
}

// ===============================================================================
// CONTROL FUNCTIONS
// ===============================================================================

void updateVacuumControl() {
  // Zuig control is now fully managed by the Pump Unit
  // This function is kept for compatibility but does minimal work
  
  // Debug output for zuig status changes
  static bool lastZuigActive = false;
  static uint32_t lastDebugTime = 0;
  uint32_t now = millis();
  
  if (pompUnitZuigActive != lastZuigActive) {
    const char* statusStr = pompUnitZuigActive ? "ACTIVATED" : "DEACTIVATED";
    Serial.printf("[ZUIG STATUS] Pump Unit zuig mode %s\n", statusStr);
    lastZuigActive = pompUnitZuigActive;
  }
  
  // Periodic status debug every 10 seconds when zuig is active
  if (pompUnitZuigActive && (now - lastDebugTime > 10000)) {
    lastDebugTime = now;
    Serial.printf("[ZUIG STATUS] Active - Vacuum: %.0f mbar, Target: %.0f mbar\n", 
                  abs(currentVacuumReading / 0.75f), abs(CFG.vacuumTargetMbar));
  }
}

void sendPumpControlMessages() {
  uint32_t now = millis();
  
  // Skip sending als Pump Unit niet connected is
  if (!pumpUnit_connected) {
    return;  // Graceful handling - geen foutmelding, gewoon skippen
  }
  
  // Send periodic updates every 2 seconds (ORIGINAL SIMPLE VERSION)
  if (now - lastPumpControlUpdate < PUMP_CONTROL_UPDATE_INTERVAL) {
    return;  // Skip sending to reduce ESP-NOW traffic
  }
  
  lastPumpControlUpdate = now;
  
  // Synchronize vacuum setpoint from UI settings (mbar to cmHg)
  currentVacuumSetpoint = convertMbarToCmHg(CFG.vacuumTargetMbar);
  
  // Regular pump control update
  MainMsgV3 msg;
  memset(&msg, 0, sizeof(msg));
  
  msg.version = 3;
  msg.arrow_full = arrowFull ? 1 : 0;
  msg.session_started = sessionActive ? 1 : 0;
  msg.punch_count = punchCount;
  msg.punch_goal = punchGoal;
  msg.vacuum_set_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  msg.force_pump_state = 0; // AUTO
  
  // Target vacuum wordt alleen gebruikt voor normale vacuum setpoint
  msg.zuig_target_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  
  sendPumpControlMessage(msg);
  
  // Debug output voor conversie verificatie
  Serial.printf("[VACUUM SYNC] UI: %.0f mbar -> Pomp Unit: %.0f mbar (via cmHg)\n", 
                abs(CFG.vacuumTargetMbar), abs(CFG.vacuumTargetMbar));
}

// Send keepalive heartbeat to maintain ESP-NOW connection
void sendPumpHeartbeat() {
  uint32_t now = millis();
  
  if (!pumpUnit_connected || (now - lastPumpHeartbeat < PUMP_HEARTBEAT_INTERVAL)) {
    return;
  }
  
  lastPumpHeartbeat = now;
  
  // Send minimal heartbeat message
  MainMsgV3 heartbeat;
  memset(&heartbeat, 0, sizeof(heartbeat));
  
  heartbeat.version = 3;
  heartbeat.arrow_full = arrowFull ? 1 : 0;
  heartbeat.session_started = sessionActive ? 1 : 0;
  heartbeat.vacuum_set_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  heartbeat.force_pump_state = 0; // AUTO
  
  // Use faster send without retries for heartbeat (don't spam logs)
  esp_err_t result = esp_now_send(pumpUnit_MAC, (uint8_t*)&heartbeat, sizeof(heartbeat));
  if (result != ESP_OK) {
    static uint32_t lastHeartbeatError = 0;
    if (now - lastHeartbeatError > 5000) { // Only log every 5 seconds
      Serial.printf("[HEARTBEAT] Failed to send to Pump Unit: %d\n", result);
      lastHeartbeatError = now;
    }
  }
}

void sendStatusUpdates() {
  uint32_t now = millis();
  
  // Heartbeat to all modules every 5 seconds - zelfs als niet connected
  static uint32_t lastHeartbeat = 0;
  if (now - lastHeartbeat > 5000) {
    lastHeartbeat = now;
    Serial.println("[HEARTBEAT] Broadcasting to all modules...");
    
    // Send heartbeat to M5Atom via monitoring message
    monitoring_message_t heartbeatMsg;
    memset(&heartbeatMsg, 0, sizeof(heartbeatMsg));
    strcpy(heartbeatMsg.status, "HEARTBEAT");
    heartbeatMsg.trustSpeed = getUserTrustSpeed();
    heartbeatMsg.sleeveSpeed = getUserSleeveSpeed();
    sendM5AtomStatusUpdate(heartbeatMsg);
  }
  
  // Status naar Body ESP (2Hz) - alleen als connected
  if (bodyESP_connected && (now - lastBodyESPUpdate >= BODY_ESP_UPDATE_INTERVAL)) {
    lastBodyESPUpdate = now;
    
    machineStatus_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    // Finale speeds met AI override factors
    msg.trust = getUserTrustSpeed() * bodyESP_trustOverride;
    msg.sleeve = getUserSleeveSpeed() * bodyESP_sleeveOverride;
    msg.suction = abs(currentVacuumReading);  // Positieve waarde voor display
    
    // Debug sleeve speed voor SnelH grafiek troubleshooting
    static uint32_t lastSleeveDebug = 0;
    if (millis() - lastSleeveDebug > 2000) {  // Elke 2 seconden
      Serial.printf("[SNELH DEBUG] g_speedStep=%d, getUserSleeveSpeed()=%.2f, final_sleeve=%.2f\n", 
                    g_speedStep, getUserSleeveSpeed(), msg.sleeve);
      lastSleeveDebug = millis();
    }
    msg.pause = 2.0f;  // Default pause tijd
    msg.vibeOn = vibeState;  // Handmatige VIBE toggle (Z-knop bit 3)
    msg.zuigActive = pompUnitZuigActive;  // Zuigen mode status van Pomp Unit
    msg.vacuumMbar = abs(currentVacuumReading / 0.75f);  // Convert cmHg naar mbar voor grafiek
    
    // C knop pause status (geïmporteerd van ui.cpp)
    msg.pauseActive = paused;
    
    // Lube sync systeem - SIMPEL: bij elke punch count reset animatie
    static uint32_t lastPunchCount = 0;
    
    // Check of er een nieuwe lube cyclus is gestart (punch count verhoogd)
    bool newLubeTrigger = (punchCount > lastPunchCount);
    if (newLubeTrigger) {
      lastPunchCount = punchCount;
      Serial.printf("[LUBE SYNC] Animatie reset! Punch: %lu, Speed: %.1f\n", punchCount, getUserTrustSpeed());
    }
    
    msg.lubeTrigger = newLubeTrigger;
    msg.cyclusTijd = getUserTrustSpeed();  // Stuur gewoon de speed door
    msg.sleevePercentage = getSleevePercentage();  // Echte sleeve positie
    
    // Stuur huidige versnelling mee
    extern uint8_t g_speedStep;
    msg.currentSpeedStep = g_speedStep;
    
    strcpy(msg.command, "STATUS_UPDATE");
    
    sendBodyESPStatusUpdate(msg);
  }
  
  // Status en Colors naar M5Atom (4Hz) - altijd versturen (M5Atom is optioneel)
  if (now - lastM5AtomUpdate >= M5ATOM_UPDATE_INTERVAL) {
    lastM5AtomUpdate = now;
    
    // Send pump colors for display
    pumpColors_message_t colorMsg;
    memset(&colorMsg, 0, sizeof(colorMsg));
    colorMsg.ver = 1;
    colorMsg.kind = 2;  // MK_PUMP_COLORS
    
    // Set pump colors based on pump status
    if (vacuumPumpStatus) {
      colorMsg.a_r = 255; colorMsg.a_g = 0; colorMsg.a_b = 255;  // Vacuum: Magenta
    } else {
      colorMsg.a_r = 0; colorMsg.a_g = 0; colorMsg.a_b = 0;     // Off: Black
    }
    
    if (lubePumpStatus) {
      colorMsg.b_r = 0; colorMsg.b_g = 255; colorMsg.b_b = 0;    // Lube: Green
    } else {
      colorMsg.b_r = 0; colorMsg.b_g = 0; colorMsg.b_b = 0;     // Off: Black
    }
    
    colorMsg.flags = (vacuumPumpStatus ? 0x01 : 0x00) | (lubePumpStatus ? 0x02 : 0x00);
    
    // LED commands: bit 2 = vacuum LED (follows vacuumPumpStatus), bit 3 = debug LED toggle
    if (vacuumPumpStatus) colorMsg.flags |= 0x04;  // Set bit 2 for vacuum LED
    
    // Vibe (bit 3) - controlled by Z button double-click
    if (vibeState) {
      colorMsg.flags |= 0x08;  // Set bit 3
      Serial.printf("[VIBE] vibeState=true, setting bit 3 in flags=0x%02X\n", colorMsg.flags);
    }
    
    sendM5AtomPumpColors(colorMsg);
    
    // Send status update
    monitoring_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.trustSpeed = getUserTrustSpeed();
    msg.sleeveSpeed = getUserSleeveSpeed();
    msg.aiOverruleActive = aiOverruleActive;
    msg.sessionTime = sessionActive ? (now / 1000.0f) : 0.0f;
    
    if (emergencyStop) strcpy(msg.status, "EMERGENCY");
    else if (sessionActive) strcpy(msg.status, "ACTIVE");
    else strcpy(msg.status, "PAUSED");
    
    sendM5AtomStatusUpdate(msg);
  }
}

void checkCommunicationTimeouts() {
  uint32_t now = millis();
  
  // Body ESP timeout check - GRACEFUL: Continue zonder AI overrides
  if (bodyESP_connected && (now - bodyESP_lastContact > BODY_ESP_TIMEOUT)) {
    handleBodyESPTimeout();
  }
  
  // Pump Unit timeout check met hysteresis - GRACEFUL: Continue met UI feedback
  static bool pumpTimeoutWarningShown = false;
  if (pumpUnit_connected && (now - pumpUnit_lastContact > PUMP_UNIT_TIMEOUT)) {
    if (!pumpTimeoutWarningShown) {
      Serial.printf("[WARNING] Pump Unit timeout: %lu ms since last contact\n", now - pumpUnit_lastContact);
      pumpTimeoutWarningShown = true;
    }
    
    // Extra grace period before marking as disconnected
    if (now - pumpUnit_lastContact > (PUMP_UNIT_TIMEOUT + 2000)) {
      Serial.println("[DISCONNECT] Pump Unit marked as offline - continuing without hardware");
      pumpUnit_connected = false;
      pumpTimeoutWarningShown = false;
      
      // Reset vacuum readings maar GEEN emergency stop
      currentVacuumReading = 0.0f;
      vacuumPumpStatus = false;
      lubePumpStatus = false;
      pompUnitZuigActive = false;  // Reset zuig status since hardware is offline
      
      Serial.println("[INFO] HoofdESP continues operation - UI and other modules functional");
    }
  } else if (!pumpUnit_connected && (now - pumpUnit_lastContact <= PUMP_UNIT_TIMEOUT)) {
    // Reconnection detected
    Serial.println("[RECONNECT] Pump Unit back online!");
    pumpUnit_connected = true;
    pumpTimeoutWarningShown = false;
  }
  
  // M5Atom timeout check - OPTIONEEL: gewoon markeren als offline
  if (m5atom_connected && (now - m5atom_lastContact > M5ATOM_TIMEOUT)) {
    Serial.println("[INFO] M5Atom communication timeout - marking as offline");
    m5atom_connected = false;
  } else if (!m5atom_connected && (now - m5atom_lastContact <= M5ATOM_TIMEOUT)) {
    // M5Atom reconnection
    Serial.println("[RECONNECT] M5Atom back online!");
    m5atom_connected = true;
  }
}

void performSafetyChecks() {
  // Controleer AI override bounds
  bodyESP_trustOverride = (bodyESP_trustOverride < 0.0f) ? 0.0f : (bodyESP_trustOverride > 1.0f) ? 1.0f : bodyESP_trustOverride;
  bodyESP_sleeveOverride = (bodyESP_sleeveOverride < 0.0f) ? 0.0f : (bodyESP_sleeveOverride > 1.0f) ? 1.0f : bodyESP_sleeveOverride;
  
  // Controleer vacuum limits
  if (currentVacuumReading < -100.0f || currentVacuumReading > 10.0f) {
    Serial.printf("[SAFETY] Abnormal vacuum reading: %.0f mbar\n", abs(currentVacuumReading / 0.75f));
  }
}

// ===============================================================================
// SAFETY FUNCTIONS
// ===============================================================================

void handlePumpError() {
  Serial.println("[SAFETY] Pump Unit ERROR state detected!");
  emergencyStop = true;
  sessionActive = false;
  
  // Stop alle activiteiten
  MainMsgV3 stopMsg;
  memset(&stopMsg, 0, sizeof(stopMsg));
  stopMsg.version = 3;
  stopMsg.force_pump_state = 1; // FORCE_OFF
  sendPumpControlMessage(stopMsg);
}

void handleBodyESPTimeout() {
  Serial.println("[WARNING] Body ESP communication timeout - continuing without AI overrides");
  bodyESP_connected = false;
  
  // Reset AI overrides naar neutrale waarden (geen beperking)
  bodyESP_trustOverride = 1.0f;  // Full speed available
  bodyESP_sleeveOverride = 1.0f; // Full speed available
  aiOverruleActive = false;       // Disable AI overrule indicators
  
  Serial.println("[INFO] System continues normal operation without AI - manual control available");
  // NOTE: sessionActive blijft ongewijzigd - gebruiker kan nog steeds handmatig besturen
}

void handleEmergencyStop() {
  Serial.println("[EMERGENCY] Emergency stop activated!");
  emergencyStop = true;
  sessionActive = false;
  aiOverruleActive = false;
  
  // Reset AI override factors
  bodyESP_trustOverride = 0.0f;
  bodyESP_sleeveOverride = 0.0f;
  
  // Onmiddellijk stop commando naar Pump Unit
  MainMsgV3 emergencyMsg;
  memset(&emergencyMsg, 0, sizeof(emergencyMsg));
  emergencyMsg.version = 3;
  emergencyMsg.force_pump_state = 1; // FORCE_OFF
  emergencyMsg.session_started = 0;
  sendPumpControlMessage(emergencyMsg);
}

void forwardVacuumDataToBodyESP(const PumpStatusMsg &status) {
  // Forward vacuum telemetrie naar Body ESP voor AI processing
  // Dit kan later worden uitgebreid met een specifieke telemetrie message
  // Voor nu gebruiken we de status update
}

// ===============================================================================
// MOTION SYNC FUNCTION - HOOFD FEATURE!
// ===============================================================================

// NIEUWE AANPAK: Motion sync als SUGGESTIE, niet als OVERRIDE
void syncMotionToAnimation() {
  static uint32_t lastMotionSync = 0;
  uint32_t now = millis();
  
  // Only sync every 200ms (minder agressief)
  if (now - lastMotionSync < 200) return;
  lastMotionSync = now;
  
  // Skip if motion blend is disabled
  if (!CFG.motionBlendEnabled) {
    static uint32_t lastDisabledMsg = 0;
    if (now - lastDisabledMsg > 5000) {
      lastDisabledMsg = now;
      Serial.println("[MOTION BLEND] Disabled - Nunchuk has full control");
    }
    return;
  }
  
  // Check if M5Atom motion data is recent and valid
  if (!m5atom_connected || (now - m5atom_lastContact > 1500)) {
    static uint32_t lastOfflineMsg = 0;
    if (now - lastOfflineMsg > 5000) {
      lastOfflineMsg = now;
      Serial.println("[MOTION BLEND] M5Atom offline - Nunchuk has full control");
    }
    return;
  }
  
  // Alleen blend als M5Atom significant beweging detecteert (> 15%)
  if (currentMotionSpeed < 15) {
    static uint32_t lastStillMsg = 0;
    if (now - lastStillMsg > 3000) {
      lastStillMsg = now;
      Serial.printf("[MOTION BLEND] M5Atom still (%d%%) - Nunchuk control\n", currentMotionSpeed);
    }
    return; // Geen blend bij lage beweging
  }
  
  extern uint8_t g_speedStep;
  
  // Zeer subtiele blend: alleen kleine aanpassingen
  float motionInfluence = (CFG.motionSpeedWeight / 100.0f) * 0.3f; // Max 30% van gewicht
  
  // Bereken motion suggestie
  uint8_t motionSuggestedStep = (currentMotionSpeed * (CFG.SPEED_STEPS - 1)) / 100;
  if (motionSuggestedStep >= CFG.SPEED_STEPS) motionSuggestedStep = CFG.SPEED_STEPS - 1;
  
  // Zeer kleine aanpassing richting motion
  int8_t adjustment = (int8_t)((motionSuggestedStep - g_speedStep) * motionInfluence);
  
  // Beperk aanpassing tot maximaal 1 step per keer
  if (adjustment > 1) adjustment = 1;
  else if (adjustment < -1) adjustment = -1;
  
  if (adjustment != 0) {
    uint8_t oldSpeed = g_speedStep;
    g_speedStep = (uint8_t)(g_speedStep + adjustment);
    
    // Clamp
    if (g_speedStep >= CFG.SPEED_STEPS) g_speedStep = CFG.SPEED_STEPS - 1;
    
    Serial.printf("[MOTION BLEND] Subtle adjust: %d->%d (M5:%d%% suggests %d, influence:%.1f%%)\n", 
                  oldSpeed, g_speedStep, currentMotionSpeed, motionSuggestedStep, motionInfluence*100);
  }
}

// ===============================================================================
// HELPER FUNCTIONS (CONTINUED)
// ===============================================================================

// g_speedStep and g_targetStrokes are now declared in ui.h and included above

float getUserTrustSpeed() {
  // Converteer speedStep naar trust speed (gebaseerd op bestaande logica)
  // CFG is already declared in config.h
  
  if (CFG.SPEED_STEPS <= 1) return 0.0f;
  float step01 = (float)g_speedStep / (float)(CFG.SPEED_STEPS - 1);
  float instF = CFG.MIN_SPEED_HZ + step01 * (CFG.MAX_SPEED_HZ - CFG.MIN_SPEED_HZ);
  
  // Converteer frequency naar trust speed (0.0-2.0 range)
  float trustSpeed = instF * 0.67f;
  return (trustSpeed < 0.0f) ? 0.0f : (trustSpeed > 2.0f) ? 2.0f : trustSpeed; // Schaal naar trust range
}

float getUserSleeveSpeed() {
  // Voor nu gebruiken we dezelfde logica als trust speed
  // Dit kan later aangepast worden voor onafhankelijke sleeve control
  return getUserTrustSpeed();
}

// Start-Lubric functie - triggered via menu
void triggerStartLube(float seconds) {
  if (!pumpUnit_connected) {
    Serial.println("[START-LUBE] Cannot trigger - Pump Unit offline!");
    return;
  }
  
  MainMsgV3 lubeMsg;
  memset(&lubeMsg, 0, sizeof(lubeMsg));
  
  lubeMsg.version = 3;
  lubeMsg.arrow_full = arrowFull ? 1 : 0;
  lubeMsg.session_started = 0;
  lubeMsg.punch_count = punchCount;
  lubeMsg.punch_goal = punchGoal;
  lubeMsg.vacuum_set_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  lubeMsg.force_pump_state = 0; // AUTO
  lubeMsg.cmd_lube_prime_now = 1; // TRIGGER LUBE PRIME (Start-Lubric)
  lubeMsg.lube_duration_ms = (uint16_t)(seconds * 1000.0f); // Converteer naar milliseconden
  
  sendPumpControlMessage(lubeMsg);
  Serial.printf("[START-LUBE] Lube prime command sent for %.1fs (%dms)!\n", seconds, lubeMsg.lube_duration_ms);
}

// Normale lube shot functie - voor gewone lube tijdens animatie
void triggerLubeShot(float seconds) {
  if (!pumpUnit_connected) {
    Serial.println("[LUBE-SHOT] Cannot trigger - Pump Unit offline!");
    return;
  }
  
  MainMsgV3 lubeMsg;
  memset(&lubeMsg, 0, sizeof(lubeMsg));
  
  lubeMsg.version = 3;
  lubeMsg.arrow_full = arrowFull ? 1 : 0;
  lubeMsg.session_started = 0;
  lubeMsg.punch_count = punchCount;
  lubeMsg.punch_goal = punchGoal;
  lubeMsg.vacuum_set_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  lubeMsg.force_pump_state = 0; // AUTO
  lubeMsg.cmd_lube_shot_now = 1; // TRIGGER LUBE SHOT (normale lube)
  lubeMsg.lube_duration_ms = (uint16_t)(seconds * 1000.0f); // Converteer naar milliseconden
  
  sendPumpControlMessage(lubeMsg);
  Serial.printf("[LUBE-SHOT] Normal lube shot sent for %.1fs (%dms)!\n", seconds, lubeMsg.lube_duration_ms);
}

// Toggle zuigen functie - stuur commando naar Pomp Unit
void sendToggleZuigenCommand() {
  if (!pumpUnit_connected) {
    Serial.println("[TOGGLE-ZUIGEN] Cannot send - Pump Unit offline!");
    return;
  }
  
  MainMsgV3 toggleMsg;
  memset(&toggleMsg, 0, sizeof(toggleMsg));
  
  toggleMsg.version = 3;
  toggleMsg.arrow_full = arrowFull ? 1 : 0;
  toggleMsg.session_started = 0;
  toggleMsg.punch_count = punchCount;
  toggleMsg.punch_goal = punchGoal;
  toggleMsg.vacuum_set_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  toggleMsg.force_pump_state = 0; // AUTO
  toggleMsg.cmd_toggle_zuigen = 1; // TOGGLE ZUIGEN COMMAND
  toggleMsg.zuig_target_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f); // Target in mbar
  
  sendPumpControlMessage(toggleMsg);
  Serial.println("[TOGGLE-ZUIGEN] Toggle zuigen command sent to Pump Unit!");
}

// Real-time arrow_full updates voor snelle auto vacuum response - SIMPLIFIED VERSION
void sendImmediateArrowUpdate() {
  if (!pumpUnit_connected) {
    return;  // Silent fail - pump unit offline
  }
  
  MainMsgV3 arrowMsg;
  memset(&arrowMsg, 0, sizeof(arrowMsg));
  
  arrowMsg.version = 3;
  arrowMsg.arrow_full = arrowFull ? 1 : 0;
  arrowMsg.session_started = sessionActive ? 1 : 0;
  arrowMsg.punch_count = punchCount;
  arrowMsg.punch_goal = punchGoal;
  arrowMsg.vacuum_set_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  arrowMsg.force_pump_state = 0; // AUTO
  arrowMsg.zuig_target_x10 = (int16_t)(convertMbarToCmHg(CFG.vacuumTargetMbar) * 10.0f);
  
  // Direct send zonder retry delays voor snelste response
  esp_err_t result = esp_now_send(pumpUnit_MAC, (uint8_t*)&arrowMsg, sizeof(arrowMsg));
  if (result == ESP_OK) {
    Serial.printf("[IMMEDIATE] arrow_full=%d sent directly\n", arrowMsg.arrow_full);
  } else {
    Serial.printf("[IMMEDIATE] FAILED: %d\n", result);
  }
}

/*
  MultiFunPlayer WebSocket Client Implementation
  
  Connects to MultiFunPlayer and receives real-time funscript data
  Applies ML autonomy to enhance/modify funscript actions
*/

#include "multifunplayer_client.h"
#include "body_config.h"

// Global instance
MultiFunPlayerClient mfpClient;
MultiFunPlayerClient* MultiFunPlayerClient::instance = nullptr;

// ===== CONFIGURATIE VERPLAATST NAAR body_config.h =====
// Alle MultiFunPlayer instellingen staan nu in body_config.h (BODY_CFG struct)
// Je kunt ze wijzigen in het body_config.h tabblad in Arduino IDE

// ===== Constructor =====
MultiFunPlayerClient::MultiFunPlayerClient() {
  connected = false;
  enabled = false;
  playing = false;
  scriptStartTime = 0;
  lastActionTime = 0;
  mlIntegrationEnabled = true;
  lastMLUpdate = 0;
  totalActions = 0;
  mlOverrides = 0;
  connectionAttempts = 0;
  
  // Default server settings
  serverHost = "127.0.0.1";
  serverPort = 8080;
  serverPath = "/";
  
  // Set static instance for callback
  instance = this;
}

// ===== Connection Management =====
void MultiFunPlayerClient::begin(const String& host, uint16_t port, const String& path) {
  serverHost = host;
  serverPort = port;
  serverPath = path;
  
  Serial.printf("[MFP] Initializing client: %s:%d%s\n", 
                serverHost.c_str(), serverPort, serverPath.c_str());
  
  webSocket.onEvent(webSocketEventStatic);
  enabled = true;
}

void MultiFunPlayerClient::connect() {
  if (!enabled) return;
  
  Serial.printf("[MFP] Connecting to %s:%d%s\n", 
                serverHost.c_str(), serverPort, serverPath.c_str());
  
  webSocket.begin(serverHost, serverPort, serverPath);
  webSocket.setReconnectInterval(5000);  // 5 second reconnect interval
  connectionAttempts++;
}

void MultiFunPlayerClient::disconnect() {
  webSocket.disconnect();
  connected = false;
  playing = false;
  Serial.println("[MFP] Disconnected from MultiFunPlayer");
}

// ===== Main Loop =====
void MultiFunPlayerClient::loop() {
  if (!enabled) return;
  
  webSocket.loop();
}

// ===== WebSocket Event Handler =====
void MultiFunPlayerClient::webSocketEventStatic(WStype_t type, uint8_t * payload, size_t length) {
  if (instance) {
    instance->onWebSocketEvent(type, payload, length);
  }
}

void MultiFunPlayerClient::onWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      connected = false;
      playing = false;
      Serial.println("[MFP] WebSocket Disconnected");
      break;
      
    case WStype_CONNECTED:
      connected = true;
      Serial.printf("[MFP] WebSocket Connected to: %s\n", payload);
      // Send identification message
      webSocket.sendTXT("{\"type\":\"identify\",\"device\":\"Body_ESP_ML\",\"version\":\"1.0\"}");
      break;
      
    case WStype_TEXT:
      {
        String message = String((char*)payload);
        processFunscriptMessage(message);
      }
      break;
      
    case WStype_ERROR:
      Serial.printf("[MFP] WebSocket Error: %s\n", payload);
      break;
      
    default:
      break;
  }
}

// ===== Message Processing =====
void MultiFunPlayerClient::processFunscriptMessage(const String& message) {
  DynamicJsonDocument doc(1024);
  
  if (deserializeJson(doc, message) != DeserializationError::Ok) {
    Serial.println("[MFP] JSON parse error");
    return;
  }
  
  String msgType = doc["type"].as<String>();
  
  if (msgType == "action") {
    // Funscript action received
    uint32_t timestamp = doc["timestamp"].as<uint32_t>();
    uint8_t position = doc["position"].as<uint8_t>();
    
    // Calculate speed based on time/position difference
    uint8_t speed = 0;
    if (lastAction.valid) {
      speed = calculateSpeed(FunscriptAction(timestamp, position, 0), lastAction);
    }
    
    FunscriptAction action(timestamp, position, speed);
    
    // Apply ML enhancement if enabled
    MLEnhancedAction enhancedAction;
    if (mlIntegrationEnabled) {
      enhancedAction = enhanceWithML(action);
    } else {
      enhancedAction.original = action;
      enhancedAction.mlPosition = position;
      enhancedAction.mlSpeed = funscriptToBodySpeed(speed);
      enhancedAction.reasoning = "Direct funscript (no ML)";
    }
    
    // Execute the action
    executeAction(enhancedAction);
    
    // Update statistics
    lastAction = action;
    lastActionTime = millis();
    totalActions++;
    
    Serial.printf("[MFP] Action: pos=%d→%d speed=%d→%d ML=%.0f%% (%s)\n",
                  action.position, enhancedAction.mlPosition,
                  action.speed, enhancedAction.mlSpeed,
                  enhancedAction.autonomyUsed * 100.0f,
                  enhancedAction.reasoning.c_str());
  }
  else if (msgType == "status") {
    processStatusMessage(message);
  }
  else if (msgType == "play") {
    startPlayback();
  }
  else if (msgType == "pause") {
    pausePlayback();
  }
  else if (msgType == "stop") {
    stopPlayback();
  }
}

void MultiFunPlayerClient::processStatusMessage(const String& message) {
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, message) == DeserializationError::Ok) {
    bool isPlaying = doc["playing"].as<bool>();
    uint32_t position = doc["position"].as<uint32_t>();
    
    if (isPlaying != playing) {
      playing = isPlaying;
      Serial.printf("[MFP] Playback %s at position %d\n", 
                    playing ? "started" : "stopped", position);
    }
  }
}

// ===== ML Enhancement =====
MLEnhancedAction MultiFunPlayerClient::enhanceWithML(const FunscriptAction& action) {
  MLEnhancedAction enhanced;
  enhanced.original = action;
  
  // Get current biometric data and ML decision
  BiometricData bio;
  bio.heartRate = 75.0f;      // TODO: Get from actual sensors
  bio.temperature = 36.8f;
  bio.gsrValue = 500.0f;
  bio.timestamp = millis();
  
  // Update stress manager with biometrics
  stressManager.update(bio);
  StressDecision decision = stressManager.getStressDecision();
  
  // Get ML autonomy level
  float autonomy = stressManager.getMLAutonomyLevel();
  enhanced.autonomyUsed = autonomy;
  
  if (autonomy > 0.0f) {
    // Apply ML autonomy
    if (decision.isMLOverride) {
      // ML wants to override
      float mlInfluence = autonomy * decision.confidence;
      
      // Blend funscript position with ML recommendation
      enhanced.mlPosition = (uint8_t)(
        action.position * (1.0f - mlInfluence) + 
        (decision.recommendedSpeed * 12.5f) * mlInfluence  // Convert speed 1-7 to position-like value
      );
      
      // ML determines speed
      enhanced.mlSpeed = decision.recommendedSpeed;
      
      // ML controls vibe/suction
      enhanced.vibeActive = decision.vibeRecommended;
      enhanced.suctionActive = decision.suctionRecommended;
      
      enhanced.reasoning = String("ML override: ") + decision.reasoning;
      mlOverrides++;
    } else {
      // ML allows funscript but may adjust
      enhanced.mlPosition = action.position;
      enhanced.mlSpeed = funscriptToBodySpeed(action.speed);
      
      // Apply minor ML adjustments
      if (decision.currentLevel >= STRESS_4_GEMIDDELD) {
        // High stress - reduce intensity
        enhanced.mlPosition = (uint8_t)(enhanced.mlPosition * 0.8f);
        enhanced.mlSpeed = max(1, enhanced.mlSpeed - 1);
        enhanced.reasoning = "ML: Reduced intensity (high stress)";
      } else {
        enhanced.reasoning = "ML: Following funscript";
      }
      
      enhanced.vibeActive = decision.vibeRecommended;
      enhanced.suctionActive = decision.suctionRecommended;
    }
  } else {
    // No ML autonomy - pure funscript
    enhanced.mlPosition = action.position;
    enhanced.mlSpeed = funscriptToBodySpeed(action.speed);
    enhanced.vibeActive = false;
    enhanced.suctionActive = false;
    enhanced.reasoning = "Pure funscript (0% autonomy)";
  }
  
  return enhanced;
}

// ===== Action Execution =====
void MultiFunPlayerClient::executeAction(const MLEnhancedAction& action) {
  // Stuur funscript commando's via ESP-NOW naar HoofdESP
  // De HoofdESP zal deze waarden gebruiken om de hardware aan te sturen
  
  Serial.printf("[MFP-EXEC] Pos=%d Speed=%d Vibe=%s Suction=%s (ML: %.0f%% - %s)\n",
                action.mlPosition, action.mlSpeed,
                action.vibeActive ? "ON" : "OFF",
                action.suctionActive ? "ON" : "OFF",
                action.autonomyUsed * 100.0f,
                action.reasoning.c_str());
  
  // Converteer funscript positie (0-100) naar trust/sleeve waarden
  // Position 0 = volledig ingetrokken, 100 = volledig uitgeschoven
  // Trust = snelheid van in/uit beweging (0.0-2.0)
  // Sleeve = snelheid van rotatie/torsie (0.0-2.0)
  
  // Map mlSpeed (1-7) naar trust speed (0.0-2.0)
  float trustValue = map(action.mlSpeed, 1, 7, 0, 200) / 100.0f;  // 1→0.0, 7→2.0
  
  // Map mlPosition (0-100) naar sleeve speed (0.0-2.0)
  // Dit is een vereenvoudigde mapping - kan later verfijnd worden
  float sleeveValue = action.mlPosition / 50.0f;  // 0→0.0, 50→1.0, 100→2.0
  
  // Stuur ESP-NOW bericht naar HoofdESP
  // Gebruik extern gedeclareerde functie uit Body_ESP.ino
  extern void sendESPNowMessage(float trust, float sleeve, bool overrule, 
                                const char* cmd, uint8_t stressLvl, 
                                bool vibe, bool zuig);
  
  sendESPNowMessage(
    trustValue,           // Trust speed berekend uit ML speed
    sleeveValue,         // Sleeve speed berekend uit ML positie
    true,                // Overrule actief (Funscript neemt over)
    "FUNSCRIPT_ACTION",  // Command type
    0,                   // Stress level (niet gebruikt in funscript mode)
    action.vibeActive,   // Vibe status van ML
    action.suctionActive // Suction status van ML
  );
}

// ===== Helper Functions =====
uint8_t MultiFunPlayerClient::calculateSpeed(const FunscriptAction& current, const FunscriptAction& previous) {
  if (!previous.valid) return 50;
  
  uint32_t timeDiff = current.timestamp - previous.timestamp;
  if (timeDiff == 0) return previous.speed;
  
  int32_t positionDiff = abs((int32_t)current.position - (int32_t)previous.position);
  
  // Speed calculation: distance/time, normalized to 0-100
  float speed = (float)positionDiff / (float)timeDiff * 1000.0f;  // per second
  speed = constrain(speed, 0.0f, 100.0f);
  
  return (uint8_t)speed;
}

// ===== Playback Control =====
void MultiFunPlayerClient::startPlayback() {
  playing = true;
  scriptStartTime = millis();
  Serial.println("[MFP] Playback started");
}

void MultiFunPlayerClient::stopPlayback() {
  playing = false;
  scriptStartTime = 0;
  Serial.println("[MFP] Playback stopped");
}

void MultiFunPlayerClient::pausePlayback() {
  playing = false;
  Serial.println("[MFP] Playback paused");
}

// ===== Status and Debug =====
void MultiFunPlayerClient::printStatus() const {
  Serial.println("[MFP] === MultiFunPlayer Client Status ===");
  Serial.printf("Connected: %s\n", connected ? "Yes" : "No");
  Serial.printf("Enabled: %s\n", enabled ? "Yes" : "No");
  Serial.printf("Playing: %s\n", playing ? "Yes" : "No");
  Serial.printf("ML Integration: %s\n", mlIntegrationEnabled ? "Yes" : "No");
  Serial.printf("Server: %s:%d%s\n", serverHost.c_str(), serverPort, serverPath.c_str());
  Serial.printf("Total Actions: %d\n", totalActions);
  Serial.printf("ML Overrides: %d (%.1f%%)\n", mlOverrides, getMLOverridePercentage());
  Serial.printf("Connection Attempts: %d\n", connectionAttempts);
  Serial.println("[MFP] ========================================");
}

String MultiFunPlayerClient::getStatusString() const {
  return String("MFP: ") + (connected ? "Connected" : "Disconnected") + 
         " (" + String(totalActions) + " actions)";
}

float MultiFunPlayerClient::getMLOverridePercentage() const {
  if (totalActions == 0) return 0.0f;
  return (float)mlOverrides / (float)totalActions * 100.0f;
}

// ===== Configuration =====
void MultiFunPlayerClient::setServer(const String& host, uint16_t port, const String& path) {
  serverHost = host;
  serverPort = port;
  serverPath = path;
  Serial.printf("[MFP] Server updated: %s:%d%s\n", host.c_str(), port, path.c_str());
}

void MultiFunPlayerClient::setReconnectInterval(uint32_t intervalMs) {
  webSocket.setReconnectInterval(intervalMs);
}

// ===== GEMAKKELIJKE SETUP FUNCTIE =====
void setupMultiFunPlayer() {
  Serial.println("[MFP] === MultiFunPlayer Setup ===");
  Serial.printf("[MFP] PC IP: %s\n", BODY_CFG.mfpPcIP.c_str());
  Serial.printf("[MFP] Port: %d\n", BODY_CFG.mfpPort);
  Serial.printf("[MFP] Path: %s\n", BODY_CFG.mfpPath.c_str());
  Serial.printf("[MFP] Auto-connect: %s\n", BODY_CFG.mfpAutoConnect ? "Yes" : "No");
  Serial.printf("[MFP] ML Integration: %s\n", BODY_CFG.mfpMLIntegration ? "Yes" : "No");
  
  // Initialize MultiFunPlayer client
  mfpClient.begin(BODY_CFG.mfpPcIP, BODY_CFG.mfpPort, BODY_CFG.mfpPath);
  mfpClient.enableMLIntegration(BODY_CFG.mfpMLIntegration);
  mfpClient.setReconnectInterval(BODY_CFG.mfpReconnectInterval);
  
  // Auto-connect if enabled
  if (BODY_CFG.mfpAutoConnect) {
    Serial.println("[MFP] Starting connection...");
    mfpClient.connect();
  }
  
  Serial.println("[MFP] Setup complete!");
  Serial.println("[MFP] === Instructions ===");
  Serial.println("[MFP] 1. Make sure PC and Body ESP are on same WiFi");
  Serial.println("[MFP] 2. Enable WebSocket Server in MultiFunPlayer");
  Serial.printf("[MFP] 3. Set MultiFunPlayer port to: %d\n", BODY_CFG.mfpPort);
  Serial.println("[MFP] 4. Load a .funscript file in MultiFunPlayer");
  Serial.println("[MFP] 5. Watch Serial Monitor for connection status");
  Serial.println("[MFP] =========================");
}

// ===== Helper Functions =====
MLEnhancedAction applyMLAutonomy(const FunscriptAction& action, float autonomyLevel) {
  // Use the global MultiFunPlayer client to enhance the action with ML
  if (mfpClient.isMLIntegrationEnabled()) {
    return mfpClient.enhanceWithML(action);
  } else {
    // No ML integration - return basic enhanced action
    MLEnhancedAction enhanced;
    enhanced.original = action;
    enhanced.mlPosition = action.position;
    enhanced.mlSpeed = funscriptToBodySpeed(action.speed);
    enhanced.vibeActive = false;
    enhanced.suctionActive = false;
    enhanced.autonomyUsed = 0.0f;
    enhanced.reasoning = "ML integration disabled";
    return enhanced;
  }
}

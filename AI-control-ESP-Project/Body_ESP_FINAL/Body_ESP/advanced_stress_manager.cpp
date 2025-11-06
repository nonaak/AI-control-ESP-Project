/*
  Advanced Stress Management System Implementation - Body ESP
*/

#include "advanced_stress_manager.h"
#include "ml_stress_analyzer.h"

// External function declarations (defined in Body_ESP.ino)
extern void startRecording();
extern void stopRecording();
extern bool isRecording;

// Global instance
AdvancedStressManager stressManager;

// ===== Constructor =====
AdvancedStressManager::AdvancedStressManager() {
  currentStressLevel = STRESS_0_NORMAAL;
  previousStressLevel = STRESS_0_NORMAAL;
  levelStartTime = 0;
  sessionStartTime = 0;
  sessionActive = false;
  historyIndex = 0;
  historyCount = 0;
  lastStressValue = 0.0f;
  lastStressTime = 0;
  mlEnabled = false;
  lastMLUpdate = 0;
  lastStressChange = CHANGE_NONE;
  
  // ML Autonomy initialization
  totalSessions = 0;
  currentAutonomyLevel = 0.0f;
  mlAutonomyActive = false;
}

// ===== Public Interface =====

void AdvancedStressManager::begin() {
  Serial.println("[STRESS] Advanced Stress Manager initialized");
  levelStartTime = millis();
  lastStressTime = millis();
  
  // Initialize biometric history buffer
  for (int i = 0; i < 10; i++) {
    biometricHistory[i] = BiometricData();
  }
}

void AdvancedStressManager::update(const BiometricData& biometrics) {
  // Update biometric history
  updateBiometricHistory(biometrics);
  
  // Only process if session is active
  if (!sessionActive) return;
  
  uint32_t now = millis();
  
  // Calculate current biometric stress
  float currentStress = calculateBiometricStress(biometrics);
  
  // Detect stress changes
  StressChangeType changeType = detectStressChange(currentStress);
  lastStressChange = changeType;
  
  // Update stress tracking
  lastStressValue = currentStress;
  lastStressTime = now;
  
  // Make decision based on current state, ML autonomy, and rules
  StressDecision decision;
  if (mlEnabled && (now - lastMLUpdate) > BODY_CFG.mlUpdateIntervalMs) {
    if (mlAutonomyActive && currentAutonomyLevel > 0.0f) {
      decision = makeHybridDecision();  // Nieuwe hybride logica
    } else {
      decision = makeMLDecision();      // Oude ML logica
    }
    lastMLUpdate = now;
  } else {
    decision = makeRuleBasedDecision();  // Rule-based fallback
  }
  
  // Update decision with change detection
  decision.changeType = changeType;
  
  Serial.printf("[STRESS] Level %d, Change: %s, Action: %d, Speed: %d\\n", 
                decision.currentLevel, 
                getStressChangeDescription(changeType).c_str(),
                decision.recommendedAction, 
                decision.recommendedSpeed);
}

StressDecision AdvancedStressManager::getStressDecision() {
  if (mlEnabled) {
    return makeMLDecision();
  } else {
    return makeRuleBasedDecision();
  }
}

void AdvancedStressManager::executeAction(const StressDecision& decision) {
  // Update stress level if changed
  if (decision.currentLevel != currentStressLevel) {
    previousStressLevel = currentStressLevel;
    currentStressLevel = decision.currentLevel;
    levelStartTime = millis();
    
    Serial.printf("[STRESS] Level transition: %d -> %d\\n", 
                  previousStressLevel, currentStressLevel);
  }
}

// ===== Session Management =====

void AdvancedStressManager::startSession() {
  sessionActive = true;
  sessionStartTime = millis();
  currentStressLevel = STRESS_0_NORMAAL;
  previousStressLevel = STRESS_0_NORMAAL;
  levelStartTime = millis();
  
  // Update ML autonomy based on session count
  totalSessions++;
  updateMLAutonomyStatus();
  
  // AUTOMATIC CSV RECORDING: Start recording session data for ML training
  if (BODY_CFG.autoRecordSessions) {
    startRecording();  // Automatically start CSV recording for this session
    Serial.println("[STRESS] Auto-recording started for this session");
  }
  
  Serial.printf("[STRESS] Session %d started - ML autonomy: %.1f%% active\n", 
                totalSessions, currentAutonomyLevel * 100.0f);
}

void AdvancedStressManager::endSession(const String& reason) {
  sessionActive = false;
  uint32_t duration = getSessionDuration();
  
  // AUTOMATIC CSV RECORDING: Stop recording when session ends
  if (BODY_CFG.autoRecordSessions) {
    stopRecording();
    Serial.println("[STRESS] Auto-recording stopped - session ended");
  }
  
  Serial.printf("[STRESS] Session ended: %s (Duration: %d seconds)\\n", 
                reason.c_str(), duration / 1000);
                
  // Log session end for ML training
  if (BODY_CFG.mlTrainingMode) {
    // TODO: Generate final ML training data with session end marker
  }
}

uint32_t AdvancedStressManager::getSessionDuration() const {
  if (!sessionActive && sessionStartTime == 0) return 0;
  return millis() - sessionStartTime;
}

// ===== Stress Level Management =====

String AdvancedStressManager::getStressLevelName(StressLevel level) const {
  switch(level) {
    case STRESS_0_NORMAAL: return "Normaal";
    case STRESS_1_GEEN: return "Geen/Beetje";
    case STRESS_2_BEETJE: return "Beetje Stress";
    case STRESS_3_IETS_MEER: return "Iets Meer";
    case STRESS_4_GEMIDDELD: return "Gemiddeld";
    case STRESS_5_MEER: return "Meer Stress";
    case STRESS_6_VEEL: return "Veel Stress";
    case STRESS_7_MAX: return "Maximum!";
    default: return "Onbekend";
  }
}

uint32_t AdvancedStressManager::getTimeInCurrentLevel() const {
  return millis() - levelStartTime;
}

// ===== ML Integration =====

MLTrainingData AdvancedStressManager::generateTrainingData(const StressDecision& decision, 
                                                          StressAction actionTaken,
                                                          const String& userResponse) {
  MLTrainingData data;
  
  // Get latest biometrics
  if (historyCount > 0) {
    int latestIndex = (historyIndex == 0) ? 9 : historyIndex - 1;
    data.biometrics = biometricHistory[latestIndex];
  }
  
  data.decision = decision;
  data.actionTaken = actionTaken;
  data.actualSpeed = stressLevelToSpeed(currentStressLevel);
  data.actualVibe = shouldVibeBeActive(currentStressLevel);
  data.actualSuction = shouldSuctionBeActive(currentStressLevel);
  data.userResponse = userResponse;
  data.sessionDurationMs = getSessionDuration();
  data.isSessionEnd = (userResponse == "KLAAR!");
  
  return data;
}

// ===== Stress Change Detection =====

StressChangeType AdvancedStressManager::getLastStressChange() const {
  return lastStressChange;
}

String AdvancedStressManager::getStressChangeDescription(StressChangeType change) const {
  switch(change) {
    case CHANGE_NONE: return "Geen verandering";
    case CHANGE_RUSTIG_OMHOOG: return "Rustig omhoog";
    case CHANGE_RUSTIG_OMLAAG: return "Rustig omlaag";
    case CHANGE_NORMAAL_OMHOOG: return "Normaal omhoog";
    case CHANGE_NORMAAL_OMLAAG: return "Normaal omlaag";
    case CHANGE_SNEL_OMHOOG: return "Snel omhoog";
    case CHANGE_SNEL_OMLAAG: return "Snel omlaag";
    case CHANGE_HEEL_SNEL_OMHOOG: return "Heel snel omhoog";
    case CHANGE_HEEL_SNEL_OMLAAG: return "Heel snel omlaag";
    default: return "Onbekend";
  }
}

// ===== Configuration =====

void AdvancedStressManager::updateConfig(const BodyConfig& config) {
  // ML settings kunnen worden bijgewerkt
  mlEnabled = config.mlStressEnabled;
  
  Serial.printf("[STRESS] Config updated - ML %s\\n", 
                mlEnabled ? "enabled" : "disabled");
}

// ===== Debug/Monitoring =====

void AdvancedStressManager::printStatus() const {
  Serial.println("[STRESS] === Advanced Stress Manager Status ===");
  Serial.printf("Current Level: %d (%s)\\n", currentStressLevel, 
                getStressLevelName(currentStressLevel).c_str());
  Serial.printf("Time in Level: %d seconds\\n", getTimeInCurrentLevel() / 1000);
  Serial.printf("Session Active: %s\\n", sessionActive ? "Yes" : "No");
  if (sessionActive) {
    Serial.printf("Session Duration: %d seconds\\n", getSessionDuration() / 1000);
  }
  Serial.printf("ML Enabled: %s\\n", mlEnabled ? "Yes" : "No");
  Serial.printf("History Count: %d/10\\n", historyCount);
  Serial.println("[STRESS] ========================================");
}

String AdvancedStressManager::getStatusString() const {
  return String("Level ") + String(currentStressLevel) + 
         " (" + getStressLevelName(currentStressLevel) + ")";
}

// ===== Private Methods =====

float AdvancedStressManager::calculateBiometricStress(const BiometricData& data) {
  // Advanced biometric stress calculation
  // Combines heart rate, temperature, and GSR into unified stress score
  
  float hrStress = 0.0f;
  if (data.heartRate > BODY_CFG.hrHighThreshold) {
    hrStress = (data.heartRate - BODY_CFG.hrHighThreshold) / 50.0f; // Normalize
  }
  
  float tempStress = 0.0f;
  if (data.temperature > BODY_CFG.tempHighThreshold) {
    tempStress = (data.temperature - BODY_CFG.tempHighThreshold) / 2.0f; // Normalize
  }
  
  float gsrStress = 0.0f;
  if (data.gsrValue > BODY_CFG.gsrHighThreshold) {
    gsrStress = (data.gsrValue - BODY_CFG.gsrHighThreshold) / 500.0f; // Normalize
  }
  
  // Weighted combination (can be tuned)
  float totalStress = (hrStress * 0.4f) + (tempStress * 0.3f) + (gsrStress * 0.3f);
  totalStress *= BODY_CFG.bioStressSensitivity;
  
  // Clamp to 0.0 - 7.0 range
  return constrain(totalStress, 0.0f, 7.0f);
}

StressChangeType AdvancedStressManager::detectStressChange(float currentStress) {
  if (lastStressTime == 0) return CHANGE_NONE;
  
  uint32_t timeDelta = millis() - lastStressTime;
  if (timeDelta < 1000) return CHANGE_NONE; // Need at least 1 second
  
  float stressDelta = currentStress - lastStressValue;
  float stressRate = abs(stressDelta) / (timeDelta / 60000.0f); // Per minute
  
  if (abs(stressDelta) < 0.1f) return CHANGE_NONE;
  
  bool increasing = stressDelta > 0;
  
  if (stressRate >= BODY_CFG.stressChangeHeelSnel) {
    return increasing ? CHANGE_HEEL_SNEL_OMHOOG : CHANGE_HEEL_SNEL_OMLAAG;
  } else if (stressRate >= BODY_CFG.stressChangeSnel) {
    return increasing ? CHANGE_SNEL_OMHOOG : CHANGE_SNEL_OMLAAG;
  } else if (stressRate >= BODY_CFG.stressChangeNormaal) {
    return increasing ? CHANGE_NORMAAL_OMHOOG : CHANGE_NORMAAL_OMLAAG;
  } else {
    return increasing ? CHANGE_RUSTIG_OMHOOG : CHANGE_RUSTIG_OMLAAG;
  }
}

StressDecision AdvancedStressManager::makeRuleBasedDecision() {
  StressDecision decision;
  decision.currentLevel = currentStressLevel;
  decision.previousLevel = previousStressLevel;
  decision.isMLPrediction = false;
  decision.confidence = 0.9f; // High confidence for rule-based
  
  uint32_t timeInLevel = getTimeInCurrentLevel();
  bool timerExpired = isTimerExpired();
  
  // Apply your detailed stress level logic here
  switch(currentStressLevel) {
    case STRESS_0_NORMAAL:
      decision.recommendedSpeed = 1;
      decision.vibeRecommended = false;
      decision.suctionRecommended = false;
      decision.reasoning = "Stress 0: Normaal, wachten op timer";
      if (timerExpired) {
        decision.currentLevel = STRESS_2_BEETJE; // Skip 1, ga naar 2
        decision.recommendedAction = ACTION_SPEED_UP;
        decision.reasoning = "Stress 0->2: Timer verlopen, naar versnelling 2";
      } else {
        decision.recommendedAction = ACTION_WAIT;
      }
      break;
      
    case STRESS_1_GEEN:
      decision.recommendedSpeed = 2;
      decision.vibeRecommended = false;
      decision.suctionRecommended = false;
      // Stress 1 logic - reactive to stress increases
      decision.recommendedAction = ACTION_WAIT;
      decision.reasoning = "Stress 1: Monitoring voor veranderingen";
      break;
      
    case STRESS_2_BEETJE:
      decision.recommendedSpeed = 3;
      decision.vibeRecommended = true;
      decision.suctionRecommended = true;
      decision.recommendedAction = ACTION_WAIT;
      decision.reasoning = "Stress 2: Vibe en zuigen aan, monitoring";
      if (timerExpired) {
        decision.currentLevel = STRESS_3_IETS_MEER;
        decision.recommendedAction = ACTION_SPEED_UP;
        decision.reasoning = "Stress 2->3: Timer verlopen, escalatie";
      }
      break;
      
    case STRESS_3_IETS_MEER:
      decision.recommendedSpeed = 4;
      decision.vibeRecommended = true;
      decision.suctionRecommended = true;
      decision.recommendedAction = ACTION_WAIT;
      decision.reasoning = "Stress 3: Verhoogde alertheid";
      if (timerExpired) {
        decision.currentLevel = STRESS_4_GEMIDDELD;
        decision.recommendedAction = ACTION_SPEED_UP;
        decision.reasoning = "Stress 3->4: Entering reactive zone";
      }
      break;
      
    case STRESS_4_GEMIDDELD:
    case STRESS_5_MEER:
    case STRESS_6_VEEL:
      // Reactive zone - decisions based on stress changes
      decision = makeReactiveDecision();
      break;
      
    case STRESS_7_MAX:
      decision.recommendedSpeed = 7;
      decision.vibeRecommended = true;
      decision.suctionRecommended = true;
      decision.recommendedAction = ACTION_MAX_MODE;
      decision.reasoning = "Stress 7: Maximum mode actief";
      break;
  }
  
  return decision;
}

StressDecision AdvancedStressManager::makeReactiveDecision() {
  StressDecision decision;
  decision.currentLevel = currentStressLevel;
  decision.previousLevel = previousStressLevel;
  decision.isMLPrediction = false;
  decision.confidence = 0.85f;
  
  // Get latest stress change
  StressChangeType change = getLastStressChange();
  
  // Base settings for current level
  decision.recommendedSpeed = stressLevelToSpeed(currentStressLevel);
  decision.vibeRecommended = shouldVibeBeActive(currentStressLevel);
  decision.suctionRecommended = shouldSuctionBeActive(currentStressLevel);
  
  // React to stress changes
  switch(change) {
    case CHANGE_HEEL_SNEL_OMHOOG:
      // Emergency response - drop to low speeds
      if (currentStressLevel >= STRESS_4_GEMIDDELD) {
        decision.currentLevel = STRESS_1_GEEN;
        decision.recommendedSpeed = 1;
        decision.vibeRecommended = false;
        decision.suctionRecommended = false;
        decision.recommendedAction = ACTION_EMERGENCY_STOP;
        decision.reasoning = "Heel snelle stress stijging - noodmaatregel";
      }
      break;
      
    case CHANGE_SNEL_OMHOOG:
      // Fast increase - reduce intensity
      if (currentStressLevel > STRESS_1_GEEN) {
        decision.currentLevel = (StressLevel)(currentStressLevel - 1);
        decision.recommendedSpeed = stressLevelToSpeed(decision.currentLevel);
        decision.vibeRecommended = false;
        decision.suctionRecommended = false;
        decision.recommendedAction = ACTION_SPEED_DOWN;
        decision.reasoning = "Snelle stress stijging - verlagen";
      }
      break;
      
    case CHANGE_HEEL_SNEL_OMLAAG:
      // Very fast decrease - can increase significantly
      if (currentStressLevel < STRESS_7_MAX) {
        decision.currentLevel = (StressLevel)min((int)STRESS_7_MAX, currentStressLevel + 2);
        decision.recommendedSpeed = stressLevelToSpeed(decision.currentLevel);
        decision.vibeRecommended = true;
        decision.suctionRecommended = true;
        decision.recommendedAction = ACTION_SPEED_UP;
        decision.reasoning = "Heel snelle stress daling - flinke verhoging";
      }
      break;
      
    case CHANGE_SNEL_OMLAAG:
      // Fast decrease - moderate increase
      if (currentStressLevel < STRESS_6_VEEL) {
        decision.currentLevel = (StressLevel)(currentStressLevel + 1);
        decision.recommendedSpeed = stressLevelToSpeed(decision.currentLevel);
        decision.vibeRecommended = true;
        decision.suctionRecommended = true;
        decision.recommendedAction = ACTION_SPEED_UP;
        decision.reasoning = "Snelle stress daling - verhogen";
      }
      break;
      
    case CHANGE_RUSTIG_OMLAAG:
    case CHANGE_NORMAAL_OMLAAG:
      // Stay at current level but ensure vibe/suction on
      decision.vibeRecommended = true;
      decision.suctionRecommended = true;
      decision.recommendedAction = ACTION_VIBE_ON;
      decision.reasoning = "Stress daalt - vibe/zuigen aanhouden";
      break;
      
    case CHANGE_RUSTIG_OMHOOG:
    case CHANGE_NORMAAL_OMHOOG:
      // Slight increase - turn off extras
      decision.vibeRecommended = false;
      decision.suctionRecommended = false;
      decision.recommendedAction = ACTION_VIBE_OFF;
      decision.reasoning = "Stress stijgt - vibe/zuigen uit";
      break;
      
    default:
      // No significant change - maintain current settings
      decision.recommendedAction = ACTION_WAIT;
      decision.reasoning = "Stabiele stress - huidige instellingen behouden";
      break;
  }
  
  return decision;
}

StressDecision AdvancedStressManager::makeMLDecision() {
  StressDecision decision = makeRuleBasedDecision(); // Fallback
  
  // If ML is available and ready, use it
  if (mlAnalyzer.hasModel() && historyCount >= 3) {
    // Get latest biometrics
    int latestIndex = (historyIndex == 0) ? 9 : historyIndex - 1;
    BiometricData latest = biometricHistory[latestIndex];
    
    // Use ML analyzer to get stress prediction
    int mlStressLevel = mlAnalyzer.analyzeStress(latest.heartRate, latest.temperature, latest.gsrValue);
    
    if (mlStressLevel >= 1 && mlStressLevel <= 7) {
      decision.currentLevel = (StressLevel)(mlStressLevel - 1); // Convert to 0-6 range
      decision.isMLPrediction = true;
      decision.confidence = 0.8f; // ML confidence
      decision.reasoning = "ML model prediction: Level " + String(mlStressLevel);
      
      // Apply ML-based actions
      decision.recommendedSpeed = stressLevelToSpeed(decision.currentLevel);
      decision.vibeRecommended = shouldVibeBeActive(decision.currentLevel);
      decision.suctionRecommended = shouldSuctionBeActive(decision.currentLevel);
      decision.recommendedAction = ACTION_SPEED_UP;
    }
  }
  
  return decision;
}

StressDecision AdvancedStressManager::makeHybridDecision() {
  // Get both rule-based and ML decisions
  StressDecision ruleDecision = makeRuleBasedDecision();
  StressDecision mlDecision = makeMLDecision();
  
  // Start with rule-based decision as base
  StressDecision hybridDecision = ruleDecision;
  hybridDecision.isMLPrediction = true;
  
  // Check if ML has high enough confidence to override rules
  bool mlCanOverride = (mlDecision.confidence >= BODY_CFG.mlOverrideConfidenceThreshold);
  
  if (mlCanOverride && currentAutonomyLevel > 0.0f) {
    float autonomyUsed = currentAutonomyLevel;
    
    // ML can influence the decision based on autonomy level
    if (currentAutonomyLevel >= 0.8f) {
      // High autonomy - ML gets significant control
      hybridDecision = mlDecision;
      hybridDecision.isMLOverride = true;
      hybridDecision.mlAutonomyUsed = autonomyUsed;
      hybridDecision.mlReasoning = "High ML autonomy: " + mlDecision.reasoning;
      
      // Check if ML wants to skip levels (if allowed)
      if (BODY_CFG.mlCanSkipLevels && abs(mlDecision.currentLevel - ruleDecision.currentLevel) > 1) {
        hybridDecision.recommendedAction = ACTION_ML_SKIP_LEVEL;
        hybridDecision.mlReasoning += " (Level skip allowed)";
      }
      
    } else if (currentAutonomyLevel >= 0.5f) {
      // Medium autonomy - blend decisions
      float mlWeight = currentAutonomyLevel;
      float ruleWeight = 1.0f - mlWeight;
      
      // Blend speed recommendations
      hybridDecision.recommendedSpeed = (uint8_t)(
        (mlDecision.recommendedSpeed * mlWeight) + 
        (ruleDecision.recommendedSpeed * ruleWeight)
      );
      
      // Use ML's vibe/suction if it's confident
      if (mlDecision.confidence > 0.8f) {
        hybridDecision.vibeRecommended = mlDecision.vibeRecommended;
        hybridDecision.suctionRecommended = mlDecision.suctionRecommended;
      }
      
      hybridDecision.isMLOverride = true;
      hybridDecision.mlAutonomyUsed = autonomyUsed;
      hybridDecision.mlReasoning = String("Blended decision (ML ") + String(mlWeight * 100, 0) + 
                                   "%, Rules " + String(ruleWeight * 100, 0) + "%)";
      
    } else if (currentAutonomyLevel >= 0.2f) {
      // Low autonomy - ML can only suggest minor adjustments
      hybridDecision = ruleDecision;  // Keep rule decision as base
      
      // ML can suggest speed adjustments within ±1
      if (abs(mlDecision.recommendedSpeed - ruleDecision.recommendedSpeed) == 1) {
        hybridDecision.recommendedSpeed = mlDecision.recommendedSpeed;
        hybridDecision.isMLOverride = true;
        hybridDecision.mlAutonomyUsed = autonomyUsed;
        hybridDecision.mlReasoning = "Minor ML adjustment: " + mlDecision.reasoning;
      }
    }
    
    // Emergency override - ML can always suggest emergency stops if enabled
    if (BODY_CFG.mlCanEmergencyOverride && mlDecision.recommendedAction == ACTION_EMERGENCY_STOP) {
      hybridDecision.recommendedAction = ACTION_EMERGENCY_STOP;
      hybridDecision.isMLOverride = true;
      hybridDecision.mlAutonomyUsed = 1.0f;  // Full autonomy for safety
      hybridDecision.mlReasoning = "ML Emergency Override: " + mlDecision.reasoning;
      Serial.println("[STRESS] ML Emergency Override activated!");
    }
    
    // Timer override - ML can ignore timers if confidence is very high and enabled
    if (BODY_CFG.mlCanIgnoreTimers && mlDecision.confidence >= 0.9f && 
        ruleDecision.recommendedAction == ACTION_WAIT) {
      hybridDecision.recommendedAction = mlDecision.recommendedAction;
      hybridDecision.isMLOverride = true;
      hybridDecision.mlAutonomyUsed = autonomyUsed;
      hybridDecision.mlReasoning = "ML Timer Override (high confidence): " + mlDecision.reasoning;
    }
  }
  
  // Update final reasoning
  if (hybridDecision.isMLOverride) {
    hybridDecision.reasoning = hybridDecision.mlReasoning;
    hybridDecision.recommendedAction = ACTION_ML_CUSTOM;
  } else {
    hybridDecision.reasoning = ruleDecision.reasoning + " (ML: " + mlDecision.reasoning + ")";
  }
  
  // Set confidence as blend of both systems
  hybridDecision.confidence = (ruleDecision.confidence * 0.3f) + (mlDecision.confidence * 0.7f);
  
  Serial.printf("[STRESS] Hybrid Decision: ML Override: %s, Autonomy Used: %.1f%%, Confidence: %.2f\n",
                hybridDecision.isMLOverride ? "YES" : "NO",
                hybridDecision.mlAutonomyUsed * 100.0f,
                hybridDecision.confidence);
  
  return hybridDecision;
}

void AdvancedStressManager::updateBiometricHistory(const BiometricData& data) {
  biometricHistory[historyIndex] = data;
  historyIndex = (historyIndex + 1) % 10;
  if (historyCount < 10) historyCount++;
}

bool AdvancedStressManager::isTimerExpired() {
  uint32_t timeout = getCurrentLevelTimeout();
  uint32_t timeInLevel = getTimeInCurrentLevel();
  
  return timeInLevel >= timeout;
}

uint32_t AdvancedStressManager::getCurrentLevelTimeout() {
  switch(currentStressLevel) {
    case STRESS_0_NORMAAL: return BODY_CFG.stressLevel0Minutes * 60000;
    case STRESS_1_GEEN: return BODY_CFG.stressLevel1Minutes * 60000;
    case STRESS_2_BEETJE: return BODY_CFG.stressLevel2Minutes * 60000;
    case STRESS_3_IETS_MEER: return BODY_CFG.stressLevel3Minutes * 60000;
    case STRESS_4_GEMIDDELD: return BODY_CFG.stressLevel4Seconds * 1000;
    case STRESS_5_MEER: return BODY_CFG.stressLevel5Seconds * 1000;
    case STRESS_6_VEEL: return BODY_CFG.stressLevel6Seconds * 1000;
    case STRESS_7_MAX: return UINT32_MAX; // Never timeout at max
    default: return 60000; // 1 minute default
  }
}

// ===== Helper Functions =====

String formatMLTrainingDataCSV(const MLTrainingData& data) {
  String csv = "";
  
  // Timestamp
  csv += String(data.biometrics.timestamp) + ",";
  
  // Biometrics
  csv += String(data.biometrics.heartRate, 1) + ",";
  csv += String(data.biometrics.temperature, 2) + ",";
  csv += String(data.biometrics.gsrValue, 1) + ",";
  
  // Stress decision
  csv += String(data.decision.currentLevel) + ",";
  csv += String(data.decision.confidence, 3) + ",";
  csv += "\"" + data.decision.reasoning + "\",";
  
  // Actions taken
  csv += String(data.actionTaken) + ",";
  csv += String(data.actualSpeed) + ",";
  csv += (data.actualVibe ? "1" : "0") + String(",");
  csv += (data.actualSuction ? "1" : "0") + String(",");
  
  // User response and session info
  csv += "\"" + data.userResponse + "\",";
  csv += String(data.sessionDurationMs) + ",";
  csv += (data.isSessionEnd ? "1" : "0");
  
  return csv;
}

// ===== ML AUTONOMY METHODS =====

void AdvancedStressManager::updateMLAutonomyStatus() {
  // ML Autonomy is always active if enabled
  mlAutonomyActive = true;
  currentAutonomyLevel = BODY_CFG.mlAutonomyLevel;
  Serial.printf("[STRESS] ML Autonomy active: %.1f%% (Session %d)\n", 
                currentAutonomyLevel * 100.0f, totalSessions);
}

void AdvancedStressManager::setMLAutonomyLevel(float level) {
  currentAutonomyLevel = constrain(level, 0.0f, 1.0f);
  mlAutonomyActive = (currentAutonomyLevel > 0.0f);  // Active zodra > 0%
  Serial.printf("[STRESS] ML Autonomy level set to %.1f%% by user\n", currentAutonomyLevel * 100.0f);
}

void AdvancedStressManager::provideFeedback(bool wasGoodDecision) {
  // Feedback is voor ML learning, niet voor autonomie aanpassing
  // De gebruiker bepaalt autonomie level via de slider
  if (!mlAutonomyActive) return;
  
  if (wasGoodDecision) {
    Serial.println("[STRESS] Positive feedback logged for ML learning");
  } else {
    Serial.println("[STRESS] Negative feedback logged for ML learning");
  }
  
  // TODO: Use feedback for actual ML model training, not autonomy control
}

void AdvancedStressManager::resetMLAutonomy() {
  // Reset alleen sessie count, autonomie level blijft wat gebruiker heeft ingesteld
  totalSessions = 0;
  Serial.println("[STRESS] ML session count reset - autonomy level unchanged");
}


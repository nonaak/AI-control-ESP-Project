/*
  ML INTEGRATION - Implementatie
  
  Koppelt alle ML componenten aan elkaar
*/

#include "ml_integration.h"

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBALE STATE
// ═══════════════════════════════════════════════════════════════════════════

MLIntegrationState mlState;

// ═══════════════════════════════════════════════════════════════════════════
//                         INITIALISATIE
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_begin() {
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] ML Integration System Startup");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  // Reset state
  memset(&mlState, 0, sizeof(mlState));
  mlState.userCorrectedLevel = -1;
  mlState.aiPredictedLevel = -1;
  
  // Initialize sub-systems
  Serial.println("[ML INT] Initializing ML Training system...");
  mlTrainer.begin();
  
  Serial.println("[ML INT] Initializing ML Stress Analyzer...");
  ml_begin();
  
  // Check voor bestaand model
  if (mlTrainer.hasTrainedModel()) {
    mlState.modelTrained = true;
    mlState.modelAccuracy = mlTrainer.getModelAccuracy();
    Serial.printf("[ML INT] ✅ Trained model loaded (accuracy: %.1f%%)\n", 
                  mlState.modelAccuracy * 100);
  } else {
    Serial.println("[ML INT] No trained model found");
    Serial.println("[ML INT] Verzamel feedback via nunchuk of GEVOEL knop!");
  }
  
  // Tel bestaande data
  mlState.totalFeedbackSamples = mlTrainer.getTotalFeedbackCount();
  mlState.totalAnnotations = 0;  // Wordt bijgehouden per sessie
  
  Serial.printf("[ML INT] Total feedback samples: %d\n", mlState.totalFeedbackSamples);
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] Ready!");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         LIVE SESSIE FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_startLiveSession() {
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] 🎬 STARTING LIVE SESSION");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  mlState.isLiveSession = true;
  mlState.liveSessionActive = true;
  mlState.liveSessionStart = millis();
  mlState.liveEdgeCount = 0;
  mlState.userCorrectedLevel = -1;
  
  // Start ML training sessie
  mlTrainer.startSession();
  
  // Reset AI runtime state
  ml_resetSessie();
  
  Serial.println("[ML INT] ML is nu aan het leren van jouw feedback!");
  Serial.println("[ML INT] Corrigeer via nunchuk wanneer AI niet goed zit.");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
}

void mlIntegration_stopLiveSession() {
  if (!mlState.liveSessionActive) return;
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] 🛑 STOPPING LIVE SESSION");
  
  uint32_t duration = (millis() - mlState.liveSessionStart) / 1000;
  Serial.printf("[ML INT] Duration: %d seconds\n", duration);
  Serial.printf("[ML INT] Edges: %d\n", mlState.liveEdgeCount);
  Serial.printf("[ML INT] Feedback samples: %d\n", mlTrainer.getSessionFeedbackCount());
  
  // Stop ML training sessie
  mlTrainer.endSession();
  
  mlState.liveSessionActive = false;
  mlState.isLiveSession = false;
  
  // Update totaal
  mlState.totalFeedbackSamples = mlTrainer.getTotalFeedbackCount();
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  // Suggestie voor training
  if (mlState.totalFeedbackSamples >= 50) {
    Serial.println("[ML INT] TIP: Je hebt genoeg data voor training!");
    Serial.println("[ML INT] Ga naar AI Training → Model Trainen");
  }
}

void mlIntegration_updateSensors(float hr, float temp, float gsr) {
  // Update state
  mlState.currentHR = hr;
  mlState.currentTemp = temp;
  mlState.currentGSR = gsr;
  
  // Update ML trainer context (voor rolling averages)
  if (mlState.liveSessionActive) {
    mlTrainer.updateSensorContext(hr, temp, gsr);
  }
  
  // Bereken stress level
  mlState.currentStressLevel = ml_getStressLevel(hr, temp, gsr);
}

void mlIntegration_logNunchukCorrection(int aiLevel, int userLevel) {
  if (!mlState.liveSessionActive) {
    Serial.println("[ML INT] WARNING: No active session, starting one...");
    mlIntegration_startLiveSession();
  }
  
  // Log naar ML trainer
  mlTrainer.logFeedback(aiLevel, userLevel);
  
  // Update state
  mlState.aiPredictedLevel = aiLevel;
  mlState.userCorrectedLevel = userLevel;
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] 📝 NUNCHUK CORRECTIE GELOGD");
  Serial.printf("[ML INT] AI koos: %d, Jij koos: %d (correctie: %+d)\n",
                aiLevel, userLevel, userLevel - aiLevel);
  Serial.printf("[ML INT] Context: HR=%.0f, T=%.1f, GSR=%.0f\n",
                mlState.currentHR, mlState.currentTemp, mlState.currentGSR);
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
}

void mlIntegration_logEdge() {
  mlState.liveEdgeCount++;
  
  if (mlState.liveSessionActive) {
    mlTrainer.logEdgeEvent();
  }
  
  Serial.printf("[ML INT] 🔥 EDGE #%d gelogd!\n", mlState.liveEdgeCount);
}

void mlIntegration_logOrgasme() {
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] 💦 ORGASME! Sessie wordt beëindigd.");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  if (mlState.liveSessionActive) {
    mlTrainer.logOrgasmeEvent();  // Dit stopt ook de sessie
  }
  
  mlState.liveSessionActive = false;
  mlState.isLiveSession = false;
  
  // Update totaal
  mlState.totalFeedbackSamples = mlTrainer.getTotalFeedbackCount();
}

// ═══════════════════════════════════════════════════════════════════════════
//                         PLAYBACK FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_startPlayback(const char* filename) {
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.printf("[ML INT] 🎬 STARTING PLAYBACK: %s\n", filename);
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  mlState.isPlaybackMode = true;
  mlState.isLiveSession = false;
  mlState.currentPlaybackFile = String(filename);
  mlState.playbackPosition = 0;
  mlState.userCorrectedLevel = -1;
  mlState.totalAnnotations = 0;
  
  // Open annotatie bestand voor dit playback bestand
  mlAnnotations.openFile(mlState.currentPlaybackFile);
  
  // Initialiseer playback screen
  playbackScreen.setFilename(filename);
  playbackScreen.clearMarkers();
  
  // Laad bestaande annotaties als markers
  int existingCount = mlAnnotations.getAnnotationCount();
  if (existingCount > 0) {
    Serial.printf("[ML INT] Loaded %d existing annotations\n", existingCount);
    // TODO: Laad markers naar playbackScreen
  }
  
  Serial.println("[ML INT] Druk GEVOEL om feedback te geven!");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
}

void mlIntegration_stopPlayback() {
  if (!mlState.isPlaybackMode) return;
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] 🛑 STOPPING PLAYBACK");
  Serial.printf("[ML INT] Annotations added: %d\n", mlState.totalAnnotations);
  
  // Sluit en sla annotaties op
  mlAnnotations.closeFile();
  
  // Export naar ML training formaat
  if (mlState.totalAnnotations > 0) {
    if (mlAnnotations.exportForTraining("playback_feedback.csv")) {
      Serial.println("[ML INT] ✅ Feedback geëxporteerd voor ML training!");
    }
  }
  
  mlState.isPlaybackMode = false;
  mlState.currentPlaybackFile = "";
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
}

void mlIntegration_updatePlayback(float timestamp, float hr, float temp, float gsr, int aiLevel) {
  // Update state
  mlState.playbackPosition = timestamp;
  mlState.currentHR = hr;
  mlState.currentTemp = temp;
  mlState.currentGSR = gsr;
  mlState.aiPredictedLevel = aiLevel;
  
  // Bereken stress level (voor weergave)
  mlState.currentStressLevel = ml_getStressLevel(hr, temp, gsr);
  
  // Check of er een bestaande annotatie is
  mlState.userCorrectedLevel = mlIntegration_getExistingFeedback();
  
  // Update playback screen
  playbackScreen.setProgress(timestamp, mlState.playbackDuration);
  playbackScreen.setSensorValues(hr, temp, gsr);
  playbackScreen.setCurrentLevel(mlState.currentStressLevel);
  playbackScreen.setAIPrediction(aiLevel);
  playbackScreen.setUserAnnotation(mlState.userCorrectedLevel);
  playbackScreen.pushLevelSample(mlState.currentStressLevel);
}

void mlIntegration_logGevoelFeedback(int userLevel) {
  if (!mlState.isPlaybackMode) {
    Serial.println("[ML INT] ERROR: Not in playback mode!");
    return;
  }
  
  // Voeg annotatie toe
  bool success = mlAnnotations.addAnnotation(
    mlState.playbackPosition,
    userLevel,
    mlState.currentHR,
    mlState.currentTemp,
    mlState.currentGSR,
    mlState.aiPredictedLevel
  );
  
  if (success) {
    mlState.totalAnnotations++;
    mlState.userCorrectedLevel = userLevel;
    
    // Voeg marker toe aan playback screen
    playbackScreen.addMarker(mlState.playbackPosition, userLevel, false, false);
    
    Serial.println("[ML INT] ═══════════════════════════════════════════════");
    Serial.println("[ML INT] 📝 GEVOEL FEEDBACK OPGESLAGEN");
    Serial.printf("[ML INT] Timestamp: %.1fs\n", mlState.playbackPosition);
    Serial.printf("[ML INT] Level: %d\n", userLevel);
    if (mlState.aiPredictedLevel >= 0) {
      Serial.printf("[ML INT] AI voorspelde: %d (correctie: %+d)\n",
                    mlState.aiPredictedLevel, userLevel - mlState.aiPredictedLevel);
    }
    Serial.println("[ML INT] ═══════════════════════════════════════════════");
  }
}

int mlIntegration_getExistingFeedback() {
  if (!mlState.isPlaybackMode) return -1;
  
  StressAnnotation ann;
  if (mlAnnotations.getAnnotationAtTime(mlState.playbackPosition, ann)) {
    return ann.userAnnotatedLevel;
  }
  return -1;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         AI BESLISSINGEN
// ═══════════════════════════════════════════════════════════════════════════

int mlIntegration_getOptimalLevel(uint8_t autonomyPercent) {
  int recommendedLevel;
  
  // Check of we een getraind model hebben
  if (mlState.modelTrained && mlTrainer.hasTrainedModel()) {
    // Gebruik ML voorspelling
    recommendedLevel = mlTrainer.predictOptimalLevel(
      mlState.currentHR, mlState.currentTemp, mlState.currentGSR
    );
    
    float confidence = mlTrainer.getConfidence();
    
    // Als confidence te laag is, fallback naar rule-based
    if (confidence < 0.5f || recommendedLevel < 0) {
      recommendedLevel = ml_getStressLevel(mlState.currentHR, mlState.currentTemp, mlState.currentGSR);
    }
  } else {
    // Geen model, gebruik rule-based
    recommendedLevel = ml_getStressLevel(mlState.currentHR, mlState.currentTemp, mlState.currentGSR);
  }
  
  // Check AI Bijbel permissies
  recommendedLevel = ml_getRecommendedActionLevel(recommendedLevel, autonomyPercent);
  
  return recommendedLevel;
}

bool mlIntegration_aiMagNaarLevel(uint8_t autonomyPercent, int level) {
  return aiBijbel_magNaarLevel(autonomyPercent, level);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         TRAINING
// ═══════════════════════════════════════════════════════════════════════════

bool mlIntegration_trainModel() {
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] 🧠 STARTING MODEL TRAINING");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  // Check of er genoeg data is
  int feedbackCount = mlTrainer.getTotalFeedbackCount();
  
  if (feedbackCount < 20) {
    Serial.printf("[ML INT] ERROR: Need at least 20 samples, have %d\n", feedbackCount);
    Serial.println("[ML INT] Verzamel meer feedback via nunchuk of GEVOEL!");
    return false;
  }
  
  Serial.printf("[ML INT] Training on %d samples...\n", feedbackCount);
  
  // Train model
  bool success = mlTrainer.trainModel(200);  // 200 epochs
  
  if (success) {
    mlState.modelTrained = true;
    mlState.modelAccuracy = mlTrainer.getModelAccuracy();
    
    Serial.println("[ML INT] ═══════════════════════════════════════════════");
    Serial.println("[ML INT] ✅ TRAINING SUCCESVOL!");
    Serial.printf("[ML INT] Accuracy: %.1f%%\n", mlState.modelAccuracy * 100);
    Serial.println("[ML INT] ML zal nu betere beslissingen maken!");
    Serial.println("[ML INT] ═══════════════════════════════════════════════");
  } else {
    Serial.println("[ML INT] ❌ TRAINING GEFAALD");
  }
  
  return success;
}

void mlIntegration_getStats(int* feedbackCount, int* annotationCount, 
                            bool* modelTrained, float* accuracy) {
  if (feedbackCount) *feedbackCount = mlTrainer.getTotalFeedbackCount();
  if (annotationCount) *annotationCount = mlAnnotations.getTotalAnnotations();
  if (modelTrained) *modelTrained = mlState.modelTrained;
  if (accuracy) *accuracy = mlState.modelAccuracy;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         UI HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_drawPlaybackScreen(Arduino_GFX* gfx, int selectedButton) {
  playbackScreen.begin(gfx);
  playbackScreen.setSelectedButton(selectedButton);
  playbackScreen.drawDynamicElements();
}

int mlIntegration_handlePlaybackTouch(int x, int y) {
  return playbackScreen.handleTouch(x, y);
}

void mlIntegration_handlePlaybackEncoder(int direction) {
  int current = playbackScreen.getSelectedButton();
  int count = playbackScreen.getButtonCount();
  
  if (direction > 0) {
    current = (current + 1) % count;
  } else {
    current = (current - 1 + count) % count;
  }
  
  playbackScreen.setSelectedButton(current);
}

void mlIntegration_handlePlaybackEncoderPress() {
  int selected = playbackScreen.getSelectedButton();
  
  // Return selected button index voor body_menu.cpp om af te handelen
  // 0=STOP, 1=PLAY/PAUZE, 2=-10s, 3=+10s, 4=GEVOEL
}

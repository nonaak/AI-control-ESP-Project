/*
  ML INTEGRATION - Implementatie
  
  Koppelt alle ML componenten aan elkaar
  Aangepast om te werken met de echte API's:
  - ml_stress_analyzer.h
  - ml_annotation.h
  - ml_trainer.h
  - ai_bijbel.h
*/

#include "ml_integration.h"
#include <SD.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBALE STATE
// ═══════════════════════════════════════════════════════════════════════════

MLIntegrationState mlState;

// Interne tracking
static uint32_t lastSensorUpdate = 0;
static int sessionFeedbackCount = 0;
static File feedbackFile;
static bool feedbackFileOpen = false;

// ═══════════════════════════════════════════════════════════════════════════
//                         FEEDBACK LOGGING (intern)
// ═══════════════════════════════════════════════════════════════════════════

static bool openFeedbackFile() {
  if (feedbackFileOpen) return true;
  
  // Maak /ml_training directory als die niet bestaat
  if (!SD.exists("/ml_training")) {
    SD.mkdir("/ml_training");
  }
  
  // Open feedback file in append mode
  feedbackFile = SD.open("/ml_training/feedback.csv", FILE_APPEND);
  if (!feedbackFile) {
    Serial.println("[ML INT] ❌ Kan feedback.csv niet openen!");
    return false;
  }
  
  // Schrijf header als bestand nieuw is
  if (feedbackFile.size() == 0) {
    feedbackFile.println("timestamp,hr,temp,gsr,ai_level,user_level,event_type");
  }
  
  feedbackFileOpen = true;
  return true;
}

static void closeFeedbackFile() {
  if (feedbackFileOpen) {
    feedbackFile.close();
    feedbackFileOpen = false;
  }
}

static void logFeedback(float hr, float temp, float gsr, int aiLevel, int userLevel, const char* eventType) {
  if (!openFeedbackFile()) return;
  
  feedbackFile.printf("%lu,%.1f,%.2f,%.0f,%d,%d,%s\n",
                      millis(), hr, temp, gsr, aiLevel, userLevel, eventType);
  feedbackFile.flush();
}

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
  
  // Initialize ML Stress Analyzer
  Serial.println("[ML INT] Initializing ML Stress Analyzer...");
  ml_begin();
  
  // Check voor bestaand model
  if (mlTrainer.hasModel()) {
    mlState.modelTrained = true;
    TrainingStatus status = mlTrainer.getStatus();
    mlState.modelAccuracy = status.currentAccuracy;
    Serial.printf("[ML INT] ✅ Trained model loaded (accuracy: %.1f%%)\n", 
                  mlState.modelAccuracy * 100);
  } else {
    Serial.println("[ML INT] No trained model found");
    Serial.println("[ML INT] Verzamel feedback via nunchuk of GEVOEL knop!");
  }
  
  // Reset counters
  mlState.totalFeedbackSamples = 0;
  mlState.totalAnnotations = 0;
  sessionFeedbackCount = 0;
  
  // Reset sessie state
  ml_resetSessie();
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  Serial.println("[ML INT] Ready!");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
}

// ═══════════════════════════════════════════════════════════════════════════
//                         LIVE SESSIE FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_startLiveSession() {
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  Serial.println("[ML INT] Starting Live Session");
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  
  mlState.isLiveSession = true;
  mlState.liveSessionActive = true;
  mlState.liveSessionStart = millis();
  mlState.liveEdgeCount = 0;
  mlState.userCorrectedLevel = -1;
  
  sessionFeedbackCount = 0;
  
  // Reset ML sessie state
  ml_resetSessie();
  
  // Open feedback file
  openFeedbackFile();
  
  Serial.println("[ML INT] Live session started - nunchuk feedback enabled");
}

void mlIntegration_stopLiveSession() {
  if (!mlState.liveSessionActive) return;
  
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  Serial.println("[ML INT] Stopping Live Session");
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  
  uint32_t duration = (millis() - mlState.liveSessionStart) / 1000;
  Serial.printf("[ML INT] Session duration: %d seconds\n", duration);
  Serial.printf("[ML INT] Edge events: %d\n", mlState.liveEdgeCount);
  Serial.printf("[ML INT] Feedback samples: %d\n", sessionFeedbackCount);
  
  // Sluit feedback file
  closeFeedbackFile();
  
  mlState.isLiveSession = false;
  mlState.liveSessionActive = false;
  
  // Update totals
  mlState.totalFeedbackSamples += sessionFeedbackCount;
  
  Serial.printf("[ML INT] Total feedback samples: %d\n", mlState.totalFeedbackSamples);
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
}

void mlIntegration_updateSensors(float hr, float temp, float gsr) {
  // Update state
  mlState.currentHR = hr;
  mlState.currentTemp = temp;
  mlState.currentGSR = gsr;
  
  // Bereken stress level met ML analyzer
  mlState.currentStressLevel = ml_getStressLevel(hr, temp, gsr);
  mlState.aiPredictedLevel = mlState.currentStressLevel;
  
  lastSensorUpdate = millis();
}

void mlIntegration_logNunchukCorrection(int aiLevel, int userLevel) {
  if (!mlState.liveSessionActive) return;
  
  mlState.userCorrectedLevel = userLevel;
  sessionFeedbackCount++;
  
  int diff = userLevel - aiLevel;
  Serial.printf("[ML INT] 📝 Nunchuk correctie: AI=%d → User=%d (diff: %+d)\n",
                aiLevel, userLevel, diff);
  
  // Log naar feedback file
  logFeedback(mlState.currentHR, mlState.currentTemp, mlState.currentGSR,
              aiLevel, userLevel, "nunchuk");
}

void mlIntegration_logEdge() {
  mlState.liveEdgeCount++;
  Serial.printf("[ML INT] 🔥 Edge event #%d logged\n", mlState.liveEdgeCount);
  
  // Registreer in ML systeem
  ml_registreerEdge();
  
  // Log naar feedback file
  logFeedback(mlState.currentHR, mlState.currentTemp, mlState.currentGSR,
              mlState.aiPredictedLevel, 7, "edge");
}

void mlIntegration_logOrgasme() {
  Serial.println("[ML INT] 💥 Orgasme event logged");
  
  // Log naar feedback file
  logFeedback(mlState.currentHR, mlState.currentTemp, mlState.currentGSR,
              mlState.aiPredictedLevel, 7, "orgasme");
  
  // Stop live sessie
  mlIntegration_stopLiveSession();
}

// ═══════════════════════════════════════════════════════════════════════════
//                         PLAYBACK FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_startPlayback(const char* filename) {
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  Serial.printf("[ML INT] Starting Playback: %s\n", filename);
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  
  mlState.isPlaybackMode = true;
  mlState.isLiveSession = false;
  mlState.currentPlaybackFile = String(filename);
  mlState.playbackPosition = 0;
  mlState.playbackDuration = 0;
  mlState.userCorrectedLevel = -1;
  
  // Open annotation file voor dit bestand
  ml_openAnnotationFile(String(filename));
  
  Serial.println("[ML INT] Playback mode - GEVOEL knop enabled");
}

void mlIntegration_stopPlayback() {
  if (!mlState.isPlaybackMode) return;
  
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  Serial.println("[ML INT] Stopping Playback");
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
  
  // Sluit en sla annotaties op
  ml_closeAnnotationFile();
  
  // Update totals (we weten niet exact hoeveel, maar we incrementen)
  mlState.totalAnnotations++;
  
  Serial.printf("[ML INT] Total annotations: %d\n", mlState.totalAnnotations);
  
  mlState.isPlaybackMode = false;
  mlState.currentPlaybackFile = "";
  
  Serial.println("[ML INT] ─────────────────────────────────────────────────");
}

void mlIntegration_updatePlayback(float timestamp, float hr, float temp, float gsr, int aiLevel) {
  mlState.playbackPosition = timestamp;
  mlState.currentHR = hr;
  mlState.currentTemp = temp;
  mlState.currentGSR = gsr;
  mlState.aiPredictedLevel = aiLevel;
  
  // Bereken eigen stress level voor vergelijking
  mlState.currentStressLevel = ml_getStressLevel(hr, temp, gsr);
}

void mlIntegration_logGevoelFeedback(int userLevel) {
  if (!mlState.isPlaybackMode) return;
  
  mlState.userCorrectedLevel = userLevel;
  
  Serial.printf("[ML INT] 📝 GEVOEL feedback @ %.1fs: level=%d\n",
                mlState.playbackPosition, userLevel);
  
  // Log annotatie via ml_annotation systeem
  ml_addPlaybackAnnotation(
    mlState.playbackPosition,
    userLevel,
    mlState.currentHR,
    mlState.currentTemp,
    mlState.currentGSR,
    mlState.aiPredictedLevel
  );
}

int mlIntegration_getExistingFeedback() {
  if (!mlState.isPlaybackMode) return -1;
  
  // Check of er al feedback is op deze positie
  return ml_getAnnotationAt(mlState.playbackPosition);
}

// ═══════════════════════════════════════════════════════════════════════════
//                         AI BESLISSINGEN
// ═══════════════════════════════════════════════════════════════════════════

int mlIntegration_getOptimalLevel(uint8_t autonomyPercent) {
  int recommendedLevel = mlState.currentStressLevel;
  
  // Als we een getraind model hebben, gebruik dat
  if (mlState.modelTrained && mlTrainer.hasModel()) {
    // Maak feature array voor predict
    float features[9] = {
      mlState.currentHR,
      mlState.currentTemp,
      mlState.currentGSR,
      0, 0, 0, 0, 0, 0  // Trends en andere features (TODO)
    };
    
    recommendedLevel = mlTrainer.predict(features);
    
    Serial.printf("[ML INT] 🤖 ML prediction: %d (rule-based: %d)\n",
                  recommendedLevel, mlState.currentStressLevel);
  }
  
  // Apply autonomy constraints van AI Bijbel
  const AIBevoegdheidConfig* config = aiBijbel_getConfig(autonomyPercent);
  
  if (config->maxLevelPermanent >= 0) {
    // Check of deze actie is toegestaan
    if (!aiBijbel_magNaarLevel(autonomyPercent, recommendedLevel)) {
      // Beperk tot max permanent
      recommendedLevel = min(recommendedLevel, (int)config->maxLevelPermanent);
    }
  }
  
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
  Serial.println("[ML INT] Starting Model Training");
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  
  int totalSamples = mlState.totalFeedbackSamples + mlState.totalAnnotations;
  Serial.printf("[ML INT] Total training samples: %d\n", totalSamples);
  
  if (totalSamples < 20) {
    Serial.println("[ML INT] ❌ Need at least 20 samples to train!");
    Serial.printf("[ML INT] Current: %d feedback + %d annotations = %d\n",
                  mlState.totalFeedbackSamples, mlState.totalAnnotations, totalSamples);
    return false;
  }
  
  // Laad feedback data
  if (!mlTrainer.loadAlyFile("/ml_training/feedback.csv")) {
    Serial.println("[ML INT] ⚠️ Geen feedback.csv gevonden, probeer .aly bestanden...");
  }
  
  // Start training
  bool success = mlTrainer.startTraining();
  
  if (success) {
    TrainingStatus status = mlTrainer.getStatus();
    mlState.modelTrained = true;
    mlState.modelAccuracy = status.currentAccuracy;
    
    Serial.printf("[ML INT] ✅ Training complete! Accuracy: %.1f%%\n",
                  mlState.modelAccuracy * 100);
    
    // Save model
    mlTrainer.saveModel("/ml_model.bin");
    Serial.println("[ML INT] Model saved to /ml_model.bin");
  } else {
    Serial.println("[ML INT] ❌ Training failed!");
  }
  
  Serial.println("[ML INT] ═══════════════════════════════════════════════");
  return success;
}

void mlIntegration_getStats(int* feedbackCount, int* annotationCount, 
                             bool* modelTrained, float* accuracy) {
  if (feedbackCount) *feedbackCount = mlState.totalFeedbackSamples;
  if (annotationCount) *annotationCount = mlState.totalAnnotations;
  if (modelTrained) *modelTrained = mlState.modelTrained;
  if (accuracy) *accuracy = mlState.modelAccuracy;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         UI HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void mlIntegration_drawPlaybackScreen(Arduino_GFX* gfx, int selectedButton) {
  // Update playback screen met huidige state
  playbackScreen.setProgress(mlState.playbackPosition, mlState.playbackDuration);
  playbackScreen.setCurrentLevel(mlState.currentStressLevel);
  playbackScreen.setAIPrediction(mlState.aiPredictedLevel);
  playbackScreen.setUserAnnotation(mlState.userCorrectedLevel);
  playbackScreen.setSensorValues(mlState.currentHR, mlState.currentTemp, mlState.currentGSR);
  playbackScreen.setSelectedButton(selectedButton);
  
  // Teken
  playbackScreen.drawDynamicElements();
}

int mlIntegration_handlePlaybackTouch(int x, int y) {
  return playbackScreen.handleTouch(x, y);
}

void mlIntegration_handlePlaybackEncoder(int direction) {
  int current = playbackScreen.getSelectedButton();
  int newIdx = current + direction;
  
  // Wrap around
  if (newIdx < 0) newIdx = playbackScreen.getButtonCount() - 1;
  if (newIdx >= playbackScreen.getButtonCount()) newIdx = 0;
  
  playbackScreen.setSelectedButton(newIdx);
}

void mlIntegration_handlePlaybackEncoderPress() {
  int selected = playbackScreen.getSelectedButton();
  
  switch (selected) {
    case 0:  // STOP
      mlIntegration_stopPlayback();
      break;
    case 1:  // PLAY/PAUSE
      // Toggle wordt afgehandeld in body_menu.cpp
      break;
    case 2:  // -10s
      mlState.playbackPosition = max(0.0f, mlState.playbackPosition - 10.0f);
      break;
    case 3:  // +10s
      mlState.playbackPosition = min(mlState.playbackDuration, mlState.playbackPosition + 10.0f);
      break;
    case 4:  // GEVOEL
      // Open popup - wordt afgehandeld in body_menu.cpp
      break;
  }
}

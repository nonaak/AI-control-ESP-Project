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
#include <SD_MMC.h>
#include <Preferences.h>  // 🔥 NIEUW: NVS voor model opslag (overleeft SD format!)

// NVS voor model opslag
static Preferences mlPrefs;

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
  
  // Check of SD_MMC beschikbaar is
  if (SD_MMC.cardType() == CARD_NONE) {
    Serial.println("[ML INT] ❌ Geen SD kaart!");
    return false;
  }
  
  // Maak /ml_training directory (zelfde methode als /recordings)
  if (!SD_MMC.exists("/ml_training")) {
    Serial.println("[ML INT] Creating /ml_training folder...");
    
    if (SD_MMC.mkdir("/ml_training")) {
      Serial.println("[ML INT] ✅ /ml_training folder aangemaakt");
    } else {
      Serial.println("[ML INT] ❌ mkdir failed, probeer direct file...");
      // Fallback: probeer direct bestand aan te maken
    }
  }
  
  // Open feedback file in append mode
  feedbackFile = SD_MMC.open("/ml_training/feedback.csv", FILE_APPEND);
  if (!feedbackFile) {
    Serial.println("[ML INT] ❌ Kan feedback.csv niet openen!");
    Serial.println("[ML INT] Feedback wordt NIET opgeslagen naar SD");
    return false;
  }
  
  // Schrijf header als bestand nieuw is
  if (feedbackFile.size() == 0) {
    feedbackFile.println("timestamp,hr,temp,gsr,ai_level,user_level,event_type");
    Serial.println("[ML INT] ✅ feedback.csv aangemaakt met header");
  } else {
    Serial.printf("[ML INT] ✅ feedback.csv geopend (%d bytes)\n", feedbackFile.size());
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

// ═══════════════════════════════════════════════════════════════════════════
//                         NVS MODEL OPSLAG (overleeft SD format!)
// ═══════════════════════════════════════════════════════════════════════════

// Model structuur voor NVS opslag (compact)
struct MLModelNVS {
  uint32_t magic;           // 0xMLMD voor validatie
  uint32_t version;         // Model versie
  float accuracy;           // Training accuracy
  uint32_t trainedTimestamp;// Wanneer getraind
  uint32_t sampleCount;     // Hoeveel samples gebruikt
  uint8_t modelData[1024];  // Neural network weights (max 1KB)
  uint16_t modelSize;       // Werkelijke grootte
  uint16_t checksum;        // Simpele validatie
};

static const uint32_t ML_MODEL_MAGIC = 0x4D4C4D44;  // "MLMD"

static uint16_t calculateChecksum(const uint8_t* data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += data[i];
  }
  return sum;
}

// Sla model op naar NVS (interne flash - overleeft SD format!)
static bool saveModelToNVS() {
  Serial.println("[ML INT] 💾 Saving model to NVS (internal flash)...");
  
  // Eerst model naar tijdelijk SD bestand exporteren
  if (!mlTrainer.saveModel("/ml_training/temp_model.bin")) {
    Serial.println("[ML INT] ❌ Kan model niet exporteren");
    return false;
  }
  
  // Lees model van SD
  File modelFile = SD_MMC.open("/ml_training/temp_model.bin", FILE_READ);
  if (!modelFile) {
    Serial.println("[ML INT] ❌ Kan temp model niet lezen");
    return false;
  }
  
  MLModelNVS nvsModel;
  memset(&nvsModel, 0, sizeof(nvsModel));
  nvsModel.magic = ML_MODEL_MAGIC;
  nvsModel.version = 1;
  nvsModel.accuracy = mlState.modelAccuracy;
  nvsModel.trainedTimestamp = millis() / 1000;
  nvsModel.sampleCount = mlState.totalFeedbackSamples + mlState.totalAnnotations;
  
  // Lees model data
  nvsModel.modelSize = modelFile.size();
  if (nvsModel.modelSize > sizeof(nvsModel.modelData)) {
    Serial.printf("[ML INT] ❌ Model te groot voor NVS (%d > %d)\n", 
                  nvsModel.modelSize, sizeof(nvsModel.modelData));
    modelFile.close();
    return false;
  }
  
  modelFile.read(nvsModel.modelData, nvsModel.modelSize);
  modelFile.close();
  
  // Bereken checksum
  nvsModel.checksum = calculateChecksum(nvsModel.modelData, nvsModel.modelSize);
  
  // Sla op in NVS
  mlPrefs.begin("ml_model", false);  // false = read/write
  size_t written = mlPrefs.putBytes("model", &nvsModel, sizeof(nvsModel));
  mlPrefs.end();
  
  if (written == sizeof(nvsModel)) {
    Serial.printf("[ML INT] ✅ Model opgeslagen in NVS (%d bytes, %.1f%% accuracy)\n",
                  nvsModel.modelSize, nvsModel.accuracy * 100);
    
    // Verwijder temp bestand
    SD_MMC.remove("/ml_training/temp_model.bin");
    return true;
  } else {
    Serial.println("[ML INT] ❌ NVS write failed");
    return false;
  }
}

// Laad model uit NVS (na SD format of corruptie)
static bool loadModelFromNVS() {
  Serial.println("[ML INT] 🔍 Checking NVS for saved model...");
  
  mlPrefs.begin("ml_model", true);  // true = read-only
  
  MLModelNVS nvsModel;
  size_t readSize = mlPrefs.getBytes("model", &nvsModel, sizeof(nvsModel));
  mlPrefs.end();
  
  if (readSize != sizeof(nvsModel)) {
    Serial.println("[ML INT] Geen model in NVS gevonden");
    return false;
  }
  
  // Valideer magic
  if (nvsModel.magic != ML_MODEL_MAGIC) {
    Serial.println("[ML INT] NVS model ongeldig (bad magic)");
    return false;
  }
  
  // Valideer checksum
  uint16_t calcSum = calculateChecksum(nvsModel.modelData, nvsModel.modelSize);
  if (calcSum != nvsModel.checksum) {
    Serial.println("[ML INT] NVS model corrupt (bad checksum)");
    return false;
  }
  
  Serial.printf("[ML INT] ✅ Found NVS model: %.1f%% accuracy, %d samples\n",
                nvsModel.accuracy * 100, nvsModel.sampleCount);
  
  // Zorg dat /ml_training bestaat
  if (!SD_MMC.exists("/ml_training")) {
    SD_MMC.mkdir("/ml_training");
  }
  
  // Schrijf model naar SD
  File modelFile = SD_MMC.open("/ml_training/model.bin", FILE_WRITE);
  if (!modelFile) {
    Serial.println("[ML INT] ❌ Kan model niet naar SD schrijven");
    return false;
  }
  
  modelFile.write(nvsModel.modelData, nvsModel.modelSize);
  modelFile.close();
  
  // Laad model in trainer
  if (mlTrainer.loadModel("/ml_training/model.bin")) {
    mlState.modelTrained = true;
    mlState.modelAccuracy = nvsModel.accuracy;
    mlState.totalFeedbackSamples = nvsModel.sampleCount;
    Serial.println("[ML INT] ✅ Model hersteld uit NVS naar SD en geladen!");
    return true;
  }
  
  return false;
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
  
  // Check voor bestaand model op SD
  if (mlTrainer.hasModel()) {
    mlState.modelTrained = true;
    TrainingStatus status = mlTrainer.getStatus();
    mlState.modelAccuracy = status.currentAccuracy;
    Serial.printf("[ML INT] ✅ Trained model loaded from SD (accuracy: %.1f%%)\n", 
                  mlState.modelAccuracy * 100);
  } else {
    // 🔥 NIEUW: Check NVS voor backup model (na SD format!)
    Serial.println("[ML INT] No model on SD - checking NVS backup...");
    if (loadModelFromNVS()) {
      Serial.println("[ML INT] ✅ Model hersteld uit NVS backup!");
    } else {
      Serial.println("[ML INT] Geen model gevonden");
      Serial.println("[ML INT] Verzamel feedback via nunchuk of GEVOEL knop!");
    }
  }
  
  // Reset counters
  if (!mlState.modelTrained) {
    mlState.totalFeedbackSamples = 0;
    mlState.totalAnnotations = 0;
  }
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
    
    // Save model naar SD
    mlTrainer.saveModel("/ml_training/model.bin");
    Serial.println("[ML INT] Model saved to /ml_training/model.bin");
    
    // 🔥 NIEUW: Backup naar NVS (overleeft SD format!)
    if (saveModelToNVS()) {
      Serial.println("[ML INT] ✅ Model backed up to NVS (safe from SD format!)");
    } else {
      Serial.println("[ML INT] ⚠️ NVS backup failed - model only on SD");
    }
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

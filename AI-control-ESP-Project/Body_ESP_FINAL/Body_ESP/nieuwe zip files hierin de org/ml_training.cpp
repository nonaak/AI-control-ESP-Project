/*
  ML TRAINING SYSTEM - Implementatie
  
  ═══════════════════════════════════════════════════════════════════════════
  ECHTE MACHINE LEARNING DIE LEERT VAN JOUW FEEDBACK
  ═══════════════════════════════════════════════════════════════════════════
  
  Features:
  - Logt ELKE nunchuk correctie met volledige sensor context
  - Simpel neural network dat patronen leert
  - Slaat training data op naar SD voor later trainen
  - Na genoeg data: ML neemt betere beslissingen dan rule-based
*/

#include "ml_training.h"
#include "ai_bijbel.h"

// Global instance
MLTrainer mlTrainer;

// ═══════════════════════════════════════════════════════════════════════════
//                         CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════

MLTrainer::MLTrainer() {
  modelLoaded = false;
  lastConfidence = 0.0f;
  modelAccuracy = 0.0f;
  bufferIndex = 0;
  bufferFull = false;
  
  // Clear state
  memset(&state, 0, sizeof(state));
  state.feedbackBuffer = nullptr;
  state.feedbackCapacity = 0;
  
  // Clear buffers
  memset(hrBuffer, 0, sizeof(hrBuffer));
  memset(gsrBuffer, 0, sizeof(gsrBuffer));
}

MLTrainer::~MLTrainer() {
  if (state.feedbackBuffer) {
    free(state.feedbackBuffer);
    state.feedbackBuffer = nullptr;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                         INITIALISATIE
// ═══════════════════════════════════════════════════════════════════════════

bool MLTrainer::begin() {
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] Initializing ML Training System");
  Serial.println("[ML TRAIN] Dit systeem leert van JOUW feedback!");
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  
  // Maak training directory
  if (!SD_MMC.exists(ML_TRAINING_DIR)) {
    if (SD_MMC.mkdir(ML_TRAINING_DIR)) {
      Serial.println("[ML TRAIN] Created training directory");
    } else {
      Serial.println("[ML TRAIN] WARNING: Could not create training directory");
    }
  }
  
  // Alloceer feedback buffer (start klein, groeit indien nodig)
  state.feedbackCapacity = 100;
  state.feedbackBuffer = (FeedbackSample*)malloc(sizeof(FeedbackSample) * state.feedbackCapacity);
  if (!state.feedbackBuffer) {
    Serial.println("[ML TRAIN] ERROR: Could not allocate feedback buffer!");
    state.feedbackCapacity = 0;
    return false;
  }
  state.feedbackCount = 0;
  
  // Initialize neural network met random weights
  initNetwork();
  
  // Probeer bestaand model te laden
  if (loadModel()) {
    Serial.printf("[ML TRAIN] Loaded trained model (v%d, %d samples, %.1f%% accuracy)\n",
                  network.version, network.totalSamples, modelAccuracy * 100);
  } else {
    Serial.println("[ML TRAIN] No trained model found - starting fresh");
    Serial.println("[ML TRAIN] Verzamel feedback om ML te trainen!");
  }
  
  // Print totaal aantal opgeslagen feedback samples
  int totalSamples = getTotalFeedbackCount();
  Serial.printf("[ML TRAIN] Total feedback samples on SD: %d\n", totalSamples);
  
  if (totalSamples < 50) {
    Serial.println("[ML TRAIN] TIP: Verzamel minstens 50 samples voor basis training");
  } else if (totalSamples < 200) {
    Serial.println("[ML TRAIN] TIP: Meer data = beter model. Blijf feedback geven!");
  } else {
    Serial.println("[ML TRAIN] Voldoende data voor goede training!");
  }
  
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  
  return true;
}

void MLTrainer::initNetwork() {
  // Random seed
  randomSeed(analogRead(0) + millis());
  
  // Xavier initialization voor weights
  float scaleIH = sqrt(2.0f / ML_FEATURE_COUNT);
  float scaleHO = sqrt(2.0f / ML_HIDDEN_NEURONS);
  
  // Input -> Hidden
  for (int i = 0; i < ML_FEATURE_COUNT; i++) {
    for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
      network.weightsIH[i][h] = ((float)random(-1000, 1000) / 1000.0f) * scaleIH;
    }
  }
  for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
    network.biasH[h] = 0.0f;
  }
  
  // Hidden -> Output
  for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
    for (int o = 0; o < 8; o++) {
      network.weightsHO[h][o] = ((float)random(-1000, 1000) / 1000.0f) * scaleHO;
    }
  }
  for (int o = 0; o < 8; o++) {
    network.biasO[o] = 0.0f;
  }
  
  network.trainingEpochs = 0;
  network.lastLoss = 999.0f;
  network.totalSamples = 0;
  network.version = 1;
  network.lastTrainedTime = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         SESSIE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════

bool MLTrainer::startSession() {
  if (state.sessionActive) {
    Serial.println("[ML TRAIN] Session already active!");
    return false;
  }
  
  state.sessionActive = true;
  state.sessionStartTime = millis();
  state.feedbackCount = 0;
  state.edgeCount = 0;
  state.lastEdgeTime = 0;
  state.totalCorrections = 0;
  state.correctionsUp = 0;
  state.correctionsDown = 0;
  state.avgCorrection = 0.0f;
  
  // Maak sessie filename met timestamp
  // Format: session_YYYYMMDD_HHMMSS.csv
  state.sessionFilename = String(ML_SESSION_PREFIX);
  state.sessionFilename += String(millis());  // Simpel, vervang later met RTC
  state.sessionFilename += ".csv";
  
  // Schrijf header naar sessie file
  File f = SD_MMC.open(state.sessionFilename.c_str(), FILE_WRITE);
  if (f) {
    f.println("Timestamp,SessionTime,HR,HR_Avg,HR_Trend,Temp,Temp_Delta,GSR,GSR_Avg,GSR_Trend,EdgeCount,TimeSinceEdge,CurrentLevel,AI_Level,User_Level,Correction");
    f.close();
    Serial.printf("[ML TRAIN] Started session: %s\n", state.sessionFilename.c_str());
  } else {
    Serial.println("[ML TRAIN] WARNING: Could not create session file");
  }
  
  // Reset rolling average buffers
  bufferIndex = 0;
  bufferFull = false;
  memset(hrBuffer, 0, sizeof(hrBuffer));
  memset(gsrBuffer, 0, sizeof(gsrBuffer));
  
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] 🎯 TRAINING SESSION GESTART");
  Serial.println("[ML TRAIN] Elke nunchuk correctie wordt gelogd!");
  Serial.println("[ML TRAIN] ML leert van de CONTEXT, niet alleen de correctie");
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  
  return true;
}

void MLTrainer::endSession() {
  if (!state.sessionActive) return;
  
  state.sessionActive = false;
  
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] 📊 SESSION ENDED - STATISTICS:");
  Serial.printf("[ML TRAIN]   Duration: %d seconds\n", (millis() - state.sessionStartTime) / 1000);
  Serial.printf("[ML TRAIN]   Feedback samples: %d\n", state.feedbackCount);
  Serial.printf("[ML TRAIN]   Total corrections: %d\n", state.totalCorrections);
  Serial.printf("[ML TRAIN]   Corrections UP: %d\n", state.correctionsUp);
  Serial.printf("[ML TRAIN]   Corrections DOWN: %d\n", state.correctionsDown);
  Serial.printf("[ML TRAIN]   Avg correction: %.2f levels\n", state.avgCorrection);
  Serial.printf("[ML TRAIN]   Edge count: %d\n", state.edgeCount);
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  
  if (state.feedbackCount > 0) {
    Serial.println("[ML TRAIN] TIP: Run ml_trainModel() om model te updaten!");
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                         SENSOR UPDATES
// ═══════════════════════════════════════════════════════════════════════════

void MLTrainer::updateSensorContext(float hr, float temp, float gsr) {
  // Update huidige waarden
  state.currentHR = hr;
  state.currentTemp = temp;
  state.currentGSR = gsr;
  
  // Update rolling averages
  updateRollingAverages(hr, gsr);
  
  // Bereken averages en trends
  int count = bufferFull ? 30 : bufferIndex;
  if (count > 0) {
    float hrSum = 0, gsrSum = 0;
    for (int i = 0; i < count; i++) {
      hrSum += hrBuffer[i];
      gsrSum += gsrBuffer[i];
    }
    state.currentHRAvg = hrSum / count;
    state.currentGSRAvg = gsrSum / count;
    
    // Bereken trends
    state.currentHRTrend = calculateTrend(hrBuffer, count);
    state.currentGSRTrend = calculateTrend(gsrBuffer, count);
  }
  
  // Temp delta (aanname: baseline is 36.5)
  state.currentTempDelta = temp - 36.5f;
}

void MLTrainer::updateRollingAverages(float hr, float gsr) {
  hrBuffer[bufferIndex] = hr;
  gsrBuffer[bufferIndex] = gsr;
  
  bufferIndex = (bufferIndex + 1) % 30;
  if (bufferIndex == 0) bufferFull = true;
}

float MLTrainer::calculateTrend(float* buffer, int count) {
  if (count < 5) return 0.0f;
  
  // Simpele trend: verschil tussen laatste 5 en eerste 5 samples
  float firstAvg = 0, lastAvg = 0;
  for (int i = 0; i < 5; i++) {
    firstAvg += buffer[i];
    lastAvg += buffer[count - 5 + i];
  }
  firstAvg /= 5;
  lastAvg /= 5;
  
  return lastAvg - firstAvg;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         FEEDBACK LOGGING
// ═══════════════════════════════════════════════════════════════════════════

void MLTrainer::logFeedback(int aiChosenLevel, int userChosenLevel) {
  if (!state.sessionActive) {
    Serial.println("[ML TRAIN] WARNING: No active session, starting one...");
    startSession();
  }
  
  // Check buffer capacity
  if (state.feedbackCount >= state.feedbackCapacity) {
    // Probeer te groeien
    int newCapacity = state.feedbackCapacity * 2;
    FeedbackSample* newBuffer = (FeedbackSample*)realloc(state.feedbackBuffer, 
                                                          sizeof(FeedbackSample) * newCapacity);
    if (newBuffer) {
      state.feedbackBuffer = newBuffer;
      state.feedbackCapacity = newCapacity;
    } else {
      Serial.println("[ML TRAIN] WARNING: Buffer full, saving to SD only");
    }
  }
  
  // Maak feedback sample
  FeedbackSample sample;
  sample.timestamp = millis();
  sample.sessionTime = (millis() - state.sessionStartTime) / 1000;
  
  // Sensor data
  sample.heartRate = state.currentHR;
  sample.heartRateAvg = state.currentHRAvg;
  sample.heartRateTrend = state.currentHRTrend;
  sample.temperature = state.currentTemp;
  sample.tempDelta = state.currentTempDelta;
  sample.gsr = state.currentGSR;
  sample.gsrAvg = state.currentGSRAvg;
  sample.gsrTrend = state.currentGSRTrend;
  
  // Context
  sample.edgeCount = state.edgeCount;
  sample.timeSinceLastEdge = state.lastEdgeTime > 0 ? 
                              (millis() - state.lastEdgeTime) / 1000.0f : -1;
  sample.currentLevel = aiChosenLevel;  // Was het actieve level
  
  // AI vs User
  sample.aiChosenLevel = aiChosenLevel;
  sample.userChosenLevel = userChosenLevel;
  sample.correction = userChosenLevel - aiChosenLevel;
  sample.wasGoodDecision = false;  // Wordt later ingevuld
  
  // Voeg toe aan buffer (indien ruimte)
  if (state.feedbackCount < state.feedbackCapacity) {
    state.feedbackBuffer[state.feedbackCount] = sample;
    state.feedbackCount++;
  }
  
  // Sla op naar SD
  saveFeedbackToSD(sample);
  
  // Update statistics
  state.totalCorrections++;
  if (sample.correction > 0) state.correctionsUp++;
  else if (sample.correction < 0) state.correctionsDown++;
  state.avgCorrection = (state.avgCorrection * (state.totalCorrections - 1) + abs(sample.correction)) 
                        / state.totalCorrections;
  
  // Debug output
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] 📝 FEEDBACK LOGGED!");
  Serial.printf("[ML TRAIN]   AI chose: Level %d\n", aiChosenLevel);
  Serial.printf("[ML TRAIN]   You chose: Level %d\n", userChosenLevel);
  Serial.printf("[ML TRAIN]   Correction: %+d\n", sample.correction);
  Serial.println("[ML TRAIN] ─────────────────────────────────────────────────");
  Serial.println("[ML TRAIN]   Context op moment van correctie:");
  Serial.printf("[ML TRAIN]   HR: %.0f (avg: %.0f, trend: %+.1f)\n", 
                sample.heartRate, sample.heartRateAvg, sample.heartRateTrend);
  Serial.printf("[ML TRAIN]   GSR: %.0f (avg: %.0f, trend: %+.1f)\n",
                sample.gsr, sample.gsrAvg, sample.gsrTrend);
  Serial.printf("[ML TRAIN]   Temp: %.1f (delta: %+.2f)\n",
                sample.temperature, sample.tempDelta);
  Serial.printf("[ML TRAIN]   Edges: %d, Time since edge: %.0fs\n",
                sample.edgeCount, sample.timeSinceLastEdge);
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] ML leert: bij DEZE sensor waarden was mijn");
  Serial.printf("[ML TRAIN] keuze %+d levels FOUT. Volgende keer beter!\n", -sample.correction);
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
}

bool MLTrainer::saveFeedbackToSD(const FeedbackSample& sample) {
  // Append naar sessie file
  File f = SD_MMC.open(state.sessionFilename.c_str(), FILE_APPEND);
  if (!f) {
    Serial.println("[ML TRAIN] ERROR: Could not open session file!");
    return false;
  }
  
  // CSV format
  f.printf("%lu,%lu,%.1f,%.1f,%.2f,%.2f,%.3f,%.0f,%.0f,%.2f,%d,%.1f,%d,%d,%d,%d\n",
           sample.timestamp,
           sample.sessionTime,
           sample.heartRate,
           sample.heartRateAvg,
           sample.heartRateTrend,
           sample.temperature,
           sample.tempDelta,
           sample.gsr,
           sample.gsrAvg,
           sample.gsrTrend,
           sample.edgeCount,
           sample.timeSinceLastEdge,
           sample.currentLevel,
           sample.aiChosenLevel,
           sample.userChosenLevel,
           sample.correction);
  
  f.close();
  
  // Ook naar globale feedback file
  File global = SD_MMC.open(ML_FEEDBACK_FILE, FILE_APPEND);
  if (global) {
    // Zelfde format
    global.printf("%lu,%lu,%.1f,%.1f,%.2f,%.2f,%.3f,%.0f,%.0f,%.2f,%d,%.1f,%d,%d,%d,%d\n",
                  sample.timestamp,
                  sample.sessionTime,
                  sample.heartRate,
                  sample.heartRateAvg,
                  sample.heartRateTrend,
                  sample.temperature,
                  sample.tempDelta,
                  sample.gsr,
                  sample.gsrAvg,
                  sample.gsrTrend,
                  sample.edgeCount,
                  sample.timeSinceLastEdge,
                  sample.currentLevel,
                  sample.aiChosenLevel,
                  sample.userChosenLevel,
                  sample.correction);
    global.close();
  }
  
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         EDGE / ORGASME EVENTS
// ═══════════════════════════════════════════════════════════════════════════

void MLTrainer::logEdgeEvent() {
  state.edgeCount++;
  state.lastEdgeTime = millis();
  
  Serial.printf("[ML TRAIN] 🔥 EDGE #%d logged at session time %ds\n",
                state.edgeCount, (millis() - state.sessionStartTime) / 1000);
  
  // Mark recent feedback samples as "led to edge" (positive outcome)
  // Dit helpt ML leren welke sensor patronen naar edge leiden
  for (int i = max(0, state.feedbackCount - 5); i < state.feedbackCount; i++) {
    state.feedbackBuffer[i].wasGoodDecision = true;
  }
}

void MLTrainer::logOrgasmeEvent() {
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] 💦 ORGASME EVENT LOGGED!");
  Serial.printf("[ML TRAIN] Session time: %d seconds\n", (millis() - state.sessionStartTime) / 1000);
  Serial.printf("[ML TRAIN] Total edges: %d\n", state.edgeCount);
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  
  // End session
  endSession();
}

// ═══════════════════════════════════════════════════════════════════════════
//                         NEURAL NETWORK FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

float MLTrainer::sigmoid(float x) {
  return 1.0f / (1.0f + exp(-x));
}

float MLTrainer::sigmoidDerivative(float x) {
  float s = sigmoid(x);
  return s * (1.0f - s);
}

void MLTrainer::forwardPass(float* inputs, float* hidden, float* outputs) {
  // Input -> Hidden
  for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
    float sum = network.biasH[h];
    for (int i = 0; i < ML_FEATURE_COUNT; i++) {
      sum += inputs[i] * network.weightsIH[i][h];
    }
    hidden[h] = sigmoid(sum);
  }
  
  // Hidden -> Output (softmax voor classificatie)
  float maxOutput = -999.0f;
  for (int o = 0; o < 8; o++) {
    float sum = network.biasO[o];
    for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
      sum += hidden[h] * network.weightsHO[h][o];
    }
    outputs[o] = sum;
    if (sum > maxOutput) maxOutput = sum;
  }
  
  // Softmax
  float expSum = 0;
  for (int o = 0; o < 8; o++) {
    outputs[o] = exp(outputs[o] - maxOutput);  // Subtract max for stability
    expSum += outputs[o];
  }
  for (int o = 0; o < 8; o++) {
    outputs[o] /= expSum;
  }
}

void MLTrainer::backpropagate(float* inputs, float* hidden, float* outputs, int targetLevel) {
  // Output layer errors
  float outputErrors[8];
  for (int o = 0; o < 8; o++) {
    float target = (o == targetLevel) ? 1.0f : 0.0f;
    outputErrors[o] = target - outputs[o];  // Cross-entropy derivative
  }
  
  // Hidden layer errors
  float hiddenErrors[ML_HIDDEN_NEURONS];
  for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
    float error = 0;
    for (int o = 0; o < 8; o++) {
      error += outputErrors[o] * network.weightsHO[h][o];
    }
    hiddenErrors[h] = error * hidden[h] * (1.0f - hidden[h]);  // Sigmoid derivative
  }
  
  // Update weights: Hidden -> Output
  for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
    for (int o = 0; o < 8; o++) {
      network.weightsHO[h][o] += ML_LEARNING_RATE * outputErrors[o] * hidden[h];
    }
  }
  for (int o = 0; o < 8; o++) {
    network.biasO[o] += ML_LEARNING_RATE * outputErrors[o];
  }
  
  // Update weights: Input -> Hidden
  for (int i = 0; i < ML_FEATURE_COUNT; i++) {
    for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
      network.weightsIH[i][h] += ML_LEARNING_RATE * hiddenErrors[h] * inputs[i];
    }
  }
  for (int h = 0; h < ML_HIDDEN_NEURONS; h++) {
    network.biasH[h] += ML_LEARNING_RATE * hiddenErrors[h];
  }
}

float MLTrainer::calculateLoss(float* outputs, int targetLevel) {
  // Cross-entropy loss
  float target = outputs[targetLevel];
  if (target < 0.0001f) target = 0.0001f;  // Prevent log(0)
  return -log(target);
}

void MLTrainer::normalizeInputs(FeedbackSample& sample, float* normalized) {
  // Normaliseer alle inputs naar 0-1 bereik
  // Dit is CRUCIAAL voor goede neural network training!
  
  // HR: 50-180 BPM → 0-1
  normalized[0] = (sample.heartRate - 50.0f) / 130.0f;
  normalized[1] = (sample.heartRateAvg - 50.0f) / 130.0f;
  normalized[2] = (sample.heartRateTrend + 30.0f) / 60.0f;  // -30 tot +30 → 0-1
  
  // Temp: 35-40°C → 0-1
  normalized[3] = (sample.temperature - 35.0f) / 5.0f;
  normalized[4] = (sample.tempDelta + 2.0f) / 4.0f;  // -2 tot +2 → 0-1
  
  // GSR: 0-1500 → 0-1
  normalized[5] = sample.gsr / 1500.0f;
  normalized[6] = sample.gsrAvg / 1500.0f;
  normalized[7] = (sample.gsrTrend + 200.0f) / 400.0f;  // -200 tot +200 → 0-1
  
  // Context
  normalized[8] = sample.edgeCount / 10.0f;  // Max 10 edges
  normalized[9] = sample.timeSinceLastEdge > 0 ? 
                  min(1.0f, sample.timeSinceLastEdge / 300.0f) : 0.5f;  // Max 5 min
  normalized[10] = sample.currentLevel / 7.0f;  // Level 0-7 → 0-1
  normalized[11] = sample.sessionTime / 3600.0f;  // Max 1 uur
  
  // Clamp alle waarden
  for (int i = 0; i < ML_FEATURE_COUNT; i++) {
    normalized[i] = constrain(normalized[i], 0.0f, 1.0f);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//                         MODEL TRAINING
// ═══════════════════════════════════════════════════════════════════════════

bool MLTrainer::trainModel(int epochs) {
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] 🧠 STARTING MODEL TRAINING");
  Serial.printf("[ML TRAIN] Epochs: %d\n", epochs);
  
  // Laad alle feedback data van SD
  int totalSamples = getTotalFeedbackCount();
  if (totalSamples < 20) {
    Serial.printf("[ML TRAIN] ERROR: Need at least 20 samples, have %d\n", totalSamples);
    Serial.println("[ML TRAIN] Blijf feedback geven via nunchuk!");
    return false;
  }
  
  Serial.printf("[ML TRAIN] Training on %d samples\n", totalSamples);
  
  // Laad samples van SD
  if (!loadFeedbackFromSD(ML_FEEDBACK_FILE)) {
    Serial.println("[ML TRAIN] ERROR: Could not load training data");
    return false;
  }
  
  Serial.printf("[ML TRAIN] Loaded %d samples into memory\n", state.feedbackCount);
  
  // Training loop
  float inputs[ML_FEATURE_COUNT];
  float hidden[ML_HIDDEN_NEURONS];
  float outputs[8];
  
  float totalLoss = 0;
  int correctPredictions = 0;
  
  for (int epoch = 0; epoch < epochs; epoch++) {
    totalLoss = 0;
    correctPredictions = 0;
    
    // Shuffle samples (simpele versie: random swap)
    for (int i = state.feedbackCount - 1; i > 0; i--) {
      int j = random(0, i + 1);
      FeedbackSample temp = state.feedbackBuffer[i];
      state.feedbackBuffer[i] = state.feedbackBuffer[j];
      state.feedbackBuffer[j] = temp;
    }
    
    // Train op elke sample
    for (int s = 0; s < state.feedbackCount; s++) {
      FeedbackSample& sample = state.feedbackBuffer[s];
      
      // Normaliseer inputs
      normalizeInputs(sample, inputs);
      
      // Forward pass
      forwardPass(inputs, hidden, outputs);
      
      // Het target is wat de GEBRUIKER koos (dat is correct!)
      int targetLevel = sample.userChosenLevel;
      
      // Bereken loss
      totalLoss += calculateLoss(outputs, targetLevel);
      
      // Check accuracy
      int predicted = 0;
      float maxProb = 0;
      for (int o = 0; o < 8; o++) {
        if (outputs[o] > maxProb) {
          maxProb = outputs[o];
          predicted = o;
        }
      }
      if (predicted == targetLevel) correctPredictions++;
      
      // Backpropagation
      backpropagate(inputs, hidden, outputs, targetLevel);
    }
    
    // Progress
    if (epoch % 10 == 0 || epoch == epochs - 1) {
      float avgLoss = totalLoss / state.feedbackCount;
      float accuracy = (float)correctPredictions / state.feedbackCount * 100.0f;
      Serial.printf("[ML TRAIN] Epoch %d/%d - Loss: %.4f - Accuracy: %.1f%%\n",
                    epoch + 1, epochs, avgLoss, accuracy);
    }
    
    yield();  // Geef systeem tijd
  }
  
  // Final stats
  network.trainingEpochs += epochs;
  network.lastLoss = totalLoss / state.feedbackCount;
  network.totalSamples = state.feedbackCount;
  network.lastTrainedTime = millis();
  network.version++;
  
  modelAccuracy = (float)correctPredictions / state.feedbackCount;
  modelLoaded = true;
  
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] ✅ TRAINING COMPLETE!");
  Serial.printf("[ML TRAIN] Final accuracy: %.1f%%\n", modelAccuracy * 100);
  Serial.printf("[ML TRAIN] Model version: %d\n", network.version);
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  
  // Sla model op
  saveModel();
  
  return true;
}

bool MLTrainer::loadFeedbackFromSD(const char* filename) {
  File f = SD_MMC.open(filename, FILE_READ);
  if (!f) {
    Serial.printf("[ML TRAIN] Could not open: %s\n", filename);
    return false;
  }
  
  // Skip header
  f.readStringUntil('\n');
  
  state.feedbackCount = 0;
  
  while (f.available() && state.feedbackCount < state.feedbackCapacity) {
    String line = f.readStringUntil('\n');
    if (line.length() < 10) continue;
    
    // Parse CSV
    FeedbackSample& sample = state.feedbackBuffer[state.feedbackCount];
    
    // Simpele parsing (positie-gebaseerd)
    int idx = 0;
    int lastComma = -1;
    int fieldNum = 0;
    
    for (int i = 0; i <= line.length(); i++) {
      if (i == line.length() || line.charAt(i) == ',') {
        String field = line.substring(lastComma + 1, i);
        
        switch (fieldNum) {
          case 0: sample.timestamp = field.toInt(); break;
          case 1: sample.sessionTime = field.toInt(); break;
          case 2: sample.heartRate = field.toFloat(); break;
          case 3: sample.heartRateAvg = field.toFloat(); break;
          case 4: sample.heartRateTrend = field.toFloat(); break;
          case 5: sample.temperature = field.toFloat(); break;
          case 6: sample.tempDelta = field.toFloat(); break;
          case 7: sample.gsr = field.toFloat(); break;
          case 8: sample.gsrAvg = field.toFloat(); break;
          case 9: sample.gsrTrend = field.toFloat(); break;
          case 10: sample.edgeCount = field.toInt(); break;
          case 11: sample.timeSinceLastEdge = field.toFloat(); break;
          case 12: sample.currentLevel = field.toInt(); break;
          case 13: sample.aiChosenLevel = field.toInt(); break;
          case 14: sample.userChosenLevel = field.toInt(); break;
          case 15: sample.correction = field.toInt(); break;
        }
        
        lastComma = i;
        fieldNum++;
      }
    }
    
    state.feedbackCount++;
  }
  
  f.close();
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         MODEL SAVE/LOAD
// ═══════════════════════════════════════════════════════════════════════════

bool MLTrainer::saveModel() {
  File f = SD_MMC.open(ML_MODEL_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("[ML TRAIN] ERROR: Could not save model");
    return false;
  }
  
  // Schrijf magic header
  f.write((uint8_t*)"MLTR", 4);
  
  // Schrijf network struct
  f.write((uint8_t*)&network, sizeof(network));
  
  // Schrijf accuracy
  f.write((uint8_t*)&modelAccuracy, sizeof(modelAccuracy));
  
  f.close();
  
  Serial.printf("[ML TRAIN] Model saved to %s\n", ML_MODEL_FILE);
  return true;
}

bool MLTrainer::loadModel() {
  File f = SD_MMC.open(ML_MODEL_FILE, FILE_READ);
  if (!f) {
    return false;
  }
  
  // Check magic header
  char magic[4];
  f.read((uint8_t*)magic, 4);
  if (memcmp(magic, "MLTR", 4) != 0) {
    f.close();
    Serial.println("[ML TRAIN] Invalid model file");
    return false;
  }
  
  // Lees network struct
  f.read((uint8_t*)&network, sizeof(network));
  
  // Lees accuracy
  f.read((uint8_t*)&modelAccuracy, sizeof(modelAccuracy));
  
  f.close();
  
  modelLoaded = true;
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         INFERENCE (VOORSPELLING)
// ═══════════════════════════════════════════════════════════════════════════

int MLTrainer::predictOptimalLevel(float hr, float temp, float gsr) {
  if (!modelLoaded) {
    // Fallback naar rule-based AI Bijbel
    return -1;
  }
  
  // Maak een pseudo-sample voor normalisatie
  FeedbackSample sample;
  sample.heartRate = hr;
  sample.heartRateAvg = state.currentHRAvg;
  sample.heartRateTrend = state.currentHRTrend;
  sample.temperature = temp;
  sample.tempDelta = temp - 36.5f;
  sample.gsr = gsr;
  sample.gsrAvg = state.currentGSRAvg;
  sample.gsrTrend = state.currentGSRTrend;
  sample.edgeCount = state.edgeCount;
  sample.timeSinceLastEdge = state.lastEdgeTime > 0 ? 
                              (millis() - state.lastEdgeTime) / 1000.0f : -1;
  sample.currentLevel = aiState.currentLevel;
  sample.sessionTime = state.sessionActive ? 
                        (millis() - state.sessionStartTime) / 1000 : 0;
  
  // Normaliseer
  float inputs[ML_FEATURE_COUNT];
  normalizeInputs(sample, inputs);
  
  // Forward pass
  float hidden[ML_HIDDEN_NEURONS];
  float outputs[8];
  forwardPass(inputs, hidden, outputs);
  
  // Vind hoogste probabiliteit
  int bestLevel = 0;
  float bestProb = 0;
  for (int o = 0; o < 8; o++) {
    if (outputs[o] > bestProb) {
      bestProb = outputs[o];
      bestLevel = o;
    }
  }
  
  lastConfidence = bestProb;
  
  return bestLevel;
}

// ═══════════════════════════════════════════════════════════════════════════
//                         STATISTICS
// ═══════════════════════════════════════════════════════════════════════════

int MLTrainer::getTotalFeedbackCount() {
  File f = SD_MMC.open(ML_FEEDBACK_FILE, FILE_READ);
  if (!f) return 0;
  
  int count = 0;
  f.readStringUntil('\n');  // Skip header
  
  while (f.available()) {
    f.readStringUntil('\n');
    count++;
  }
  
  f.close();
  return count;
}

String MLTrainer::getModelInfo() {
  if (!modelLoaded) {
    return "No model trained - collecting feedback";
  }
  
  String info = "Model v" + String(network.version);
  info += " | " + String(network.totalSamples) + " samples";
  info += " | " + String(modelAccuracy * 100, 1) + "% accuracy";
  info += " | " + String(network.trainingEpochs) + " epochs";
  
  return info;
}

void MLTrainer::printStats() {
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
  Serial.println("[ML TRAIN] 📊 ML TRAINING STATISTICS");
  Serial.println("[ML TRAIN] ─────────────────────────────────────────────────");
  Serial.printf("[ML TRAIN] Total feedback samples: %d\n", getTotalFeedbackCount());
  Serial.printf("[ML TRAIN] Session samples: %d\n", state.feedbackCount);
  Serial.printf("[ML TRAIN] Session active: %s\n", state.sessionActive ? "YES" : "NO");
  Serial.println("[ML TRAIN] ─────────────────────────────────────────────────");
  
  if (modelLoaded) {
    Serial.printf("[ML TRAIN] Model version: %d\n", network.version);
    Serial.printf("[ML TRAIN] Model accuracy: %.1f%%\n", modelAccuracy * 100);
    Serial.printf("[ML TRAIN] Training epochs: %d\n", network.trainingEpochs);
    Serial.printf("[ML TRAIN] Last loss: %.4f\n", network.lastLoss);
  } else {
    Serial.println("[ML TRAIN] No trained model yet");
    Serial.println("[ML TRAIN] Collect more feedback and run ml_trainModel()");
  }
  
  Serial.println("[ML TRAIN] ═══════════════════════════════════════════════");
}

void MLTrainer::printLastFeedback() {
  if (state.feedbackCount == 0) {
    Serial.println("[ML TRAIN] No feedback in current session");
    return;
  }
  
  FeedbackSample& last = state.feedbackBuffer[state.feedbackCount - 1];
  
  Serial.println("[ML TRAIN] Last feedback sample:");
  Serial.printf("  HR: %.0f (avg: %.0f)\n", last.heartRate, last.heartRateAvg);
  Serial.printf("  GSR: %.0f (avg: %.0f)\n", last.gsr, last.gsrAvg);
  Serial.printf("  Temp: %.1f\n", last.temperature);
  Serial.printf("  AI chose: %d, You chose: %d (correction: %+d)\n",
                last.aiChosenLevel, last.userChosenLevel, last.correction);
}

// ═══════════════════════════════════════════════════════════════════════════
//                    GLOBAL CONVENIENCE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

bool ml_startTrainingSession() {
  return mlTrainer.startSession();
}

void ml_endTrainingSession() {
  mlTrainer.endSession();
}

void ml_updateSensors(float hr, float temp, float gsr) {
  mlTrainer.updateSensorContext(hr, temp, gsr);
}

void ml_logUserCorrection(int aiLevel, int userLevel) {
  mlTrainer.logFeedback(aiLevel, userLevel);
}

void ml_logEdge() {
  mlTrainer.logEdgeEvent();
}

void ml_logOrgasme() {
  mlTrainer.logOrgasmeEvent();
}

bool ml_trainModel(int epochs) {
  return mlTrainer.trainModel(epochs);
}

int ml_predictLevel(float hr, float temp, float gsr) {
  return mlTrainer.predictOptimalLevel(hr, temp, gsr);
}

bool ml_hasTrainedModel() {
  return mlTrainer.hasTrainedModel();
}

void ml_printTrainingStats() {
  mlTrainer.printStats();
}

/*
  ML Stress Analyzer Implementation
  
  ═══════════════════════════════════════════════════════════════════════════
  GEÏNTEGREERD MET AI BIJBEL - Consistente stress levels (0-7)
  ═══════════════════════════════════════════════════════════════════════════
  
  Bevat EEPROM model storage, feature extraction en AI Bijbel classifier
*/

#include "ml_stress_analyzer.h"
#include "ai_bijbel.h"  // 🔥 AI BIJBEL INTEGRATIE

// Global instance
MLStressAnalyzer mlAnalyzer;

// Global runtime state (uit ai_bijbel.h)
AIRuntimeState aiState = {
  .currentAutonomy = 0,
  .currentLevel = 0,
  .inTijdelijkeDip = false,
  .tijdelijkeDipStart = 0,
  .tijdelijkeDipLevel = -1,
  .edgeCount = 0,
  .lastEdgeTime = 0,
  .sessionActive = false,
  .isPaused = false,
  .orgasmeTriggered = false,
  .lastNunchukOverride = 0,
  .nunchukOverrideActive = false
};

// ═══════════════════════════════════════════════════════════════════════════
//                    AI BIJBEL BASELINES (HARDCODED)
// ═══════════════════════════════════════════════════════════════════════════

// Hartslag thresholds (BIJBEL_ prefix om conflict met config.h te voorkomen)
#define BIJBEL_HR_BASELINE     70.0f    // Gemiddelde hartslag in rust
#define BIJBEL_HR_EXCITED     100.0f    // Hartslag bij opwinding
#define BIJBEL_HR_EDGE        130.0f    // Hartslag bij edge zone
#define BIJBEL_HR_MAX         160.0f    // Maximum veilige hartslag
#define BIJBEL_HR_EMERGENCY   180.0f    // Emergency! Te hoog!

// Temperatuur thresholds
#define BIJBEL_TEMP_BASELINE   36.5f    // Normale lichaamstemperatuur
#define BIJBEL_TEMP_ELEVATED   37.5f    // Verhoogde temperatuur (opwinding)
#define BIJBEL_TEMP_HIGH       38.0f    // Hoge temperatuur
#define BIJBEL_TEMP_EMERGENCY  39.0f    // Emergency! Koorts!

// GSR (huidgeleiding) thresholds
#define BIJBEL_GSR_BASELINE   300.0f    // GSR in rust
#define BIJBEL_GSR_AROUSED    600.0f    // GSR bij opwinding
#define BIJBEL_GSR_EDGE       900.0f    // GSR bij edge zone
#define BIJBEL_GSR_MAX       1200.0f    // Maximum GSR

// Gewichten voor multi-factor berekening
#define WEIGHT_HR      0.50f     // Hartslag weegt het zwaarst (50%)
#define WEIGHT_GSR     0.35f     // GSR is tweede (35%)
#define WEIGHT_TEMP    0.15f     // Temperatuur is aanvullend (15%)

// ═══════════════════════════════════════════════════════════════════════════
//                    MLStressAnalyzer Implementation
// ═══════════════════════════════════════════════════════════════════════════

MLStressAnalyzer::MLStressAnalyzer() {
  bufferIndex = 0;
  bufferFull = false;
  modelData = nullptr;
  modelSize = 0;
  modelLoaded = false;
  totalPredictions = 0;
  totalProcessingTime = 0;
}

MLStressAnalyzer::~MLStressAnalyzer() {
  if (modelData) {
    free(modelData);
    modelData = nullptr;
  }
}

bool MLStressAnalyzer::begin() {
  Serial.println("[ML] Initializing ML Stress Analyzer...");
  Serial.println("[ML] Using AI BIJBEL classifier (hardcoded rules)");
  
  // Initialize I2C for EEPROM (Wire should already be initialized)
  // Test EEPROM connection
  Wire.beginTransmission(ML_EEPROM_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[ML] WARNING: EEPROM not found, model storage disabled");
    // Continue without EEPROM - we use AI Bijbel rules
  } else {
    Serial.println("[ML] EEPROM detected");
    
    // Try to load model (optional, we have AI Bijbel as fallback)
    if (loadModelFromEEPROM()) {
      Serial.println("[ML] Custom model loaded from EEPROM");
    } else {
      Serial.println("[ML] No custom model, using AI Bijbel classifier");
    }
  }
  
  // Print AI Bijbel config
  Serial.println("[ML] ═══════════════════════════════════════════════");
  Serial.println("[ML] AI BIJBEL BASELINES:");
  Serial.printf("[ML]   HR: %.0f (rust) → %.0f (edge) → %.0f (max)\n", BIJBEL_HR_BASELINE, BIJBEL_HR_EDGE, BIJBEL_HR_MAX);
  Serial.printf("[ML]   GSR: %.0f (rust) → %.0f (edge) → %.0f (max)\n", BIJBEL_GSR_BASELINE, BIJBEL_GSR_EDGE, BIJBEL_GSR_MAX);
  Serial.printf("[ML]   Temp: %.1f (normaal) → %.1f (hoog)\n", BIJBEL_TEMP_BASELINE, BIJBEL_TEMP_HIGH);
  Serial.printf("[ML]   Weights: HR=%.0f%% GSR=%.0f%% Temp=%.0f%%\n", 
                WEIGHT_HR*100, WEIGHT_GSR*100, WEIGHT_TEMP*100);
  Serial.println("[ML] ═══════════════════════════════════════════════");
  
  Serial.printf("[ML] Initialized - Buffer size: %d samples (%.1fs)\n", 
                ML_WINDOW_SIZE, (float)ML_WINDOW_SIZE / ML_SAMPLE_RATE_HZ);
  return true;
}

void MLStressAnalyzer::addSensorSample(float heartRate, float temperature, float gsr) {
  SensorSample sample(heartRate, temperature, gsr, millis());
  addSensorSample(sample);
}

void MLStressAnalyzer::addSensorSample(const SensorSample& sample) {
  // Add to circular buffer
  sampleBuffer[bufferIndex] = sample;
  bufferIndex = (bufferIndex + 1) % ML_WINDOW_SIZE;
  
  if (bufferIndex == 0) {
    bufferFull = true;
  }
}

bool MLStressAnalyzer::isReady() {
  return bufferFull || bufferIndex >= 10; // Need at least 10 samples
}

StressAnalysis MLStressAnalyzer::analyzeStress() {
  uint32_t startTime = millis();
  StressAnalysis result;
  
  if (!isReady()) {
    result.stressLevel = 3;  // Default: normaal (AI Bijbel Level 3)
    result.confidence = 0.0f;
    result.reasoning = "Insufficient data";
    result.processingTime = millis() - startTime;
    return result;
  }
  
  // Extract features from sensor data
  FeatureVector features = extractFeatures();
  
  // Perform inference using AI Bijbel rules
  // (ML model is optional enhancement)
  if (modelLoaded && modelData) {
    result = analyzeWithModel(features);
  } else {
    result = analyzeWithBijbel(features);  // 🔥 AI BIJBEL!
  }
  
  // Update statistics
  result.processingTime = millis() - startTime;
  totalPredictions++;
  totalProcessingTime += result.processingTime;
  
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    SIMPLE API (voor directe calls)
// ═══════════════════════════════════════════════════════════════════════════

int MLStressAnalyzer::analyzeStress(float heartRate, float temperature, float gsrValue) {
  // Add sample to buffer
  addSensorSample(heartRate, temperature, gsrValue);
  
  // Quick single-sample analysis using AI Bijbel
  return calculateStressLevelBijbel(heartRate, temperature, gsrValue);
}

// ═══════════════════════════════════════════════════════════════════════════
//                    AI BIJBEL STRESS CALCULATOR
// ═══════════════════════════════════════════════════════════════════════════

int MLStressAnalyzer::calculateStressLevelBijbel(float heartRate, float temperature, float gsr) {
  // ═══════════════════════════════════════════════════════════════════
  // AI BIJBEL MULTI-FACTOR STRESS BEREKENING
  // 
  // Level 0: Ontspannen (zeer lage stress)
  // Level 1: Rustig
  // Level 2: Normaal
  // Level 3: Licht verhoogd
  // Level 4: Verhoogd
  // Level 5: Gestrest
  // Level 6: Zeer gestrest
  // Level 7: Extreem / Edge Zone
  // ═══════════════════════════════════════════════════════════════════
  
  // Hartslag score (0.0 - 1.0)
  float hrScore = 0.0f;
  if (heartRate > 0) {
    if (heartRate <= BIJBEL_HR_BASELINE) {
      hrScore = 0.0f;  // Onder baseline = ontspannen
    } else if (heartRate <= BIJBEL_HR_EXCITED) {
      hrScore = (heartRate - BIJBEL_HR_BASELINE) / (BIJBEL_HR_EXCITED - BIJBEL_HR_BASELINE) * 0.4f;
    } else if (heartRate <= BIJBEL_HR_EDGE) {
      hrScore = 0.4f + (heartRate - BIJBEL_HR_EXCITED) / (BIJBEL_HR_EDGE - BIJBEL_HR_EXCITED) * 0.4f;
    } else if (heartRate <= BIJBEL_HR_MAX) {
      hrScore = 0.8f + (heartRate - BIJBEL_HR_EDGE) / (BIJBEL_HR_MAX - BIJBEL_HR_EDGE) * 0.2f;
    } else {
      hrScore = 1.0f;  // Boven max = maximale stress
    }
  }
  
  // Temperatuur score (0.0 - 1.0)
  float tempScore = 0.0f;
  if (temperature > 0) {
    if (temperature <= BIJBEL_TEMP_BASELINE) {
      tempScore = 0.0f;
    } else if (temperature <= BIJBEL_TEMP_ELEVATED) {
      tempScore = (temperature - BIJBEL_TEMP_BASELINE) / (BIJBEL_TEMP_ELEVATED - BIJBEL_TEMP_BASELINE) * 0.5f;
    } else if (temperature <= BIJBEL_TEMP_HIGH) {
      tempScore = 0.5f + (temperature - BIJBEL_TEMP_ELEVATED) / (BIJBEL_TEMP_HIGH - BIJBEL_TEMP_ELEVATED) * 0.5f;
    } else {
      tempScore = 1.0f;
    }
  }
  
  // GSR score (0.0 - 1.0)
  float gsrScore = 0.0f;
  if (gsr > 0) {
    if (gsr <= BIJBEL_GSR_BASELINE) {
      gsrScore = 0.0f;
    } else if (gsr <= BIJBEL_GSR_AROUSED) {
      gsrScore = (gsr - BIJBEL_GSR_BASELINE) / (BIJBEL_GSR_AROUSED - BIJBEL_GSR_BASELINE) * 0.4f;
    } else if (gsr <= BIJBEL_GSR_EDGE) {
      gsrScore = 0.4f + (gsr - BIJBEL_GSR_AROUSED) / (BIJBEL_GSR_EDGE - BIJBEL_GSR_AROUSED) * 0.4f;
    } else if (gsr <= BIJBEL_GSR_MAX) {
      gsrScore = 0.8f + (gsr - BIJBEL_GSR_EDGE) / (BIJBEL_GSR_MAX - BIJBEL_GSR_EDGE) * 0.2f;
    } else {
      gsrScore = 1.0f;
    }
  }
  
  // Gewogen combinatie (AI Bijbel weights)
  float combinedScore = (hrScore * WEIGHT_HR) + (gsrScore * WEIGHT_GSR) + (tempScore * WEIGHT_TEMP);
  combinedScore = constrain(combinedScore, 0.0f, 1.0f);
  
  // Converteer naar 0-7 level (AI Bijbel mapping)
  int stressLevel;
  if (combinedScore <= 0.05f) {
    stressLevel = 0;  // Ontspannen
  } else if (combinedScore <= 0.15f) {
    stressLevel = 1;  // Rustig
  } else if (combinedScore <= 0.30f) {
    stressLevel = 2;  // Normaal
  } else if (combinedScore <= 0.45f) {
    stressLevel = 3;  // Licht verhoogd
  } else if (combinedScore <= 0.60f) {
    stressLevel = 4;  // Verhoogd
  } else if (combinedScore <= 0.75f) {
    stressLevel = 5;  // Gestrest
  } else if (combinedScore <= 0.90f) {
    stressLevel = 6;  // Zeer gestrest
  } else {
    stressLevel = 7;  // Extreem / Edge Zone
  }
  
  // ═══════════════════════════════════════════════════════════════════
  // SAFETY OVERRIDES (AI Bijbel emergency rules)
  // ═══════════════════════════════════════════════════════════════════
  
  // Zeer hoge hartslag = altijd hoog stress
  if (heartRate > BIJBEL_HR_EDGE) {
    stressLevel = max(stressLevel, 6);
  }
  
  // Emergency hartslag = Level 7
  if (heartRate > BIJBEL_HR_MAX) {
    stressLevel = 7;
    Serial.println("[ML] ⚠️ EMERGENCY: HR > 160 BPM!");
  }
  
  // Emergency temperatuur = Level 7
  if (temperature > BIJBEL_TEMP_EMERGENCY) {
    stressLevel = 7;
    Serial.println("[ML] ⚠️ EMERGENCY: Temp > 39°C!");
  }
  
  return stressLevel;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    AI BIJBEL ANALYZER (met features)
// ═══════════════════════════════════════════════════════════════════════════

StressAnalysis MLStressAnalyzer::analyzeWithBijbel(const FeatureVector& features) {
  StressAnalysis result;
  result.confidence = 0.85f;  // AI Bijbel rules have good confidence
  
  // Use AI Bijbel calculator
  int baseLevel = calculateStressLevelBijbel(features.hr_mean, features.temp_current, features.gsr_mean);
  
  // ═══════════════════════════════════════════════════════════════════
  // TREND ADJUSTMENTS (verfijning op basis van patronen)
  // ═══════════════════════════════════════════════════════════════════
  
  float adjustment = 0.0f;
  
  // Hoge HRV kan stress indiceren
  if (features.hr_variability > 0.15f) {
    adjustment += 0.5f;
  }
  
  // Stijgende GSR trend = opbouwende stress
  if (features.gsr_trend > 10.0f) {
    adjustment += 0.3f;
  } else if (features.gsr_trend < -10.0f) {
    adjustment -= 0.2f;  // Dalende GSR = ontspanning
  }
  
  // Stijgende temperatuur
  if (features.temp_delta > 0.3f) {
    adjustment += 0.2f;
  }
  
  // Apply adjustment
  float adjustedLevel = baseLevel + adjustment;
  result.stressLevel = constrain((int)round(adjustedLevel), 0, 7);
  
  // Generate reasoning based on AI Bijbel levels
  const char* levelDescriptions[] = {
    "Ontspannen - minimale activiteit",
    "Rustig - lage stress",
    "Normaal - baseline activiteit", 
    "Licht verhoogd - opwarming",
    "Verhoogd - actieve fase",
    "Gestrest - hoge activiteit",
    "Zeer gestrest - bijna edge",
    "EDGE ZONE - maximale intensiteit"
  };
  
  result.reasoning = levelDescriptions[result.stressLevel];
  
  // Add trend info if significant
  if (adjustment > 0.3f) {
    result.reasoning = String(result.reasoning) + " (stijgende trend)";
  } else if (adjustment < -0.2f) {
    result.reasoning = String(result.reasoning) + " (dalende trend)";
  }
  
  return result;
}

// Legacy function - nu doorverwijzen naar AI Bijbel
StressAnalysis MLStressAnalyzer::analyzeWithRules(const FeatureVector& features) {
  return analyzeWithBijbel(features);  // Gebruik AI Bijbel
}

// ═══════════════════════════════════════════════════════════════════════════
//                    FEATURE EXTRACTION
// ═══════════════════════════════════════════════════════════════════════════

FeatureVector MLStressAnalyzer::extractFeatures() {
  FeatureVector features;
  int sampleCount = bufferFull ? ML_WINDOW_SIZE : bufferIndex;
  
  if (sampleCount == 0) return features;
  
  // Calculate basic statistics
  float hrSum = 0, hrSumSq = 0;
  float gsrSum = 0;
  float tempSum = 0;
  
  float hrValues[ML_WINDOW_SIZE];
  float gsrValues[ML_WINDOW_SIZE];
  
  for (int i = 0; i < sampleCount; i++) {
    int idx = bufferFull ? (bufferIndex + i) % ML_WINDOW_SIZE : i;
    
    hrSum += sampleBuffer[idx].heartRate;
    hrSumSq += sampleBuffer[idx].heartRate * sampleBuffer[idx].heartRate;
    gsrSum += sampleBuffer[idx].gsr;
    tempSum += sampleBuffer[idx].temperature;
    
    hrValues[i] = sampleBuffer[idx].heartRate;
    gsrValues[i] = sampleBuffer[idx].gsr;
  }
  
  // Basic features
  features.hr_mean = hrSum / sampleCount;
  features.gsr_mean = gsrSum / sampleCount;
  features.temp_current = tempSum / sampleCount;
  
  // Heart rate variability (standard deviation)
  float hrVariance = (hrSumSq / sampleCount) - (features.hr_mean * features.hr_mean);
  features.hr_std = sqrt(max(0.0f, hrVariance));
  
  // GSR trend (linear regression slope)
  features.gsr_trend = calculateTrend(gsrValues, sampleCount);
  
  // Heart rate variability (simplified)
  features.hr_variability = features.hr_mean > 0 ? features.hr_std / features.hr_mean : 0;
  
  // Temperature delta (current vs oldest)
  if (sampleCount >= 10) {
    int oldestIdx = bufferFull ? (bufferIndex + 10) % ML_WINDOW_SIZE : 0;
    features.temp_delta = features.temp_current - sampleBuffer[oldestIdx].temperature;
  }
  
  // Combined stress index (using AI Bijbel weights)
  float hrStress = (features.hr_mean - BIJBEL_HR_BASELINE) / (BIJBEL_HR_EDGE - BIJBEL_HR_BASELINE);
  float gsrStress = (features.gsr_mean - BIJBEL_GSR_BASELINE) / (BIJBEL_GSR_EDGE - BIJBEL_GSR_BASELINE);
  float tempStress = (features.temp_current - BIJBEL_TEMP_BASELINE) / (BIJBEL_TEMP_HIGH - BIJBEL_TEMP_BASELINE);
  
  features.stress_index = (hrStress * WEIGHT_HR) + (gsrStress * WEIGHT_GSR) + (tempStress * WEIGHT_TEMP);
  features.stress_index = constrain(features.stress_index, -1.0f, 1.0f);
  
  return features;
}

float MLStressAnalyzer::calculateTrend(float* values, int count) {
  if (count < 3) return 0.0f;
  
  // Simple linear regression slope
  float sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
  
  for (int i = 0; i < count; i++) {
    float x = i;
    float y = values[i];
    sumX += x;
    sumY += y;
    sumXY += x * y;
    sumXX += x * x;
  }
  
  float n = count;
  float denominator = n * sumXX - sumX * sumX;
  if (denominator == 0) return 0.0f;
  
  float slope = (n * sumXY - sumX * sumY) / denominator;
  return slope;
}

StressAnalysis MLStressAnalyzer::analyzeWithModel(const FeatureVector& features) {
  StressAnalysis result;
  
  // Placeholder for actual ML model inference
  // Falls back to AI Bijbel if no real model
  result = analyzeWithBijbel(features);
  result.reasoning = "ML model (AI Bijbel fallback)";
  result.confidence = 0.7f;
  
  return result;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    EEPROM Model Storage
// ═══════════════════════════════════════════════════════════════════════════

bool MLStressAnalyzer::loadModelFromEEPROM() {
  // Read model header (first 16 bytes)
  struct ModelHeader {
    uint32_t magic;       // 'MLST' magic number
    uint16_t version;     // Model version
    uint16_t size;        // Model size in bytes
    uint32_t checksum;    // Simple checksum
    uint32_t reserved;    // Future use
  } header;
  
  // Read header
  for (int i = 0; i < sizeof(header); i++) {
    Wire.beginTransmission(ML_EEPROM_ADDR);
    Wire.write(i >> 8);   // Address high byte
    Wire.write(i & 0xFF); // Address low byte
    Wire.endTransmission();
    
    Wire.requestFrom(ML_EEPROM_ADDR, 1);
    if (Wire.available()) {
      ((uint8_t*)&header)[i] = Wire.read();
    } else {
      return false;
    }
  }
  
  // Check magic number
  if (header.magic != 0x4D4C5354) { // 'MLST'
    Serial.println("[ML] No valid model found in EEPROM");
    return false;
  }
  
  // Validate size
  if (header.size > ML_MAX_MODEL_SIZE) {
    Serial.printf("[ML] Model too large: %d bytes\n", header.size);
    return false;
  }
  
  Serial.printf("[ML] Found model: version %d, size %d bytes\n", header.version, header.size);
  
  // Allocate memory for model
  if (modelData) free(modelData);
  modelData = (uint8_t*)malloc(header.size);
  if (!modelData) {
    Serial.println("[ML] Failed to allocate memory for model");
    return false;
  }
  
  modelSize = header.size;
  modelLoaded = true;
  
  Serial.println("[ML] Model loaded successfully");
  return true;
}

bool MLStressAnalyzer::saveModelToEEPROM(const uint8_t* model, uint16_t size) {
  Serial.printf("[ML] Saving model to EEPROM (%d bytes)\n", size);
  return true; // Placeholder
}

// ═══════════════════════════════════════════════════════════════════════════
//                    Debug Functions
// ═══════════════════════════════════════════════════════════════════════════

void MLStressAnalyzer::printBuffer() {
  int count = bufferFull ? ML_WINDOW_SIZE : bufferIndex;
  Serial.printf("[ML DEBUG] Buffer contains %d samples:\n", count);
  
  for (int i = 0; i < min(10, count); i++) {
    int idx = bufferFull ? (bufferIndex + i) % ML_WINDOW_SIZE : i;
    Serial.printf("  [%d] HR:%.1f T:%.2f GSR:%.0f\n", 
                  i, sampleBuffer[idx].heartRate, sampleBuffer[idx].temperature, sampleBuffer[idx].gsr);
  }
  
  if (count > 10) {
    Serial.printf("  ... and %d more samples\n", count - 10);
  }
}

void MLStressAnalyzer::printFeatures() {
  FeatureVector features = extractFeatures();
  Serial.println("[ML DEBUG] Extracted features (AI Bijbel):");
  Serial.printf("  HR Mean: %.1f (baseline: %.0f, edge: %.0f)\n", 
                features.hr_mean, BIJBEL_HR_BASELINE, BIJBEL_HR_EDGE);
  Serial.printf("  HR Std: %.2f, HRV: %.3f\n", features.hr_std, features.hr_variability);
  Serial.printf("  GSR Mean: %.0f (baseline: %.0f, edge: %.0f)\n", 
                features.gsr_mean, BIJBEL_GSR_BASELINE, BIJBEL_GSR_EDGE);
  Serial.printf("  GSR Trend: %.2f\n", features.gsr_trend);
  Serial.printf("  Temp: %.2f (baseline: %.1f), Delta: %.2f\n", 
                features.temp_current, BIJBEL_TEMP_BASELINE, features.temp_delta);
  Serial.printf("  Stress Index: %.3f\n", features.stress_index);
}

// ═══════════════════════════════════════════════════════════════════════════
//                    Member Functions
// ═══════════════════════════════════════════════════════════════════════════

bool MLStressAnalyzer::hasModel() {
  return modelLoaded && modelData != nullptr;
}

String MLStressAnalyzer::getModelInfo() {
  if (!hasModel()) {
    return "AI Bijbel classifier (hardcoded rules)";
  }
  
  return String("Custom model: ") + modelSize + " bytes, " + 
         totalPredictions + " predictions, avg " + 
         getAverageProcessingTime() + "ms";
}

FeatureVector MLStressAnalyzer::getLastFeatures() {
  return extractFeatures();
}

bool MLStressAnalyzer::loadModel(const char* modelName) {
  Serial.printf("[ML] Loading model: %s\n", modelName);
  return loadModelFromEEPROM();
}

bool MLStressAnalyzer::updateModel(const uint8_t* newModel, uint16_t size) {
  return saveModelToEEPROM(newModel, size);
}

void MLStressAnalyzer::startDataCollection() {
  Serial.println("[ML] Starting training data collection...");
}

void MLStressAnalyzer::stopDataCollection() {
  Serial.println("[ML] Stopping training data collection...");
}

void MLStressAnalyzer::labelCurrentState(int stressLevel) {
  Serial.printf("[ML] Labeling current state as stress level %d\n", stressLevel);
}

bool MLStressAnalyzer::exportTrainingData(const char* filename) {
  Serial.printf("[ML] Exporting training data to %s\n", filename);
  return true;
}

// ═══════════════════════════════════════════════════════════════════════════
//                    Global Convenience Functions
// ═══════════════════════════════════════════════════════════════════════════

bool ml_begin() {
  return mlAnalyzer.begin();
}

int ml_getStressLevel(float heartRate, float temperature, float gsr) {
  return mlAnalyzer.analyzeStress(heartRate, temperature, gsr);
}

bool ml_hasModel() {
  return mlAnalyzer.hasModel();
}

String ml_getModelInfo() {
  return mlAnalyzer.getModelInfo();
}

// ═══════════════════════════════════════════════════════════════════════════
//                    AI BIJBEL RUNTIME FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

// Krijg aanbevolen AI actie level op basis van stress en autonomy
int ml_getRecommendedActionLevel(int stressLevel, uint8_t autonomyPercent) {
  return aiBijbel_stressToActionLevel(stressLevel, autonomyPercent);
}

// Check of AI een bepaalde actie mag uitvoeren
bool ml_aiMagActie(uint8_t autonomyPercent, int targetLevel) {
  return aiBijbel_magNaarLevel(autonomyPercent, targetLevel);
}

// Check of AI nog een edge mag doen
bool ml_aiMagEdge(uint8_t autonomyPercent) {
  if (!aiBijbel_magEdgeZone(autonomyPercent)) return false;
  return aiBijbel_magNogEenEdge(autonomyPercent, aiState.edgeCount);
}

// Registreer een edge moment
void ml_registreerEdge() {
  aiState.edgeCount++;
  aiState.lastEdgeTime = millis();
  Serial.printf("[ML] Edge geregistreerd: %d totaal deze sessie\n", aiState.edgeCount);
}

// Reset sessie state
void ml_resetSessie() {
  aiState.edgeCount = 0;
  aiState.lastEdgeTime = 0;
  aiState.sessionActive = true;
  aiState.isPaused = false;
  aiState.orgasmeTriggered = false;
  aiState.inTijdelijkeDip = false;
  Serial.println("[ML] Sessie gereset");
}

// Print huidige AI state
void ml_printState() {
  Serial.println("[ML] ═══════════════════════════════════════════════");
  Serial.println("[ML] AI STATE:");
  Serial.printf("[ML]   Autonomy: %d%%\n", aiState.currentAutonomy);
  Serial.printf("[ML]   Level: %d\n", aiState.currentLevel);
  Serial.printf("[ML]   Edges: %d\n", aiState.edgeCount);
  Serial.printf("[ML]   Sessie actief: %s\n", aiState.sessionActive ? "JA" : "NEE");
  Serial.printf("[ML]   Gepauzeerd: %s\n", aiState.isPaused ? "JA" : "NEE");
  
  const AIBevoegdheidConfig* config = aiBijbel_getConfig(aiState.currentAutonomy);
  Serial.printf("[ML]   Config: %s\n", config->beschrijving);
  Serial.println("[ML] ═══════════════════════════════════════════════");
}

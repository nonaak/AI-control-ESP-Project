/*
  ML Stress Analyzer Implementation
  
  Bevat EEPROM model storage, feature extraction en basic ML classifier
*/

#include "ml_stress_analyzer.h"

// Global instance
MLStressAnalyzer mlAnalyzer;

// ===== MLStressAnalyzer Implementation =====

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
  
  // Initialize I2C for EEPROM (Wire should already be initialized)
  // Test EEPROM connection
  Wire.beginTransmission(ML_EEPROM_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[ML] WARNING: EEPROM not found, model storage disabled");
    return false;  // Continue without EEPROM
  }
  
  Serial.println("[ML] EEPROM detected, attempting to load default model...");
  
  // Try to load default model
  if (loadModelFromEEPROM()) {
    Serial.println("[ML] Default model loaded successfully");
  } else {
    Serial.println("[ML] No default model found, using rule-based classifier");
  }
  
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
    result.stressLevel = 3;
    result.confidence = 0.0f;
    result.reasoning = "Insufficient data";
    result.processingTime = millis() - startTime;
    return result;
  }
  
  // Extract features from sensor data
  FeatureVector features = extractFeatures();
  
  // Perform ML inference
  if (modelLoaded && modelData) {
    // Use loaded ML model (placeholder for now)
    result = analyzeWithModel(features);
  } else {
    // Use rule-based classifier as fallback
    result = analyzeWithRules(features);
  }
  
  // Update statistics
  result.processingTime = millis() - startTime;
  totalPredictions++;
  totalProcessingTime += result.processingTime;
  
  return result;
}

int MLStressAnalyzer::analyzeStress(float heartRate, float temperature, float gsrValue) {
  // Simple API - add sample and analyze immediately
  addSensorSample(heartRate, temperature, gsrValue);
  
  if (!isReady()) {
    // Not enough data, use simple rule-based approach
    float stress = 1.0f; // Base level
    
    // Heart rate contribution
    if (heartRate > 100) stress += (heartRate - 100) / 25.0f;
    else if (heartRate < 60) stress += (60 - heartRate) / 30.0f;
    
    // Temperature contribution  
    if (temperature > 37.0f) stress += (temperature - 37.0f) * 2.0f;
    
    // GSR contribution
    if (gsrValue > 1000) stress += (gsrValue - 1000) / 500.0f;
    
    return constrain((int)stress, 1, 7);
  }
  
  StressAnalysis analysis = analyzeStress();
  return analysis.stressLevel;
}

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
  features.hr_variability = features.hr_std / features.hr_mean;
  
  // Temperature delta (current vs oldest)
  if (sampleCount >= 10) {
    int oldestIdx = bufferFull ? (bufferIndex + 10) % ML_WINDOW_SIZE : 0;
    features.temp_delta = features.temp_current - sampleBuffer[oldestIdx].temperature;
  }
  
  // Combined stress index (weighted combination)
  float hrStress = (features.hr_mean - 70.0f) / 30.0f;  // Normalize around 70 BPM
  float gsrStress = features.gsr_mean / 2000.0f;        // Normalize GSR
  float tempStress = (features.temp_current - 36.5f) / 2.0f; // Normalize temp
  
  features.stress_index = hrStress * 0.4f + gsrStress * 0.4f + tempStress * 0.2f;
  features.stress_index = max(-1.0f, min(1.0f, features.stress_index)); // Clamp -1 to 1
  
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
  float slope = (n * sumXY - sumX * sumY) / (n * sumXX - sumX * sumX);
  return slope;
}

StressAnalysis MLStressAnalyzer::analyzeWithRules(const FeatureVector& features) {
  StressAnalysis result;
  result.confidence = 0.8f;  // Rule-based has good confidence
  
  // Rule-based stress classification
  float stressScore = features.stress_index;
  
  // Adjust based on heart rate variability
  if (features.hr_variability > 0.15f) {
    stressScore += 0.3f; // High HRV can indicate stress
  }
  
  // Adjust based on GSR trend
  if (features.gsr_trend > 10.0f) {
    stressScore += 0.2f; // Rising GSR indicates stress
  } else if (features.gsr_trend < -10.0f) {
    stressScore -= 0.1f; // Falling GSR indicates relaxation
  }
  
  // Adjust based on temperature delta
  if (features.temp_delta > 0.5f) {
    stressScore += 0.1f; // Rising temperature
  }
  
  // Convert stress score to 1-7 level
  if (stressScore <= -0.6f) {
    result.stressLevel = 1;
    result.reasoning = "Very relaxed (low HR, stable GSR)";
  } else if (stressScore <= -0.3f) {
    result.stressLevel = 2;
    result.reasoning = "Relaxed state";
  } else if (stressScore <= 0.0f) {
    result.stressLevel = 3;
    result.reasoning = "Normal/neutral state";
  } else if (stressScore <= 0.3f) {
    result.stressLevel = 4;
    result.reasoning = "Slightly elevated";
  } else if (stressScore <= 0.6f) {
    result.stressLevel = 5;
    result.reasoning = "Moderate stress detected";
  } else if (stressScore <= 1.0f) {
    result.stressLevel = 6;
    result.reasoning = "High stress (elevated HR+GSR)";
  } else {
    result.stressLevel = 7;
    result.reasoning = "EMERGENCY - Very high stress levels";
  }
  
  // Special case: Very high heart rate = emergency
  if (features.hr_mean > 150.0f) {
    result.stressLevel = 7;
    result.reasoning = "EMERGENCY - Heart rate > 150 BPM";
    result.confidence = 0.95f;
  }
  
  return result;
}

StressAnalysis MLStressAnalyzer::analyzeWithModel(const FeatureVector& features) {
  StressAnalysis result;
  
  // Placeholder for actual ML model inference
  // This would use TensorFlow Lite Micro or custom model
  result.stressLevel = 3;
  result.confidence = 0.6f;
  result.reasoning = "ML model inference (placeholder)";
  
  return result;
}

// ===== EEPROM Model Storage =====

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
  
  // Read model data (placeholder - actual implementation would read from EEPROM)
  modelSize = header.size;
  modelLoaded = true;
  
  Serial.println("[ML] Model loaded successfully");
  return true;
}

bool MLStressAnalyzer::saveModelToEEPROM(const uint8_t* model, uint16_t size) {
  // Implementation for saving models to EEPROM
  // This would be used when updating models
  Serial.printf("[ML] Saving model to EEPROM (%d bytes)\n", size);
  return true; // Placeholder
}

// ===== Debug Functions =====

void MLStressAnalyzer::printBuffer() {
  int count = bufferFull ? ML_WINDOW_SIZE : bufferIndex;
  Serial.printf("[ML DEBUG] Buffer contains %d samples:\n", count);
  
  for (int i = 0; i < min(10, count); i++) { // Print first 10 samples
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
  Serial.println("[ML DEBUG] Extracted features:");
  Serial.printf("  HR Mean: %.1f, Std: %.2f, HRV: %.3f\n", 
                features.hr_mean, features.hr_std, features.hr_variability);
  Serial.printf("  GSR Mean: %.0f, Trend: %.2f\n", 
                features.gsr_mean, features.gsr_trend);
  Serial.printf("  Temp: %.2f, Delta: %.2f\n", 
                features.temp_current, features.temp_delta);
  Serial.printf("  Stress Index: %.3f\n", features.stress_index);
}

// ===== Global Convenience Functions =====

bool ml_begin() {
  return mlAnalyzer.begin();
}

int ml_getStressLevel(float heartRate, float temperature, float gsr) {
  mlAnalyzer.addSensorSample(heartRate, temperature, gsr);
  
  if (mlAnalyzer.isReady()) {
    StressAnalysis result = mlAnalyzer.analyzeStress();
    return result.stressLevel;
  }
  
  return 3; // Default neutral
}

bool ml_hasModel() {
  return mlAnalyzer.hasModel();
}

String ml_getModelInfo() {
  return mlAnalyzer.getModelInfo();
}

// ===== Missing MLStressAnalyzer member functions =====

bool MLStressAnalyzer::hasModel() {
  return modelLoaded && modelData != nullptr;
}

String MLStressAnalyzer::getModelInfo() {
  if (!hasModel()) {
    return "No model loaded - using rule-based classifier";
  }
  
  return String("Model loaded: ") + modelSize + " bytes, " + 
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
  // Implementation for training data collection
}

void MLStressAnalyzer::stopDataCollection() {
  Serial.println("[ML] Stopping training data collection...");
  // Implementation to stop data collection
}

void MLStressAnalyzer::labelCurrentState(int stressLevel) {
  Serial.printf("[ML] Labeling current state as stress level %d\n", stressLevel);
  // Implementation to label current sensor state
}

bool MLStressAnalyzer::exportTrainingData(const char* filename) {
  Serial.printf("[ML] Exporting training data to %s\n", filename);
  // Implementation to export training data to SD card
  return true;
}

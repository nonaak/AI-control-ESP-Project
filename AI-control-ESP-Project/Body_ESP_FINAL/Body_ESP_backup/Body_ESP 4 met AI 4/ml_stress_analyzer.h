/*
  ML Stress Analyzer Header
  
  ═══════════════════════════════════════════════════════════════════════════
  GEÏNTEGREERD MET AI BIJBEL - Consistente stress levels (0-7)
  ═══════════════════════════════════════════════════════════════════════════
  
  Stress Levels (AI Bijbel):
    0 - Ontspannen (recovery/cooldown)
    1 - Rustig (zeer langzaam)
    2 - Normaal (langzaam)
    3 - Licht verhoogd (medium langzaam)
    4 - Verhoogd (medium)
    5 - Gestrest (medium snel)
    6 - Zeer gestrest (snel)
    7 - Extreem / Edge Zone (maximum)
*/

#ifndef ML_STRESS_ANALYZER_H
#define ML_STRESS_ANALYZER_H

#include <Arduino.h>
#include <Wire.h>

// ═══════════════════════════════════════════════════════════════════════════
//                         CONFIGURATIE
// ═══════════════════════════════════════════════════════════════════════════

#define ML_WINDOW_SIZE      30      // Samples in sliding window
#define ML_SAMPLE_RATE_HZ   10      // Expected samples per second
#define ML_EEPROM_ADDR      0x50    // I2C address for AT24C256
#define ML_MAX_MODEL_SIZE   4096    // Max model size in bytes

// ═══════════════════════════════════════════════════════════════════════════
//                         DATA STRUCTURES
// ═══════════════════════════════════════════════════════════════════════════

// Single sensor sample
struct SensorSample {
  float heartRate;
  float temperature;
  float gsr;
  uint32_t timestamp;
  
  SensorSample() : heartRate(0), temperature(0), gsr(0), timestamp(0) {}
  SensorSample(float hr, float temp, float g, uint32_t ts) 
    : heartRate(hr), temperature(temp), gsr(g), timestamp(ts) {}
};

// Extracted features for ML
struct FeatureVector {
  // Heart rate features
  float hr_mean;
  float hr_std;
  float hr_variability;    // HRV indicator
  
  // GSR features
  float gsr_mean;
  float gsr_trend;         // Rising/falling trend
  
  // Temperature features
  float temp_current;
  float temp_delta;        // Change from baseline
  
  // Combined features
  float stress_index;      // Combined stress indicator
  
  FeatureVector() : hr_mean(0), hr_std(0), hr_variability(0), 
                    gsr_mean(0), gsr_trend(0), 
                    temp_current(0), temp_delta(0), stress_index(0) {}
};

// Analysis result
struct StressAnalysis {
  int stressLevel;           // 0-7 (AI Bijbel levels)
  float confidence;          // 0.0-1.0
  String reasoning;          // Why this level
  uint32_t processingTime;   // ms to compute
  
  StressAnalysis() : stressLevel(3), confidence(0), reasoning(""), processingTime(0) {}
};

// ═══════════════════════════════════════════════════════════════════════════
//                         MLStressAnalyzer CLASS
// ═══════════════════════════════════════════════════════════════════════════

class MLStressAnalyzer {
public:
  MLStressAnalyzer();
  ~MLStressAnalyzer();
  
  // ─── Initialization ───
  bool begin();
  
  // ─── Data Input ───
  void addSensorSample(float heartRate, float temperature, float gsr);
  void addSensorSample(const SensorSample& sample);
  
  // ─── Analysis ───
  bool isReady();
  StressAnalysis analyzeStress();
  
  // Simple API - direct stress level (0-7)
  int analyzeStress(float heartRate, float temperature, float gsrValue);
  
  // AI Bijbel stress calculator
  int calculateStressLevelBijbel(float heartRate, float temperature, float gsr);
  
  // ─── Model Management ───
  bool hasModel();
  String getModelInfo();
  bool loadModel(const char* modelName);
  bool updateModel(const uint8_t* newModel, uint16_t size);
  
  // ─── Training ───
  void startDataCollection();
  void stopDataCollection();
  void labelCurrentState(int stressLevel);
  bool exportTrainingData(const char* filename);
  
  // ─── Debug ───
  void printBuffer();
  void printFeatures();
  FeatureVector getLastFeatures();
  
  float getAverageProcessingTime() { 
    return totalPredictions > 0 ? (float)totalProcessingTime / totalPredictions : 0; 
  }

private:
  // Circular buffer for sensor samples
  SensorSample sampleBuffer[ML_WINDOW_SIZE];
  int bufferIndex;
  bool bufferFull;
  
  // Model storage
  uint8_t* modelData;
  uint16_t modelSize;
  bool modelLoaded;
  
  // Statistics
  uint32_t totalPredictions;
  uint32_t totalProcessingTime;
  
  // ─── Internal Functions ───
  FeatureVector extractFeatures();
  float calculateTrend(float* values, int count);
  StressAnalysis analyzeWithModel(const FeatureVector& features);
  StressAnalysis analyzeWithRules(const FeatureVector& features);  // Legacy
  StressAnalysis analyzeWithBijbel(const FeatureVector& features); // AI Bijbel!
  
  // ─── EEPROM Functions ───
  bool loadModelFromEEPROM();
  bool saveModelToEEPROM(const uint8_t* model, uint16_t size);
};

// ═══════════════════════════════════════════════════════════════════════════
//                         GLOBAL INSTANCE
// ═══════════════════════════════════════════════════════════════════════════

extern MLStressAnalyzer mlAnalyzer;

// ═══════════════════════════════════════════════════════════════════════════
//                    CONVENIENCE FUNCTIONS (Simple API)
// ═══════════════════════════════════════════════════════════════════════════

// Initialize ML system
bool ml_begin();

// Get stress level (0-7) from current sensor values
int ml_getStressLevel(float heartRate, float temperature, float gsr);

// Check if custom model is loaded
bool ml_hasModel();

// Get model info string
String ml_getModelInfo();

// ═══════════════════════════════════════════════════════════════════════════
//                    AI BIJBEL RUNTIME FUNCTIES
// ═══════════════════════════════════════════════════════════════════════════

// Krijg aanbevolen AI actie level op basis van stress en autonomy
int ml_getRecommendedActionLevel(int stressLevel, uint8_t autonomyPercent);

// Check of AI een bepaalde actie mag uitvoeren
bool ml_aiMagActie(uint8_t autonomyPercent, int targetLevel);

// Check of AI nog een edge mag doen
bool ml_aiMagEdge(uint8_t autonomyPercent);

// Registreer een edge moment
void ml_registreerEdge();

// Reset sessie state (bij start nieuwe sessie)
void ml_resetSessie();

// Print huidige AI state naar Serial
void ml_printState();

// ═══════════════════════════════════════════════════════════════════════════
//                    STRESS LEVEL DESCRIPTIONS
// ═══════════════════════════════════════════════════════════════════════════

// Helper macro voor stress level namen
#define ML_STRESS_NAME(level) ( \
  (level) == 0 ? "Ontspannen" : \
  (level) == 1 ? "Rustig" : \
  (level) == 2 ? "Normaal" : \
  (level) == 3 ? "Licht verhoogd" : \
  (level) == 4 ? "Verhoogd" : \
  (level) == 5 ? "Gestrest" : \
  (level) == 6 ? "Zeer gestrest" : \
  (level) == 7 ? "EDGE ZONE" : "Onbekend" \
)

// Helper macro voor stress level kleuren (RGB)
#define ML_STRESS_COLOR_R(level) ( \
  (level) == 0 ? 0 : \
  (level) == 1 ? 0 : \
  (level) == 2 ? 0 : \
  (level) == 3 ? 128 : \
  (level) == 4 ? 200 : \
  (level) == 5 ? 255 : \
  (level) == 6 ? 255 : \
  (level) == 7 ? 255 : 128 \
)

#define ML_STRESS_COLOR_G(level) ( \
  (level) == 0 ? 200 : \
  (level) == 1 ? 255 : \
  (level) == 2 ? 255 : \
  (level) == 3 ? 255 : \
  (level) == 4 ? 200 : \
  (level) == 5 ? 100 : \
  (level) == 6 ? 50 : \
  (level) == 7 ? 0 : 128 \
)

#define ML_STRESS_COLOR_B(level) ( \
  (level) == 0 ? 255 : \
  (level) == 1 ? 200 : \
  (level) == 2 ? 100 : \
  (level) == 3 ? 0 : \
  (level) == 4 ? 0 : \
  (level) == 5 ? 0 : \
  (level) == 6 ? 50 : \
  (level) == 7 ? 100 : 128 \
)

#endif // ML_STRESS_ANALYZER_H

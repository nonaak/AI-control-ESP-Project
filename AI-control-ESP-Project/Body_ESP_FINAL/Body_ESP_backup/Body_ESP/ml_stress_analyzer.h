/*
  ML Stress Analyzer - Machine Learning voor ESP32 CYD
  
  Analyseert sensor data (hartslag, temperatuur, GSR) om stress level te bepalen
  Gebruikt 32KB I2C EEPROM voor model storage
  Integreert met bestaande AI Stress Management systeem
*/

#ifndef ML_STRESS_ANALYZER_H
#define ML_STRESS_ANALYZER_H

#include <Arduino.h>
#include <Wire.h>

// ===== ML Configuration =====
#define ML_EEPROM_ADDR 0x50        // I2C adres van 32KB EEPROM
#define ML_EEPROM_SIZE 32768       // 32KB totaal
#define ML_MAX_MODEL_SIZE 16384    // 16KB voor ML model
#define ML_SAMPLE_RATE_HZ 10       // Sensor sampling rate
#define ML_WINDOW_SIZE 30          // 30 samples = 3 seconden geschiedenis

// ===== Sensor Data Structures =====
struct SensorSample {
  float heartRate;      // BPM (50-200 range)
  float temperature;    // °C (35-40 range) 
  float gsr;           // GSR value (0-4095 range)
  uint32_t timestamp;   // milliseconden
  
  // Constructor
  SensorSample() : heartRate(0), temperature(0), gsr(0), timestamp(0) {}
  SensorSample(float hr, float temp, float g, uint32_t ts) 
    : heartRate(hr), temperature(temp), gsr(g), timestamp(ts) {}
};

struct FeatureVector {
  // Basic features
  float hr_mean;        // Gemiddelde hartslag
  float hr_std;         // Standaard deviatie hartslag
  float temp_current;   // Huidige temperatuur
  float gsr_mean;       // Gemiddelde GSR
  float gsr_trend;      // GSR trend (stijgend/dalend)
  
  // Advanced features
  float hr_variability; // Hart rate variabiliteit (HRV)
  float temp_delta;     // Temperatuur verandering
  float stress_index;   // Gecombineerde stress indicator
  
  // Constructor
  FeatureVector() { memset(this, 0, sizeof(FeatureVector)); }
};

struct StressAnalysis {
  int stressLevel;      // 1-7 stress level output
  float confidence;     // 0.0-1.0 betrouwbaarheid
  String reasoning;     // Debug: waarom deze level?
  uint32_t processingTime; // Inference tijd in ms
  
  // Constructor  
  StressAnalysis() : stressLevel(3), confidence(0.5f), reasoning(""), processingTime(0) {}
};

// ===== ML Model Interface =====
class MLStressAnalyzer {
private:
  // Data buffers
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
  
  // Private methods
  bool loadModelFromEEPROM();
  bool saveModelToEEPROM(const uint8_t* model, uint16_t size);
  FeatureVector extractFeatures();
  float calculateHRV();
  float calculateTrend(float* values, int count);
  StressAnalysis analyzeWithRules(const FeatureVector& features);
  StressAnalysis analyzeWithModel(const FeatureVector& features);
  
public:
  // Constructor/Destructor
  MLStressAnalyzer();
  ~MLStressAnalyzer();
  
  // Initialization
  bool begin();
  bool loadModel(const char* modelName = "default");
  
  // Data input
  void addSensorSample(float heartRate, float temperature, float gsr);
  void addSensorSample(const SensorSample& sample);
  
  // ML Inference
  StressAnalysis analyzeStress();
  int analyzeStress(float heartRate, float temperature, float gsrValue); // Simple API
  bool isReady(); // Heeft genoeg data voor analyse?
  
  // Model management
  bool hasModel();
  String getModelInfo();
  bool updateModel(const uint8_t* newModel, uint16_t size);
  
  // Training data collection
  void startDataCollection();
  void stopDataCollection();
  void labelCurrentState(int stressLevel); // User feedback
  bool exportTrainingData(const char* filename);
  
  // Statistics
  uint32_t getPredictionCount() { return totalPredictions; }
  float getAverageProcessingTime() { return totalPredictions > 0 ? (float)totalProcessingTime / totalPredictions : 0; }
  
  // Debug
  void printBuffer();
  void printFeatures();
  FeatureVector getLastFeatures();
};

// ===== Convenience Functions =====

// Initialize ML system
bool ml_begin();

// Quick stress analysis from current sensors  
int ml_getStressLevel(float heartRate, float temperature, float gsr);

// Training mode
void ml_startTraining();
void ml_stopTraining(); 
void ml_labelStress(int level);

// Model management
bool ml_hasModel();
bool ml_loadModel(const char* name);
String ml_getModelInfo();

// Global instance
extern MLStressAnalyzer mlAnalyzer;

#endif // ML_STRESS_ANALYZER_H
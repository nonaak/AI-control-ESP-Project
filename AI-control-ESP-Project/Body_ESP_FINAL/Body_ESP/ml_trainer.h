/*
  ML Trainer - Complete ML training workflow
  
  Integreert:
  - Decision Tree (ID3)
  - Data parsing (.aly / .csv)
  - Model save/load (SD kaart)
  - Training progress tracking
  - AI-assisted annotation
*/

#ifndef ML_TRAINER_H
#define ML_TRAINER_H

#include <Arduino.h>
#include <vector>
#include "ml_decision_tree.h"
#include "ml_data_parser.h"

// ===== Training Configuration =====

struct TrainingConfig {
  int maxDepth;              // Decision tree max depth
  int minSamplesLeaf;        // Min samples per leaf node
  bool normalizeFeatures;    // Normalize features voor training
  
  TrainingConfig() : maxDepth(10), minSamplesLeaf(2), normalizeFeatures(true) {}
};

// ===== Training Status =====

struct TrainingStatus {
  bool isTraining;
  bool isComplete;
  bool hasError;
  String errorMessage;
  
  int totalSamples;
  int processedSamples;
  float currentAccuracy;
  uint32_t startTime;
  String currentPhase;
  
  TrainingStatus() : isTraining(false), isComplete(false), hasError(false),
                     totalSamples(0), processedSamples(0), currentAccuracy(0.0f), startTime(0) {}
};

// ===== ML Trainer Class =====

class MLTrainer {
private:
  DecisionTree* model;
  TrainingConfig config;
  TrainingStatus status;
  
  std::vector<TrainingSample> trainingData;
  std::vector<TrainingSample> validationData;
  
  // Helper functions
  void splitTrainValidation(const std::vector<TrainingSample>& allData, float validationRatio = 0.2f);
  float evaluateModel(const std::vector<TrainingSample>& testData);
  
public:
  MLTrainer();
  ~MLTrainer();
  
  // Configuration
  void setConfig(const TrainingConfig& cfg) { config = cfg; }
  TrainingConfig getConfig() { return config; }
  
  // Data loading
  bool loadAlyFile(const char* filename);
  bool loadMultipleAlyFiles(const char** filenames, int count);
  void clearData();
  
  // Training
  bool startTraining();
  void stopTraining();
  TrainingStatus getStatus() { return status; }
  
  // Model management
  bool saveModel(const char* filename);
  bool loadModel(const char* filename);
  DecisionTree* getModel() { return model; }
  bool hasModel() { return model && model->hasModel(); }
  
  // Prediction (voor AI annotation)
  int predict(const float features[9]);
  
  // AI-assisted annotation
  bool loadCsvForAnnotation(const char* filename, std::vector<TrainingSample>& samples);
  bool savePredictions(const char* outputFilename, const std::vector<TrainingSample>& samples);
};

// ===== Global Trainer Instance =====
extern MLTrainer mlTrainer;

#endif // ML_TRAINER_H

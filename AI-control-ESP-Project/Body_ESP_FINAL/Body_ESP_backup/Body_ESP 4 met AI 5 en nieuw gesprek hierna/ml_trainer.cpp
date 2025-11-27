/*
  ML Trainer Implementation
  
  Complete training workflow voor Decision Tree ML
*/

#include "ml_trainer.h"
#include <algorithm>
#include <SD.h>

// Global instance
MLTrainer mlTrainer;

// ===== Constructor / Destructor =====

MLTrainer::MLTrainer() {
  model = new DecisionTree(config.maxDepth, config.minSamplesLeaf);
}

MLTrainer::~MLTrainer() {
  if (model) {
    delete model;
    model = nullptr;
  }
}

// ===== Data Loading =====

bool MLTrainer::loadAlyFile(const char* filename) {
  Serial.printf("[TRAINER] Laden .aly bestand: %s\n", filename);
  
  std::vector<TrainingSample> samples;
  DatasetStats stats;
  
  if (!parseAlyFile(filename, samples, stats)) {
    Serial.println("[TRAINER] Fout: kan bestand niet laden");
    return false;
  }
  
  // Voeg samples toe aan training data
  trainingData.insert(trainingData.end(), samples.begin(), samples.end());
  
  Serial.printf("[TRAINER] Geladen: %d samples (totaal nu: %d)\n", 
                samples.size(), trainingData.size());
  
  return true;
}

bool MLTrainer::loadMultipleAlyFiles(const char** filenames, int count) {
  bool success = true;
  
  for (int i = 0; i < count; i++) {
    if (!loadAlyFile(filenames[i])) {
      success = false;
    }
  }
  
  return success && !trainingData.empty();
}

void MLTrainer::clearData() {
  trainingData.clear();
  validationData.clear();
  Serial.println("[TRAINER] Training data gewist");
}

// ===== Training =====

bool MLTrainer::startTraining() {
  if (trainingData.empty()) {
    Serial.println("[TRAINER] Fout: geen training data!");
    status.hasError = true;
    status.errorMessage = "Geen training data geladen";
    return false;
  }
  
  Serial.printf("[TRAINER] Start training met %d samples...\n", trainingData.size());
  
  status.isTraining = true;
  status.isComplete = false;
  status.hasError = false;
  status.startTime = millis();
  status.currentPhase = "Voorbereiden";
  status.totalSamples = trainingData.size();
  status.processedSamples = 0;
  
  // Split data in training/validation (80/20)
  status.currentPhase = "Splitsen data";
  splitTrainValidation(trainingData, 0.2f);
  
  Serial.printf("[TRAINER] Training: %d, Validation: %d\n", 
                trainingData.size(), validationData.size());
  
  // Normaliseer features indien gewenst
  if (config.normalizeFeatures) {
    status.currentPhase = "Normaliseren";
    for (auto& sample : trainingData) {
      normalizeFeatures(sample.features);
    }
    for (auto& sample : validationData) {
      normalizeFeatures(sample.features);
    }
  }
  
  // Train model
  status.currentPhase = "Trainen model";
  
  // Update config in model
  if (model) {
    delete model;
  }
  model = new DecisionTree(config.maxDepth, config.minSamplesLeaf);
  
  if (!model->train(trainingData)) {
    Serial.println("[TRAINER] Training mislukt!");
    status.isTraining = false;
    status.hasError = true;
    status.errorMessage = "Training gefaald";
    return false;
  }
  
  status.processedSamples = trainingData.size();
  
  // Evalueer model op validation set
  status.currentPhase = "Evalueren";
  status.currentAccuracy = evaluateModel(validationData);
  
  Serial.printf("[TRAINER] Training voltooid! Accuracy: %.1f%%\n", status.currentAccuracy * 100);
  
  status.isTraining = false;
  status.isComplete = true;
  
  return true;
}

void MLTrainer::stopTraining() {
  status.isTraining = false;
  Serial.println("[TRAINER] Training gestopt");
}

// ===== Helper Functions =====

void MLTrainer::splitTrainValidation(const std::vector<TrainingSample>& allData, float validationRatio) {
  trainingData.clear();
  validationData.clear();
  
  // Simple split: laatste validationRatio deel wordt validation
  int validationSize = (int)(allData.size() * validationRatio);
  int trainingSize = allData.size() - validationSize;
  
  for (int i = 0; i < trainingSize; i++) {
    trainingData.push_back(allData[i]);
  }
  
  for (int i = trainingSize; i < allData.size(); i++) {
    validationData.push_back(allData[i]);
  }
}

float MLTrainer::evaluateModel(const std::vector<TrainingSample>& testData) {
  if (testData.empty() || !model || !model->hasModel()) {
    return 0.0f;
  }
  
  int correct = 0;
  
  for (const auto& sample : testData) {
    int prediction = model->predict(sample.features);
    if (prediction == sample.label) {
      correct++;
    }
  }
  
  return (float)correct / testData.size();
}

// ===== Model Management =====

bool MLTrainer::saveModel(const char* filename) {
  if (!model || !model->hasModel()) {
    Serial.println("[TRAINER] Fout: geen model om op te slaan");
    return false;
  }
  
  Serial.printf("[TRAINER] Opslaan model naar: %s\n", filename);
  
  // Serialiseer model naar JSON
  String json = model->serialize();
  
  // Schrijf naar SD kaart
  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("[TRAINER] Fout: kan bestand niet schrijven");
    return false;
  }
  
  file.print(json);
  file.close();
  
  Serial.printf("[TRAINER] Model opgeslagen (%d bytes)\n", json.length());
  
  return true;
}

bool MLTrainer::loadModel(const char* filename) {
  Serial.printf("[TRAINER] Laden model van: %s\n", filename);
  
  File file = SD.open(filename);
  if (!file) {
    Serial.println("[TRAINER] Fout: kan bestand niet openen");
    return false;
  }
  
  // Lees JSON
  String json = "";
  while (file.available()) {
    json += (char)file.read();
  }
  file.close();
  
  Serial.printf("[TRAINER] Model JSON gelezen (%d bytes)\n", json.length());
  
  // Deserialize
  if (!model) {
    model = new DecisionTree();
  }
  
  if (!model->deserialize(json)) {
    Serial.println("[TRAINER] Fout: model deserialisatie mislukt");
    return false;
  }
  
  Serial.println("[TRAINER] Model succesvol geladen");
  
  return true;
}

// ===== Prediction =====

int MLTrainer::predict(const float features[9]) {
  if (!model || !model->hasModel()) {
    Serial.println("[TRAINER] Fout: geen model geladen");
    return -1;
  }
  
  return model->predict(features);
}

// ===== AI-Assisted Annotation =====

bool MLTrainer::loadCsvForAnnotation(const char* filename, std::vector<TrainingSample>& samples) {
  Serial.printf("[TRAINER] Laden .csv voor annotatie: %s\n", filename);
  
  DatasetStats stats;
  if (!parseCsvFile(filename, samples, stats)) {
    Serial.println("[TRAINER] Fout: kan .csv niet laden");
    return false;
  }
  
  // Check of we een model hebben
  if (!model || !model->hasModel()) {
    Serial.println("[TRAINER] Waarschuwing: geen model geladen, kan geen voorspellingen maken");
    return true; // Data is geladen, maar geen predictions
  }
  
  // Maak voorspellingen voor elk sample
  Serial.println("[TRAINER] Genereren AI voorspellingen...");
  
  for (auto& sample : samples) {
    // Normaliseer indien nodig
    float features[9];
    memcpy(features, sample.features, sizeof(features));
    
    if (config.normalizeFeatures) {
      normalizeFeatures(features);
    }
    
    // Predict
    sample.label = model->predict(features);
  }
  
  Serial.printf("[TRAINER] AI voorspellingen gegenereerd voor %d samples\n", samples.size());
  
  return true;
}

bool MLTrainer::savePredictions(const char* outputFilename, const std::vector<TrainingSample>& samples) {
  return saveAlyFile(outputFilename, samples);
}

/*
  ML Decision Tree - ID3 Algoritme voor ESP32
  
  Implementeert Decision Tree learning op sensor data
  Voor gebruik met 9 sensoren: Time,HR,Temp,GSR,Adem,Trust,SleevePos,Suction,Vibe
  Output: 7 stress levels (1-7)
  
  Stress Levels:
  1 = Very relaxed
  2 = Relaxed
  3 = Normal/neutral
  4 = Slightly elevated
  5 = Moderate stress
  6 = High stress
  7 = EMERGENCY
*/

#ifndef ML_DECISION_TREE_H
#define ML_DECISION_TREE_H

#include <Arduino.h>
#include <vector>

// ===== Data Structures =====

struct TrainingSample {
  float features[9];  // HR, Temp, GSR, Adem, Trust, SleevePos, Suction, Vibe, Time
  int label;          // 1-7 (7 stress levels)
  
  TrainingSample() : label(-1) {
    memset(features, 0, sizeof(features));
  }
};

struct DecisionNode {
  bool isLeaf;
  int label;              // Voor leaf nodes
  int featureIndex;       // Welke feature testen (0-8)
  float threshold;        // Split threshold
  DecisionNode* left;     // <= threshold
  DecisionNode* right;    // > threshold
  
  DecisionNode() : isLeaf(false), label(-1), featureIndex(-1), threshold(0.0f), left(nullptr), right(nullptr) {}
  
  ~DecisionNode() {
    if (left) delete left;
    if (right) delete right;
  }
};

// ===== Decision Tree Class =====

class DecisionTree {
private:
  DecisionNode* root;
  int maxDepth;
  int minSamplesLeaf;
  
  // Helper functions
  float calculateEntropy(const std::vector<TrainingSample>& samples);
  float calculateInformationGain(const std::vector<TrainingSample>& samples, int featureIdx, float threshold);
  void findBestSplit(const std::vector<TrainingSample>& samples, int& bestFeature, float& bestThreshold);
  DecisionNode* buildTree(const std::vector<TrainingSample>& samples, int depth);
  int predictNode(const DecisionNode* node, const float features[9]);
  int getMajorityLabel(const std::vector<TrainingSample>& samples);
  
  // Serialisatie helpers
  void serializeNode(const DecisionNode* node, String& json, int depth);
  DecisionNode* deserializeNode(const String& json, int& pos);
  
public:
  DecisionTree(int maxDepth = 10, int minSamplesLeaf = 2);
  ~DecisionTree();
  
  // Training
  bool train(const std::vector<TrainingSample>& trainingData);
  
  // Prediction
  int predict(const float features[9]);
  
  // Model persistence
  String serialize();
  bool deserialize(const String& json);
  
  // Info
  bool hasModel() { return root != nullptr; }
  void clear();
};

// ===== Helper Functions =====

// Feature namen voor debugging
const char* getFeatureName(int index);

// Data normalisatie helpers
void normalizeFeatures(float features[9]);
float normalizeHR(float hr);       // 40-200 BPM -> 0-1
float normalizeTemp(float temp);   // 30-40°C -> 0-1
float normalizeGSR(float gsr);     // 0-4095 -> 0-1
float normalizeAdem(float adem);   // 0-100% -> 0-1
// Trust, SleevePos, Suction, Vibe worden genormaliseerd in normalizeFeatures()

#endif // ML_DECISION_TREE_H
